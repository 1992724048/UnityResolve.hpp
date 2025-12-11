#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <algorithm>

#include "../Core/UnityExternalMemory.hpp"
#include "../Core/UnityExternalMemoryConfig.hpp"
#include "../Core/UnityExternalTypes.hpp"
#include "../GameObjectManager/UnityExternalGOM.hpp"
#include "../GameObjectManager/Native/NativeGameObject.hpp"

#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"

namespace UnityExternal {

// Read camera's matrix directly from +0x100 (as provided by Unity)
// nativeCamera + 0x100 -> 4x4 matrix (16 floats)
inline bool Camera_GetMatrix(std::uintptr_t nativeCamera, glm::mat4& outMatrix)
{
    outMatrix = glm::mat4(1.0f);
    if (!nativeCamera) return false;

    const IMemoryAccessor* acc = GetGlobalMemoryAccessor();
    if (!acc) return false;

    float data[16] = {};
    if (!acc->Read(nativeCamera + 0x100u, data, sizeof(data))) return false;

    outMatrix = glm::make_mat4(data);
    return true;
}


// Check if component is enabled (nativeComponent + 0x38 -> enabled byte)
inline bool IsComponentEnabled(std::uintptr_t nativeComponent)
{
    if (!nativeComponent) return false;
    std::uint8_t enabled = 0;
    const IMemoryAccessor* acc = GetGlobalMemoryAccessor();
    if (!acc) return false;
    if (!acc->Read(nativeComponent + 0x38u, &enabled, 1)) return true; // Assume enabled if can't read
    return enabled != 0;
}

// Find main camera: use GOMWalker helpers to get tag=5 GameObject, then find Camera component on it.
inline bool FindMainCamera(const GOMWalker& walker,
                           std::uintptr_t gomGlobalAddress,
                           std::uintptr_t& outNativeCamera,
                           std::uintptr_t& outManagedCamera)
{
    outNativeCamera = 0;
    outManagedCamera = 0;

    const IMemoryAccessor* acc = GetGlobalMemoryAccessor();
    if (!acc || !gomGlobalAddress) return false;

    std::uintptr_t mainGoNative = 0;
    std::uintptr_t mainGoManaged = 0;
    if (!FindGameObjectThroughTag(walker, gomGlobalAddress, 5, mainGoNative, mainGoManaged) || !mainGoNative) {
        return false;
    }

    std::uintptr_t componentArray = 0;
    if (!ReadPtrGlobal(mainGoNative + 0x30u, componentArray) || !componentArray) {
        return false;
    }

    std::int32_t componentCount = 0;
    if (!ReadInt32Global(mainGoNative + 0x40u, componentCount) || componentCount <= 0 || componentCount > 1024) {
        return false;
    }

    for (int c = 0; c < componentCount; ++c)
    {
        std::uintptr_t compEntryAddr = componentArray + static_cast<std::uintptr_t>(c) * 16u;
        std::uintptr_t nativeComp = 0;
        if (!ReadPtrGlobal(compEntryAddr + 0x8u, nativeComp) || !nativeComp) continue;

        std::uintptr_t managedComp = 0;
        ReadPtrGlobal(nativeComp + 0x28u, managedComp);
        if (!managedComp) continue;

        std::string typeName;
        std::uintptr_t vtable = 0, monoClass = 0, namePtr = 0;
        if (ReadPtrGlobal(managedComp + 0x0u, vtable) &&
            ReadPtrGlobal(vtable + 0x0u, monoClass) &&
            ReadPtrGlobal(monoClass + 0x48u, namePtr)) {
            ReadCString(*acc, namePtr, typeName);
        }

        if (typeName == "Camera") {
            if (!IsComponentEnabled(nativeComp)) {
                continue;
            }
            outNativeCamera = nativeComp;
            outManagedCamera = managedComp;
            return true;
        }
    }

    return false;
}

} // namespace UnityExternal
