#pragma once

#if WINDOWS_MODE

#include <cstdint>
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")
#include <iostream>
#include <vector>

struct UnityGOMInfo {
    std::uintptr_t address;
    std::uintptr_t offset;
};

namespace UnityResolveGOM {

namespace {
    constexpr std::size_t kUnityFunctionScanLen = 0x400;
}

struct ModuleRange {
    std::uintptr_t base;
    std::uintptr_t end;
};

inline bool GetModuleRange(const wchar_t* name, ModuleRange& range) {
    HMODULE hMod = GetModuleHandleW(name);
    if (!hMod) return false;

    MODULEINFO mi{};
    if (!GetModuleInformation(GetCurrentProcess(), hMod, &mi, sizeof(mi))) return false;

    range.base = reinterpret_cast<std::uintptr_t>(mi.lpBaseOfDll);
    range.end  = range.base + static_cast<std::uintptr_t>(mi.SizeOfImage);
    return true;
}

inline bool IsInModule(const ModuleRange& range, std::uintptr_t addr) {
    return addr >= range.base && addr < range.end;
}

inline std::uintptr_t FindFirstMovInFunction(std::uintptr_t func, std::size_t maxLen, 
                                             const ModuleRange& unity) {
    const auto* code = reinterpret_cast<const unsigned char*>(func);

    for (std::size_t i = 0; i + 7 <= maxLen; ++i) {
        const unsigned char* p = code + i;

        if (p[0] == 0x48 && p[1] == 0x8B) {
            unsigned char modrm = p[2];
            if ((modrm & 0xC7) == 0x05) {
                auto disp   = *reinterpret_cast<const std::int32_t*>(p + 3);
                auto target = reinterpret_cast<std::uintptr_t>(p + 7) + static_cast<std::uintptr_t>(disp);
                if (IsInModule(unity, target)) {
                    return target;
                }
            }
        }
    }

    return 0;
}

inline void FindUnityGlobalAndCallOrder(std::uintptr_t func, std::size_t maxLen, const ModuleRange& unity,
                                        std::uintptr_t& unityGlobal, std::uintptr_t& firstCallTarget,
                                        std::size_t& globalPos, std::size_t& callPos) {
    const auto* code = reinterpret_cast<const unsigned char*>(func);
    unityGlobal = 0;
    firstCallTarget = 0;
    globalPos = static_cast<std::size_t>(-1);
    callPos = static_cast<std::size_t>(-1);

    for (std::size_t i = 0; i + 7 <= maxLen; ++i) {
        const unsigned char* p = code + i;

        // mov r64, [rip+disp32]
        if (p[0] == 0x48 && p[1] == 0x8B && globalPos == static_cast<std::size_t>(-1)) {
            unsigned char modrm = p[2];
            if ((modrm & 0xC7) == 0x05) {
                auto disp   = *reinterpret_cast<const std::int32_t*>(p + 3);
                auto target = reinterpret_cast<std::uintptr_t>(p + 7) + static_cast<std::uintptr_t>(disp);
                if (IsInModule(unity, target)) {
                    unityGlobal = target;
                    globalPos = i;
                }
            }
        }

        // mov rax, [abs]
        if (i + 10 <= maxLen && p[0] == 0x48 && p[1] == 0xA1 && globalPos == static_cast<std::size_t>(-1)) {
            auto target = *reinterpret_cast<const std::uintptr_t*>(p + 2);
            if (IsInModule(unity, target)) {
                unityGlobal = target;
                globalPos = i;
            }
        }

        // call rel32 -> only accept UnityPlayer targets
        if (p[0] == 0xE8 && callPos == static_cast<std::size_t>(-1)) {
            auto rel  = *reinterpret_cast<const std::int32_t*>(p + 1);
            auto tgt = reinterpret_cast<std::uintptr_t>(p + 5) + static_cast<std::uintptr_t>(rel);
            if (IsInModule(unity, tgt)) {
                firstCallTarget = tgt;
                callPos = i;
            }
        }

        if (globalPos != static_cast<std::size_t>(-1) && callPos != static_cast<std::size_t>(-1))
            break;
    }
}

inline std::uintptr_t FindFirstCallToUnityPlayer(std::uintptr_t func, std::size_t maxLen,
                                                 const ModuleRange& unityPlayer) {
    const auto* code = reinterpret_cast<const unsigned char*>(func);

    for (std::size_t i = 0; i + 5 <= maxLen; ++i) {
        const unsigned char* p = code + i;
        if (p[0] == 0xE8) {
            auto rel = *reinterpret_cast<const std::int32_t*>(p + 1);
            auto tgt = reinterpret_cast<std::uintptr_t>(p + 5) + static_cast<std::uintptr_t>(rel);
            if (IsInModule(unityPlayer, tgt)) {
                return tgt;
            }
        }
    }
    return 0;
}

inline std::uintptr_t ResolveGOMFromUnityPlayerEntry(std::uintptr_t entryFunc,
                                                      const ModuleRange& unityPlayer) {
    std::uintptr_t entryGlobal = 0;
    std::uintptr_t entryCall   = 0;
    std::size_t gpos = static_cast<std::size_t>(-1);
    std::size_t cpos = static_cast<std::size_t>(-1);
    FindUnityGlobalAndCallOrder(entryFunc, kUnityFunctionScanLen, unityPlayer, entryGlobal, entryCall, gpos, cpos);

    // mov in entry comes first -> this global is GOM
    if (gpos != static_cast<std::size_t>(-1) && (cpos == static_cast<std::size_t>(-1) || gpos < cpos)) {
        return entryGlobal;
    }

    // call in entry comes first -> jump once and search first mov in that function
    if (cpos != static_cast<std::size_t>(-1) && (gpos == static_cast<std::size_t>(-1) || cpos < gpos)) {
        std::uintptr_t foundGlobal = FindFirstMovInFunction(entryCall, kUnityFunctionScanLen, unityPlayer);
        if (foundGlobal) {
            return foundGlobal;
        }
    }

    return 0;
}

inline std::uintptr_t FindFirstMovToModule(std::uintptr_t func, std::size_t maxLen, 
                                           const ModuleRange& targetModule) {
    const auto* code = reinterpret_cast<const unsigned char*>(func);

    for (std::size_t i = 0; i + 7 <= maxLen; ++i) {
        const unsigned char* p = code + i;

        if (p[0] == 0x48 && p[1] == 0x8B && p[2] == 0x05) {
            auto disp   = *reinterpret_cast<const std::int32_t*>(p + 3);
            auto target = reinterpret_cast<std::uintptr_t>(p + 7) + static_cast<std::uintptr_t>(disp);
            if (IsInModule(targetModule, target)) {
                __try {
                    return *reinterpret_cast<std::uintptr_t*>(target);
                } __except(EXCEPTION_EXECUTE_HANDLER) {
                    return 0;
                }
            }
        }

        if (i + 10 <= maxLen && p[0] == 0x48 && p[1] == 0xA1) {
            auto target = *reinterpret_cast<const std::uintptr_t*>(p + 2);
            if (IsInModule(targetModule, target)) {
                __try {
                    return *reinterpret_cast<std::uintptr_t*>(target);
                } __except(EXCEPTION_EXECUTE_HANDLER) {
                    return 0;
                }
            }
        }

        if (i + 10 <= maxLen && p[0] == 0x48 && p[1] == 0xB8) {
            auto target = *reinterpret_cast<const std::uintptr_t*>(p + 2);
            if (IsInModule(targetModule, target)) {
                return target;
            }
        }
    }

    return 0;
}

inline UnityGOMInfo FindGameObjectManagerImpl() {
    UnityGOMInfo info{0, 0};
    
    HMODULE il2cppModule = GetModuleHandleW(L"GameAssembly.dll");
    HMODULE monoModule = nullptr;
    if (!il2cppModule) {
        monoModule = GetModuleHandleW(L"mono-2.0-bdwgc.dll");
        if (!monoModule) monoModule = GetModuleHandleW(L"mono-2.0-sgen.dll");
        if (!monoModule) monoModule = GetModuleHandleW(L"mono.dll");
        if (!monoModule) monoModule = GetModuleHandleW(L"mono-2.0.dll");
    }

    if (!monoModule && !il2cppModule) return info;

    bool isIl2cpp = (il2cppModule != nullptr);
    void* runtimeModule = isIl2cpp ? static_cast<void*>(il2cppModule) : static_cast<void*>(monoModule);

    std::cout << "Runtime: " << (isIl2cpp ? "IL2CPP" : "MONO") << std::endl;

    UnityResolve::Init(runtimeModule, isIl2cpp ? UnityResolve::Mode::Il2Cpp : UnityResolve::Mode::Mono);
    UnityResolve::ThreadAttach();

    auto coreAssembly = UnityResolve::Get("UnityEngine.CoreModule.dll");
    if (!coreAssembly) return info;

    auto cameraClass = coreAssembly->Get("Camera");
    if (!cameraClass) return info;
    auto getMainMethod = cameraClass->Get<UnityResolve::Method>("get_main");
    if (!getMainMethod) return info;

    auto nativePtr = getMainMethod->Cast<void>();
    if (!nativePtr) return info;
    std::uintptr_t getMainAddr = reinterpret_cast<std::uintptr_t>(nativePtr);

    // Call get_main once to ensure initialization
    getMainMethod->Invoke<void*>(nullptr);

    ModuleRange unityPlayer{};
    if (!GetModuleRange(L"UnityPlayer.dll", unityPlayer)) return info;

    std::uintptr_t unityEntry = 0;
    if (isIl2cpp) {
        ModuleRange gameAssembly{};
        if (!GetModuleRange(L"GameAssembly.dll", gameAssembly)) return info;
        unityEntry = FindFirstMovToModule(getMainAddr, kUnityFunctionScanLen, gameAssembly);
        if (unityEntry && !IsInModule(unityPlayer, unityEntry)) unityEntry = 0;
        if (!unityEntry) {
            unityEntry = FindFirstMovToModule(getMainAddr, kUnityFunctionScanLen, unityPlayer);
        }
    } else {
        unityEntry = FindFirstMovToModule(getMainAddr, kUnityFunctionScanLen, unityPlayer);
    }

    if (!unityEntry) return info;

    std::cout << "Entry[0]->UnityPlayer.dll+0x"
              << std::hex << std::uppercase << (unityEntry - unityPlayer.base)
              << std::dec << std::nouppercase << std::endl;

    std::uintptr_t step2Func = FindFirstCallToUnityPlayer(unityEntry, kUnityFunctionScanLen, unityPlayer);
    if (!step2Func) return info;

    std::cout << "Entry[1]->UnityPlayer.dll+0x"
              << std::hex << std::uppercase << (step2Func - unityPlayer.base)
              << std::dec << std::nouppercase << std::endl;

    std::uintptr_t unityGlobal = ResolveGOMFromUnityPlayerEntry(step2Func, unityPlayer);
    if (!unityGlobal) return info;

    info.address = unityGlobal;
    info.offset  = unityGlobal - unityPlayer.base;
    std::cout << "GameObjectManager->UnityPlayer.dll+0x" << std::hex << std::uppercase << info.offset
              << std::dec << std::nouppercase << std::endl;
    return info;
}

inline UnityGOMInfo FindGameObjectManager() {
    static bool initialized = false;
    static UnityGOMInfo cached{0, 0};

    if (initialized) {
        return cached;
    }

    __try {
        cached = FindGameObjectManagerImpl();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        cached = UnityGOMInfo{0, 0};
    }

    initialized = true;
    return cached;
}

inline std::uintptr_t SafeReadPtr(std::uintptr_t addr) {
    __try {
        return *reinterpret_cast<std::uintptr_t*>(addr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

inline std::int32_t SafeReadInt32(std::uintptr_t addr) {
    __try {
        return *reinterpret_cast<std::int32_t*>(addr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

struct GameObjectInfo {
    std::uintptr_t nativeObject;
    UnityResolve::UnityType::GameObject* managedObject;
};

inline std::vector<GameObjectInfo> EnumerateGameObjectsFromManager(std::uintptr_t manager) {
    std::vector<GameObjectInfo> result;
    if (!manager) return result;

    std::uintptr_t listHead = SafeReadPtr(manager + 0x28);
    if (!listHead) return result;

    const std::size_t kMaxObjects = 1000000;
    std::uintptr_t node = listHead;

    for (std::size_t i = 0; node && i < kMaxObjects; ++i) {
        std::uintptr_t nativeObject = SafeReadPtr(node + 0x10);
        std::uintptr_t managedObject = 0;
        std::uintptr_t next = SafeReadPtr(node + 0x8);

        if (nativeObject) {
            managedObject = SafeReadPtr(nativeObject + 0x28);
        }

        if (managedObject) {
            auto managed = reinterpret_cast<UnityResolve::UnityType::GameObject*>(managedObject);
            result.push_back(GameObjectInfo{nativeObject, managed});
        }

        if (!next || next == listHead) {
            break;
        }

        node = next;
    }

    return result;
}

inline std::vector<GameObjectInfo> EnumerateGameObjects() {
    UnityGOMInfo info = FindGameObjectManager();
    if (!info.address) return {};

    std::uintptr_t manager = SafeReadPtr(info.address);
    if (!manager) return {};

    return EnumerateGameObjectsFromManager(manager);
}

struct ComponentInfo {
    std::uintptr_t nativeComponent;
    UnityResolve::UnityType::Component* managedComponent;
};

inline std::vector<ComponentInfo> EnumerateComponents() {
    std::vector<ComponentInfo> result;

    auto gameObjects = EnumerateGameObjects();
    if (gameObjects.empty()) return result;

    const int kMaxComponentsPerObject = 1024;

    for (const auto &info : gameObjects) {
        if (!info.nativeObject) {
            continue;
        }

        auto nativeGo = info.nativeObject;

        auto componentPool = SafeReadPtr(nativeGo + 0x30);
        if (!componentPool) {
            continue;
        }

        auto componentCount = SafeReadInt32(nativeGo + 0x40);
        if (componentCount <= 0 || componentCount > kMaxComponentsPerObject) {
            continue;
        }

        for (int i = 0; i < componentCount; ++i) {
            auto slotAddr = componentPool + 0x8 + static_cast<std::uintptr_t>(i) * 0x10;
            auto compNative = SafeReadPtr(slotAddr);
            if (!compNative) {
                continue;
            }

            auto managedComp = SafeReadPtr(compNative + 0x28);
            if (!managedComp) {
                continue;
            }

            auto managed = reinterpret_cast<UnityResolve::UnityType::Component*>(managedComp);
            result.push_back(ComponentInfo{compNative, managed});
        }
    }

    return result;
}

inline std::vector<ComponentInfo> EnumerateComponentsByGetComponents() {
    std::vector<ComponentInfo> result;

    auto gomGameObjects = EnumerateGameObjects();
    if (gomGameObjects.empty()) return result;

    // 收集 GOM 中所有原生 GameObject 指针
    std::vector<std::uintptr_t> gomNativeGos;
    gomNativeGos.reserve(gomGameObjects.size());
    for (const auto &info : gomGameObjects) {
        if (info.nativeObject) {
            gomNativeGos.push_back(info.nativeObject);
        }
    }
    if (gomNativeGos.empty()) return result;

    auto coreAssembly = UnityResolve::Get("UnityEngine.CoreModule.dll");
    if (!coreAssembly) return result;

    auto gameObjectClass = coreAssembly->Get("GameObject");
    auto componentClass = coreAssembly->Get("Component");
    if (!gameObjectClass || !componentClass) return result;

    // 通过托管 API 获取所有 GameObject，再用 native 指针与 GOM 结果匹配
    auto managedGos = gameObjectClass->FindObjectsByType<UnityResolve::UnityType::GameObject*>();
    for (auto go : managedGos) {
        if (!go) continue;

        auto nativeGo = SafeReadPtr(reinterpret_cast<std::uintptr_t>(go) + 0x10);
        if (!nativeGo) continue;

        bool inGom = false;
        for (auto ng : gomNativeGos) {
            if (ng == nativeGo) {
                inGom = true;
                break;
            }
        }
        if (!inGom) continue;

        auto comps = go->GetComponents<UnityResolve::UnityType::Component*>(componentClass);
        for (auto comp : comps) {
            if (!comp) continue;

            std::uintptr_t nativeComp = SafeReadPtr(reinterpret_cast<std::uintptr_t>(comp) + 0x10);
            result.push_back(ComponentInfo{nativeComp, comp});
        }
    }

    return result;
}

} // namespace UnityResolveGOM

#endif // WINDOWS_MODE
