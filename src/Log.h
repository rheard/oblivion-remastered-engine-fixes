#pragma once

struct OBSEInterface;

// The plugin's log, Documents\My Games\<the game's save folder>\OBSE\Logs\<fileName>, beside OBSE64's.
void OpenLog(const OBSEInterface * obse, const char * fileName);

void Log(const char * fmt, ...);
