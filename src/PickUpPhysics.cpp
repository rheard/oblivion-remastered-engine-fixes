// Picking up an object a mod placed in the world crashed the game.
//
// When the player picks something up, the game sends Unreal its Reset_Saved_Data message for the
// object, with a flag set that has Unreal turn the object's physics back on. Unreal's handler for
// that message (0x0476B070) casts the object's actor to VPhysicsControllable and calls it without
// checking, and the actors the Remastered makes for objects a plugin placed have no physics: taking
// Move Quest Items' book from the shelf in First Edition crashed. The handler here does what the
// game's does, with the checks: with no physics there's nothing to turn back on, and only the
// object's saved data is reset. The rest of the game's code that uses the same physics interface
// checks first already.

#include "Fixes.h"
#include "Log.h"
#include "Patch.h"

namespace
{
	// Unreal's handler for Reset_Saved_Data: the handler queue (0x04919D40) calls it with its target,
	// the reference's form ID and the flag
	const std::uintptr_t kResetSavedData = 0x0476B070;
	const std::uint8_t kResetSavedDataStart[] = { 0x40, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x8B, 0xFA, 0x45, 0x84, 0xC0, 0x74, 0x41 };

	typedef void * (* _GetHandlerObject)(void * target);								// the target's +8
	typedef void * (* _GetPhysicsControllableClass)();									// UVPhysicsControllable
	typedef void * (* _CastToInterface)(void * object, void * interfaceClass);
	typedef void (* _SetSimulatePhysics)(void * component, bool simulate);
	typedef void * (* _GetSavedDataSubsystem)(void * worldContext);
	typedef void (* _ResetSavedData)(void * subsystem, std::uint32_t formID);

	int g_noPhysicsCount = 0;

	void ResetSavedData(void * target, std::uint32_t formID, bool turnPhysicsBackOn)
	{
		if(turnPhysicsBackOn)
		{
			void * object = target ? GameFunction <_GetHandlerObject>(0x00D890E0)(target) : nullptr;

			void * physics = nullptr;
			if(object)
				physics = GameFunction <_CastToInterface>(0x01115A40)(object, GameFunction <_GetPhysicsControllableClass>(0x046C4A20)());

			// IVPhysicsControllable's +0xA8 gives the component that simulates
			void * component = nullptr;
			if(physics)
				component = (*reinterpret_cast <void * (***)(void *)>(physics))[0xA8 / 8](physics);

			if(component)
			{
				GameFunction <_SetSimulatePhysics>(0x04893870)(component, true);
			}
			else if(g_noPhysicsCount < 10)
			{
				g_noPhysicsCount++;
				Log("Reset_Saved_Data for %08X: no physics to turn back on, skipped%s", formID,
					g_noPhysicsCount == 10 ? " (not logging this any more)" : "");
			}
		}

		if(void * subsystem = GameFunction <_GetSavedDataSubsystem>(0x048BA920)(nullptr))
			GameFunction <_ResetSavedData>(0x048CB0B0)(subsystem, formID);
	}
}

bool InstallPickUpPhysicsFix()
{
	return ReplaceFunction(kResetSavedData, reinterpret_cast <void *>(&ResetSavedData), kResetSavedDataStart, sizeof(kResetSavedDataStart));
}
