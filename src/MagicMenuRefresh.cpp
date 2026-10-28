// A spell a script removed from the player stayed in the open magic menu.
//
// The magic menu's list is a copy, rebuilt only when the menu opens, an item is clicked or the
// filter changes, so a spell a script removed while the menu was open (Delete Spell removes the one
// under the cursor when you press Delete) stayed listed until the menu was opened again. Here the
// player's RemoveSpell also rebuilds the open menu's list, as a click does.

#include "Fixes.h"
#include "Patch.h"

namespace
{
	// The player's RemoveSpell: PlayerCharacter's virtual function 200 (the vtable is at 0x0861CBA0),
	// which the RemoveSpell command calls. 0x06775DA0 rebuilds the magic menu's list and sends Unreal
	// the new one, as a click does; it finds the menu itself, and does nothing while it's closed.
	const std::uintptr_t kPlayerCharacterVtable = 0x0861CBA0;
	const unsigned kRemoveSpellSlot = 200;
	const std::uintptr_t kPlayerRemoveSpell = 0x06603AB0;
	const std::uintptr_t kRebuildMagicMenuList = 0x06775DA0;

	typedef bool (* _RemoveSpell)(void * actor, void * spell);
	typedef void (* _RebuildMagicMenuList)();

	_RemoveSpell g_removeSpell = nullptr;

	bool RemoveSpell(void * actor, void * spell)
	{
		bool removed = g_removeSpell(actor, spell);

		if(removed)
			GameFunction <_RebuildMagicMenuList>(kRebuildMagicMenuList)();

		return removed;
	}
}

bool InstallMagicMenuRefreshFix()
{
	return ReplaceVirtual(kPlayerCharacterVtable, kRemoveSpellSlot, kPlayerRemoveSpell, reinterpret_cast <void *>(&RemoveSpell),
		reinterpret_cast <void **>(&g_removeSpell));
}
