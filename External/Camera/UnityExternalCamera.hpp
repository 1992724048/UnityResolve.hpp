#pragma once

#include <cstdint>
#include <vector>
#include <string>

#include "../Core/UnityExternalMemory.hpp"
#include "../Core/UnityExternalMemoryConfig.hpp"
#include "../Core/UnityExternalTypes.hpp"
#include "../GameObjectManager/UnityExternalGOM.hpp"

#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"

namespace UnityExternal {

// Read camera's view-projection matrix
// nativeCamera + 0x100 -> 4x4 matrix (16 floats)
inline bool Camera_GetMatrix(std::uintptr_t nativeCamera, glm::mat4& outMatrix)
{
    outMatrix = glm::mat4(1.0f);
    if (!nativeCamera) {
        return false;
    }

    const IMemoryAccessor* acc = GetGlobalMemoryAccessor();
    if (!acc) {
        return false;
    }

    float data[16] = {};
    if (!acc->Read(nativeCamera + 0x100u, data, sizeof(data))) {
        return false;
    }

    outMatrix = glm::make_mat4(data);
    return true;
}

// Find main camera with priority:
// 1. GameObject name "Main Camera"
// 2. GameObject name "Camera Top"
// 3. First enabled Camera component
inline bool FindMainCamera(const GOMWalker& walker,
                           std::uintptr_t gomGlobalAddress,
                           std::uintptr_t& outNativeCamera,
                           std::uintptr_t& outManagedCamera)
{
    outNativeCamera = 0;
    outManagedCamera = 0;

    const IMemoryAccessor* acc = GetGlobalMemoryAccessor();
    if (!acc || !gomGlobalAddress) {
        return false;
    }

    std::vector<ComponentEntry> components;
    if (!walker.EnumerateComponentsFromGlobal(gomGlobalAddress, components)) {
        return false;
    }
    if (components.empty()) {
        return false;
    }

    RuntimeKind runtime = walker.GetRuntime();

    // Candidates
    std::uintptr_t mainCameraNative = 0, mainCameraManaged = 0;
    std::uintptr_t cameraTopNative = 0, cameraTopManaged = 0;
    std::uintptr_t firstEnabledNative = 0, firstEnabledManaged = 0;

    for (const auto& entry : components) {
        if (!entry.managedComponent || !entry.nativeComponent) {
            continue;
        }

        TypeInfo typeInfo;
        if (!GetManagedType(runtime, *acc, entry.managedComponent, typeInfo)) {
            continue;
        }

        if (typeInfo.name != "Camera") {
            continue;
        }

        // nativeCamera+0x30 -> GameObject native
        std::uintptr_t goNative = 0;
        if (!ReadPtrGlobal(entry.nativeComponent + 0x30u, goNative) || !goNative) {
            continue;
        }

        // GameObject+0x60 -> name pointer
        std::uintptr_t namePtr = 0;
        if (!ReadPtr(*acc, goNative + 0x60u, namePtr) || !namePtr) {
            continue;
        }

        std::string goName;
        if (!ReadCString(*acc, namePtr, goName)) {
            continue;
        }

        // Priority 1: "Main Camera"
        if (goName == "Main Camera" && !mainCameraNative) {
            mainCameraNative = entry.nativeComponent;
            mainCameraManaged = entry.managedComponent;
            break; // Highest priority, return immediately
        }

        // Priority 2: "Camera Top"
        if (goName == "Camera Top" && !cameraTopNative) {
            cameraTopNative = entry.nativeComponent;
            cameraTopManaged = entry.managedComponent;
            continue;
        }

        // Priority 3: First camera (as fallback)
        if (!firstEnabledNative) {
            firstEnabledNative = entry.nativeComponent;
            firstEnabledManaged = entry.managedComponent;
        }
    }

    // Return by priority
    if (mainCameraNative) {
        outNativeCamera = mainCameraNative;
        outManagedCamera = mainCameraManaged;
        return true;
    }
    if (cameraTopNative) {
        outNativeCamera = cameraTopNative;
        outManagedCamera = cameraTopManaged;
        return true;
    }
    if (firstEnabledNative) {
        outNativeCamera = firstEnabledNative;
        outManagedCamera = firstEnabledManaged;
        return true;
    }

    return false;
}

} // namespace UnityExternal
