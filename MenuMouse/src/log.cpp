#include "log.h"

#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace mm {

static FILE *g_file = nullptr;
static bool g_debug = false;
static std::mutex g_lock;

void LogOpen(const wchar_t *path) {
	g_file = _wfopen(path, L"w");
}

void SetDebugLogging(bool enabled) {
	g_debug = enabled;
}

static void LogV(const char *fmt, va_list args) {
	if (!g_file)
		return;
	SYSTEMTIME t;
	GetLocalTime(&t);
	std::lock_guard<std::mutex> guard(g_lock);
	fprintf(g_file, "[%02d:%02d:%02d.%03d] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
	vfprintf(g_file, fmt, args);
	fputc('\n', g_file);
	fflush(g_file);
}

void Log(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	LogV(fmt, args);
	va_end(args);
}

void LogDebug(const char *fmt, ...) {
	if (!g_debug)
		return;
	va_list args;
	va_start(args, fmt);
	LogV(fmt, args);
	va_end(args);
}

} // namespace mm
