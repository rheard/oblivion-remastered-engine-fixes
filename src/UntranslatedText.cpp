// Text a mod added showed with "[NL]" in front.
//
// The Remastered keeps its names and messages as translation keys, and Unreal shows any text that
// isn't one with "[NL]" in front: the names a plugin gives cells and items, and every message an
// older mod's scripts show. Unreal joins the marker to the text (0x04883EA0); emptied, it leaves the
// text as it is.

#include "Fixes.h"
#include "Patch.h"

namespace
{
	const std::uintptr_t kNotLocalizedMarker = 0x07D7B56C;
	const char kNotLocalizedMarkerText[] = "[NL]";
}

bool InstallUntranslatedTextFix()
{
	if(!CodeMatches(kNotLocalizedMarker, reinterpret_cast <const std::uint8_t *>(kNotLocalizedMarkerText), sizeof(kNotLocalizedMarkerText)))
		return false;

	const char empty = 0;
	return WriteData(reinterpret_cast <void *>(GameAddress(kNotLocalizedMarker)), &empty, sizeof(empty));
}
