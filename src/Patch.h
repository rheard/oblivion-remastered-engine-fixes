#pragma once

#include <cstddef>
#include <cstdint>

struct OBSEInterface;

// Changing the game's code. Addresses are offsets into Oblivion Remastered 1.512.105's executable
// (relative virtual addresses), and every change first checks that the game's bytes there are the
// ones expected, changing nothing otherwise.

void InitPatching(const OBSEInterface * obse);

std::uintptr_t GameAddress(std::uintptr_t rva);

template <typename T>
T GameFunction(std::uintptr_t rva)
{
	return reinterpret_cast <T>(GameAddress(rva));
}

// whether the game's bytes at rva are the ones expected
bool CodeMatches(std::uintptr_t rva, const std::uint8_t * expected, std::size_t size);

// writes over the game's code, or over its data
bool WriteCode(void * at, const void * data, std::size_t size);
bool WriteData(void * at, const void * data, std::size_t size);

// Points a virtual function at replacement, if it's still the function expected, and gives the
// original: the vtable at vtableRva, its slot'th entry.
bool ReplaceVirtual(std::uintptr_t vtableRva, unsigned slot, std::uintptr_t expectedRva, void * replacement, void ** original);

// Replaces the function at rva with a jump to replacement, if it starts as expected.
bool ReplaceFunction(std::uintptr_t rva, void * replacement, const std::uint8_t * expected, std::size_t expectedSize);

// Sends the game's call at callRva to replacement instead, if it calls targetRva.
bool RedirectCall(std::uintptr_t callRva, std::uintptr_t targetRva, void * replacement);

// Sends calls to the function at rva to replacement, if it starts as expected, and gives a way to
// call the original: its first prologueSize bytes (whole instructions that don't depend on where
// they are), then a jump back past them.
bool HookFunction(std::uintptr_t rva, void * replacement, const std::uint8_t * expected, std::size_t prologueSize, void ** original);
