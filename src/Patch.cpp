#include "Patch.h"
#include <windows.h>
#include <cstring>
#include "PluginAPI.h"

namespace
{
	std::uintptr_t g_gameBase = 0;

	// executable memory near the game's code, from OBSE64's branch pool, within reach of a 5-byte call
	OBSETrampolineInterface * g_trampolines = nullptr;
	PluginHandle g_pluginHandle = static_cast <PluginHandle>(kPluginHandle_Invalid);

	// jmp qword ptr [rip], then the address
	const std::size_t kJumpSize = 14;

	void WriteJump(void * at, const void * target)
	{
		std::uint8_t jump[kJumpSize] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
		memcpy(jump + 6, &target, sizeof(target));
		memcpy(at, jump, sizeof(jump));
	}

	void * AllocateNearGame(std::size_t size)
	{
		return g_trampolines ? g_trampolines->AllocateFromBranchPool(g_pluginHandle, size) : nullptr;
	}
}

void InitPatching(const OBSEInterface * obse)
{
	g_gameBase = reinterpret_cast <std::uintptr_t>(GetModuleHandle(nullptr));
	g_trampolines = static_cast <OBSETrampolineInterface *>(obse->QueryInterface(kInterface_Trampoline));
	g_pluginHandle = obse->GetPluginHandle();
}

std::uintptr_t GameAddress(std::uintptr_t rva)
{
	return g_gameBase + rva;
}

bool CodeMatches(std::uintptr_t rva, const std::uint8_t * expected, std::size_t size)
{
	return memcmp(reinterpret_cast <const void *>(GameAddress(rva)), expected, size) == 0;
}

bool WriteCode(void * at, const void * data, std::size_t size)
{
	DWORD protect = 0;
	if(!VirtualProtect(at, size, PAGE_EXECUTE_READWRITE, &protect)) return false;
	memcpy(at, data, size);
	VirtualProtect(at, size, protect, &protect);
	FlushInstructionCache(GetCurrentProcess(), at, size);

	return true;
}

bool WriteData(void * at, const void * data, std::size_t size)
{
	DWORD protect = 0;
	if(!VirtualProtect(at, size, PAGE_READWRITE, &protect)) return false;
	memcpy(at, data, size);
	VirtualProtect(at, size, protect, &protect);

	return true;
}

bool ReplaceVirtual(std::uintptr_t vtableRva, unsigned slot, std::uintptr_t expectedRva, void * replacement, void ** original)
{
	void ** entry = reinterpret_cast <void **>(GameAddress(vtableRva)) + slot;
	if(*entry != reinterpret_cast <void *>(GameAddress(expectedRva))) return false;

	void * previous = *entry;
	if(!WriteData(entry, &replacement, sizeof(replacement))) return false;

	*original = previous;
	return true;
}

bool ReplaceFunction(std::uintptr_t rva, void * replacement, const std::uint8_t * expected, std::size_t expectedSize)
{
	auto * code = reinterpret_cast <std::uint8_t *>(GameAddress(rva));
	if(memcmp(code, expected, expectedSize) != 0) return false;

	std::uint8_t jump[kJumpSize];
	WriteJump(jump, replacement);

	return WriteCode(code, jump, sizeof(jump));
}

bool RedirectCall(std::uintptr_t callRva, std::uintptr_t targetRva, void * replacement)
{
	auto * call = reinterpret_cast <std::uint8_t *>(GameAddress(callRva));
	if(call[0] != 0xE8 || callRva + 5 + *reinterpret_cast <const std::int32_t *>(call + 1) != targetRva) return false;

	auto * stub = static_cast <std::uint8_t *>(AllocateNearGame(kJumpSize));
	if(!stub) return false;
	WriteJump(stub, replacement);

	std::int64_t distance = stub - (call + 5);
	if(distance != std::int32_t(distance)) return false;

	std::int32_t rel = std::int32_t(distance);
	return WriteCode(call + 1, &rel, sizeof(rel));
}

bool HookFunction(std::uintptr_t rva, void * replacement, const std::uint8_t * expected, std::size_t prologueSize, void ** original)
{
	auto * code = reinterpret_cast <std::uint8_t *>(GameAddress(rva));
	if(prologueSize < kJumpSize || memcmp(code, expected, prologueSize) != 0) return false;

	auto * trampoline = static_cast <std::uint8_t *>(AllocateNearGame(prologueSize + kJumpSize));
	if(!trampoline) return false;
	memcpy(trampoline, code, prologueSize);
	WriteJump(trampoline + prologueSize, code + prologueSize);

	*original = trampoline;
	return ReplaceFunction(rva, replacement, expected, prologueSize);
}
