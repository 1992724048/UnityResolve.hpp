#pragma once

#include <cstdint>
#include <string>
#include "UnityExternalMemory.hpp"

namespace UnityExternal {

// Runtime type enumeration
enum class RuntimeKind : int {
    Il2Cpp,
    Mono
};

// Type information returned by GetManagedType
struct TypeInfo {
    std::string name;
    std::string namespaze;
};

namespace detail {

// IL2CPP internal structures
struct Il2CppManagedObjectHeader {
    std::uintptr_t klass;
};

struct Il2CppClassInternal {
    unsigned char pad[0x10];
    std::uintptr_t name;
    std::uintptr_t namespaze;
};

// Mono internal structures
struct MonoManagedObjectHeader {
    std::uintptr_t vtable;
};

struct MonoVTableInternal {
    std::uintptr_t klass;
};

struct MonoClassInternal {
    unsigned char pad[0x48];
    std::uintptr_t name;
    std::uintptr_t namespaze;
};

inline bool GetIl2CppType(const IMemoryAccessor& mem, std::uintptr_t managedObj, TypeInfo& out) {
    Il2CppManagedObjectHeader header{};
    if (!ReadValue(mem, managedObj, header)) {
        return false;
    }
    if (!header.klass) {
        return false;
    }

    Il2CppClassInternal klass{};
    if (!ReadValue(mem, header.klass, klass)) {
        return false;
    }

    std::string name;
    std::string ns;
    if (!ReadCString(mem, klass.name, name)) {
        return false;
    }
    if (klass.namespaze) {
        if (!ReadCString(mem, klass.namespaze, ns)) {
            return false;
        }
    }

    out.name = name;
    out.namespaze = ns;
    return true;
}

inline bool GetMonoType(const IMemoryAccessor& mem, std::uintptr_t managedObj, TypeInfo& out) {
    MonoManagedObjectHeader header{};
    if (!ReadValue(mem, managedObj, header)) {
        return false;
    }
    if (!header.vtable) {
        return false;
    }

    MonoVTableInternal vtable{};
    if (!ReadValue(mem, header.vtable, vtable)) {
        return false;
    }
    if (!vtable.klass) {
        return false;
    }

    MonoClassInternal klass{};
    if (!ReadValue(mem, vtable.klass, klass)) {
        return false;
    }

    std::string name;
    std::string ns;
    if (!ReadCString(mem, klass.name, name)) {
        return false;
    }
    if (klass.namespaze) {
        if (!ReadCString(mem, klass.namespaze, ns)) {
            return false;
        }
    }

    out.name = name;
    out.namespaze = ns;
    return true;
}

} // namespace detail

// Get managed object's type info (class name + namespace)
inline bool GetManagedType(RuntimeKind runtime, const IMemoryAccessor& mem, std::uintptr_t managedObject, TypeInfo& out) {
    if (!managedObject) {
        return false;
    }

    if (runtime == RuntimeKind::Il2Cpp) {
        return detail::GetIl2CppType(mem, managedObject, out);
    }

    return detail::GetMonoType(mem, managedObject, out);
}

// Get only the class name
inline bool GetManagedClassName(RuntimeKind runtime, const IMemoryAccessor& mem, std::uintptr_t managedObject, std::string& out) {
    TypeInfo info;
    if (!GetManagedType(runtime, mem, managedObject, info)) {
        return false;
    }
    out = info.name;
    return true;
}

// Get only the namespace
inline bool GetManagedNamespace(RuntimeKind runtime, const IMemoryAccessor& mem, std::uintptr_t managedObject, std::string& out) {
    TypeInfo info;
    if (!GetManagedType(runtime, mem, managedObject, info)) {
        return false;
    }
    out = info.namespaze;
    return true;
}

} // namespace UnityExternal
