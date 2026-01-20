#pragma once
 
#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>

#include "../Core/UnityExternalMemory.hpp"
#include "../Core/UnityExternalMemoryConfig.hpp"
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
    const IMemoryAccessor& Accessor() const { return mem_; }

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
 
    // Newer Unity: GameObjectManager is a hash_map keyed by PersistentTypeID.
    // Layout (from IDA):
    //   managerAddress -> base_hash_map
    //   [0x00] buckets pointer
    //   [0x08] bucketCount (int)
    // Each bucket is 24 bytes; bucket+0x10 is listHead, nodes are a linked list:
    //   node+0x08 -> next, node+0x10 -> nativeGameObject
    // We traverse all buckets to collect all GameObjects.

    std::uintptr_t buckets = 0;
    if (!ReadPtr(mem_, managerAddress + 0x0, buckets) || !buckets) {
        return false;
    }

    std::int32_t bucketCount = 0;
    if (!ReadInt32(mem_, managerAddress + 0x8, bucketCount) || bucketCount <= 0 || bucketCount > 0x100000) {
        return false;
    }

    const std::size_t kMaxObjects = 1000000;
    const std::uintptr_t bucketStride = 24; // 0x18

    const IMemoryAccessor& acc = mem_;

    for (std::int32_t bi = 0; bi < bucketCount; ++bi) {
        std::uintptr_t bucketPtr = buckets + static_cast<std::uintptr_t>(bi) * bucketStride;
        std::uintptr_t listHead = 0;
        if (!ReadPtr(mem_, bucketPtr + 0x10, listHead) || !listHead) {
            continue;
        }

        std::uintptr_t node = 0;
        if (!ReadPtr(mem_, listHead + 0x8, node) || !node) {
            continue;
        }

        bool isGameObjectBucket = true;
        std::uintptr_t firstNative = 0;
        std::uintptr_t firstManaged = 0;
        if (ReadPtr(mem_, node + 0x10, firstNative) && firstNative) {
            ReadPtr(mem_, firstNative + 0x28, firstManaged);
        }

        if (firstManaged) {
            TypeInfo info;
            if (GetManagedType(runtime_, acc, firstManaged, info) && info.name != "GameObject") {
                isGameObjectBucket = false;
            }
        }

        if (!isGameObjectBucket) {
            continue;
        }

        for (std::size_t i = 0; node && i < kMaxObjects; ++i) {
            std::uintptr_t nativeObject = 0;
            std::uintptr_t managedObject = 0;
            std::uintptr_t next = 0;

            if (!ReadPtr(mem_, node + 0x10, nativeObject)) {
                break;
            }
            if (nativeObject) {
                ReadPtr(mem_, nativeObject + 0x28, managedObject);
            }

            if (nativeObject || managedObject) {
                GameObjectEntry entry{};
                entry.node = node;
                entry.nativeObject = nativeObject;
                entry.managedObject = managedObject;
                out.push_back(entry);
            }

            if (!ReadPtr(mem_, node + 0x8, next) || !next || next == listHead) {
                break;
            }
            node = next;
        }
    }

    return !out.empty();
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
            continue;
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

            // Don't skip managed=0, some built-in components may have null managed ptr
            std::uintptr_t managedComponent = 0;
            ReadPtr(mem_, nativeComponent + 0x28, managedComponent);

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

// Find first GameObject with given tag using GOMWalker enumeration.
inline bool FindGameObjectThroughTag(const GOMWalker& walker,
                                     std::uintptr_t gomGlobalAddress,
                                     std::int32_t tag,
                                     std::uintptr_t& outNativeObject,
                                     std::uintptr_t& outManagedObject)
{
    outNativeObject = 0;
    outManagedObject = 0;

    std::vector<GameObjectEntry> gameObjects;
    if (!walker.EnumerateGameObjectsFromGlobal(gomGlobalAddress, gameObjects)) {
        return false;
    }

    for (const auto& go : gameObjects) {
        if (!go.nativeObject) continue;

        std::uint16_t tagValue = 0;
        if (ReadValue(walker.Accessor(), go.nativeObject + 0x54u, tagValue) &&
            static_cast<std::int32_t>(tagValue) == tag) {
            outNativeObject = go.nativeObject;
            outManagedObject = go.managedObject;
            return true;
        }
    }

    return false;
}

// Find first GameObject with given name using GOMWalker enumeration.
inline bool FindGameObjectThroughName(const GOMWalker& walker,
                                      std::uintptr_t gomGlobalAddress,
                                      const std::string& name,
                                      std::uintptr_t& outNativeObject,
                                      std::uintptr_t& outManagedObject)
{
    outNativeObject = 0;
    outManagedObject = 0;

    const IMemoryAccessor& acc = walker.Accessor();

    std::vector<GameObjectEntry> gameObjects;
    if (!walker.EnumerateGameObjectsFromGlobal(gomGlobalAddress, gameObjects) || gameObjects.empty()) {
        return false;
    }

    for (const auto& go : gameObjects) {
        if (!go.nativeObject) {
            continue;
        }

        std::uintptr_t namePtr = 0;
        if (!ReadPtr(acc, go.nativeObject + 0x60u, namePtr) || !namePtr) {
            continue;
        }

        std::string goName;
        if (!ReadCString(acc, namePtr, goName)) {
            continue;
        }

        if (goName == name) {
            outNativeObject = go.nativeObject;
            outManagedObject = go.managedObject;
            return true;
        }
    }

    return false;
}

// Get component on a GameObject by type ID.
inline bool GetComponentThroughTypeId(std::uintptr_t gameObjectNative,
                                      std::int32_t typeId,
                                      std::uintptr_t& outNativeComponent,
                                      std::uintptr_t& outManagedComponent)
{
    outNativeComponent = 0;
    outManagedComponent = 0;

    if (!gameObjectNative) {
        return false;
    }

    std::uintptr_t pool = 0;
    if (!ReadPtrGlobal(gameObjectNative + 0x30u, pool) || !pool) {
        return false;
    }

    std::int32_t count = 0;
    if (!ReadInt32Global(gameObjectNative + 0x40u, count)) {
        return false;
    }
    if (count <= 0 || count > 1024) {
        return false;
    }

    for (int i = 0; i < count; ++i) {
        std::uintptr_t entryAddr = pool + static_cast<std::uintptr_t>(i) * 16u;
        std::int32_t typeIdValue = 0;
        if (!ReadInt32Global(entryAddr, typeIdValue)) {
            continue;
        }

        if (typeIdValue != typeId) {
            continue;
        }

        std::uintptr_t slotAddr = pool + 0x8u + static_cast<std::uintptr_t>(i) * 0x10u;
        std::uintptr_t nativeComp = 0;
        if (!ReadPtrGlobal(slotAddr, nativeComp) || !nativeComp) {
            continue;
        }

        std::uintptr_t managedComp = 0;
        ReadPtrGlobal(nativeComp + 0x28u, managedComp);

        outNativeComponent = nativeComp;
        outManagedComponent = managedComp;
        return true;
    }

    return false;
}

// Get component on a GameObject by managed type name.
inline bool GetComponentThroughTypeName(std::uintptr_t gameObjectNative,
                                        const std::string& typeName,
                                        std::uintptr_t& outNativeComponent,
                                        std::uintptr_t& outManagedComponent,
                                        RuntimeKind runtime,
                                        const IMemoryAccessor& acc)
{
    outNativeComponent = 0;
    outManagedComponent = 0;

    if (!gameObjectNative) {
        return false;
    }

    std::uintptr_t pool = 0;
    if (!ReadPtr(acc, gameObjectNative + 0x30u, pool) || !pool) {
        return false;
    }

    std::int32_t count = 0;
    if (!ReadInt32(acc, gameObjectNative + 0x40u, count)) {
        return false;
    }
    if (count <= 0 || count > 1024) {
        return false;
    }

    for (int i = 0; i < count; ++i) {
        std::uintptr_t slotAddr = pool + 0x8u + static_cast<std::uintptr_t>(i) * 0x10u;
        std::uintptr_t nativeComp = 0;
        if (!ReadPtr(acc, slotAddr, nativeComp) || !nativeComp) {
            continue;
        }

        std::uintptr_t managedComp = 0;
        ReadPtr(acc, nativeComp + 0x28u, managedComp);
        if (!managedComp) {
            continue;
        }

        TypeInfo info;
        if (GetManagedType(runtime, acc, managedComp, info) && info.name == typeName) {
            outNativeComponent = nativeComp;
            outManagedComponent = managedComp;
            return true;
        }
    }

    return false;
}

inline bool FindGameObjectWithComponentThroughTypeName(const GOMWalker& walker,
                                                       std::uintptr_t gomGlobalAddress,
                                                       const std::string& typeName,
                                                       std::uintptr_t& outGameObjectNative,
                                                       std::uintptr_t& outNativeComponent,
                                                       std::uintptr_t& outManagedComponent)
{
    outGameObjectNative = 0;
    outNativeComponent = 0;
    outManagedComponent = 0;

    const IMemoryAccessor& acc = walker.Accessor();

    std::vector<GameObjectEntry> gameObjects;
    if (!walker.EnumerateGameObjectsFromGlobal(gomGlobalAddress, gameObjects) || gameObjects.empty()) {
        return false;
    }

    for (const auto& go : gameObjects) {
        if (!go.nativeObject) {
            continue;
        }

        std::uintptr_t nativeComp = 0;
        std::uintptr_t managedComp = 0;
        if (GetComponentThroughTypeName(go.nativeObject, typeName, nativeComp, managedComp, walker.GetRuntime(), acc)) {
            outGameObjectNative = go.nativeObject;
            outNativeComponent = nativeComp;
            outManagedComponent = managedComp;
            return true;
        }
    }

    return false;
}

} // namespace UnityExternal
