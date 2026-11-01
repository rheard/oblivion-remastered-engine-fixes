// Stopping a magic shader on an object with no Unreal actor crashed the game.
//
// When a shader on an object stops, or its effect goes, the game sends Unreal a message, which
// Unreal handles later (Remove_Magic_Shader_Hit_Effect, 0x0476A410). The message holds the object's
// pairing entry, and the entry its actor (+8), which is empty before Unreal has made the actor and
// after it has gone: an item whose cell is on its way in or out, or one just picked up. The handler
// checks for that, but only after asking the actor for its world (0x02DC0350), which reads through
// the empty pointer. Starting a shader has the check first (Trigger_Magic_Shader_Hit_Effect,
// 0x04775BA0), so there the object just doesn't show it.
//
// The game's own shaders are mostly on characters, so it rarely gets here. Detect Item plays one on
// every item around the player, and crashed the game the second time it was cast.
//
// The handler's call now goes through a check, an empty actor having no world, and with no world
// the handler does nothing: the effect went with the actor.

#include "Fixes.h"
#include "Patch.h"

namespace
{
	const std::uintptr_t kRemoveEffect_GetWorldCall = 0x0476A463;
	const std::uintptr_t kObject_GetWorld = 0x02DC0350;

	typedef void * (* _Object_GetWorld)(void * object);

	void * Object_GetWorld(void * object)
	{
		return object ? GameFunction <_Object_GetWorld>(kObject_GetWorld)(object) : nullptr;
	}
}

bool InstallMagicShaderRemovalFix()
{
	return RedirectCall(kRemoveEffect_GetWorldCall, kObject_GetWorld, reinterpret_cast <void *>(&Object_GetWorld));
}
