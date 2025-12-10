#pragma once

#include <cstdint>
#include "UnityExternalMemory.hpp"

namespace UnityExternal {

// Global memory accessor singleton
inline const IMemoryAccessor* g_memoryAccessor = nullptr;

inline void SetGlobalMemoryAccessor(const IMemoryAccessor* accessor) {
    g_memoryAccessor = accessor;
}

inline const IMemoryAccessor* GetGlobalMemoryAccessor() {
    return g_memoryAccessor;
}

// Global helper functions using the global accessor
template <typename T>
inline bool ReadValueGlobal(std::uintptr_t address, T& out) {
    const IMemoryAccessor* acc = GetGlobalMemoryAccessor();
    if (!acc) return false;
    return ReadValue(*acc, address, out);
}

template <typename T>
inline bool WriteValueGlobal(std::uintptr_t address, const T& value) {
    const IMemoryAccessor* acc = GetGlobalMemoryAccessor();
    if (!acc) return false;
    return WriteValue(*acc, address, value);
}

inline bool ReadPtrGlobal(std::uintptr_t address, std::uintptr_t& out) {
    return ReadValueGlobal(address, out);
}

inline bool ReadInt32Global(std::uintptr_t address, std::int32_t& out) {
    return ReadValueGlobal(address, out);
}

} // namespace UnityExternal
