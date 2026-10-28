// Thefts gave the player fractions of a gold in bounty.
//
// The player's bounty is a float. A theft adds the item's value times fCrimeGoldSteal (0.5),
// unrounded (0x06955040), times the faction multiplier of whoever reports it (0x068098F0), so
// stealing a 3-gold item adds 1.5, and GetCrimeGold reads a bounty under 1 as 1. Mods that count
// bounty in whole gold get it wrong: Crime has witnesses gives each witness a whole share, rounded
// up, then takes a bounty below the shares to mean it has been paid, and lets the crime stand. Here
// every change to the player's bounty is a whole number of gold, rounded, and at least 1 when it
// isn't none. The game shows bounties in whole gold anyway.

#include "Fixes.h"
#include "Patch.h"
#include <cmath>

namespace
{
	// PlayerCharacter's virtual function 163, which crimes and ModCrimeGold call
	const std::uintptr_t kPlayerCharacterVtable = 0x0861CBA0;
	const unsigned kModCrimeGoldSlot = 0x518 / 8;
	const std::uintptr_t kPlayerModCrimeGold = 0x0680B460;

	typedef void (* _ModCrimeGold)(void * actor, float amount);

	_ModCrimeGold g_modCrimeGold = nullptr;

	void ModCrimeGold(void * actor, float amount)
	{
		float whole = std::round(amount);
		if(amount > 0 && whole < 1)
			whole = 1;
		else if(amount < 0 && whole > -1)
			whole = -1;

		g_modCrimeGold(actor, whole);
	}
}

bool InstallWholeGoldBountyFix()
{
	return ReplaceVirtual(kPlayerCharacterVtable, kModCrimeGoldSlot, kPlayerModCrimeGold, reinterpret_cast <void *>(&ModCrimeGold),
		reinterpret_cast <void **>(&g_modCrimeGold));
}
