// A mod's scripted spell showed its effect as "<MISSING STRING TABLE ENTRY>".
//
// The game's plugins keep the names of their script effects as text, as Oblivion's did, and the
// Remastered's translations of them are under keys made from that text. Loading a script effect
// (0x06880820), the game replaces its name with such a key: "LOC_FN_", then the text without its
// spaces and punctuation, so "Script Effect" becomes LOC_FN_ScriptEffect. It does that to every
// plugin's, and a name only a mod has has no translation, so the key finds nothing in the string
// tables and Unreal shows the placeholder they give for that: Detect Item's effect showed as
// "<MISSING STRING TABLE ENTRY> on Self".
//
// Here the loader's text is remembered under its key. When the string tables have nothing for that
// key (FText::FromStringTable, 0x00E630F0), the text is the name as the mod wrote it. A name they
// do have keeps its translation.

#include <cstring>
#include <intrin.h>
#include <map>
#include <shared_mutex>
#include <string>
#include <string_view>
#include "Fixes.h"
#include "Patch.h"

namespace
{
	// the loader's copy of the name as the plugin has it, into a std::string, and the key it stores
	const std::uintptr_t kLoadName_CopyTextCall = 0x06880A60;
	const std::uintptr_t kStdString_Assign = 0x019D5230;
	const std::uintptr_t kLoadName_SetKeyCall = 0x06880C04;
	const std::uintptr_t kBSString_Set = 0x06562E40;

	const std::uintptr_t kFText_FromStringTable = 0x00E630F0;
	const std::uint8_t kFText_FromStringTableStart[] =
	{
		0x48, 0x89, 0x5C, 0x24, 0x08,					// mov [rsp + 0x08], rbx
		0x48, 0x89, 0x6C, 0x24, 0x10,					// mov [rsp + 0x10], rbp
		0x48, 0x89, 0x74, 0x24, 0x18,					// mov [rsp + 0x18], rsi
	};

	const std::uintptr_t kFText_FromString = 0x00E62E70;
	const std::uintptr_t kFText_ToString = 0x00E63E60;
	const std::uintptr_t kFString_FromText = 0x00DDAE00;
	const std::uintptr_t kFMemory_Free = 0x00E26500;

	const wchar_t kMissingEntry[] = L"<MISSING STRING TABLE ENTRY>";
	const wchar_t kNameKeyPrefix[] = L"LOC_FN_";

	// Unreal's: the count includes the terminating zero
	struct FString
	{
		const wchar_t	* data;
		std::int32_t	count;
		std::int32_t	capacity;
	};

	struct SharedCount
	{
		void			** vtable;
		volatile long	shared;
		volatile long	weak;
	};

	struct FText
	{
		void			* data;
		SharedCount		* count;
		std::uint32_t	flags;
	};

	typedef void * (* _StdString_Assign)(void * string, const char * text, std::size_t length);
	typedef void * (* _BSString_Set)(void * string, const char * text, std::uint32_t minLength);
	typedef FText * (* _FText_FromStringTable)(FText * out, std::uint64_t table, const FString * key, std::uint8_t loadingPolicy);
	typedef FText * (* _FText_FromString)(FText * out, const FString * text);
	typedef const FString * (* _FText_ToString)(const FText * text);
	typedef FString * (* _FString_FromText)(FString * out, const char * text);
	typedef void (* _FMemory_Free)(const void * memory);

	_FText_FromStringTable g_fromStringTable = nullptr;

	// the loader's text, from its copy until it stores the key
	thread_local const char * g_loadingName = nullptr;

	// the game's own converter, so these match the keys as Unreal is given them
	std::wstring Widen(const char * text)
	{
		FString converted = { };
		GameFunction <_FString_FromText>(kFString_FromText)(&converted, text);

		std::wstring result = converted.data ? converted.data : L"";
		GameFunction <_FMemory_Free>(kFMemory_Free)(converted.data);

		return result;
	}

	// the game loads them on its own thread, and Unreal reads them on its
	std::map <std::wstring, std::wstring, std::less <>> g_names;
	std::shared_mutex g_namesLock;

	void * LoadName_CopyText(void * string, const char * text, std::size_t length)
	{
		g_loadingName = text;

		return GameFunction <_StdString_Assign>(kStdString_Assign)(string, text, length);
	}

	void * LoadName_SetKey(void * string, const char * key, std::uint32_t minLength)
	{
		if(g_loadingName)
		{
			std::wstring wideKey = Widen(key);
			std::wstring wideName = Widen(g_loadingName);

			std::unique_lock lock(g_namesLock);
			g_names.insert_or_assign(std::move(wideKey), std::move(wideName));
		}

		g_loadingName = nullptr;

		return GameFunction <_BSString_Set>(kBSString_Set)(string, key, minLength);
	}

	void Release(FText * text)
	{
		SharedCount * count = text->count;
		if(!count) return;

		if(_InterlockedDecrement(&count->shared) == 0)
		{
			reinterpret_cast <void (*)(SharedCount *)>(count->vtable[0])(count);

			if(_InterlockedDecrement(&count->weak) == 0)
				reinterpret_cast <void (*)(SharedCount *, int)>(count->vtable[1])(count, 1);
		}
	}

	FText * FromStringTable(FText * out, std::uint64_t table, const FString * key, std::uint8_t loadingPolicy)
	{
		g_fromStringTable(out, table, key, loadingPolicy);

		if(!key->data || wcsncmp(key->data, kNameKeyPrefix, _countof(kNameKeyPrefix) - 1))
			return out;

		const FString * shown = GameFunction <_FText_ToString>(kFText_ToString)(out);
		if(!shown->data || wcscmp(shown->data, kMissingEntry))
			return out;

		std::shared_lock lock(g_namesLock);

		auto name = g_names.find(std::wstring_view(key->data));
		if(name != g_names.end())
		{
			Release(out);

			FString text = { name->second.c_str(), std::int32_t(name->second.size() + 1), std::int32_t(name->second.size() + 1) };
			GameFunction <_FText_FromString>(kFText_FromString)(out, &text);
		}

		return out;
	}
}

bool InstallScriptEffectNamesFix()
{
	return RedirectCall(kLoadName_CopyTextCall, kStdString_Assign, reinterpret_cast <void *>(&LoadName_CopyText))
		&& RedirectCall(kLoadName_SetKeyCall, kBSString_Set, reinterpret_cast <void *>(&LoadName_SetKey))
		&& HookFunction(kFText_FromStringTable, reinterpret_cast <void *>(&FromStringTable), kFText_FromStringTableStart,
			sizeof(kFText_FromStringTableStart), reinterpret_cast <void **>(&g_fromStringTable));
}
