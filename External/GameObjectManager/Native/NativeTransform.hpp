#pragma once

#include <cstdint>
#include <immintrin.h>

#include "../../Core/UnityExternalMemory.hpp"
#include "../../Core/UnityExternalMemoryConfig.hpp"
#include "../../Core/UnityExternalTypes.hpp"
#include "NativeGameObject.hpp"

namespace UnityExternal {

struct Vector3f {
    float x;
    float y;
    float z;
};

struct TransformHierarchyState {
    std::uintptr_t nodeData;
    std::uintptr_t parentIndices;
};

// Native Transform layout:
// +0x28 -> managed Transform pointer
// +0x30 -> native GameObject pointer
// +0x38 -> hierarchy state pointer
// +0x40 -> index in hierarchy (int32)

struct NativeTransform {
    std::uintptr_t address;

    NativeTransform() : address(0) {}
    explicit NativeTransform(std::uintptr_t addr) : address(addr) {}

    bool IsValid() const { return address != 0; }

    // Get managed pointer (+0x28)
    bool GetManaged(std::uintptr_t& outManaged) const {
        outManaged = 0;
        if (!address) return false;
        return ReadPtrGlobal(address + 0x28u, outManaged) && outManaged != 0;
    }

    // Get native GameObject (+0x30)
    bool GetGameObject(std::uintptr_t& outGameObject) const {
        outGameObject = 0;
        if (!address) return false;
        return ReadPtrGlobal(address + 0x30u, outGameObject) && outGameObject != 0;
    }

    // Read hierarchy state for position calculation
    bool ReadHierarchyState(TransformHierarchyState& outState, std::int32_t& outIndex) const {
        if (!address) return false;

        std::uintptr_t statePtr = 0;
        if (!ReadPtrGlobal(address + 0x38u, statePtr) || !statePtr) {
            return false;
        }

        if (!ReadPtrGlobal(statePtr + 0x18u, outState.nodeData)) {
            return false;
        }

        if (!ReadPtrGlobal(statePtr + 0x20u, outState.parentIndices)) {
            return false;
        }

        if (!ReadInt32Global(address + 0x40u, outIndex)) {
            return false;
        }

        if (!outState.nodeData || !outState.parentIndices || outIndex < 0) {
            return false;
        }

        return true;
    }

    // Get world position
    bool GetWorldPosition(Vector3f& outPos, int maxDepth = 256) const;
};

// Compute world position from hierarchy state (SSE optimized)
inline bool ComputeWorldPositionFromHierarchy(const TransformHierarchyState& state,
                                              std::int32_t index,
                                              Vector3f& outPos,
                                              int maxDepth = 256) {
    const IMemoryAccessor* mem = GetGlobalMemoryAccessor();
    if (!mem) return false;

    if (!state.nodeData || !state.parentIndices || index < 0 || maxDepth <= 0) {
        return false;
    }

    float selfNode[12] = {};
    std::uintptr_t selfAddr = state.nodeData + static_cast<std::uintptr_t>(index) * 48u;
    if (!mem->Read(selfAddr, selfNode, sizeof(selfNode))) {
        return false;
    }

    __m128 acc = _mm_loadu_ps(selfNode);

    int parent = 0;
    std::uintptr_t parentAddr = state.parentIndices + static_cast<std::uintptr_t>(index) * sizeof(std::int32_t);
    if (!mem->Read(parentAddr, &parent, sizeof(parent))) {
        return false;
    }

    int depth = 0;
    while (parent >= 0 && depth < maxDepth) {
        float node[12] = {};
        std::uintptr_t nodeAddr = state.nodeData + static_cast<std::uintptr_t>(parent) * 48u;
        if (!mem->Read(nodeAddr, node, sizeof(node))) {
            return false;
        }

        __m128 t  = _mm_loadu_ps(node + 0);
        __m128 qv = _mm_loadu_ps(node + 4);
        __m128 m  = _mm_loadu_ps(node + 8);

        __m128 v14 = _mm_mul_ps(m, acc);

        __m128i qvi = _mm_castps_si128(qv);
        __m128 v15  = _mm_castsi128_ps(_mm_shuffle_epi32(qvi, 219));
        __m128 v16  = _mm_castsi128_ps(_mm_shuffle_epi32(qvi, 113));
        __m128 v17  = _mm_castsi128_ps(_mm_shuffle_epi32(qvi, 142));

        __m128i v14i = _mm_castps_si128(v14);
        __m128 v14_x = _mm_castsi128_ps(_mm_shuffle_epi32(v14i, 0));
        __m128 v14_y = _mm_castsi128_ps(_mm_shuffle_epi32(v14i, 85));
        __m128 v14_z = _mm_castsi128_ps(_mm_shuffle_epi32(v14i, 170));

        const __m128 two = _mm_set1_ps(2.0f);

        __m128 q1 = _mm_castsi128_ps(_mm_shuffle_epi32(qvi, 85));
        __m128 q2 = _mm_castsi128_ps(_mm_shuffle_epi32(qvi, 170));
        __m128 q0 = _mm_castsi128_ps(_mm_shuffle_epi32(qvi, 0));

        __m128 part0 = _mm_mul_ps(
            _mm_sub_ps(
                _mm_mul_ps(_mm_mul_ps(q1, two), v16),
                _mm_mul_ps(_mm_mul_ps(q2, two), v17)),
            v14_x);

        __m128 part1 = _mm_mul_ps(
            _mm_sub_ps(
                _mm_mul_ps(_mm_mul_ps(q2, two), v15),
                _mm_mul_ps(_mm_mul_ps(q0, two), v16)),
            v14_y);

        __m128 part2 = _mm_mul_ps(
            _mm_sub_ps(
                _mm_mul_ps(_mm_mul_ps(q0, two), v17),
                _mm_mul_ps(_mm_mul_ps(q1, two), v15)),
            v14_z);

        acc = _mm_add_ps(_mm_add_ps(_mm_add_ps(part0, v14), _mm_add_ps(part1, part2)), t);

        parentAddr = state.parentIndices + static_cast<std::uintptr_t>(parent) * sizeof(std::int32_t);
        if (!mem->Read(parentAddr, &parent, sizeof(parent))) {
            return false;
        }

        ++depth;
    }

    float tmp[4];
    _mm_storeu_ps(tmp, acc);
    outPos.x = tmp[0];
    outPos.y = tmp[1];
    outPos.z = tmp[2];
    return true;
}

inline bool NativeTransform::GetWorldPosition(Vector3f& outPos, int maxDepth) const {
    TransformHierarchyState state{};
    std::int32_t index = 0;
    if (!ReadHierarchyState(state, index)) {
        return false;
    }
    return ComputeWorldPositionFromHierarchy(state, index, outPos, maxDepth);
}

// Helper: Read hierarchy state from transform address
inline bool ReadTransformHierarchyState(std::uintptr_t transformAddress,
                                        TransformHierarchyState& outState,
                                        std::int32_t& outIndex) {
    NativeTransform t(transformAddress);
    return t.ReadHierarchyState(outState, outIndex);
}

// Helper: Get world position from transform address
inline bool GetTransformWorldPosition(std::uintptr_t transformAddress,
                                      Vector3f& outPos,
                                      int maxDepth = 256) {
    NativeTransform t(transformAddress);
    return t.GetWorldPosition(outPos, maxDepth);
}

// Find Transform component on a GameObject
inline bool FindTransformOnGameObject(RuntimeKind runtime,
                                      std::uintptr_t gameObjectNative,
                                      std::uintptr_t& outTransformNative)
{
    const IMemoryAccessor* acc = GetGlobalMemoryAccessor();
    if (!acc || !gameObjectNative) return false;

    NativeGameObject go(gameObjectNative);

    std::int32_t count = 0;
    if (!go.GetComponentCount(count) || count <= 0) return false;

    for (std::int32_t i = 0; i < count; ++i) {
        std::uintptr_t nativeComp = 0;
        if (!go.GetComponent(i, nativeComp)) continue;

        std::uintptr_t managedComp = 0;
        if (!ReadPtrGlobal(nativeComp + 0x28u, managedComp) || !managedComp) continue;

        TypeInfo info;
        if (!GetManagedType(runtime, *acc, managedComp, info)) continue;

        if (info.name == "Transform") {
            outTransformNative = nativeComp;
            return true;
        }
    }

    return false;
}

} // namespace UnityExternal
