#pragma once

#include <cstdint>
#include <string>

#include "../../Core/UnityExternalMemory.hpp"
#include "../../Core/UnityExternalMemoryConfig.hpp"
#include "../../Core/UnityExternalTypes.hpp"

namespace UnityExternal {

// Native GameObject layout:
// +0x28 -> managed GameObject pointer
// +0x30 -> component pool pointer
// +0x40 -> component count (int32)
// +0x60 -> name string pointer

struct NativeGameObject {
    std::uintptr_t address;

    NativeGameObject() : address(0) {}
    explicit NativeGameObject(std::uintptr_t addr) : address(addr) {}

    bool IsValid() const { return address != 0; }

    // Get managed pointer (+0x28)
    bool GetManaged(std::uintptr_t& outManaged) const {
        outManaged = 0;
        if (!address) return false;
        return ReadPtrGlobal(address + 0x28u, outManaged) && outManaged != 0;
    }

    // Get component pool (+0x30)
    bool GetComponentPool(std::uintptr_t& outPool) const {
        outPool = 0;
        if (!address) return false;
        return ReadPtrGlobal(address + 0x30u, outPool) && outPool != 0;
    }

    // Get component count (+0x40)
    bool GetComponentCount(std::int32_t& outCount) const {
        outCount = 0;
        if (!address) return false;
        return ReadInt32Global(address + 0x40u, outCount);
    }

    // Get name (+0x60 -> string pointer)
    std::string GetName() const {
        const IMemoryAccessor* acc = GetGlobalMemoryAccessor();
        if (!acc || !address) return std::string();

        std::uintptr_t namePtr = 0;
        if (!ReadPtrGlobal(address + 0x60u, namePtr) || !namePtr) {
            return std::string();
        }

        std::string name;
        if (!ReadCString(*acc, namePtr, name)) {
            return std::string();
        }
        return name;
    }

    // Get component by index
    bool GetComponent(int index, std::uintptr_t& outNativeComponent) const {
        outNativeComponent = 0;
        if (!address || index < 0) return false;

        std::uintptr_t pool = 0;
        if (!GetComponentPool(pool) || !pool) return false;

        std::int32_t count = 0;
        if (!GetComponentCount(count) || index >= count) return false;

        std::uintptr_t slotAddr = pool + 0x8u + static_cast<std::uintptr_t>(index) * 0x10u;
        return ReadPtrGlobal(slotAddr, outNativeComponent) && outNativeComponent != 0;
    }
};

} // namespace UnityExternal
