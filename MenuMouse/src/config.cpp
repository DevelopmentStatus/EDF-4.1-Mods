#include "config.h"

#include <windows.h>

namespace mm {

Config g_config;

static const wchar_t *kDefaults[][2] = {
	{L"Enabled", L"1"},
	{L"HoverSelect", L"1"},
	{L"ClickConfirm", L"1"},
	{L"RightClickBack", L"1"},
	{L"WheelScroll", L"1"},
	{L"ShowCursorInMenus", L"1"},
	{L"DebugLog", L"0"},
};

static bool ReadBool(const wchar_t *ini, const wchar_t *key, bool def) {
	return GetPrivateProfileIntW(L"MenuMouse", key, def ? 1 : 0, ini) != 0;
}

void LoadConfig(const wchar_t *ini) {
	// Fill in any keys missing from an existing file, so new settings show up.
	wchar_t probe[8];
	for (auto &kv : kDefaults) {
		if (!GetPrivateProfileStringW(L"MenuMouse", kv[0], L"", probe, 8, ini))
			WritePrivateProfileStringW(L"MenuMouse", kv[0], kv[1], ini);
	}
	g_config.enabled = ReadBool(ini, L"Enabled", true);
	g_config.hoverSelect = ReadBool(ini, L"HoverSelect", true);
	g_config.clickConfirm = ReadBool(ini, L"ClickConfirm", true);
	g_config.rightClickBack = ReadBool(ini, L"RightClickBack", true);
	g_config.wheelScroll = ReadBool(ini, L"WheelScroll", true);
	g_config.showCursorInMenus = ReadBool(ini, L"ShowCursorInMenus", true);
	g_config.debugLog = ReadBool(ini, L"DebugLog", false);
}

} // namespace mm
