// A magic shader's glow only showed on characters, weapons and shields, not on other items.
//
// Unreal draws a shader on an object in two parts: the particles of the blueprint its form asset names,
// and a glow over the object's meshes, the form's overlay material. The second is done by the game's
// status effect component (BPC_StatusEffect, VOblivionRuntimeSettings' StatusEffectComponentClass),
// which listens to its owner for shaders starting and stopping. Only characters, weapons (shields are
// weapons here) and worn clothes make one for themselves as they begin play (0x0484D640, 0x0484E3A0,
// 0x047F2BE0), and the component only starts listening by itself if its owner is a character or a
// weapon. So a shader on a tomato had particles at most. In the original game shaders showed on
// anything, and Detect Item plays one on every item around the player.
//
// Starting a shader on any other object now first gives it the component, as those make theirs, and
// connects it to the object as the component would connect itself to a weapon. It stays for as long as
// the object's actor does, and it handles everything from there: finding the meshes, Nanite, fading the
// glow in and out, and taking it off again.

#include "Fixes.h"
#include "Patch.h"

namespace
{
	const std::uintptr_t kStartEffect_SpawnCall = 0x04773260;
	const std::uintptr_t kSpawnEffect = 0x04772910;

	const std::uintptr_t kRuntimeSettings_StaticClass = 0x046AD100;
	const std::uintptr_t kStatusEffectTarget_StaticClass = 0x046F9EE0;
	const std::uintptr_t kPairedPawn_StaticClass = 0x046B6FA0;
	const std::uintptr_t kActor_AddComponentByClass = 0x02DE3140;
	const std::uintptr_t kComponent_Register = 0x02FF3B10;
	const std::uintptr_t kName_FromText = 0x00F3B740;
	const std::uintptr_t kWeakPointer_Set = 0x01125C20;
	const std::uintptr_t kMulticastDelegate_Add = 0x0484B7E0;
	const std::uintptr_t kIdentityTransform = 0x09011500;

	// where things are in the engine's objects
	const std::size_t kObject_Class = 0x10;
	const std::size_t kClass_BaseChain = 0x30;
	const std::size_t kClass_BaseChainLast = 0x38;
	const std::size_t kClass_DefaultObject = 0x110;
	const std::size_t kActor_FindComponentByClass = 0x6F0 / sizeof(void *);
	const std::size_t kRuntimeSettings_StatusEffectComponentClass = 0x68;
	const std::size_t kStatusEffectTarget_OnStart = 0x2B0;
	const std::size_t kStatusEffectTarget_OnStop = 0x2C0;

	const char kComponent_StartFunction[] = "VStatusEffectTarget OnTextureEffectStart";
	const char kComponent_StopFunction[] = "VStatusEffectTarget OnTextureEffectStop";
	const int kNameFindOrAdd = 1;

	struct ScriptDelegate
	{
		std::uint64_t object;	// a weak pointer
		std::uint64_t function;	// a name
	};

	typedef void * (* _SpawnEffect)(void * target, void * form);
	typedef char * (* _StaticClass)();
	typedef void * (* _Actor_AddComponentByClass)(void * actor, void * componentClass, bool manualAttachment, const void * transform, bool deferFinish);
	typedef void (* _Component_Register)(void * component);
	typedef std::uint64_t * (* _Name_FromText)(std::uint64_t * name, const char * text, int findType);
	typedef void (* _WeakPointer_Set)(std::uint64_t * pointer, void * object);
	typedef void (* _MulticastDelegate_Add)(void * delegate, const ScriptDelegate * added);
	typedef void * (* _Actor_FindComponentByClass)(void * actor, void * componentClass);

	template <typename T>
	T Field(void * object, std::size_t offset)
	{
		return *reinterpret_cast <T *>(reinterpret_cast <char *>(object) + offset);
	}

	bool IsA(void * object, std::uintptr_t staticClassRva)
	{
		char * theirs = Field <char *>(object, kObject_Class);
		char * wanted = GameFunction <_StaticClass>(staticClassRva)();
		const int last = Field <int>(wanted, kClass_BaseChainLast);

		return last <= Field <int>(theirs, kClass_BaseChainLast) &&
			Field <char **>(theirs, kClass_BaseChain)[last] == wanted + kClass_BaseChain;
	}

	void Connect(void * target, std::size_t delegateOffset, void * component, const char * function)
	{
		ScriptDelegate added;
		GameFunction <_WeakPointer_Set>(kWeakPointer_Set)(&added.object, component);
		GameFunction <_Name_FromText>(kName_FromText)(&added.function, function, kNameFindOrAdd);
		GameFunction <_MulticastDelegate_Add>(kMulticastDelegate_Add)(reinterpret_cast <char *>(target) + delegateOffset, &added);
	}

	void GiveStatusEffectComponent(void * target)
	{
		if (!IsA(target, kStatusEffectTarget_StaticClass) || IsA(target, kPairedPawn_StaticClass))
			return;

		void * settings = Field <void *>(GameFunction <_StaticClass>(kRuntimeSettings_StaticClass)(), kClass_DefaultObject);
		if (!settings)
			return;

		void * componentClass = Field <void *>(settings, kRuntimeSettings_StatusEffectComponentClass);
		if (!componentClass)
			return;

		void ** vtable = Field <void **>(target, 0);
		if (reinterpret_cast <_Actor_FindComponentByClass>(vtable[kActor_FindComponentByClass])(target, componentClass))
			return;

		void * component = GameFunction <_Actor_AddComponentByClass>(kActor_AddComponentByClass)(
			target, componentClass, false, reinterpret_cast <const void *>(GameAddress(kIdentityTransform)), false);
		if (!component)
			return;

		GameFunction <_Component_Register>(kComponent_Register)(component);

		Connect(target, kStatusEffectTarget_OnStart, component, kComponent_StartFunction);
		Connect(target, kStatusEffectTarget_OnStop, component, kComponent_StopFunction);
	}

	// the first thing the game does to start a shader on an object
	void * SpawnEffect(void * target, void * form)
	{
		if (target)
			GiveStatusEffectComponent(target);

		return GameFunction <_SpawnEffect>(kSpawnEffect)(target, form);
	}
}

bool InstallItemShaderGlowFix()
{
	return RedirectCall(kStartEffect_SpawnCall, kSpawnEffect, reinterpret_cast <void *>(&SpawnEffect));
}
