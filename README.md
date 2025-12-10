# UnityResolve.hpp

Unity 游戏逆向工具库，支持 **内注入** 和 **跨进程外挂** 两种模式。

> [!NOTE]
> 有任何新功能建议或 Bug，欢迎提交 Issue 或 Pull Request。

---

## 目录

- [概述](#概述)
- [External 跨进程模块](#external-跨进程模块)
- [UnityResolve 内注入模块](#unityresolve-内注入模块)

---

## 概述

| 模块 | 说明 | 平台 |
|------|------|------|
| **UnityResolve.hpp** | 内注入库，通过 DLL 注入调用托管 API | Windows / Android / Linux / iOS / HarmonyOS |
| **External/** | 跨进程外挂库，纯外部内存读取 | Windows |

> [!WARNING]
> 如果编译器支持，请务必开启 SEH（结构化异常处理）。

> [!TIP]
> 高版本 Android 崩溃问题请参考 [Issue #11](https://github.com/issuimo/UnityResolve.hpp/issues/11)。

**示例项目**
- [Phasmophobia Cheat (IL2CPP)](https://github.com/issuimo/PhasmophobiaCheat)
- [SausageMan Cheat (IL2CPP)](https://github.com/1992724048/SausageManCheat/tree/master/GPP32)

---

## External 跨进程模块

独立于内注入的**纯外部内存读取模块**，适用于跨进程外挂开发。

### 依赖

需要手动添加 [GLM 库](https://github.com/g-truc/glm)：

```bash
# 克隆 GLM 到项目根目录
git clone https://github.com/g-truc/glm.git
# 或下载 Release 解压到项目根目录
```

确保目录结构为：
```
项目根目录/
├── glm/
│   ├── glm.hpp
│   └── ...
└── External/
    └── ...
```

### 目录结构

```
External/
├── Core/                  # 基础内存接口
│   ├── UnityExternalMemory.hpp        # IMemoryAccessor 接口
│   ├── UnityExternalMemoryConfig.hpp  # 全局访问器 + ReadPtrGlobal 等
│   └── UnityExternalTypes.hpp         # RuntimeKind / TypeInfo / GetManagedType
│
├── MemoryRead/            # 内存读取实现（可替换）
│   └── UnityExternalMemoryWinAPI.hpp  # 默认 WinAPI 实现
│
├── GameObjectManager/     # GOM 遍历 + 原生结构
│   ├── UnityExternalGOM.hpp           # GOMWalker
│   ├── Managed/ManagedObject.hpp      # 托管对象封装
│   └── Native/                        # 原生结构
│       ├── NativeGameObject.hpp
│       ├── NativeComponent.hpp
│       └── NativeTransform.hpp
│
├── Camera/                # 相机 + W2S
│   ├── UnityExternalCamera.hpp        # FindMainCamera / Camera_GetMatrix
│   └── UnityExternalWorldToScreen.hpp # WorldToScreenPoint
│
└── ExternalResolve.hpp    # 统一入口
```

### 快速开始

```cpp
#include "External/ExternalResolve.hpp"

// 1. 创建内存访问器
HANDLE hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
UnityExternal::WinAPIMemoryAccessor accessor(hProcess);
UnityExternal::SetGlobalMemoryAccessor(&accessor);

// 2. 遍历所有 GameObject
std::uintptr_t gomGlobal = unityPlayerBase + GOM_OFFSET;
UnityExternal::GOMWalker walker(accessor, UnityExternal::RuntimeKind::Mono);

std::vector<UnityExternal::GameObjectEntry> gameObjects;
walker.EnumerateGameObjectsFromGlobal(gomGlobal, gameObjects);

for (const auto& go : gameObjects) {
    UnityExternal::NativeGameObject nativeGO(go.nativeObject);
    std::string name = nativeGO.GetName();
    // ...
}

// 3. 获取 Transform 世界坐标
UnityExternal::Vector3f pos;
UnityExternal::GetTransformWorldPosition(transformNative, pos);

// 4. 相机 W2S
glm::mat4 camMatrix;
UnityExternal::Camera_GetMatrix(cameraNative, camMatrix);
auto result = UnityExternal::WorldToScreenPoint(camMatrix, screen, glm::vec3(pos.x, pos.y, pos.z));
```

### 自定义内存访问器

默认使用 `WinAPIMemoryAccessor`（基于 `ReadProcessMemory`），可替换为驱动或其他实现：

```cpp
class MyDriverAccessor : public UnityExternal::IMemoryAccessor {
public:
    bool Read(std::uintptr_t address, void* buffer, std::size_t size) const override {
        return MyDriver::ReadMemory(address, buffer, size);
    }
    bool Write(std::uintptr_t address, const void* buffer, std::size_t size) const override {
        return MyDriver::WriteMemory(address, buffer, size);
    }
};

MyDriverAccessor accessor;
UnityExternal::SetGlobalMemoryAccessor(&accessor);
```

---

## UnityResolve 内注入模块

通过 DLL 注入使用托管 API。

### 支持平台

- [x] Windows
- [x] Android
- [x] Linux
- [x] iOS
- [x] HarmonyOS

### 支持类型

Camera, Transform, Component, GameObject, Rigidbody, MonoBehaviour, Renderer, Mesh, Physics, Collider, Vector2/3/4, Quaternion, Matrix4x4, Array, List, Dictionary, String, Animator, Time, FieldInfo 等。

### 依赖

> [!CAUTION]
> 新版本强制要求 [GLM 库](https://github.com/g-truc/glm)

### 基础用法

#### 更改平台
```cpp
#define WINDOWS_MODE 1
#define ANDROID_MODE 0
#define LINUX_MODE 0
```

#### 初始化
```cpp
// Windows
UnityResolve::Init(GetModuleHandle(L"GameAssembly.dll"), UnityResolve::Mode::Il2Cpp);
// Linux / Android / iOS
UnityResolve::Init(dlopen("libil2cpp.so", RTLD_NOW), UnityResolve::Mode::Il2Cpp);
```

#### 线程附加
```cpp
UnityResolve::ThreadAttach();
// ... 操作 ...
UnityResolve::ThreadDetach();
```

#### 获取类和方法
```cpp
auto assembly = UnityResolve::Get("Assembly-CSharp.dll");
auto pClass = assembly->Get("PlayerController");

// 获取字段
auto field = pClass->Get<UnityResolve::Field>("health");
int health = pClass->GetValue<int>(playerInstance, "health");
pClass->SetValue<int>(playerInstance, "health", 100);

// 调用方法
auto method = pClass->Get<UnityResolve::Method>("TakeDamage");
method->Invoke<void>(playerInstance, 50);
```

#### W2S
```cpp
Camera* pCamera = UnityResolve::UnityType::Camera::GetMain();
Vector3 screenPos = pCamera->WorldToScreenPoint(worldPos, Eye::Left);
```

#### Dump
```cpp
UnityResolve::DumpToFile("./output/");
```

### GOM 内注入支持

`UnityResolve.GOM.hpp` 提供内注入环境下的原生 GOM 遍历（仅 Windows）：

```cpp
#include "UnityResolve.hpp"
#if WINDOWS_MODE
#include "UnityResolve.GOM.hpp"
#endif

auto gos = UnityResolveGOM::EnumerateGameObjects();
for (const auto& g : gos) {
    // g.nativeObject / g.managedObject
}
```

> [!WARNING]
> 需开启 SEH，不同 Unity 版本可能存在兼容性差异。

### 更多用法

#### Mono 注入
```cpp
// 仅 Mono 模式
UnityResolve::AssemblyLoad assembly("./MonoCsharp.dll");
```

#### 创建 C# 对象
```cpp
// 字符串
auto str = UnityResolve::UnityType::String::New("hello");

// 数组
auto array = UnityResolve::UnityType::Array<int>::New(pClass, 10);

// 实例
auto obj = pClass->New<MyClass*>();
```

#### 查找对象
```cpp
// 查找所有指定类型的对象
std::vector<Player*> players = pClass->FindObjectsByType<Player*>();

// 获取组件
auto comps = gameobj->GetComponents<T*>(pClass);
auto children = gameobj->GetComponentsInChildren<T*>(pClass);
```

#### 获取子类类型名
```cpp
auto pClass = UnityResolve::Get("UnityEngine.CoreModule.dll")->Get("MonoBehaviour");
auto obj = pClass->FindObjectsByType<MonoBehaviour*>()[0];
std::string typeName = obj->GetType()->GetFullName();
```
