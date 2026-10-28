// A save holding a spell in flight crashed as it loaded once the spell's mod was gone.
//
// A save holds the magic projectiles in flight. Loading one, each magic projectile's InitLoadGame
// (vtable slot 22: 0x068A8850, which the Ball, Bolt and Fog classes' own call first) turns the form
// IDs saved for its caster and its spell into the caster's MagicCaster (+0x158, 0x06652E80) and the
// spell's MagicItem (+0x160, 0x0664EE60). A projectile whose caster is gone it puts on the save
// loader's list of forms to delete (0x06703280), and the loader deletes them (0x06714E80) once every
// form it loaded has been through slots 22 and 23. A spell that's gone crashes the game instead:
// 0x068A8850 goes on to use its effects (0x0664C720, 0x0664EC60) and its effect's model (slot 89,
// 0x068A8C70), and the classes' own parts its light and its first effect (0x068A95B0), none of
// them checking. So taking out a mod whose scripts cast spells crashed saves made while it ran:
// Enchantment Restore Over Time casts one at the player every few seconds.
//
// 0x068A8850 also has Unreal show the projectile when the spell's first effect is on Target (its
// pairable's slot 1, 0x068A8FD0, sends Spawn_Magic_Projectile). Unreal makes the actor once the
// effect's assets have loaded, and by then a projectile deleted with the load has sent its
// Destroy_Magic_Projectile, which found no actor, so the effect would stay in the world where the
// projectile was. A projectile whose caster is gone goes the same way.
//
// So a projectile the game deletes as the save loads, its spell or its caster gone, is given Summon
// Skeleton to be built with, a spell on Self, which Unreal isn't asked to show, and no caster: the
// game builds it, then deletes it with the load. The spell's effect is lost, as it would be had it
// missed.

#include "Fixes.h"
#include "Log.h"
#include "Patch.h"

namespace
{
	const std::uintptr_t kProjectileInitLoad = 0x068A8850;
	const std::uint8_t kProjectileInitLoadStart[] =
	{
		0x48, 0x89, 0x5C, 0x24, 0x10,					// mov [rsp + 0x10], rbx
		0x48, 0x89, 0x6C, 0x24, 0x18,					// mov [rsp + 0x18], rbp
		0x48, 0x89, 0x74, 0x24, 0x20,					// mov [rsp + 0x20], rsi
		0x57,											// push rdi
		0x48, 0x83, 0xEC, 0x30,							// sub rsp, 0x30
		0x48, 0x8B, 0xF9,								// mov rdi, rcx
		0x41, 0x8B, 0xF0,								// mov esi, r8d
		0x8B, 0x89, 0x58, 0x01, 0x00, 0x00,				// mov ecx, [rcx + 0x158]
		0x8B, 0xEA,										// mov ebp, edx
		0xE8, 0x09, 0xA6, 0xDA, 0xFF,					// call 0x06652E80 (the caster)
		0x8B, 0x8F, 0x60, 0x01, 0x00, 0x00,				// mov ecx, [rdi + 0x160]
		0x48, 0x89, 0x87, 0x58, 0x01, 0x00, 0x00,		// mov [rdi + 0x158], rax
		0xE8, 0xD7, 0x65, 0xDA, 0xFF,					// call 0x0664EE60 (the spell)
	};
	const std::size_t kProjectileInitLoadPrologue = 20;	// up to the first call, run from the trampoline

	// at its end: no caster or no spell, and the projectile goes on the list to delete
	const std::uintptr_t kProjectileDeleteCheck = 0x068A89A4;
	const std::uint8_t kProjectileDeleteCheckCode[] =
	{
		0x48, 0x83, 0xBF, 0x58, 0x01, 0x00, 0x00, 0x00,	// cmp qword ptr [rdi + 0x158], 0
		0x74, 0x0A,										// je (the call)
		0x48, 0x83, 0xBF, 0x60, 0x01, 0x00, 0x00, 0x00,	// cmp qword ptr [rdi + 0x160], 0
		0x75, 0x0F,										// jne (past it)
		0x48, 0x8B, 0x0D, 0xC1, 0xA4, 0xBD, 0x02,		// mov rcx, [0x09482E80] (the save loader)
		0x48, 0x8B, 0xD7,								// mov rdx, rdi
		0xE8, 0xB9, 0xA8, 0xE5, 0xFF,					// call 0x06703280
	};

	const std::uintptr_t kLookupMagicCaster = 0x06652E80;
	const std::uintptr_t kLookupMagicItem = 0x0664EE60;
	const std::size_t kProjectileOffset_Caster = 0x158;
	const std::size_t kProjectileOffset_Spell = 0x160;
	const std::size_t kFormOffset_FormID = 0x10;
	const std::uint32_t kStandInSpell = 0x000AA05F;	// Summon Skeleton, Oblivion.esm

	typedef void (* _ProjectileInitLoad)(void * projectile, std::uint32_t changeFlags, std::uint32_t unk);
	typedef void * (* _LookupMagicCaster)(std::uint32_t formID);
	typedef void * (* _LookupMagicItem)(std::uint32_t formID);

	_ProjectileInitLoad g_projectileInitLoad = nullptr;
	int g_deletedCount = 0;

	void ProjectileInitLoad(void * projectile, std::uint32_t changeFlags, std::uint32_t unk)
	{
		// still the saved form IDs, which 0x068A8850 replaces with what they're for
		auto * data = static_cast <std::uint8_t *>(projectile);
		auto & caster = *reinterpret_cast <std::uint64_t *>(data + kProjectileOffset_Caster);
		auto & spell = *reinterpret_cast <std::uint64_t *>(data + kProjectileOffset_Spell);

		// looked up as 0x068A8850 does next (for a caster that isn't an actor, the lookup gives it what
		// it needs to cast, if it hasn't that already)
		auto lookupMagicItem = GameFunction <_LookupMagicItem>(kLookupMagicItem);
		bool spellGone = !lookupMagicItem(std::uint32_t(spell));
		if(spellGone || !GameFunction <_LookupMagicCaster>(kLookupMagicCaster)(std::uint32_t(caster)))
		{
			bool standIn = lookupMagicItem(kStandInSpell) != nullptr;

			if(g_deletedCount < 10)
			{
				g_deletedCount++;
				Log("magic projectile %08X in the save, cast by %08X with %08X: its %s isn't loaded, %s%s",
					*reinterpret_cast <std::uint32_t *>(data + kFormOffset_FormID), std::uint32_t(caster), std::uint32_t(spell),
					spellGone ? "spell" : "caster",
					standIn ? "so it's built with Summon Skeleton, which Unreal doesn't show, for the game to delete as the save loads"
						: "and Summon Skeleton, to build it with, isn't either: left alone",
					g_deletedCount == 10 ? " (not logging this any more)" : "");
			}

			if(standIn)
			{
				caster = 0;
				spell = kStandInSpell;
			}
		}

		g_projectileInitLoad(projectile, changeFlags, unk);
	}
}

bool InstallSavedProjectileFix()
{
	return CodeMatches(kProjectileInitLoad, kProjectileInitLoadStart, sizeof(kProjectileInitLoadStart)) &&
		CodeMatches(kProjectileDeleteCheck, kProjectileDeleteCheckCode, sizeof(kProjectileDeleteCheckCode)) &&
		HookFunction(kProjectileInitLoad, reinterpret_cast <void *>(&ProjectileInitLoad), kProjectileInitLoadStart,
			kProjectileInitLoadPrologue, reinterpret_cast <void **>(&g_projectileInitLoad));
}
