#pragma once

// The fixes. Each install function changes the game's code and returns true, or, if that code isn't
// what this version of the plugin expects, changes nothing and returns false. Each fix's file
// explains the bug and how it's fixed.

bool InstallPickUpPhysicsFix();		// PickUpPhysics.cpp
bool InstallMagicMenuRefreshFix();	// MagicMenuRefresh.cpp
bool InstallTokenRemovalFix();		// TokenRemoval.cpp
bool InstallWholeGoldBountyFix();	// WholeGoldBounty.cpp
bool InstallUntranslatedTextFix();	// UntranslatedText.cpp
bool InstallSavedProjectileFix();	// SavedProjectiles.cpp
bool InstallMagicShaderRemovalFix();	// MagicShaderRemoval.cpp
bool InstallScriptEffectNamesFix();	// ScriptEffectNames.cpp
bool InstallItemShaderGlowFix();	// ItemShaderGlow.cpp
bool InstallFortifyAlchemyFix();	// FortifyAlchemy.cpp
