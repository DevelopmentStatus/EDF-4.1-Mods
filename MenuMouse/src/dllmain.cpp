#include <windows.h>

#include "PluginAPI.h"
#include "config.h"
#include "hooks.h"
#include "log.h"
#include "ui_cursor.h"

#include <MinHook.h>
#include <string>

static HMODULE g_self;

static std::wstring SiblingPath(const wchar_t *fileName) {
	wchar_t path[MAX_PATH];
	GetModuleFileNameW(g_self, path, MAX_PATH);
	std::wstring dir(path);
	dir.resize(dir.find_last_of(L"\\/") + 1);
	return dir + fileName;
}

extern "C" __declspec(dllexport) bool EDFMLAPI EML4_Load(PluginInfo *info) {
	info->infoVersion = PluginInfo::MaxInfoVer;
	info->name = "MenuMouse";
	info->version = PLUG_VER(0, 1, 0, 0);

	mm::LogOpen(SiblingPath(L"MenuMouse.log").c_str());
	mm::LoadConfig(SiblingPath(L"MenuMouse.ini").c_str());
	mm::SetDebugLogging(mm::g_config.debugLog);
	mm::Log("MenuMouse 0.1.0 loading, EDF41.exe base %p", GetModuleHandleW(nullptr));

	if (!mm::g_config.enabled) {
		mm::Log("Disabled in MenuMouse.ini");
		return false;
	}
	if (MH_Initialize() != MH_OK) {
		mm::Log("MinHook failed to initialize");
		return false;
	}
	// Menu hooks first: if the game's UI code can't be found there is no
	// reason to touch the cursor APIs at all.
	if (!mm::InstallMenuHooks()) {
		mm::Log("Menu hooks unavailable; plugin unloading (game unchanged)");
		MH_Uninitialize();
		return false;
	}
	if (!mm::InstallInputHooks()) {
		MH_DisableHook(MH_ALL_HOOKS);
		MH_Uninitialize();
		return false;
	}
	mm::Log("Ready");
	return true;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
	if (reason == DLL_PROCESS_ATTACH) {
		g_self = module;
		DisableThreadLibraryCalls(module);
	}
	return TRUE;
}
