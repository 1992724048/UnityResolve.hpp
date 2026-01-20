#pragma once

#include <cstdint>

#include "../../Core/UnityExternalMemory.hpp"
#include "../../Core/UnityExternalMemoryConfig.hpp"

namespace UnityExternal {

// Native Component layout:
// +0x28 -> managed component pointer
// +0x30 -> native GameObject pointer

struct NativeComponent {
    std::uintptr_t address;

    NativeComponent() : address(0) {}
    explicit NativeComponent(std::uintptr_t addr) : address(addr) {}

    bool IsValid() const { return address != 0; }

    // Get managed component pointer (+0x28)
    bool GetManaged(std::uintptr_t& outManaged) const {
        outManaged = 0;
        if (!address) return false;
        return ReadPtrGlobal(address + 0x28u, outManaged) && outManaged != 0;
    }

    // Get native GameObject pointer (+0x30)
    bool GetGameObject(std::uintptr_t& outGameObject) const {
        outGameObject = 0;
        if (!address) return false;
        return ReadPtrGlobal(address + 0x30u, outGameObject) && outGameObject != 0;
    }
};

// Helper function
inline bool NativeComponent_GetGameObject(std::uintptr_t nativeComponent, std::uintptr_t& outGameObjectNative) {
    outGameObjectNative = 0;
    if (!nativeComponent) return false;
    return ReadPtrGlobal(nativeComponent + 0x30u, outGameObjectNative) && outGameObjectNative != 0;
}

} // namespace UnityExternal
