#include "sigscan.h"

#include <windows.h>
#include <vector>

namespace mm {

uintptr_t ModuleBase() {
	return reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
}

size_t ModuleSize() {
	auto base = reinterpret_cast<const uint8_t *>(ModuleBase());
	auto nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(base + reinterpret_cast<const IMAGE_DOS_HEADER *>(base)->e_lfanew);
	return nt->OptionalHeader.SizeOfImage;
}

static bool ParsePattern(const char *pattern, std::vector<int> &out) {
	for (const char *p = pattern; *p;) {
		if (*p == ' ') {
			++p;
		} else if (*p == '?') {
			out.push_back(-1);
			while (*p == '?')
				++p;
		} else {
			char hex[3] = {p[0], p[1], 0};
			if (!p[1])
				return false;
			out.push_back(static_cast<int>(strtoul(hex, nullptr, 16)));
			p += 2;
		}
	}
	return !out.empty();
}

uintptr_t SigScan(const char *pattern) {
	std::vector<int> bytes;
	if (!ParsePattern(pattern, bytes))
		return 0;

	auto base = reinterpret_cast<const uint8_t *>(ModuleBase());
	auto dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
	auto nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
	auto section = IMAGE_FIRST_SECTION(nt);
	for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
		if (!(section->Characteristics & IMAGE_SCN_MEM_EXECUTE))
			continue;
		const uint8_t *start = base + section->VirtualAddress;
		size_t size = section->Misc.VirtualSize;
		if (size < bytes.size())
			continue;
		for (size_t off = 0; off <= size - bytes.size(); ++off) {
			size_t j = 0;
			while (j < bytes.size() && (bytes[j] < 0 || start[off + j] == bytes[j]))
				++j;
			if (j == bytes.size())
				return reinterpret_cast<uintptr_t>(start + off);
		}
	}
	return 0;
}

uintptr_t ResolveRel32(uintptr_t addr, int dispOffset, int instrLen) {
	if (!addr)
		return 0;
	int32_t disp = *reinterpret_cast<const int32_t *>(addr + dispOffset);
	return addr + instrLen + disp;
}

} // namespace mm
