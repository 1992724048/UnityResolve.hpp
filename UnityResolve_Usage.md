# UnityResolve 支持库用法总结

> 本文说明 `UnityResolve.hpp` 与 `UnityResolve.GOM.hpp` 的核心用法，便于在任意 Unity/Mono/Il2Cpp 游戏中快速集成。

---

## 1. UnityResolve.hpp 基本概念

### 1.1 初始化与运行模式

- **运行模式**：
  - `UnityResolve::Mode::Il2Cpp`
  - `UnityResolve::Mode::Mono`
- **初始化入口**：
  - `UnityResolve::Init(void* hmodule, UnityResolve::Mode mode)`
  - `hmodule`：
    - Il2Cpp：`HMODULE gameAsm = GetModuleHandleW(L"GameAssembly.dll");`
    - Mono：`HMODULE mono = GetModuleHandleW(L"mono-2.0-bdwgc.dll");` 等

示例：

```cpp
HMODULE hGameAsm = GetModuleHandleW(L"GameAssembly.dll");
if (hGameAsm) {
    UnityResolve::Init(hGameAsm, UnityResolve::Mode::Il2Cpp);
} else {
    HMODULE hMono = GetModuleHandleW(L"mono.dll");
    if (hMono) UnityResolve::Init(hMono, UnityResolve::Mode::Mono);
}
```

### 1.2 线程附加 / 分离

- 在非 Unity 主线程中调用托管 API 时，需要先附加线程：
  - `UnityResolve::ThreadAttach();`
- 结束时可选调用：
  - `UnityResolve::ThreadDetach();`

通常做法：

```cpp
DWORD WINAPI WorkerThread(LPVOID) {
    UnityResolve::ThreadAttach();
    // 托管调用逻辑
    UnityResolve::ThreadDetach();
    return 0;
}
```

### 1.3 程序集与类查询

- 全局程序集列表：`UnityResolve::assembly`（`std::vector<Assembly*>`）
- 按名称获取程序集：

```cpp
UnityResolve::Assembly* core = UnityResolve::Get("UnityEngine.CoreModule.dll");
UnityResolve::Assembly* game = UnityResolve::Get("BlueArchive.dll");
```

- 在程序集内查找类：

```cpp
using UR = UnityResolve;
UR::Assembly* asmBA = UR::Get("BlueArchive.dll");
if (asmBA) {
    // 按类名 + 命名空间 + 父类名（后两个可用 "*" 通配）
    UR::Class* battle = asmBA->Get("Battle", "MX.Logic.Battles");
}
```

### 1.4 字段与方法查询

在 `UnityResolve::Class` 上提供统一的 `Get` 接口：

- 获取字段对象：`Field*`：

```cpp
UnityResolve::Field* fld = battle->Get<UnityResolve::Field>("playerGroup");
int32_t offset = fld ? fld->offset : 0;
```

- 直接按字段名获取偏移：`int32_t*`：

```cpp
int32_t* pOffset = battle->Get<int32_t>("playerGroup");
int32_t offset = pOffset ? *pOffset : 0;
```

- 获取方法：`Method*`，可带参数类型过滤：

```cpp
// 按方法名查找（不区分重载）
UnityResolve::Method* m1 = battle->Get<UnityResolve::Method>("Tick");

// 带参数类型过滤（参数列表长度与类型名匹配）
UnityResolve::Method* m2 = battle->Get<UnityResolve::Method>(
    "DoSomething",
    {"System.Int32", "System.String"}
);
```

### 1.5 字段读写辅助

`Class` 提供了简化的实例字段读写封装：

```cpp
// 按字段名读写
int v = battle->GetValue<int>(obj, "hp");
battle->SetValue<int>(obj, "hp", 9999);

// 按偏移读写
int v2 = battle->GetValue<int>(obj, offset);
battle->SetValue<int>(obj, offset, 123);
```

### 1.6 方法调用（Invoke / RuntimeInvoke）

#### 1.6.1 直接函数指针调用：`Method::Invoke`

适用于：方法已经编译成 native（Il2Cpp 默认如此，Mono 需 `Compile()`）。

```cpp
UnityResolve::Method* method = battle->Get<UnityResolve::Method>("get_PlayerCount");
if (method) {
    int count = method->Invoke<int>(instance /*this*/);
}
```

#### 1.6.2 runtime 调用：`Method::RuntimeInvoke`

封装了 `il2cpp_runtime_invoke` / `mono_runtime_invoke`：

```cpp
UnityResolve::Method* method = battle->Get<UnityResolve::Method>("Tick");
if (method) {
    method->RuntimeInvoke<void>(instance /*this*/);
}
```

### 1.7 UnityType 辅助结构

`UnityResolve::UnityType` 内提供常用 Unity 结构体 / 类的 C++ 映射和静态方法，例如：

- 向量与矩阵：`Vector2`/`Vector3`/`Vector4`/`Quaternion`/`Matrix4x4`
- UnityEngine 对象包装：`GameObject`/`Transform`/`Camera`/`Rigidbody` 等
- 集合类型：`UnityType::Array<T>` / `List<T>` / `Dictionary<TKey, TValue>`

示例：获取所有摄像机：

```cpp
using UR = UnityResolve;
std::vector<UR::UnityType::Camera*> cams = UR::UnityType::Camera::GetAllCamera();
```

示例：通过 `GameObject` 获取组件：

```cpp
using UR = UnityResolve;
UR::UnityType::GameObject* go = UR::UnityType::GameObject::Find("Player");
if (go) {
    UR::UnityType::Transform* tr = go->GetTransform();
}
```

---

## 2. UnityResolve.GOM.hpp：GOM 探测

`UnityResolve.GOM.hpp` 提供自动定位 Unity `GameObjectManager` 全局指针的封装 API，无需手动维护偏移表。

### 2.1 平台要求

- **仅 Windows**：依赖 `WINDOWS_MODE` 和 `Psapi.h`
- **运行时支持**：Mono 和 IL2CPP
- **前置条件**：
  - `UnityPlayer.dll` 已加载
  - `UnityEngine.CoreModule.dll` 和 `Camera.get_main` 可访问

### 2.2 核心 API

```cpp
struct UnityGOMInfo {
    std::uintptr_t address; // GOM 全局变量的绝对地址
    std::uintptr_t offset;  // 相对 UnityPlayer.dll 的偏移
};

UnityGOMInfo UnityResolveGOM::FindGameObjectManager();
```

**返回值**：
- `address == 0`：扫描失败
- `address != 0`：成功，`offset` 可用于跨版本适配

### 2.3 内部原理概览（可选阅读）

内部实现大致会完成以下几步（仅作概念说明，具体指令匹配与偏移细节以源码为准）：  

1. **检测运行时类型**  
   - Il2Cpp：优先检测 `GameAssembly.dll`  
   - Mono：在一组常见 mono 模块名中查找已加载模块  

2. **获取扫描起点**  
   - 使用 `UnityResolve::Init` + `UnityResolve::ThreadAttach` 完成托管运行时初始化  
   - 通过反射拿到 `UnityEngine.Camera.get_main` 的 native 函数指针，作为入口起点  

3. **沿调用链解析 GOM 全局指针**  
   - 在入口附近跟踪进入 `UnityPlayer.dll` 的调用  
   - 在 UnityPlayer 的相关函数中，解析指向全局区域的访问，并识别出 `GameObjectManager` 全局变量  

运行时会输出简单日志，示例：  

```text
Runtime: IL2CPP
GameObjectManager->UnityPlayer.dll+0x1D15C78
```

### 2.4 使用示例

```cpp
#include <windows.h>
#include <iostream>
#include "UnityResolve.hpp"

DWORD WINAPI MainThread(LPVOID param) {
    HMODULE hModule = static_cast<HMODULE>(param);
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);

    std::cout << "[UnityCatcher] Installation Successful" << std::endl;

    UnityGOMInfo gom = UnityResolveGOM::FindGameObjectManager();
    if (!gom.address) {
        std::cout << "GameObjectManager not found" << std::endl;
    }

    std::cout << "Exiting..." << std::endl;
    FreeConsole();
    FreeLibraryAndExitThread(hModule, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        HANDLE hThread = CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
```

**注意**：`FindGameObjectManager()` 在 Debug/开发阶段会打印少量扫描日志（运行时类型、GOM 偏移等），Release 版可按需关闭这些输出。

---

## 3. 基于 GOM 的遍历接口

### 3.1 GameObject 遍历

`UnityResolve.GOM.hpp` 内提供对 GOM 链表的统一封装：  

```cpp
struct GameObjectInfo {
    std::uintptr_t                          nativeObject;   // 原生 GameObject 指针
    UnityResolve::UnityType::GameObject*    managedObject;  // 托管 GameObject 实例
};

std::vector<GameObjectInfo> UnityResolveGOM::EnumerateGameObjects();
```

- 返回值包含 GOM 管理的所有 GameObject（过滤掉空指针）。  
- 需要在 `FindGameObjectManager()` 成功之后使用。  

### 3.2 Component 遍历（原生组件池）

```cpp
struct ComponentInfo {
    std::uintptr_t                           nativeComponent;   // 原生 Component 指针
    UnityResolve::UnityType::Component*      managedComponent;  // 托管 Component 实例
};

std::vector<ComponentInfo> UnityResolveGOM::EnumerateComponents();
```

- 直接通过原生 `GameObject` 的组件池遍历组件。  
- 适合需要同时关注 native/managed 指针结构的场景。  

### 3.3 Component 遍历（推荐：托管 GetComponents）

```cpp
std::vector<ComponentInfo> UnityResolveGOM::EnumerateComponentsByGetComponents();
```

- 使用托管 `GameObject.GetComponents` 统一获取组件列表，然后为每个组件补充原生指针。  
- 只保留 GOM 中存在的 GameObject 对应的组件，更接近 Unity“逻辑视角”的组件集合。  
- 推荐用于**统计 / 打印所有组件类型**等用途。  

### 3.4 统计与打印示例

下例展示如何统计并打印所有组件（示例逻辑与 `example_UnityCatcher.cpp` 保持一致，只保留核心部分）：  

```cpp
using UR = UnityResolve;

// 1. 自动定位 GOM
UnityGOMInfo gom = UnityResolveGOM::FindGameObjectManager();
if (!gom.address) {
    std::cout << "GameObjectManager not found" << std::endl;
    return;
}

// 2. 遍历 GameObject 和 Component
auto gameObjects = UnityResolveGOM::EnumerateGameObjects();
auto components  = UnityResolveGOM::EnumerateComponentsByGetComponents();

std::cout << "GameObjects: " << gameObjects.size() << std::endl;
std::cout << "Components: "  << components.size()  << std::endl;

// 3. 打印每个组件的原生地址与完整类型名
for (const auto &c : components) {
    if (!c.managedComponent) continue;

    auto type = c.managedComponent->GetType();
    if (!type) continue;

    const char* ns   = type->GetNamespace();
    const char* name = type->GetName();

    std::cout << std::hex << std::uppercase
              << "0x" << c.nativeComponent << "-"
              << (ns && *ns ? ns : "None") << "." << (name ? name : "")
              << std::dec << std::nouppercase
              << std::endl;
}
```

---

## 4. 集成建议

1. **一次性初始化**：
   - `UnityResolve::Init` + `ThreadAttach` 只需在进程启动时调用一次
   - `FindGameObjectManager()` 在启动阶段调用一次，缓存 `offset`

2. **性能优化**：
   - 避免在高频逻辑中重复调用 GOM 扫描
   - 将 `offset` 保存为全局常量，用于后续 GOM 遍历

3. **架构分层**：
   - **UnityResolve**：处理反射（类/字段/方法）和托管调用
   - **GOM 扫描**：一次性定位 GameObjectManager 地址
   - **业务逻辑**：使用 GOM 地址遍历场景对象

这样实现"通用框架 + 游戏业务"解耦，提升跨版本适配能力.
