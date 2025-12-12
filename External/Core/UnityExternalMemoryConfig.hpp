#pragma once

#include <cstdint>
#include <atomic>
#include "UnityExternalMemory.hpp"

namespace UnityExternal {

// Global memory accessor singleton (atomic pointer for thread-safety)
inline std::atomic<const IMemoryAccessor*> g_memoryAccessor{nullptr};

inline void SetGlobalMemoryAccessor(const IMemoryAccessor* accessor) {
    g_memoryAccessor.store(accessor, std::memory_order_release);
}

inline const IMemoryAccessor* GetGlobalMemoryAccessor() {
    return g_memoryAccessor.load(std::memory_order_acquire);
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
