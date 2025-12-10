#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "../Core/UnityExternalMemory.hpp"
#include "../Core/UnityExternalTypes.hpp"

namespace UnityExternal {

struct GameObjectEntry {
    std::uintptr_t node;
    std::uintptr_t nativeObject;
    std::uintptr_t managedObject;
};

struct ComponentEntry {
    std::uintptr_t nativeComponent;
    std::uintptr_t managedComponent;
};

class GOMWalker {
public:
    GOMWalker(const IMemoryAccessor& mem, RuntimeKind runtime)
        : mem_(mem), runtime_(runtime) {}

    bool ReadManagerFromGlobal(std::uintptr_t gomGlobalAddress, std::uintptr_t& managerAddress) const;
    bool EnumerateGameObjects(std::uintptr_t managerAddress, std::vector<GameObjectEntry>& out) const;
    bool EnumerateGameObjectsFromGlobal(std::uintptr_t gomGlobalAddress, std::vector<GameObjectEntry>& out) const;
    bool EnumerateComponents(std::uintptr_t managerAddress, std::vector<ComponentEntry>& out) const;
    bool EnumerateComponentsFromGlobal(std::uintptr_t gomGlobalAddress, std::vector<ComponentEntry>& out) const;

    RuntimeKind GetRuntime() const { return runtime_; }

private:
    const IMemoryAccessor& mem_;
    RuntimeKind runtime_;
};

inline bool GOMWalker::ReadManagerFromGlobal(std::uintptr_t gomGlobalAddress, std::uintptr_t& managerAddress) const {
    managerAddress = 0;
    if (!gomGlobalAddress) {
        return false;
    }
    return ReadPtr(mem_, gomGlobalAddress, managerAddress);
}

inline bool GOMWalker::EnumerateGameObjects(std::uintptr_t managerAddress, std::vector<GameObjectEntry>& out) const {
    out.clear();
    if (!managerAddress) {
        return false;
    }

    std::uintptr_t listHead = 0;
    if (!ReadPtr(mem_, managerAddress + 0x28, listHead) || !listHead) {
        return false;
    }

    const std::size_t kMaxObjects = 1000000;
    std::uintptr_t firstNode = listHead;
    std::uintptr_t tail = 0;
    ReadPtr(mem_, firstNode + 0x0, tail);

    std::uintptr_t node = firstNode;
    for (std::size_t i = 0; node && i < kMaxObjects; ++i) {
        std::uintptr_t nativeObject = 0;
        std::uintptr_t managedObject = 0;
        std::uintptr_t next = 0;

        if (!ReadPtr(mem_, node + 0x10, nativeObject)) {
            return false;
        }
        if (nativeObject) {
            if (!ReadPtr(mem_, nativeObject + 0x28, managedObject)) {
                return false;
            }
        }

        if (nativeObject || managedObject) {
            GameObjectEntry entry{};
            entry.node = node;
            entry.nativeObject = nativeObject;
            entry.managedObject = managedObject;
            out.push_back(entry);
        }

        if (tail && node == tail) {
            break;
        }

        if (!ReadPtr(mem_, node + 0x8, next)) {
            return false;
        }
        if (!next || next == firstNode) {
            break;
        }

        node = next;
    }

    return true;
}

inline bool GOMWalker::EnumerateGameObjectsFromGlobal(std::uintptr_t gomGlobalAddress, std::vector<GameObjectEntry>& out) const {
    std::uintptr_t managerAddress = 0;
    if (!ReadManagerFromGlobal(gomGlobalAddress, managerAddress) || !managerAddress) {
        return false;
    }
    return EnumerateGameObjects(managerAddress, out);
}

inline bool GOMWalker::EnumerateComponents(std::uintptr_t managerAddress, std::vector<ComponentEntry>& out) const {
    out.clear();

    std::vector<GameObjectEntry> gameObjects;
    if (!EnumerateGameObjects(managerAddress, gameObjects)) {
        return false;
    }
    if (gameObjects.empty()) {
        return true;
    }

    const int kMaxComponentsPerObject = 1024;

    for (const auto& info : gameObjects) {
        if (!info.nativeObject) {
            continue;
        }

        std::uintptr_t componentPool = 0;
        if (!ReadPtr(mem_, info.nativeObject + 0x30, componentPool) || !componentPool) {
            continue;
        }

        std::int32_t componentCount = 0;
        if (!ReadInt32(mem_, info.nativeObject + 0x40, componentCount)) {
            return false;
        }
        if (componentCount <= 0 || componentCount > kMaxComponentsPerObject) {
            continue;
        }

        for (int i = 0; i < componentCount; ++i) {
            std::uintptr_t slotAddr = componentPool + 0x8 + static_cast<std::uintptr_t>(i) * 0x10;

            std::uintptr_t nativeComponent = 0;
            if (!ReadPtr(mem_, slotAddr, nativeComponent) || !nativeComponent) {
                continue;
            }

            std::uintptr_t managedComponent = 0;
            if (!ReadPtr(mem_, nativeComponent + 0x28, managedComponent) || !managedComponent) {
                continue;
            }

            ComponentEntry entry{};
            entry.nativeComponent = nativeComponent;
            entry.managedComponent = managedComponent;
            out.push_back(entry);
        }
    }

    return true;
}

inline bool GOMWalker::EnumerateComponentsFromGlobal(std::uintptr_t gomGlobalAddress, std::vector<ComponentEntry>& out) const {
    std::uintptr_t managerAddress = 0;
    if (!ReadManagerFromGlobal(gomGlobalAddress, managerAddress) || !managerAddress) {
        return false;
    }
    return EnumerateComponents(managerAddress, out);
}

} // namespace UnityExternal
