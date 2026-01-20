#pragma once

#include <cstdint>
#include <string>

#include "../../Core/UnityExternalMemory.hpp"
#include "../../Core/UnityExternalMemoryConfig.hpp"
#include "../../Core/UnityExternalTypes.hpp"

namespace UnityExternal {

// Managed object wrapper (works for both Mono and IL2CPP)
// All managed objects share the same structure:
// +0x00 -> vtable (Mono) or klass (IL2CPP)
// +0x10 -> native pointer
struct ManagedObject {
    RuntimeKind runtime;
    std::uintptr_t address;

    ManagedObject() : runtime(RuntimeKind::Mono), address(0) {}
    ManagedObject(RuntimeKind r, std::uintptr_t addr) : runtime(r), address(addr) {}

    bool IsValid() const { return address != 0; }

    // Get native pointer from managed object (+0x10)
    bool GetNative(std::uintptr_t& outNative) const {
        outNative = 0;
        if (!address) return false;
        return ReadPtrGlobal(address + 0x10u, outNative) && outNative != 0;
    }

    // Get type info (class name + namespace)
    bool GetTypeInfo(TypeInfo& out) const {
        const IMemoryAccessor* acc = GetGlobalMemoryAccessor();
        if (!acc || !address) return false;
        return GetManagedType(runtime, *acc, address, out);
    }

    // Get class name
    std::string GetClassName() const {
        TypeInfo info;
        if (!GetTypeInfo(info)) return std::string();
        return info.name;
    }

    // Get namespace
    std::string GetNamespace() const {
        TypeInfo info;
        if (!GetTypeInfo(info)) return std::string();
        return info.namespaze;
    }
};

} // namespace UnityExternal
