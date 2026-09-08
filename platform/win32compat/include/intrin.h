#pragma once
#include <cstdint>

// x86 intrinsics with no arm64 equivalent. The bodies are placeholders; the point is to
// let the rest of the translation unit be compiled and counted.
// clang supplies _rotl and _rotr as builtins under -fms-extensions.
static inline unsigned long long __rdtsc() { return 0ULL; }
static inline void __cpuid(int regs[4], int) { regs[0] = regs[1] = regs[2] = regs[3] = 0; }
static inline void __cpuidex(int regs[4], int, int) { regs[0] = regs[1] = regs[2] = regs[3] = 0; }
