// An item whose script removed the item itself crashed the game.
//
// Many older mods give a character a hidden item, a token, whose script does the work and then
// removes the token with RemoveItem: Crime has witnesses does when a witness dies. Removing the
// item frees the variables its script is running with, which the rest of the script reads, and
// which the script runner clears once the script is done, so the game crashed. Classic Oblivion's
// memory allocator left freed memory as it was, which is why such mods worked there; vanilla's
// RemoveMe stops the script once it has removed the item instead.
//
// Scripts on the items in a container run from 0x06647F90. For each scripted item it points a
// temporary reference at the item (its base form at +0x50) and runs the item's script on it with
// the item's own variables: 0x06630EE0, called at 0x06648152, with the container as the containing
// object. Once a script's last statement has run, the runner clears its events (0x0662E860, called
// at 0x0694D9AE); a script that stopped on an error isn't cleared.
//
// So while an item's script runs, RemoveItem of that item from its container (the game's command,
// 0x06A15BE0) waits until the script is done: in place of clearing the events, the same call is made
// again, with the same arguments and variables, which are still there. A removal from inside an
// expression, whose arguments are only a copy, is made at once and stops the script, as RemoveMe
// does. If a held removal doesn't take (the item count is checked), removals are made at once,
// stopping the script, from then on.

#include "Fixes.h"
#include "Log.h"
#include "Patch.h"
#include <cstring>

namespace
{
	const std::uintptr_t kScriptExecute = 0x06630EE0;
	const std::uintptr_t kItemScriptExecuteCall = 0x06648152;
	const std::uintptr_t kScriptEndClearEvents = 0x0662E860;
	const std::uintptr_t kScriptEndClearEventsCall = 0x0694D9AE;
	const std::uintptr_t kRemoveItemCommand = 0x06A15BE0;
	const std::uint8_t kRemoveItemCommandStart[] =
	{
		0x40, 0x55,										// push rbp
		0x53,											// push rbx
		0x56,											// push rsi
		0x57,											// push rdi
		0x41, 0x56,										// push r14
		0x48, 0x8D, 0xAC, 0x24, 0x10, 0xFF, 0xFF, 0xFF,	// lea rbp, [rsp - 0xF0]
	};
	const std::uintptr_t kExtractArgs = 0x06630750;
	const std::uintptr_t kGetItemCountCondition = 0x068CA1D0;
	const std::size_t kScriptOffset_Data = 0x50;
	const std::size_t kReferenceOffset_BaseForm = 0x50;

	typedef bool (* _ScriptExecute)(void * script, void * thisObj, void * locals, void * containingObj, bool unk);
	typedef void (* _ClearEvents)(void * locals);
	typedef bool (* _CommandExecute)(const void * paramInfo, const char * scriptData, void * thisObj, void * containingObj,
		void * script, void * locals, double * result, std::uint32_t * opcodeOffsetPtr);
	typedef bool (* _ExtractArgs)(const void * paramInfo, const char * scriptData, std::uint32_t * opcodeOffsetPtr,
		void * thisObj, void * containingObj, void * script, void * locals, ...);
	typedef bool (* _ConditionFunction)(void * thisObj, void * arg1, void * arg2, double * result);

	// the call to make again once the script is done
	struct HeldRemoval
	{
		const void *	paramInfo;
		const char *	scriptData;
		std::uint32_t	argsOffset;
		void *			from;
		void *			containingObj;
		void *			script;
		void *			locals;
		void *			item;
	};

	struct ItemScriptRun
	{
		void *	container;
		void *	item;
		void *	locals;
		bool	held;
		HeldRemoval	removal;
	};

	ItemScriptRun * g_itemScriptRun = nullptr;
	_CommandExecute g_removeItem = nullptr;
	int g_heldCount = 0;
	bool g_holdRemovals = true;	// until a held removal doesn't take

	double ItemCount(void * ref, void * item)
	{
		double count = 0;
		GameFunction <_ConditionFunction>(kGetItemCountCondition)(ref, item, nullptr, &count);
		return count;
	}

	void DoHeldRemoval(ItemScriptRun & run, const char * when)
	{
		HeldRemoval & removal = run.removal;
		run.held = false;

		double before = ItemCount(removal.from, removal.item);

		std::uint32_t offset = removal.argsOffset;
		double result = 0;
		bool done = g_removeItem(removal.paramInfo, removal.scriptData, removal.from, removal.containingObj,
			removal.script, removal.locals, &result, &offset);

		double after = ItemCount(removal.from, removal.item);

		if(g_heldCount < 10)
		{
			g_heldCount++;
			Log("an item's script removed its own item: removed %s (count %.0f, then %.0f%s)%s", when, before, after,
				done ? "" : ", RemoveItem failed", g_heldCount == 10 ? " (not logging this any more)" : "");
		}

		if(after >= before && before > 0)
		{
			g_holdRemovals = false;
			Log("the removal didn't take: from now on such removals are made at once and stop the script, as RemoveMe does");
		}
	}

	bool RemoveItemCommand(const void * paramInfo, const char * scriptData, void * thisObj, void * containingObj,
		void * script, void * locals, double * result, std::uint32_t * opcodeOffsetPtr)
	{
		ItemScriptRun * run = g_itemScriptRun;
		if(run && thisObj && thisObj == run->container && locals == run->locals && scriptData)
		{
			std::uint32_t offset = *opcodeOffsetPtr;
			void * item = nullptr;
			std::uint32_t count = 0;

			if(GameFunction <_ExtractArgs>(kExtractArgs)(paramInfo, scriptData, &offset, thisObj, containingObj, script, locals, &item, &count) &&
				item && item == run->item)
			{
				// a statement of its own has the script's bytecode, still there at the end; from an
				// expression the arguments are in a copy, so remove at once and stop, as RemoveMe does
				const char * bytecode = *reinterpret_cast <const char * const *>(static_cast <std::uint8_t *>(script) + kScriptOffset_Data);
				if(scriptData != bytecode || !g_holdRemovals)
				{
					g_removeItem(paramInfo, scriptData, thisObj, containingObj, script, locals, result, opcodeOffsetPtr);
					return false;
				}

				// the first one only: it takes the variables away
				if(!run->held)
				{
					run->held = true;
					run->removal = { paramInfo, scriptData, *opcodeOffsetPtr, thisObj, containingObj, script, locals, item };
				}

				*opcodeOffsetPtr = offset;
				return true;
			}
		}

		return g_removeItem(paramInfo, scriptData, thisObj, containingObj, script, locals, result, opcodeOffsetPtr);
	}

	// Once a script's last statement has run: for an item's script that removed its own item, the
	// removal happens here, in place of clearing the events, as the variables go with the item.
	void ScriptEnd(void * locals)
	{
		ItemScriptRun * run = g_itemScriptRun;
		if(run && run->held && locals == run->locals)
		{
			DoHeldRemoval(*run, "at the end of its script");
			return;
		}

		GameFunction <_ClearEvents>(kScriptEndClearEvents)(locals);
	}

	bool ItemScriptExecute(void * script, void * itemRef, void * locals, void * container, bool unk)
	{
		ItemScriptRun run = {};
		run.container = container;
		run.item = itemRef ? *reinterpret_cast <void **>(static_cast <std::uint8_t *>(itemRef) + kReferenceOffset_BaseForm) : nullptr;
		run.locals = locals;

		ItemScriptRun * outer = g_itemScriptRun;
		g_itemScriptRun = &run;
		bool ran = GameFunction <_ScriptExecute>(kScriptExecute)(script, itemRef, locals, container, unk);

		// a script that stopped on an error didn't reach its end; the item, and its variables, are still there
		if(run.held)
			DoHeldRemoval(run, "after its script stopped");

		g_itemScriptRun = outer;

		return ran;
	}
}

bool InstallTokenRemovalFix()
{
	// all three or none: a removal held without the others would wait forever, or never happen
	std::uint8_t itemScriptCall[5], scriptEndCall[5];
	memcpy(itemScriptCall, reinterpret_cast <void *>(GameAddress(kItemScriptExecuteCall)), sizeof(itemScriptCall));
	memcpy(scriptEndCall, reinterpret_cast <void *>(GameAddress(kScriptEndClearEventsCall)), sizeof(scriptEndCall));

	bool itemScriptHooked = RedirectCall(kItemScriptExecuteCall, kScriptExecute, reinterpret_cast <void *>(&ItemScriptExecute));
	bool scriptEndHooked = itemScriptHooked &&
		RedirectCall(kScriptEndClearEventsCall, kScriptEndClearEvents, reinterpret_cast <void *>(&ScriptEnd));
	bool removeItemHooked = scriptEndHooked &&
		HookFunction(kRemoveItemCommand, reinterpret_cast <void *>(&RemoveItemCommand), kRemoveItemCommandStart,
			sizeof(kRemoveItemCommandStart), reinterpret_cast <void **>(&g_removeItem));

	if(removeItemHooked)
		return true;

	if(scriptEndHooked)
		WriteCode(reinterpret_cast <void *>(GameAddress(kScriptEndClearEventsCall)), scriptEndCall, sizeof(scriptEndCall));
	if(itemScriptHooked)
		WriteCode(reinterpret_cast <void *>(GameAddress(kItemScriptExecuteCall)), itemScriptCall, sizeof(itemScriptCall));

	return false;
}
