// A script effect never showed the magnitude in its record in the magic menus.
//
// The menus show an effect with a row the game fills in for it (0x06880150): its magnitude, if it has
// one and its magic effect isn't marked as having none, in the unit its magic effect gives, and, when
// that's feet, the same number again as its area, as Detect Life shows. Script Effect is marked as
// having none, and no script effect in the game has one. A mod can still put one in a spell's record,
// for a script that works to that magnitude, and nothing told the player what it was.
//
// A script effect with a magnitude in its record now shows it, in the unit of the effect it looks like
// (the visual effect it names). The flag itself stays as it is, on the one Script Effect that every
// scripted spell shares, since the game also reads it for more than showing an effect. Script effects
// with no magnitude, which is all of the game's, look as they always did.
//
// The list of active effects then writes over a row's magnitude with the running effect's, or with
// nothing for one marked as having none, so there it's put back as the row is added.

#include "Fixes.h"
#include "Patch.h"

namespace
{
	const std::uintptr_t kFillRow = 0x06880150;
	const std::uintptr_t kActiveEffects_AddRowCall = 0x06775C24;
	const std::uintptr_t kRows_Add = 0x0675DF70;
	const std::uintptr_t kMagicEffect_Find = 0x06887720;

	const std::uint8_t kFillRowStart[] =
	{
		0x48, 0x89, 0x5C, 0x24, 0x08,	// mov [rsp+8], rbx
		0x48, 0x89, 0x6C, 0x24, 0x18,	// mov [rsp+18h], rbp
		0x48, 0x89, 0x54, 0x24, 0x10	// mov [rsp+10h], rdx
	};

	// where things are in the game's objects
	const std::size_t kEffect_Code = 0x08;
	const std::size_t kEffect_Magnitude = 0x0C;
	const std::size_t kEffect_Script = 0x20;
	const std::size_t kScript_VisualEffect = 0x18;
	const std::size_t kMagicEffect_Flags = 0xB0;
	const std::size_t kRow_Code = 0x04;
	const std::size_t kRow_Magnitude = 0x50;
	const std::size_t kRow_MagnitudeUnit = 0x54;
	const std::size_t kRow_Area = 0x58;

	const std::int32_t kScriptEffectCode = 0x46464553;	// SEFF
	const std::int32_t kIngredient = 8;					// a kind of magic item: its effects aren't known
	const std::uint32_t kFlag_MagnitudeIsPercent = 0x00000008;
	const std::uint32_t kFlag_MagnitudeIsFeet = 0x80000000;
	const std::int32_t kUnit_Percent = 0;				// EModernMagnitudeType
	const std::int32_t kUnit_Feet = 2;
	const std::int32_t kUnit_Raw = 3;

	typedef void * (* _FillRow)(void * effect, void * row, std::int32_t itemType, float effectiveness, char constant, char poison, char scroll);
	typedef void (* _Rows_Add)(void * rows, void * row);
	typedef void * (* _MagicEffect_Find)(std::int32_t code);

	_FillRow s_fillRow = nullptr;

	// the magnitude of the script effect whose row was last filled in, or 0 for any other effect
	std::int32_t s_filledMagnitude = 0;

	template <typename T>
	T & Field(void * object, std::size_t offset)
	{
		return *reinterpret_cast <T *>(reinterpret_cast <char *>(object) + offset);
	}

	std::uint32_t VisualEffectFlags(void * effect)
	{
		void * script = Field <void *>(effect, kEffect_Script);
		if (!script)
			return 0;

		void * visualEffect = GameFunction <_MagicEffect_Find>(kMagicEffect_Find)(Field <std::int32_t>(script, kScript_VisualEffect));
		return visualEffect ? Field <std::uint32_t>(visualEffect, kMagicEffect_Flags) : 0;
	}

	void * FillRow(void * effect, void * row, std::int32_t itemType, float effectiveness, char constant, char poison, char scroll)
	{
		void * result = s_fillRow(effect, row, itemType, effectiveness, constant, poison, scroll);

		s_filledMagnitude = 0;
		const std::int32_t magnitude = Field <std::int32_t>(effect, kEffect_Magnitude);
		if (Field <std::int32_t>(effect, kEffect_Code) != kScriptEffectCode || magnitude <= 0 || itemType == kIngredient)
			return result;

		s_filledMagnitude = magnitude;
		Field <std::int32_t>(row, kRow_Magnitude) = magnitude;

		const std::uint32_t flags = VisualEffectFlags(effect);
		if (flags & kFlag_MagnitudeIsFeet)
		{
			Field <std::int32_t>(row, kRow_MagnitudeUnit) = kUnit_Feet;
			Field <std::int32_t>(row, kRow_Area) = magnitude;
		}
		else
		{
			Field <std::int32_t>(row, kRow_MagnitudeUnit) = (flags & kFlag_MagnitudeIsPercent) ? kUnit_Percent : kUnit_Raw;
		}

		return result;
	}

	void AddActiveRow(void * rows, void * row)
	{
		if (s_filledMagnitude > 0 && Field <std::int32_t>(row, kRow_Code) == kScriptEffectCode)
			Field <std::int32_t>(row, kRow_Magnitude) = s_filledMagnitude;

		GameFunction <_Rows_Add>(kRows_Add)(rows, row);
	}
}

bool InstallScriptEffectMagnitudeFix()
{
	return HookFunction(kFillRow, reinterpret_cast <void *>(&FillRow), kFillRowStart, sizeof(kFillRowStart), reinterpret_cast <void **>(&s_fillRow))
		&& RedirectCall(kActiveEffects_AddRowCall, kRows_Add, reinterpret_cast <void *>(&AddActiveRow));
}
