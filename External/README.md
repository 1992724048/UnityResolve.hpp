# UnityExternal

Unity 游戏跨进程外挂库，纯外部内存读取，无需注入。

## 特性

- **跨进程内存读取**：基于 `ReadProcessMemory`，支持自定义驱动接口
- **GOM 遍历**：枚举所有 GameObject / Component
- **Transform 世界坐标**：通过 Hierarchy 层级计算真实世界坐标
- **相机 W2S**：读取相机矩阵，世界坐标转屏幕坐标
- **支持 Mono / IL2CPP**：自动适配两种运行时的内存结构

> [!IMPORTANT]
> **需要手动传入 GOM 偏移**  
> GameObjectManager 全局指针偏移因游戏/Unity版本不同而异，需自行逆向查找。  
> 格式：`UnityPlayer.dll + 偏移`，例如 `unityPlayerBase + 0x1AE8C50`

## 依赖

需要手动添加 [GLM 库](https://github.com/g-truc/glm)：

```bash
git clone https://github.com/g-truc/glm.git
```

目录结构：
```
项目根目录/
├── glm/
│   ├── glm.hpp
│   └── ...
└── External/
    └── ...
```

## 目录结构

```
External/
├── Core/                  # 基础内存接口
│   ├── UnityExternalMemory.hpp        # IMemoryAccessor 接口
│   ├── UnityExternalMemoryConfig.hpp  # 全局访问器 + ReadPtrGlobal
│   └── UnityExternalTypes.hpp         # RuntimeKind / TypeInfo / GetManagedType
│
├── MemoryRead/            # 内存读取实现（可替换）
│   └── UnityExternalMemoryWinAPI.hpp  # 默认 WinAPI 实现
│
├── GameObjectManager/     # GOM 遍历 + 原生结构
│   ├── UnityExternalGOM.hpp           # GOMWalker
│   ├── Managed/ManagedObject.hpp      # 托管对象封装
│   └── Native/
│       ├── NativeGameObject.hpp
│       ├── NativeComponent.hpp
│       └── NativeTransform.hpp
│
├── Camera/                # 相机 + W2S
│   ├── UnityExternalCamera.hpp        # FindMainCamera / Camera_GetMatrix
│   └── UnityExternalWorldToScreen.hpp # WorldToScreenPoint
│
├── Analysis/              # 内存结构分析文档
│   ├── IL2CPP内存结构.txt
│   ├── MONO内存结构.txt
│   └── UnityExternalTransform_PosAlgorithm.txt
│
└── ExternalResolve.hpp    # 统一入口（include 这个即可）
```

## 快速开始

```cpp
#include "External/ExternalResolve.hpp"

int main() {
    // 1. 打开目标进程
    HANDLE hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);

    // 2. 创建内存访问器
    UnityExternal::WinAPIMemoryAccessor accessor(hProcess);
    UnityExternal::SetGlobalMemoryAccessor(&accessor);

    // 3. 创建 GOM Walker
    std::uintptr_t gomGlobal = unityPlayerBase + GOM_OFFSET;  // 需自行查找偏移
    UnityExternal::GOMWalker walker(accessor, UnityExternal::RuntimeKind::Mono);
    // 或 RuntimeKind::Il2Cpp

    // 4. 遍历所有 GameObject
    std::vector<UnityExternal::GameObjectEntry> gameObjects;
    walker.EnumerateGameObjectsFromGlobal(gomGlobal, gameObjects);

    for (const auto& go : gameObjects) {
        UnityExternal::NativeGameObject nativeGO(go.nativeObject);
        std::string name = nativeGO.GetName();
        // ...
    }

    // 5. 查找主相机
    std::uintptr_t camNative = 0, camManaged = 0;
    UnityExternal::FindMainCamera(walker, gomGlobal, camNative, camManaged);

    // 6. 读取相机矩阵
    glm::mat4 camMatrix;
    UnityExternal::Camera_GetMatrix(camNative, camMatrix);

    // 7. 获取 Transform 世界坐标
    UnityExternal::Vector3f worldPos;
    UnityExternal::GetTransformWorldPosition(transformNative, worldPos);

    // 8. 世界坐标转屏幕坐标
    UnityExternal::ScreenRect screen{ 0, 0, 1920, 1080 };
    auto result = UnityExternal::WorldToScreenPoint(camMatrix, screen, 
        glm::vec3(worldPos.x, worldPos.y, worldPos.z));

    if (result.visible) {
        // 绘制 ESP ...
    }

    CloseHandle(hProcess);
    return 0;
}
```

## 自定义内存访问器

默认使用 `WinAPIMemoryAccessor`，可替换为驱动或其他实现：

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

## 关键偏移

### GOM 全局指针

需要在 `UnityPlayer.dll` 中查找 GameObjectManager 全局指针偏移，不同游戏/版本偏移不同。

### 内存结构

参考 `Analysis/` 目录下的文档：

| 运行时 | 类名偏移 |
|--------|----------|
| **IL2CPP** | `managed+0x00 → klass+0x10 → name` |
| **Mono** | `managed+0x00 → vtable+0x00 → klass+0x48 → name` |

### 原生结构偏移

| 结构 | 偏移 | 说明 |
|------|------|------|
| NativeGameObject | +0x30 | 组件池指针 |
| NativeGameObject | +0x40 | 组件数量 |
| NativeGameObject | +0x60 | 名称字符串指针 |
| NativeComponent | +0x28 | 托管组件指针 |
| NativeComponent | +0x30 | 原生 GameObject 指针 |
| NativeTransform | +0x38 | Hierarchy State 指针 |
| NativeTransform | +0x40 | Hierarchy 索引 |
| Camera | +0x100 | 视图投影矩阵 (4x4) |

## 平台

- Windows x64

## 许可

MIT License
