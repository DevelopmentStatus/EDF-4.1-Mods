#pragma once

namespace mm {

void LogOpen(const wchar_t *path);
void Log(const char *fmt, ...);
void LogDebug(const char *fmt, ...);
void SetDebugLogging(bool enabled);

} // namespace mm
