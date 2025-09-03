> [!IMPORTANT]
> 新版代码正在重构中  \
> **The codebase is currently under refactoring.**

> 示例代码 (Example code)
> - [Phasmophobia Cheat (il2cpp, old)](https://github.com/issuimo/PhasmophobiaCheat/tree/main)  
> - [SausageMan Cheat (il2cpp)](https://github.com/1992724048/SausageManCheat/tree/master/GPP32)

> [!NOTE]\
> 有任何新功能建议或 Bug，欢迎直接提交 Issue；当然也欢迎你动手修改代码后向本仓库发起 Pull Request。  
> New feature requests or bug reports are welcome via Issues, and Pull Requests are just as appreciated if you’d like to contribute code directly.

> [!WARNING]\
> 如果编译器支持，请务必开启 SEH（结构化异常处理）。  
> If your compiler supports it, please enable SEH (Structured Exception Handling).

> [!TIP]\
> 高版本 Android 上可能出现的崩溃问题，请参考 [此 Issue](https://github.com/issuimo/UnityResolve.hpp/issues/11)。  
> For potential crash issues on newer Android versions, see [this issue](https://github.com/issuimo/UnityResolve.hpp/issues/11).
<hr>
<h3 align="center">简要概述 (Brief overview)</h3>
<hr>

# UnityResolve.hpp
> ### 支持的平台 (Supported platforms)
> - [X] Windows
> - [X] Android
> - [X] Linux
> - [X] IOS
> - [X] HarmonyOS

> ### 类型 (Types)
> - [X] Camera
> - [X] Transform
> - [X] Component
> - [X] Object (Unity)
> - [X] LayerMask
> - [X] Rigidbody
> - [x] MonoBehaviour
> - [x] Renderer
> - [x] Mesh
> - [X] Behaviour
> - [X] Physics
> - [X] GameObject
> - [X] Collider
> - [X] Vector4
> - [X] Vector3
> - [X] Vector2
> - [X] Quaternion
> - [X] Bounds
> - [X] Plane
> - [X] Ray
> - [X] Rect
> - [X] Color
> - [X] Matrix4x4
> - [X] Array
> - [x] String
> - [x] Object (C#)
> - [X] Type (C#)
> - [X] List
> - [X] Dictionary
> - [X] Animator
> - [X] CapsuleCollider
> - [X] BoxCollider
> - [X] Time
> - [X] FieldInfo
> - More...

> ### 功能 (Functions)
> - [X] Mono注入 (Mono Inject)
> - [X] DumpToFile
> - [X] 附加线程 (Thread Attach / Detach)
> - [X] 修改静态变量值 (Modifying the value of a static variable)
> - [X] 获取对象 (Obtaining an instance)
> - [X] 创建C#字符串 (Create C# String)
> - [X] 创建C#数组 (Create C# Array)
> - [X] 创建C#对象 (Create C# instance)
> - [X] 世界坐标转屏幕坐标/屏幕坐标转世界坐标 (WorldToScreenPoint/ScreenToWorldPoint)
> - [X] 获取继承子类的名称 (Get the name of the inherited subclass)
> - [X] 获取函数地址(变量偏移) 及调用(修改/获取) (Get the function address (variable offset) and invoke (modify/get))
> - [x] 获取Gameobject组件 (Get GameObject component)
> - More...
<hr>
<h3 align="center">功能使用 (How to use)</h3>
<hr>

#### 使用GLM (use glm)
> [!CAUTION]
> 新版本强制性要求 \
> Mandatory requirements for new versions

[GLM Library](https://github.com/g-truc/glm)
> ``` C++
> #define USE_GLM // 新版本不需要添加 (New versions do not need to be added)
> #include "UnityResolve.hpp"
> ```

#### 更改平台 (Change platform)
> ``` c++
> #define WINDOWS_MODE 1 // 如果需要请改为 1 (1 if you need)
> #define ANDROID_MODE 0
> #define LINUX_MODE 0
> ```

#### 初始化 (Initialization)
> ``` c++
> // Windows
> UnityResolve::Init(GetModuleHandle(L"GameAssembly.dll | mono.dll"), UnityResolve::Mode::Mono);
> // Linux、Android、IOS、HarmonyOS
> UnityResolve::Init(dlopen(L"GameAssembly.so | mono.so", RTLD_NOW), UnityResolve::Mode::Mono);
> ```

#### 附加线程 (Thread Attach / Detach)
> [!TIP]
> 如果你是在游戏主线程使用或者通过Hook Update/LateUpdate 那么并不需要该功能 \
> If you are using it on the main thread of the game or via Hook Update/LateUpdate, you don't need this feature

> ``` c++
> // C# GC Attach
> UnityResolve::ThreadAttach();
> 
> // C# GC Detach
> UnityResolve::ThreadDetach();
> ```

#### Mono注入 (Mono Inject)
> [!TIP]
> 仅 Mono 模式可用 \
> Only Mono mode is available

> ``` c++
> UnityResolve::AssemblyLoad assembly("./MonoCsharp.dll");
> UnityResolve::AssemblyLoad assembly("./MonoCsharp.dll", "MonoCsharp", "Inject", "MonoCsharp.Inject:Load()");
> ```

#### 获取函数地址(变量偏移) 及调用(修改/获取) (Get the function address (variable offset) and invoke (modify/get))
> ``` c++
> const auto assembly = UnityResolve::get("assembly.dll | 程序集名称.dll");
> const auto pClass   = assembly->get("className | 类名称");
>                    // assembly->get("className | 类名称", "*");
>                    // assembly->get("className | 类名称", "namespace | 空间命名");
> 
> const auto field       = pClass->get<UnityResolve::Field>("Field Name | 变量名");
> const auto fieldOffset = pClass->get<std::int32_t>("Field Name | 变量名");
> const int  time        = pClass->GetValue<int>(obj Instance | 对象地址, "time");
>                       // pClass->GetValue(obj Instance*, name);
>                        = pClass->SetValue<int>(obj Instance | 对象地址, "time", 114514);
>                       // pClass->SetValue(obj Instance*, name, value);
> const auto method      = pClass->Get<UnityResolve::Method>("Method Name | 函数名");
>                       // pClass->Get<UnityResolve::Method>("Method Name | 函数名", { "System.String" });
>                       // pClass->Get<UnityResolve::Method>("Method Name | 函数名", { "*", "System.String" });
>                       // pClass->Get<UnityResolve::Method>("Method Name | 函数名", { "*", "", "System.String" });
>                       // pClass->Get<UnityResolve::Method>("Method Name | 函数名", { "*", "System.Int32", "System.String" });
>                       // pClass->Get<UnityResolve::Method>("Method Name | 函数名", { "*", "System.Int32", "System.String", "*" });
>                       // "*" == ""
> 
> const auto functionPtr = method->function;
> 
> const auto method1 = pClass->Get<UnityResolve::Method>("method name1 | 函数名称1");
> const auto method2 = pClass->Get<UnityResolve::Method>("method name2 | 函数名称2");
> 
> method1->Invoke<int>(114, 514, "114514");
> // Invoke<return type>(args...);
>
> // Cast<return type, args...>(void);
> // Cast(UnityResolve::MethodPointer<return type, args...>&);
> const UnityResolve::MethodPointer<void, int, bool> ptr = method2->Cast<void, int, bool>();
> ptr(114514, true);
>
> UnityResolve::MethodPointer<void, int, bool> add;
> ptr = method1->Cast(add);
>
> std::function<void(int, bool)> add2;
> method->Cast(add2);
>
> UnityResolve::Field::Variable<Vector3, Player> syncPos;
> syncPos.Init(pClass->Get<UnityResolve::Field>("syncPos"));
> auto pos = syncPos[playerInstance];
> auto pos = syncPos.Get(playerInstance);
>
> ```

#### 转存储到文件 (DumpToFile)
> ``` C++
> UnityResolve::DumpToFile("./output/");
> ```

#### 创建C#字符串 (Create C# String)
> ``` c++
> const auto str     = UnityResolve::UnityType::String::New("string | 字符串");
> std::string cppStr = str.ToString();
> ```

#### 创建C#数组 (Create C# Array)
> ``` c++
> const auto assembly = UnityResolve::Get("assembly.dll | 程序集名称.dll");
> const auto pClass   = assembly->Get("className | 类名称");
> const auto array    = UnityResolve::UnityType::Array<T>::New(pClass, size);
> std::vector<T> cppVector = array.ToVector();
> ```

#### 创建C#对象 (Create C# instance)
> ``` c++
> const auto assembly = UnityResolve::Get("assembly.dll | 程序集名称.dll");
> const auto pClass   = assembly->Get("className | 类名称");
> const auto pGame    = pClass->New<Game*>();
> ```

#### 获取对象 (Obtaining an instance)
> [!TIP]
> 仅 Unity 对象 \
> Unity objects only

> ``` c++
> const auto assembly = UnityResolve::Get("assembly.dll | 程序集名称.dll");
> const auto pClass   = assembly->Get("className | 类名称");
> std::vector<Player*> playerVector = pClass->FindObjectsByType<Player*>();
> // FindObjectsByType<return type>(void);
> playerVector.size();
> ```

#### 世界坐标转屏幕坐标/屏幕坐标转世界坐标 (WorldToScreenPoint/ScreenToWorldPoint)
> ``` c++
> Camera* pCamera = UnityResolve::UnityType::Camera::GetMain();
> Vector3 point   = pCamera->WorldToScreenPoint(Vector3, Eye::Left);
> Vector3 world   = pCamera->ScreenToWorldPoint(point, Eye::Left);
> ```

#### 获取继承子类的名称 (Get the name of the inherited subclass)
> ``` c++
> const auto assembly = UnityResolve::Get("UnityEngine.CoreModule.dll");
> const auto pClass   = assembly->Get("MonoBehaviour");
> Parent* pParent     = pClass->FindObjectsByType<Parent*>()[0];
> std::string child   = pParent->GetType()->GetFullName();
> ```

#### 获取Gameobject组件 (Get GameObject component)
> ``` c++
> std::vector<T*> objs = gameobj->GetComponents<T*>(UnityResolve::Get("assembly.dll")->Get("class")));
>                     // gameobj->GetComponents<return type>(Class* component)
> std::vector<T*> objs = gameobj->GetComponentsInChildren<T*>(UnityResolve::Get("assembly.dll")->Get("class")));
>                     // gameobj->GetComponentsInChildren<return type>(Class* component)
> std::vector<T*> objs = gameobj->GetComponentsInParent<T*>(UnityResolve::Get("assembly.dll")->Get("class")));
>                     // gameobj->GetComponentsInParent<return type>(Class* component)
> ```

