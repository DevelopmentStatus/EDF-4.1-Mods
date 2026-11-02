#pragma once

namespace mm {

struct Config {
	bool enabled = true;
	bool hoverSelect = true;   // moving the mouse over an item selects it
	bool clickConfirm = true;  // left click on an item activates it
	bool rightClickBack = true;
	bool wheelScroll = true;
	bool showCursorInMenus = true;
	bool debugLog = false;
};

extern Config g_config;

// Reads MenuMouse.ini next to the DLL, writing a default one if it is missing.
void LoadConfig(const wchar_t *iniPath);

} // namespace mm
