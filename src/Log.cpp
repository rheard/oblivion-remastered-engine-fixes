#include "Log.h"
#include <windows.h>
#include <shlobj.h>
#include <share.h>
#include <cstdarg>
#include <cstdio>
#include <string>
#include "PluginAPI.h"

namespace
{
	FILE * g_log = nullptr;
}

void OpenLog(const OBSEInterface * obse, const char * fileName)
{
	char documents[MAX_PATH];
	if(FAILED(SHGetFolderPathA(nullptr, CSIDL_MYDOCUMENTS, nullptr, SHGFP_TYPE_CURRENT, documents))) return;

	std::string path = std::string(documents) + "\\My Games\\" + obse->GetSaveFolderName() + "\\OBSE\\Logs\\" + fileName;
	g_log = _fsopen(path.c_str(), "w", _SH_DENYWR);
}

void Log(const char * fmt, ...)
{
	if(!g_log) return;

	va_list args;
	va_start(args, fmt);
	vfprintf(g_log, fmt, args);
	va_end(args);

	fputc('\n', g_log);
	fflush(g_log);
}
