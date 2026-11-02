// Fortify Alchemy and Fortify Luck did nothing for the potions you brewed or the ingredients you ate.
//
// Luck counts toward every skill (0x068A2BD0: the skill plus 0.4 per point of Luck above 50, kept
// between 0 and 100). Mercantile and Sneak ask for both values as they are now, magic included. The
// alchemy menu (0x067A3840, through 0x065B17E0) and eating an ingredient (0x065A2D50) ask for the
// base values, so the game's rings, its Doomstone powers and any spell changed the number on the
// stats screen and nothing else. The altar in Frostcrag Spire looked like an exception only because
// it gives an ability: a fortifying ability goes into the player's base value itself (0x069A41B0).
//
// Both now ask for the values as they are, so Drain and Damage count too. The limit of 100 stays, and
// what comes with mastery still goes by the base value, as it does for every skill.
//
// The Unofficial Patch has worked around this since the original game: equipping an item that
// fortifies Alchemy also gives the player an ability of the same size, which reaches the base value.
// Its three Rings of Alchemy dispel their own enchantment as they're equipped, leaving just the
// ability. On its other items both stay, and with the enchantment now counting too the item would
// count twice, so while both are on the player only one of them does.

#include "Fixes.h"
#include "Patch.h"
#include <algorithm>
#include <iterator>

namespace
{
	const std::uintptr_t kAlchemyMenu_SkillCall = 0x067A453A;
	const std::uintptr_t kActor_GetBaseSkillWithLuck = 0x065B17E0;
	const std::uintptr_t kEatIngredient_SkillCall = 0x065A32EF;
	const std::uintptr_t kSkillWithLuck = 0x068A2BD0;
	const std::uintptr_t kActor_GetBaseValue = 0x065AEF60;
	const std::uintptr_t kMagicItem_GetFormID = 0x0664EEF0;
	const std::uintptr_t kPlayerSingleton = 0x0941F708;

	const int kActorValue_Luck = 7;
	const int kActorValue_Alchemy = 0x13;

	// where things are in the game's objects
	const std::size_t kActor_GetValue = 0x578 / sizeof(void *);
	const std::size_t kActor_MagicTarget = 0x178;
	const std::size_t kMagicTarget_GetEffects = 0x10 / sizeof(void *);
	const std::size_t kEffect_MagicItem = 0x10;
	const std::size_t kEffect_EffectItem = 0x18;
	const std::size_t kEffect_Applied = 0x20;
	const std::size_t kEffect_Finished = 0x21;
	const std::size_t kEffect_Magnitude = 0x28;
	const std::size_t kEffect_SpellType = 0x40;
	const std::size_t kEffectItem_ActorValue = 0x1C;

	const int kSpellType_Ability = 4;

	struct EffectNode
	{
		void		* effect;
		EffectNode	* next;
	};

	// The Unofficial Patch's abilities, by their form IDs within the patch, and the enchantments of
	// the items that give them.
	struct StandIn
	{
		std::uint32_t	ability;
		std::uint32_t	enchantment;
	};

	const StandIn kStandIns[] =
	{
		{ 0x004316, 0x0CA008 },	// Apron of the Master Artisan
		{ 0x004317, 0x0CA010 },	// Cowl of the Druid
		{ 0x004319, 0x0CA10E },	// Mantle of the Woodsman
		{ 0x00431B, 0x0CA017 },	// Ring of Wortcraft
		{ 0x00D032, 0x09DEBE },	// Ring of Wizardry
		{ 0x00D033, 0x0983FF },	// Base Ring of Alchemy
		{ 0x00D035, 0x098400 },	// Ring of Alchemy
		{ 0x00D037, 0x098401 },	// Grand Ring of Alchemy
	};

	typedef float (* _SkillWithLuck)(int skill, int luck);
	typedef int (* _Actor_GetBaseValue)(void * actor, int actorValue);
	typedef int (* _Actor_GetValue)(void * actor, int actorValue);
	typedef std::uint32_t (* _MagicItem_GetFormID)(void * item);
	typedef EffectNode * (* _MagicTarget_GetEffects)(void * target);

	template <typename T>
	T Field(void * object, std::size_t offset)
	{
		return *reinterpret_cast <T *>(reinterpret_cast <char *>(object) + offset);
	}

	// how much of the actor's Alchemy is an Unofficial Patch ability on top of its item's enchantment
	float CountedTwice(void * actor)
	{
		float abilities[std::size(kStandIns)] = {};
		float enchantments[std::size(kStandIns)] = {};

		void * target = reinterpret_cast <char *>(actor) + kActor_MagicTarget;
		auto getEffects = reinterpret_cast <_MagicTarget_GetEffects>(Field <void **>(target, 0)[kMagicTarget_GetEffects]);

		for(EffectNode * node = getEffects(target); node; node = node->next)
		{
			void * effect = node->effect;
			if(!effect || !Field <bool>(effect, kEffect_Applied) || Field <bool>(effect, kEffect_Finished))
				continue;

			void * effectItem = Field <void *>(effect, kEffect_EffectItem);
			if(!effectItem || Field <int>(effectItem, kEffectItem_ActorValue) != kActorValue_Alchemy)
				continue;

			const std::uint32_t formID = GameFunction <_MagicItem_GetFormID>(kMagicItem_GetFormID)(Field <void *>(effect, kEffect_MagicItem));
			const bool isAbility = Field <int>(effect, kEffect_SpellType) == kSpellType_Ability;

			for(std::size_t i = 0; i < std::size(kStandIns); i++)
			{
				if(formID == kStandIns[i].enchantment)
					enchantments[i] += Field <float>(effect, kEffect_Magnitude);
				else if(isAbility && (formID >> 24) && (formID & 0x00FFFFFF) == kStandIns[i].ability)
					abilities[i] += Field <float>(effect, kEffect_Magnitude);
			}
		}

		float twice = 0;
		for(std::size_t i = 0; i < std::size(kStandIns); i++)
			twice += std::min(abilities[i], enchantments[i]);

		return twice;
	}

	float AlchemySkill(void * actor)
	{
		auto getValue = reinterpret_cast <_Actor_GetValue>(Field <void **>(actor, 0)[kActor_GetValue]);

		const int alchemy = getValue(actor, kActorValue_Alchemy) - int(CountedTwice(actor));
		const int luck = getValue(actor, kActorValue_Luck);

		return GameFunction <_SkillWithLuck>(kSkillWithLuck)(alchemy, luck);
	}

	float AlchemyMenu_Skill(void * actor, int)
	{
		return AlchemySkill(actor);
	}

	// Eating an ingredient reads the eater's base values itself and only passes the values here, not
	// who's eating. If they're the player's, it's taken to be the player. An NPC with exactly the same
	// two would be too.
	float EatIngredient_Skill(int skill, int luck)
	{
		void * player = *reinterpret_cast <void **>(GameAddress(kPlayerSingleton));
		auto getBaseValue = GameFunction <_Actor_GetBaseValue>(kActor_GetBaseValue);

		if(player && skill == getBaseValue(player, kActorValue_Alchemy) && luck == getBaseValue(player, kActorValue_Luck))
			return AlchemySkill(player);

		return GameFunction <_SkillWithLuck>(kSkillWithLuck)(skill, luck);
	}
}

bool InstallFortifyAlchemyFix()
{
	return RedirectCall(kAlchemyMenu_SkillCall, kActor_GetBaseSkillWithLuck, reinterpret_cast <void *>(&AlchemyMenu_Skill))
		&& RedirectCall(kEatIngredient_SkillCall, kSkillWithLuck, reinterpret_cast <void *>(&EatIngredient_Skill));
}
