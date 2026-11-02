// Remastered Engine Fixes: an OBSE64 plugin for Oblivion Remastered that fixes engine bugs mods run
// into. README.md lists them for players; each fix's own file explains the bug and the fix.

#include <cstdint>
#include "Fixes.h"
#include "Log.h"
#include "Patch.h"
#include "PluginAPI.h"

namespace
{
	// the game version the addresses are for, as OBSE64 numbers it (MAKE_EXE_VERSION(1, 512, 105))
	const std::uint32_t kRuntime_1_512_105 = 0x12000690;

	const std::uint32_t kPluginVersion = (PLUGIN_VERSION_MAJOR << 24) | (PLUGIN_VERSION_MINOR << 16) | (PLUGIN_VERSION_PATCH << 8);

	struct Fix
	{
		const char	* name;
		bool		(* install)();
	};

	const Fix kFixes[] =
	{
		{ "Picking up objects mods placed", InstallPickUpPhysicsFix },
		{ "Spells removed while the magic menu is open", InstallMagicMenuRefreshFix },
		{ "Items whose scripts remove them", InstallTokenRemovalFix },
		{ "The player's bounty in whole gold", InstallWholeGoldBountyFix },
		{ "\"[NL]\" in front of untranslated text", InstallUntranslatedTextFix },
		{ "Saved spells whose mod is gone", InstallSavedProjectileFix },
		{ "Stopping a magic shader on an object with no actor", InstallMagicShaderRemovalFix },
		{ "The names of mods' script effects", InstallScriptEffectNamesFix },
		{ "Magic shaders' glow on items", InstallItemShaderGlowFix },
		{ "Fortify Alchemy and Luck in alchemy", InstallFortifyAlchemyFix },
	};
}

extern "C"
{
	__declspec(dllexport) OBSEPluginVersionData OBSEPlugin_Version =
	{
		OBSEPluginVersionData::kVersion,

		kPluginVersion,
		"Remastered Engine Fixes",
		"Ryan Heard",

		0,	// not address independent
		0,	// not structure independent
		{ kRuntime_1_512_105, 0 },

		0,	// any OBSE64
		0, 0, { 0 }
	};

	__declspec(dllexport) bool OBSEPlugin_Load(const OBSEInterface * obse)
	{
		InitPatching(obse);
		OpenLog(obse, "RemasteredEngineFixes.log");

		Log("Remastered Engine Fixes %s, game runtime %08X", PLUGIN_VERSION_STRING, obse->runtimeVersion);

		for(const Fix & fix : kFixes)
			Log("%s: %s", fix.name, fix.install() ? "fixed" : "the game's code isn't what this version expects, left alone");

		return true;
	}
}
