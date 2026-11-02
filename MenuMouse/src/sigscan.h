#pragma once

#include <cstdint>

namespace mm {

// Scans the main executable's image for an IDA-style pattern ("48 8B ?? 05").
// Returns the address of the first match, or 0.
uintptr_t SigScan(const char *pattern);

// Resolves a rip-relative operand: addr points at the instruction,
// dispOffset is where the int32 displacement starts, instrLen the full length.
uintptr_t ResolveRel32(uintptr_t addr, int dispOffset, int instrLen);

uintptr_t ModuleBase();
size_t ModuleSize();

} // namespace mm
