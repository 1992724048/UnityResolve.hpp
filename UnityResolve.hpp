#ifndef UNITYRESOLVE_HPP
#define UNITYRESOLVE_HPP

// ============================== 自动检测当前环境 ==============================

#if defined(_WIN32) || defined(_WIN64)
#define WINDOWS_MODE 1
#else
#define WINDOWS_MODE 0
#endif

#if defined(__ANDROID__)
#define ANDROID_MODE 1
#else
#define ANDROID_MODE 0
#endif

#if defined(TARGET_OS_IOS)
#define IOS_MODE 1
#else
#define IOS_MODE 0
#endif

#if defined(__linux__) && !defined(__ANDROID__)
#define LINUX_MODE 1
#else
#define LINUX_MODE 0
#endif

#if defined(__harmony__) && !defined(_HARMONYOS)
#define HARMONYOS_MODE 1
#else
#define HARMONYOS_MODE 0
#endif

// ============================== 强制设置当前执行环境 ==============================

// #define WINDOWS_MODE 0
// #define ANDROID_MODE 1 // 设置运行环境
// #define LINUX_MODE 0
// #define IOS_MODE 0
// #define HARMONYOS_MODE 0

// ============================== 导入对应环境依赖 ==============================

#if WINDOWS_MODE || LINUX_MODE || IOS_MODE
#include <format>
#include <ranges>
#include <regex>
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <codecvt>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <numbers>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include <filesystem>
#include <sstream>
#include <locale>
#include <memory>

// from https://github.com/g-truc/glm
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/detail/type_quat.hpp>

#if WINDOWS_MODE
#define NOMINMAX
#include <windows.h>
#undef GetObject
#endif

#if WINDOWS_MODE
#ifdef _WIN64
#define UNITY_CALLING_CONVENTION __fastcall
#elif _WIN32
#define UNITY_CALLING_CONVENTION __cdecl
#endif
#elif ANDROID_MODE || LINUX_MODE || IOS_MODE || HARMONYOS_MODE
#include <dlfcn.h>
#include <locale>
#define UNITY_CALLING_CONVENTION
#endif

class UnityResolve final {
public:
    struct Assembly;
    struct Type;
    struct Class;
    struct Field;
    struct Method;
    class UnityType;

    enum class Mode : char { Il2Cpp, Mono, };

    struct Assembly final {
        void* assembly_;
        std::string name;
        std::string file;
        std::vector<std::shared_ptr<Class>> classes;

        [[nodiscard]] auto get(const std::string& _str_class, const std::string& _str_namespace = "*", const std::string& _str_parent = "*") const -> std::shared_ptr<Class> {
            for (const auto& p_class : classes) {
                if (_str_class == p_class->name && (_str_namespace == "*" || p_class->namespaze == _str_namespace) && (_str_parent == "*" || p_class->parent == _str_parent)) {
                    return p_class;
                }
            }
            return nullptr;
        }
    };

    struct Type final {
        void* type_;
        std::string name;
        int size;

        // UnityType::CsType*
        [[nodiscard]] auto get_cs_type() const -> void* {
            if (mode_ == Mode::Il2Cpp) {
                return invoke<void*>("il2cpp_type_get_object", type_);
            }
            return invoke<void*>("mono_type_get_object", pDomain, type_);
        }
    };

    struct Class final {
        std::string name;
        std::string parent;
        std::string namespaze;
        std::vector<std::shared_ptr<Field>> fields;
        std::vector<std::shared_ptr<Method>> methods;
        void* obj_type;
        void* class_;

        template<typename RType>
        auto get(const std::string& _name, const std::vector<std::string>& args = {}) -> RType* {
            if constexpr (std::is_same_v<RType, Field>) {
                for (const auto& p_field : fields) {
                    if (p_field->name == _name) {
                        return static_cast<RType*>(p_field);
                    }
                }
            }
            if constexpr (std::is_same_v<RType, std::int32_t>) {
                for (const auto& p_field : fields) {
                    if (p_field->name == _name) {
                        return reinterpret_cast<RType*>(p_field->offset);
                    }
                }
            }
            if constexpr (std::is_same_v<RType, Method>) {
                for (const auto& p_method : methods) {
                    if (p_method->name == _name) {
                        if (p_method->args.empty() && args.empty()) {
                            return static_cast<RType*>(p_method);
                        }
                        if (p_method->args.size() == args.size()) {
                            size_t index{0};
                            for (size_t i{0}; const auto& typeName : args) {
                                if (typeName == "*" || typeName.empty() ? true : p_method->args[i++]->type->name == typeName) {
                                    index++;
                                }
                            }
                            if (index == p_method->args.size()) {
                                return static_cast<RType*>(p_method);
                            }
                        }
                    }
                }

                for (const auto& p_method : methods) {
                    if (p_method->name == _name) {
                        return static_cast<RType*>(p_method);
                    }
                }
            }
            return nullptr;
        }

        template<typename RType>
        auto get_value(void* _obj, const std::string& _name) -> RType {
            return *reinterpret_cast<RType*>(reinterpret_cast<uintptr_t>(_obj) + get<Field>(_name)->offset);
        }

        template<typename RType>
        static auto get_value(void* _obj, const unsigned int _offset) -> RType {
            return *reinterpret_cast<RType*>(reinterpret_cast<uintptr_t>(_obj) + _offset);
        }

        template<typename RType>
        auto set_value(void* _obj, const std::string& _name, RType _value) -> void {
            *reinterpret_cast<RType*>(reinterpret_cast<uintptr_t>(_obj) + get<Field>(_name)->offset) = _value;
        }

        template<typename RType>
        static auto set_value(void* _obj, const unsigned int _offset, RType _value) -> void {
            *reinterpret_cast<RType*>(reinterpret_cast<uintptr_t>(_obj) + _offset) = _value;
        }

        // UnityType::CsType*
        [[nodiscard]] auto get_type() -> void* {
            if (obj_type) {
                return obj_type;
            }
            if (mode_ == Mode::Il2Cpp) {
                const auto pUType = invoke<void*, void*>("il2cpp_class_get_type", class_);
                obj_type = invoke<void*>("il2cpp_type_get_object", pUType);
                return obj_type;
            }
            const auto type = invoke<void*, void*>("mono_class_get_type", class_);
            obj_type = invoke<void*>("mono_type_get_object", pDomain, type);
            return obj_type;
        }

        /**
         * \brief 获取类所有实例
         * \tparam T 返回数组类型
         * \param type 类
         * \return 返回实例指针数组
         */
        template<typename T>
        auto find_objects_by_type() -> std::vector<T> {
            static Method* p_method;

            if (!p_method) {
                p_method = UnityResolve::get("UnityEngine.CoreModule.dll")->get("Object")->get<Method>("FindObjectsOfType", {"System.Type"});
            }
            if (!obj_type) {
                obj_type = this->get_type();
            }

            if (p_method && obj_type) {
                if (auto array = p_method->invoke<UnityType::Array<T>*>(obj_type)) {
                    return array->to_vector();
                }
            }

            return std::vector<T>(0);
        }

        template<typename T>
        auto create() -> T* {
            return mode_ == Mode::Il2Cpp ? invoke<T*, void*>("il2cpp_object_new", class_) : invoke<T*, void*, void*>("mono_object_new", pDomain, class_);
        }
    };

    struct Field final {
        void* field_;
        std::string name;
        std::shared_ptr<Type> type;
        std::shared_ptr<Class> klass;
        std::int32_t offset; // If offset is -1, then it's thread static
        bool static_field;
        void* v_table;

        template<typename T>
        auto set_static_value(T* _value) const -> void {
            if (!static_field) {
                return;
            }
            if (mode_ == Mode::Il2Cpp) {
                return invoke<void, void*, T*>("il2cpp_field_static_set_value", field_, _value);
            }
            const auto v_table = invoke<void*>("mono_class_vtable", pDomain, klass->class_);
            return invoke<void, void*, void*, T*>("mono_field_static_set_value", v_table, field_, _value);
        }

        template<typename T>
        auto get_static_value(T* _value) const -> void {
            if (!static_field) {
                return;
            }
            if (mode_ == Mode::Il2Cpp) {
                return invoke<void, void*, T*>("il2cpp_field_static_get_value", field_, _value);
            }
            const auto v_table = invoke<void*>("mono_class_vtable", pDomain, klass->class_);
            return invoke<void, void*, void*, T*>("mono_field_static_get_value", v_table, field_, _value);
        }

        template<typename T, typename C>
        struct Variable {
        private:
            std::int32_t offset{0};

        public:
            auto init(const Field* _field) -> void {
                offset = _field->offset;
            }

            auto get(C* _obj) -> T {
                return *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(_obj) + offset);
            }

            auto set(C* _obj, T _value) -> void {
                *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(_obj) + offset) = _value;
            }

            auto operator[](C* _obj) -> T& {
                return *reinterpret_cast<T*>(offset + reinterpret_cast<std::uintptr_t>(_obj));
            }
        };
    };

    template<typename Return, typename... Args>
    using MethodPointer = Return(UNITY_CALLING_CONVENTION *)(Args...);

    struct Method final {
        void* method_;
        std::string name;
        std::shared_ptr<Class> klass;
        Type* return_type;
        std::int32_t flags;
        bool static_function;
        void* function;

        struct Arg {
            std::string name;
            Type* type;
        };

        std::vector<Arg*> args;

        template<typename Return, typename... Args>
        auto invoke(Args... _args) -> Return {
            compile();
            if (function) {
                return reinterpret_cast<Return(UNITY_CALLING_CONVENTION *)(Args...)>(function)(_args...);
            }
            throw std::runtime_error(name + " is nullptr");
        }

        auto compile() -> void {
            if (method_ && !function && mode_ == Mode::Mono) {
                function = invoke<void*>("mono_compile_method", method_);
            }
        }

        template<typename Return, typename Obj = void, typename... Args>
        auto runtime_invoke(Obj* _obj, Args... _args) -> Return {
            void* arg_array[sizeof...(Args)] = {static_cast<void*>(&_args)...};

            if (mode_ == Mode::Il2Cpp) {
                if constexpr (std::is_void_v<Return>) {
                    UnityResolve::invoke<void*>("il2cpp_runtime_invoke", method_, _obj, sizeof...(Args) ? arg_array : nullptr, nullptr);
                    return;
                } else {
                    return Unbox<Return>(invoke<void*>("il2cpp_runtime_invoke", method_, _obj, sizeof...(Args) ? arg_array : nullptr, nullptr));
                }
            }

            if constexpr (std::is_void_v<Return>) {
                UnityResolve::invoke<void*>("mono_runtime_invoke", method_, _obj, sizeof...(Args) ? arg_array : nullptr, nullptr);
                return;
            } else {
                return Unbox<Return>(invoke<void*>("mono_runtime_invoke", method_, _obj, sizeof...(Args) ? arg_array : nullptr, nullptr));
            }
        }

        template<typename Return, typename... Args>
        auto cast() -> MethodPointer<Return, Args...> {
            compile();
            if (function) {
                return reinterpret_cast<MethodPointer<Return, Args...>>(function);
            }
            return nullptr;
        }

        template<typename Return, typename... Args>
        auto cast(MethodPointer<Return, Args...>& _ptr) -> MethodPointer<Return, Args...> {
            compile();
            if (function) {
                _ptr = reinterpret_cast<MethodPointer<Return, Args...>>(function);
                return reinterpret_cast<MethodPointer<Return, Args...>>(function);
            }
            return nullptr;
        }

        template<typename Return, typename... Args>
        auto cast(std::function<Return(Args...)>& _ptr) -> std::function<Return(Args...)> {
            compile();
            if (function) {
                _ptr = reinterpret_cast<MethodPointer<Return, Args...>>(function);
                return reinterpret_cast<MethodPointer<Return, Args...>>(function);
            }
            return nullptr;
        }

        template<typename T>
        auto unbox(void* _obj) -> T {
            return mode_ == Mode::Il2Cpp ? static_cast<T>(invoke<void*>("mono_object_unbox", _obj)) : static_cast<T>(invoke<void*>("il2cpp_object_unbox", _obj));
        }
    };

    class AssemblyLoad {
    public:
        explicit AssemblyLoad(const std::string& _path, std::string _namespaze = "", std::string _class_name = "", std::string _desc = "") {
            if (mode_ == Mode::Mono) {
                assembly = invoke<void*>("mono_domain_assembly_open", pDomain, _path.data());
                image = invoke<void*>("mono_assembly_get_image", assembly);
                if (_namespaze.empty() || _class_name.empty() || _desc.empty()) {
                    return;
                }
                klass = invoke<void*>("mono_class_from_name", image, _namespaze.data(), _class_name.data());
                const auto entry_point_method_desc = invoke<void*>("mono_method_desc_new", _desc.data(), true);
                method = invoke<void*>("mono_method_desc_search_in_class", entry_point_method_desc, klass);
                invoke<void>("mono_method_desc_free", entry_point_method_desc);
                invoke<void*>("mono_runtime_invoke", method, nullptr, nullptr, nullptr);
            }
        }

        void* assembly;
        void* image;
        void* klass;
        void* method;
    };

    static auto thread_attach() -> void {
        if (mode_ == Mode::Il2Cpp) {
            invoke<void*>("il2cpp_thread_attach", pDomain);
        } else {
            invoke<void*>("mono_thread_attach", pDomain);
            invoke<void*>("mono_jit_thread_attach", pDomain);
        }
    }

    static auto thread_detach() -> void {
        if (mode_ == Mode::Il2Cpp) {
            invoke<void*>("il2cpp_thread_detach", pDomain);
        } else {
            invoke<void*>("mono_thread_detach", pDomain);
            invoke<void*>("mono_jit_thread_detach", pDomain);
        }
    }

    static auto init(void* _hmodule, const Mode _mode = Mode::Mono) -> void {
        mode_ = _mode;
        hmodule_ = _hmodule;

        if (mode_ == Mode::Il2Cpp) {
            pDomain = invoke<void*>("il2cpp_domain_get");
            while (!invoke<bool>("il2cpp_is_vm_thread", nullptr)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            invoke<void*>("il2cpp_thread_attach", pDomain);
            foreach_assembly();
        } else {
            pDomain = invoke<void*>("mono_get_root_domain");
            invoke<void*>("mono_thread_attach", pDomain);
            invoke<void*>("mono_jit_thread_attach", pDomain);

            foreach_assembly();
        }
    }

#if WINDOWS_MODE || LINUX_MODE || IOS_MODE /*__cplusplus >= 202002L*/
    static auto dump_to_file(const std::filesystem::path& _path) -> void {
        std::ofstream io(_path / "dump.cs", std::fstream::out);
        if (!io) {
            return;
        }

        for (const auto& pAssembly : assembly) {
            for (const auto& pClass : pAssembly->classes) {
                io << std::format("\tnamespace: {}", pClass->namespaze.empty() ? "" : pClass->namespaze);
                io << "\n";
                io << std::format("\tAssembly: {}\n", pAssembly->name.empty() ? "" : pAssembly->name);
                io << std::format("\tAssemblyFile: {} \n", pAssembly->file.empty() ? "" : pAssembly->file);
                io << std::format("\tclass {}{} ", pClass->name, pClass->parent.empty() ? "" : " : " + pClass->parent);
                io << "{\n\n";
                for (const auto& pField : pClass->fields) {
                    io << std::format("\t\t{:+#06X} | {}{} {};\n", pField->offset, pField->static_field ? "static " : "", pField->type->name, pField->name);
                }
                io << "\n";
                for (const auto& pMethod : pClass->methods) {
                    io << std::format("\t\t[Flags: {:032b}] [ParamsCount: {:04d}] |RVA: {:+#010X}|\n",
                                      pMethod->flags,
                                      pMethod->args.size(),
                                      reinterpret_cast<std::uint64_t>(pMethod->function) - reinterpret_cast<std::uint64_t>(hmodule_));
                    io << std::format("\t\t{}{} {}(", pMethod->static_function ? "static " : "", pMethod->return_type->name, pMethod->name);
                    std::string params{};
                    for (const auto& pArg : pMethod->args) {
                        params += std::format("{} {}, ", pArg->type->name, pArg->name);
                    }
                    if (!params.empty()) {
                        params.pop_back();
                        params.pop_back();
                    }
                    io << (params.empty() ? "" : params) << ");\n\n";
                }
                io << "\t}\n\n";
            }
        }

        io << '\n';
        io.close();
    }
#endif

    template<typename Return, typename... Args>
    static auto invoke(const std::string& funcName, Args... args) -> Return {
#if WINDOWS_MODE
        if (!address_.contains(funcName) || !address_[funcName]) {
            address_[funcName] = static_cast<void*>(GetProcAddress(static_cast<HMODULE>(hmodule_), funcName.c_str()));
        }
#elif ANDROID_MODE || LINUX_MODE || IOS_MODE || HARMONYOS_MODE
        if (address_.find(funcName) == address_.end() || !address_[funcName]) {
            address_[funcName] = dlsym(hmodule_, funcName.c_str());
        }
#endif

        if (address_[funcName] != nullptr) {
            try {
                return reinterpret_cast<Return(UNITY_CALLING_CONVENTION *)(Args...)>(address_[funcName])(args...);
            } catch (...) {
                return Return();
            }
        }
        return Return();
    }

    inline static std::vector<std::shared_ptr<Assembly>> assembly;

    static auto get(const std::string& _str_assembly) -> std::shared_ptr<Assembly> {
        for (const auto& pAssembly : assembly) {
            if (pAssembly->name == _str_assembly) {
                return pAssembly;
            }
        }
        return nullptr;
    }

private:
    static auto foreach_assembly() -> void {
        // 遍历程序集
        if (mode_ == Mode::Il2Cpp) {
            size_t nrofassemblies = 0;
            const auto assemblies = invoke<void**>("il2cpp_domain_get_assemblies", pDomain, &nrofassemblies);
            for (auto i = 0; std::cmp_less(i, nrofassemblies); i++) {
                const auto ptr = assemblies[i];
                if (ptr == nullptr) {
                    continue;
                }
                auto assembly = std::make_shared<Assembly>();
                assembly->assembly_ = ptr;
                const auto image = invoke<void*>("il2cpp_assembly_get_image", ptr);
                assembly->file = invoke<const char*>("il2cpp_image_get_filename", image);
                assembly->name = invoke<const char*>("il2cpp_image_get_name", image);
                UnityResolve::assembly.push_back(assembly);
                foreach_class(assembly, image);
            }
        } else {
            invoke<void*, void (*)(void* ptr, std::vector<std::shared_ptr<Assembly>>&), std::vector<std::shared_ptr<Assembly>>&>("mono_assembly_foreach",
                                                                                                 [](void* ptr, std::vector<std::shared_ptr<Assembly>>& _v) {
                                                                                                     if (ptr == nullptr) {
                                                                                                         return;
                                                                                                     }

                                                                                                     auto assembly = std::make_shared<Assembly>();
                                                                                                     assembly->assembly_ = ptr;
                                                                                                     void* image;
                                                                                                     try {
                                                                                                         image = invoke<void*>("mono_assembly_get_image", ptr);
                                                                                                         assembly->file = invoke<const char*>("mono_image_get_filename", image);
                                                                                                         assembly->name = invoke<const char*>("mono_image_get_name", image);
                                                                                                         assembly->name += ".dll";
                                                                                                         _v.push_back(assembly);
                                                                                                     } catch (...) {
                                                                                                         return;
                                                                                                     }

                                                                                                     foreach_class(assembly, image);
                                                                                                 },
                                                                                                 assembly);
        }
    }

    static auto foreach_class(const std::shared_ptr<Assembly>& assembly, void* image) -> void {
        // 遍历类
        if (mode_ == Mode::Il2Cpp) {
            const auto count = invoke<int>("il2cpp_image_get_class_count", image);
            for (auto i = 0; i < count; i++) {
                const auto pClass = invoke<void*>("il2cpp_image_get_class", image, i);
                if (pClass == nullptr) {
                    continue;
                }
                const auto pAClass = std::make_shared<Class>();
                pAClass->class_ = pClass;
                pAClass->name = invoke<const char*>("il2cpp_class_get_name", pClass);
                if (const auto pPClass = invoke<void*>("il2cpp_class_get_parent", pClass)) {
                    pAClass->parent = invoke<const char*>("il2cpp_class_get_name", pPClass);
                }
                pAClass->namespaze = invoke<const char*>("il2cpp_class_get_namespace", pClass);
                assembly->classes.push_back(pAClass);

                foreach_fields(pAClass, pClass);
                foreach_method(pAClass, pClass);

                void* i_class;
                void* iter{};
                do {
                    if ((i_class = invoke<void*>("il2cpp_class_get_interfaces", pClass, &iter))) {
                        foreach_fields(pAClass, i_class);
                        foreach_method(pAClass, i_class);
                    }
                } while (i_class);
            }
        } else {
            try {
                const void* table = invoke<void*>("mono_image_get_table_info", image, 2);
                const auto count = invoke<int>("mono_table_info_get_rows", table);
                for (auto i = 0; i < count; i++) {
                    const auto pClass = invoke<void*>("mono_class_get", image, 0x02000000 | (i + 1));
                    if (pClass == nullptr) {
                        continue;
                    }

                    const auto pAClass = std::make_shared<Class>();
                    pAClass->class_ = pClass;
                    try {
                        pAClass->name = invoke<const char*>("mono_class_get_name", pClass);
                        if (const auto pPClass = invoke<void*>("mono_class_get_parent", pClass)) {
                            pAClass->parent = invoke<const char*>("mono_class_get_name", pPClass);
                        }
                        pAClass->namespaze = invoke<const char*>("mono_class_get_namespace", pClass);
                        assembly->classes.push_back(pAClass);
                    } catch (...) {
                        return;
                    }

                    foreach_fields(pAClass, pClass);
                    foreach_method(pAClass, pClass);

                    void* iClass;
                    void* iiter{};

                    do {
                        try {
                            if ((iClass = invoke<void*>("mono_class_get_interfaces", pClass, &iiter))) {
                                foreach_fields(pAClass, iClass);
                                foreach_method(pAClass, iClass);
                            }
                        } catch (...) {
                            return;
                        }
                    } while (iClass);
                }
            } catch (...) {}
        }
    }

    static auto foreach_fields(const std::shared_ptr<Class>& klass, void* pKlass) -> void {
        // 遍历成员
        if (mode_ == Mode::Il2Cpp) {
            void* iter = nullptr;
            void* field;
            do {
                if ((field = invoke<void*>("il2cpp_class_get_fields", pKlass, &iter))) {
                    const auto pField = std::make_shared<Field>();
                    pField->field_ = field;
                    pField->name = invoke<const char*>("il2cpp_field_get_name", field);
                    const auto type = std::make_shared<Type>();
                    type->type_ = invoke<void*>("il2cpp_field_get_type", field);
                    pField->type = type;
                    pField->klass = klass;
                    pField->offset = invoke<int>("il2cpp_field_get_offset", field);
                    pField->static_field = false;
                    pField->v_table = nullptr;
                    pField->static_field = pField->offset <= 0;
                    auto name = invoke<char*>("il2cpp_type_get_name", pField->type->type_);
                    pField->type->name = name;
                    invoke<void>("il2cpp_free", name);
                    pField->type->size = -1;
                    klass->fields.push_back(pField);
                }
            } while (field);
        } else {
            void* iter = nullptr;
            void* field;
            do {
                try {
                    if ((field = invoke<void*>("mono_class_get_fields", pKlass, &iter))) {
                        const auto pField = std::make_shared<Field>();
                        pField->field_ = field;
                        pField->name = invoke<const char*>("mono_field_get_name", field);
                        const auto type = std::make_shared<Type>();
                        type->type_ = invoke<void*>("mono_field_get_type", field);
                        pField->type = type;
                        pField->klass = klass;
                        pField->offset = invoke<int>("mono_field_get_offset", field);
                        pField->static_field = false;
                        pField->v_table = nullptr;
                        int tSize{};
                        /*pField->static_field = pField->offset <= 0;*/
                        int flags = invoke<int>("mono_field_get_flags", field);
                        if (flags & 0x10) // 0x10=FIELD_ATTRIBUTE_STATIC
                        {
                            pField->static_field = true;
                        }
                        pField->type->name = invoke<const char*>("mono_type_get_name", pField->type->type_);
                        pField->type->size = invoke<int>("mono_type_size", pField->type->type_, &tSize);
                        klass->fields.push_back(pField);
                    }
                } catch (...) {
                    return;
                }
            } while (field);
        }
    }

    static auto foreach_method(const std::shared_ptr<Class>& _klass, void* pKlass) -> void {
        // 遍历方法
        if (mode_ == Mode::Il2Cpp) {
            void* iter = nullptr;
            void* method;
            do {
                if ((method = invoke<void*>("il2cpp_class_get_methods", pKlass, &iter))) {
                    int fFlags{};
                    const auto pMethod = std::make_shared<Method>();
                    pMethod->method_ = method;
                    pMethod->name = invoke<const char*>("il2cpp_method_get_name", method);
                    pMethod->klass = _klass;
                    pMethod->return_type = new Type{.type_ = invoke<void*>("il2cpp_method_get_return_type", method),};
                    pMethod->flags = invoke<int>("il2cpp_method_get_flags", method, &fFlags);

                    pMethod->static_function = pMethod->flags & 0x10;
                    auto name = invoke<char*>("il2cpp_type_get_name", pMethod->return_type->type_);
                    pMethod->return_type->name = name;
                    invoke<void>("il2cpp_free", name);
                    pMethod->return_type->size = -1;
                    pMethod->function = *static_cast<void**>(method);
                    _klass->methods.push_back(pMethod);
                    const auto argCount = invoke<int>("il2cpp_method_get_param_count", method);
                    for (auto index = 0; index < argCount; index++) {
                        auto arg = new Method::Arg();
                        arg->name = invoke<const char*>("il2cpp_method_get_param_name", method, index);
                        {
                            auto pType = new Type();
                            pType->type_ = invoke<void*>("il2cpp_method_get_param", method, index);
                            auto type_name = invoke<char*>("il2cpp_type_get_name", pType->type_);
                            pType->name = type_name;
                            invoke<void>("il2cpp_free", type_name);
                            pType->size = -1;
                            arg->type = pType;
                        }
                        pMethod->args.emplace_back(arg);
                    }
                }
            } while (method);
        } else {
            void* iter = nullptr;
            void* method;

            do {
                try {
                    if ((method = invoke<void*>("mono_class_get_methods", pKlass, &iter))) {
                        const auto signature = invoke<void*>("mono_method_signature", method);
                        if (!signature) {
                            continue;
                        }

                        int fFlags{};
                        const auto pMethod = std::make_shared<Method>();
                        pMethod->method_ = method;

                        char** names = nullptr;
                        try {
                            pMethod->name = invoke<const char*>("mono_method_get_name", method);
                            pMethod->klass = _klass;
                            pMethod->return_type = new Type{.type_ = invoke<void*>("mono_signature_get_return_type", signature)};

                            pMethod->flags = invoke<int>("mono_method_get_flags", method, &fFlags);
                            pMethod->static_function = pMethod->flags & 0x10;

                            pMethod->return_type->name = invoke<const char*>("mono_type_get_name", pMethod->return_type->type_);
                            int tSize{};
                            pMethod->return_type->size = invoke<int>("mono_type_size", pMethod->return_type->type_, &tSize);

                            _klass->methods.push_back(pMethod);

                            int param_count = invoke<int>("mono_signature_get_param_count", signature);
                            names = new char*[param_count];
                            invoke<void>("mono_method_get_param_names", method, names);
                        } catch (...) {
                            continue;
                        }

                        void* mIter = nullptr;
                        void* mType;
                        int iname = 0;

                        do {
                            try {
                                if ((mType = invoke<void*>("mono_signature_get_params", signature, &mIter))) {
                                    int t_size{};
                                    try {
                                        pMethod->args.push_back(new Method::Arg{
                                            names[iname],
                                            new Type{.type_ = mType, .name = invoke<const char*>("mono_type_get_name", mType), .size = invoke<int>("mono_type_size", mType, &t_size)}
                                        });
                                    } catch (...) {}
                                    iname++;
                                }
                            } catch (...) {
                                break;
                            }
                        } while (mType);
                    }
                } catch (...) {
                    return;
                }
            } while (method);
        }
    }

public:
    class UnityType final {
    public:
        using IntPtr = std::uintptr_t;
        using Int32 = std::int32_t;
        using Int64 = std::int64_t;
        using Char = wchar_t;
        using Int16 = std::int16_t;
        using Byte = std::uint8_t;
        using Vector3 = glm::vec3;
        using Vector2 = glm::vec2;
        using Vector4 = glm::vec4;
        using Quaternion = glm::quat;
        using Matrix4x4 = glm::mat4x4;
        struct Camera;
        struct Transform;
        struct Component;
        struct UnityObject;
        struct LayerMask;
        struct Rigidbody;
        struct Physics;
        struct GameObject;
        struct Collider;
        struct Bounds;
        struct Plane;
        struct Ray;
        struct Rect;
        struct Color;
        template<typename T>
        struct Array;
        struct String;
        struct Object;
        template<typename T>
        struct List;
        template<typename TKey, typename TValue>
        struct Dictionary;
        struct Behaviour;
        struct MonoBehaviour;
        struct CSType;
        struct Mesh;
        struct Renderer;
        struct Animator;
        struct CapsuleCollider;
        struct BoxCollider;
        struct FieldInfo;
        struct MethodInfo;
        struct PropertyInfo;
        struct Assembly;
        struct EventInfo;
        struct MemberInfo;
        struct Time;
        struct RaycastHit;

        struct Bounds {
            Vector3 m_vCenter;
            Vector3 m_vExtents;
        };

        struct Plane {
            Vector3 m_vNormal;
            float fDistance;
        };

        struct Ray {
            Vector3 m_vOrigin;
            Vector3 m_vDirection;
        };

        struct RaycastHit {
            Vector3 m_Point;
            Vector3 m_Normal;
        };

        struct Rect {
            float fX, fY;
            float fWidth, fHeight;

            Rect() {
                fX = fY = fWidth = fHeight = 0.f;
            }

            Rect(const float f1, const float f2, const float f3, const float f4) {
                fX = f1;
                fY = f2;
                fWidth = f3;
                fHeight = f4;
            }
        };

        struct Color {
            float r, g, b, a;

            Color() {
                r = g = b = a = 0.f;
            }

            explicit Color(const float fRed = 0.f, const float fGreen = 0.f, const float fBlue = 0.f, const float fAlpha = 1.f) {
                r = fRed;
                g = fGreen;
                b = fBlue;
                a = fAlpha;
            }
        };

        struct Object {
            union {
                void* klass{nullptr};
                void* vtable;
            } il2cpp_class;

            struct MonitorData* monitor{nullptr};

            auto get_type() -> CSType* {
                static Method* method = get("mscorlib.dll")->get("Object", "System")->get<Method>("GetType");
                return method->invoke<CSType*>(this);
            }

            auto to_string() -> String* {
                static Method* method = get("mscorlib.dll")->get("Object", "System")->get<Method>("ToString");
                return method->invoke<String*>(this);
            }

            auto get_hash_code() -> int {
                static Method* method = get("mscorlib.dll")->get("Object", "System")->get<Method>("GetHashCode");
                return method->invoke<int>(this);
            }
        };

        enum class BindingFlags : uint32_t {
            Default              = 0,
            IgnoreCase           = 1,
            DeclaredOnly         = 2,
            Instance             = 4,
            Static               = 8,
            Public               = 16,
            NonPublic            = 32,
            FlattenHierarchy     = 64,
            invokeMethod         = 256,
            CreateInstance       = 512,
            GetField             = 1024,
            SetField             = 2048,
            GetProperty          = 4096,
            SetProperty          = 8192,
            PutDispProperty      = 16384,
            PutRefDispProperty   = 32768,
            ExactBinding         = 65536,
            SuppressChangeType   = 131072,
            OptionalParamBinding = 262144,
            IgnoreReturn         = 16777216,
        };

        enum class FieldAttributes : uint32_t {
            FieldAccessMask = 7,
            PrivateScope    = 0,
            Private         = 1,
            FamANDAssem     = 2,
            Assembly        = 3,
            Family          = 4,
            FamORAssem      = 5,
            Public          = 6,
            Static          = 16,
            InitOnly        = 32,
            Literal         = 64,
            NotSerialized   = 128,
            HasFieldRVA     = 256,
            SpecialName     = 512,
            RTSpecialName   = 1024,
            HasFieldMarshal = 4096,
            PinvokeImpl     = 8192,
            HasDefault      = 32768,
            ReservedMask    = 38144
        };

        enum class MemberTypes : uint32_t {
            Constructor = 1,
            Event       = 2,
            Field       = 4,
            Method      = 8,
            Property    = 16,
            TypeInfo    = 32,
            Custom      = 64,
            NestedType  = 128,
            All         = 191
        };

        struct MemberInfo {};

        struct FieldInfo : MemberInfo {
            auto get_is_init_only() -> bool {
                static Method* method = get("mscorlib.dll")->get("FieldInfo", "System.Reflection", "MemberInfo")->get<Method>("get_IsInitOnly");
                return method->invoke<bool>(this);
            }

            auto get_is_literal() -> bool {
                static Method* method = get("mscorlib.dll")->get("FieldInfo", "System.Reflection", "MemberInfo")->get<Method>("get_IsLiteral");
                return method->invoke<bool>(this);
            }

            auto get_is_not_serialized() -> bool {
                static Method* method = get("mscorlib.dll")->get("FieldInfo", "System.Reflection", "MemberInfo")->get<Method>("get_IsNotSerialized");
                return method->invoke<bool>(this);
            }

            auto get_is_static() -> bool {
                static Method* method = get("mscorlib.dll")->get("FieldInfo", "System.Reflection", "MemberInfo")->get<Method>("get_IsStatic");
                return method->invoke<bool>(this);
            }

            auto get_is_family() -> bool {
                static Method* method = get("mscorlib.dll")->get("FieldInfo", "System.Reflection", "MemberInfo")->get<Method>("get_IsFamily");
                return method->invoke<bool>(this);
            }

            auto get_is_private() -> bool {
                static Method* method = get("mscorlib.dll")->get("FieldInfo", "System.Reflection", "MemberInfo")->get<Method>("get_IsPrivate");
                return method->invoke<bool>(this);
            }

            auto get_is_public() -> bool {
                static Method* method = get("mscorlib.dll")->get("FieldInfo", "System.Reflection", "MemberInfo")->get<Method>("get_IsPublic");
                return method->invoke<bool>(this);
            }

            auto get_attributes() -> FieldAttributes {
                static Method* method = get("mscorlib.dll")->get("FieldInfo", "System.Reflection", "MemberInfo")->get<Method>("get_Attributes");
                return method->invoke<FieldAttributes>(this);
            }

            auto get_member_type() -> MemberTypes {
                static Method* method = get("mscorlib.dll")->get("FieldInfo", "System.Reflection", "MemberInfo")->get<Method>("get_MemberType");
                return method->invoke<MemberTypes>(this);
            }

            auto get_field_offset() -> int {
                static Method* method = get("mscorlib.dll")->get("FieldInfo", "System.Reflection", "MemberInfo")->get<Method>("GetFieldOffset");
                return method->invoke<int>(this);
            }

            template<typename T>
            auto get_value(Object* _object) -> T {
                static Method* method = get("mscorlib.dll")->get("FieldInfo", "System.Reflection", "MemberInfo")->get<Method>("GetValue");
                return method->invoke<T>(this, _object);
            }

            template<typename T>
            auto set_value(Object* _object, T _value) -> void {
                static Method* method = get("mscorlib.dll")->get("FieldInfo", "System.Reflection", "MemberInfo")->get<Method>("SetValue", {"System.Object", "System.Object"});
                return method->invoke<T>(this, _object, _value);
            }
        };

        struct CSType {
            auto format_type_name() -> String* {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("FormatTypeName");
                return method->invoke<String*>(this);
            }

            auto get_full_name() -> String* {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_FullName");
                return method->invoke<String*>(this);
            }

            auto get_namespace() -> String* {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_Namespace");
                return method->invoke<String*>(this);
            }

            auto get_is_serializable() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsSerializable");
                return method->invoke<bool>(this);
            }

            auto get_contains_generic_parameters() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_ContainsGenericParameters");
                return method->invoke<bool>(this);
            }

            auto get_is_visible() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsVisible");
                return method->invoke<bool>(this);
            }

            auto get_is_nested() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsNested");
                return method->invoke<bool>(this);
            }

            auto get_is_array() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsArray");
                return method->invoke<bool>(this);
            }

            auto get_is_by_ref() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsByRef");
                return method->invoke<bool>(this);
            }

            auto GetIsPointer() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsPointer");
                return method->invoke<bool>(this);
            }

            auto GetIsConstructedGenericType() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsConstructedGenericType");
                return method->invoke<bool>(this);
            }

            auto GetIsGenericParameter() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsGenericParameter");
                return method->invoke<bool>(this);
            }

            auto GetIsGenericMethodParameter() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsGenericMethodParameter");
                return method->invoke<bool>(this);
            }

            auto GetIsGenericType() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsGenericType");
                return method->invoke<bool>(this);
            }

            auto GetIsGenericTypeDefinition() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsGenericTypeDefinition");
                return method->invoke<bool>(this);
            }

            auto GetIsSZArray() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsSZArray");
                return method->invoke<bool>(this);
            }

            auto GetIsVariableBoundArray() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsVariableBoundArray");
                return method->invoke<bool>(this);
            }

            auto GetHasElementType() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_HasElementType");
                return method->invoke<bool>(this);
            }

            auto GetIsAbstract() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsAbstract");
                return method->invoke<bool>(this);
            }

            auto GetIsSealed() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsSealed");
                return method->invoke<bool>(this);
            }

            auto GetIsClass() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsClass");
                return method->invoke<bool>(this);
            }

            auto GetIsNestedAssembly() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsNestedAssembly");
                return method->invoke<bool>(this);
            }

            auto GetIsNestedPublic() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsNestedPublic");
                return method->invoke<bool>(this);
            }

            auto GetIsNotPublic() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsNotPublic");
                return method->invoke<bool>(this);
            }

            auto GetIsPublic() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsPublic");
                return method->invoke<bool>(this);
            }

            auto GetIsExplicitLayout() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsExplicitLayout");
                return method->invoke<bool>(this);
            }

            auto GetIsCOMObject() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsCOMObject");
                return method->invoke<bool>(this);
            }

            auto GetIsContextful() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsContextful");
                return method->invoke<bool>(this);
            }

            auto GetIsCollectible() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsCollectible");
                return method->invoke<bool>(this);
            }

            auto GetIsEnum() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsEnum");
                return method->invoke<bool>(this);
            }

            auto GetIsMarshalByRef() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsMarshalByRef");
                return method->invoke<bool>(this);
            }

            auto GetIsPrimitive() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsPrimitive");
                return method->invoke<bool>(this);
            }

            auto GetIsValueType() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsValueType");
                return method->invoke<bool>(this);
            }

            auto GetIsSignatureType() -> bool {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("get_IsSignatureType");
                return method->invoke<bool>(this);
            }

            auto GetField(const std::string& name,
                          const BindingFlags flags = static_cast<BindingFlags>(static_cast<int>(BindingFlags::Instance) | static_cast<int>(BindingFlags::Static) | static_cast<int>(
                              BindingFlags::Public))) -> FieldInfo* {
                static Method* method = get("mscorlib.dll")->get("Type", "System", "MemberInfo")->get<Method>("GetField", {"System.String name", "System.Reflection.BindingFlags"});
                return method->invoke<FieldInfo*>(this, String::create(name), flags);
            }
        };

        struct String : Object {
            int32_t length{0};
            wchar_t str_data[32]{};

            [[nodiscard]] auto to_string() const -> std::string {
#if WINDOWS_MODE // 性能非常高 30% 提升
                if (length <= 0) {
                    return {};
                }

                const int len = WideCharToMultiByte(CP_UTF8, 0, str_data, length, nullptr, 0, nullptr, nullptr);
                if (len == 0) {
                    throw std::runtime_error("Failed to convert wide string to UTF-8 multibyte");
                }

                std::string str(len, 0);
                if (WideCharToMultiByte(CP_UTF8, 0, str_data, length, str.data(), len, nullptr, nullptr) == 0) {
                    throw std::runtime_error("Failed to convert wide string to UTF-8 multibyte");
                }

                return str;
#else
                std::wstring_convert<std::codecvt_utf8<wchar_t>> converterX;
                return converterX.to_bytes(m_firstChar);
#endif
            }

            auto operator[](const int _i) const -> wchar_t {
                return str_data[_i];
            }

            auto operator=(const std::string& _new_string) const -> String* {
                return create(_new_string);
            }

            auto operator==(const std::wstring& _new_string) const -> bool {
                return equals(_new_string);
            }

            auto clear() -> void {
                memset(str_data, '\0', length);
                length = 0;
            }

            [[nodiscard]] auto equals(const std::wstring& _new_string) const -> bool {
                if (_new_string.size() != length) {
                    return false;
                }
                if (std::memcmp(_new_string.data(), str_data, length) != 0) {
                    return false;
                }
                return true;
            }

            static auto create(const std::string& _str) -> String* {
                return mode_ == Mode::Il2Cpp
                           ? invoke<String*, const char*>("il2cpp_string_new", _str.c_str())
                           : invoke<String*, void*, const char*>("mono_string_new", invoke<void*>("mono_get_root_domain"), _str.c_str());
            }
        };

        template<typename T>
        struct Array : Object {
            struct {
                std::uintptr_t length;
                std::int32_t lower_bound;
            }* bounds{nullptr};

            std::uintptr_t max_length{0};
            T** vector{};

            auto get_data() -> uintptr_t {
                return reinterpret_cast<uintptr_t>(&vector);
            }

            auto operator[](const unsigned int m_uIndex) -> T& {
                return *reinterpret_cast<T*>(get_data() + sizeof(T) * m_uIndex);
            }

            auto at(const unsigned int m_uIndex) -> T& {
                return operator[](m_uIndex);
            }

            auto insert(T* m_pArray, uintptr_t m_uSize, const uintptr_t m_uIndex = 0) -> void {
                if ((m_uSize + m_uIndex) >= max_length) {
                    if (m_uIndex >= max_length) {
                        return;
                    }

                    m_uSize = max_length - m_uIndex;
                }

                for (uintptr_t u = 0; m_uSize > u; ++u) {
                    operator[](u + m_uIndex) = m_pArray[u];
                }
            }

            auto fill(T m_tValue) -> void {
                for (uintptr_t u = 0; max_length > u; ++u) {
                    operator[](u) = m_tValue;
                }
            }

            auto remove_at(const unsigned int m_uIndex) -> void {
                if (m_uIndex >= max_length) {
                    return;
                }

                if (max_length > (m_uIndex + 1)) {
                    for (auto u = m_uIndex; (max_length - m_uIndex) > u; ++u) {
                        operator[](u) = operator[](u + 1);
                    }
                }

                --max_length;
            }

            auto remove_range(const unsigned int m_uIndex, unsigned int m_uCount) -> void {
                if (m_uCount == 0) {
                    m_uCount = 1;
                }

                const auto m_uTotal = m_uIndex + m_uCount;
                if (m_uTotal >= max_length) {
                    return;
                }

                if (max_length > (m_uTotal + 1)) {
                    for (auto u = m_uIndex; (max_length - m_uTotal) >= u; ++u) {
                        operator[](u) = operator[](u + m_uCount);
                    }
                }

                max_length -= m_uCount;
            }

            auto remove_all() -> void {
                if (max_length > 0) {
                    memset(get_data(), 0, sizeof(Type) * max_length);
                    max_length = 0;
                }
            }

            auto to_vector() -> std::vector<T> {
                std::vector<T> rs(this->max_length); // 换成 mimalloc 分配内存性能更好
                std::memcpy(rs.data(), get_data(), rs.size()); // 想性能更好可用试试 fast_memcpy (avx)
                return rs;
            }

            auto resize(int _new_size) -> void {
                static Method* method = get("mscorlib.dll")->get("Array")->get<Method>("Resize");
                return method->invoke<void>(this, _new_size);
            }

            static auto create(const Class* _kalss, const std::uintptr_t _size) -> Array* {
                return mode_ == Mode::Il2Cpp
                           ? UnityResolve::invoke<Array*, void*, std::uintptr_t>("il2cpp_array_new", _kalss->class_, _size)
                           : UnityResolve::invoke<Array*, void*, void*, std::uintptr_t>("mono_array_new", pDomain, _kalss->class_, _size);
            }
        };

        template<typename Type>
        struct List : Object {
            Array<Type>* data;
            int size{};
            int version{};
            void* sync_root{};

            auto to_array() -> Array<Type>* {
                return data;
            }

            static auto create(const Class* kalss, const std::uintptr_t _size) -> List* {
                auto p_list = new List();
                p_list->pList = Array<Type>::New(kalss, _size);
                p_list->size = _size;
                return p_list;
            }

            auto operator[](const unsigned int _m_u_index) -> Type& {
                return data->At(_m_u_index);
            }

            auto add(Type _data) -> void {
                static Method* method = get("mscorlib.dll")->get("List`1")->get<Method>("Add");
                return method->invoke<void>(this, _data);
            }

            auto remove(Type _data) -> bool {
                static Method* method = get("mscorlib.dll")->get("List`1")->get<Method>("Remove");
                return method->invoke<bool>(this, _data);
            }

            auto remove_at(int _index) -> void {
                static Method* method = get("mscorlib.dll")->get("List`1")->get<Method>("RemoveAt");
                return method->invoke<void>(this, _index);
            }

            auto for_each(void (*_action)(Type _data)) -> void {
                static Method* method = get("mscorlib.dll")->get("List`1")->get<Method>("ForEach");
                return method->invoke<void>(this, _action);
            }

            auto get_range(int index, int count) -> List* {
                static Method* method = get("mscorlib.dll")->get("List`1")->get<Method>("GetRange");
                return method->invoke<List*>(this, index, count);
            }

            auto clear() -> void {
                static Method* method = get("mscorlib.dll")->get("List`1")->get<Method>("Clear");
                return method->invoke<void>(this);
            }

            auto sort(int (*_comparison)(Type* _x, Type* _y)) -> void {
                static Method* method = get("mscorlib.dll")->get("List`1")->get<Method>("Sort", {"*"});
                return method->invoke<void>(this, _comparison);
            }
        };

        template<typename TKey, typename TValue>
        struct Dictionary : Object {
            struct Entry {
                int iHashCode;
                int iNext;
                TKey tKey;
                TValue tValue;
            };

            Array<int>* pBuckets;
            Array<Entry*>* pEntries;
            int iCount;
            int iVersion;
            int iFreeList;
            int iFreeCount;
            void* pComparer;
            void* pKeys;
            void* pValues;

            auto get_entry() -> Entry* {
                return reinterpret_cast<Entry*>(pEntries->GetData());
            }

            auto get_key_by_index(const int iIndex) -> TKey {
                TKey tKey = {0};

                Entry* pEntry = get_entry();
                if (pEntry) {
                    tKey = pEntry[iIndex].tKey;
                }

                return tKey;
            }

            auto get_value_by_index(const int iIndex) -> TValue {
                TValue tValue = {0};

                Entry* pEntry = get_entry();
                if (pEntry) {
                    tValue = pEntry[iIndex].tValue;
                }

                return tValue;
            }

            auto get_value_by_key(const TKey tKey) -> TValue {
                TValue tValue = {0};
                for (auto i = 0; i < iCount; i++) {
                    if (get_entry()[i].tKey == tKey) {
                        tValue = get_entry()[i].tValue;
                    }
                }
                return tValue;
            }

            auto operator[](const TKey tKey) const -> TValue {
                return GetValueByKey(tKey);
            }
        };

        struct UnityObject : Object {
            void* m_CachedPtr;

            auto get_name() -> String* {
                static Method* method = get("UnityEngine.CoreModule.dll")->get("Object")->get<Method>("get_name");
                return method->invoke<String*>(this);
            }

            auto to_string() -> String* {
                static Method* method = get("UnityEngine.CoreModule.dll")->get("Object")->get<Method>("ToString");
                return method->invoke<String*>(this);
            }

            static auto to_string(UnityObject* obj) -> String* {
                if (!obj) {
                    return {};
                }
                static Method* method = get("UnityEngine.CoreModule.dll")->get("Object")->get<Method>("ToString", {"*"});
                return method->invoke<String*>(obj);
            }

            static auto instantiate(UnityObject* original) -> UnityObject* {
                if (!original) {
                    return nullptr;
                }
                static Method* method = get("UnityEngine.CoreModule.dll")->get("Object")->get<Method>("Instantiate", {"*"});
                return method->invoke<UnityObject*>(original);
            }

            static auto destroy(UnityObject* original) -> void {
                if (!original) {
                    return;
                }
                static Method* method = get("UnityEngine.CoreModule.dll")->get("Object")->get<Method>("Destroy", {"*"});
                return method->invoke<void>(original);
            }

            static auto dont_destroy_on_load(UnityObject* target) -> void {
                if (!target) {
                    return;
                }
                static Method* method = get("UnityEngine.CoreModule.dll")->get("Object")->get<Method>("DontDestroyOnLoad", {"*"});
                return method->invoke<void>(target);
            }
        };

        struct Component : UnityObject {
            auto get_transform() -> Transform* {
                static Method* method = get("UnityEngine.CoreModule.dll")->get("Component")->get<Method>("get_transform");
                return method->invoke<Transform*>(this);
            }

            auto get_game_object() -> GameObject* {
                static Method* method = get("UnityEngine.CoreModule.dll")->get("Component")->get<Method>("get_gameObject");
                return method->invoke<GameObject*>(this);
            }

            auto get_tag() -> String* {
                static Method* method = get("UnityEngine.CoreModule.dll")->get("Component")->get<Method>("get_tag");
                return method->invoke<String*>(this);
            }

            template<typename T>
            auto get_components_in_children() -> std::vector<T> {
                static Method* method = get("UnityEngine.CoreModule.dll")->get("Component")->get<Method>("GetComponentsInChildren");
                return method->invoke<Array<T>*>(this)->to_vector();
            }

            template<typename T>
            auto get_components_in_children(Class* pClass) -> std::vector<T> {
                static Method* method = get("UnityEngine.CoreModule.dll")->get("Component")->get<Method>("GetComponentsInChildren", {"System.Type"});
                return method->invoke<Array<T>*>(this, pClass->get_type())->to_vector();
            }

            template<typename T>
            auto get_components() -> std::vector<T> {
                static Method* method = get("UnityEngine.CoreModule.dll")->get("Component")->get<Method>("GetComponents");
                return method->invoke<Array<T>*>(this)->to_vector();
            }

            template<typename T>
            auto get_components(Class* pClass) -> std::vector<T> {
                static Method* method = get("UnityEngine.CoreModule.dll")->get("Component")->get<Method>("GetComponents", {"System.Type"});
                return method->invoke<Array<T>*>(this, pClass->get_type())->to_vector();
            }

            template<typename T>
            auto get_components_in_parent() -> std::vector<T> {
                static Method* method = get("UnityEngine.CoreModule.dll")->get("Component")->get<Method>("GetComponentsInParent");
                return method->invoke<Array<T>*>(this)->to_vector();
            }

            template<typename T>
            auto get_components_in_parent(Class* pClass) -> std::vector<T> {
                static Method* method = get("UnityEngine.CoreModule.dll")->get("Component")->get<Method>("GetComponentsInParent", {"System.Type"});
                return method->invoke<Array<T>*>(this, pClass->get_type())->to_vector();
            }

            template<typename T>
            auto get_component_in_children(Class* pClass) -> T {
                static Method* method = get("UnityEngine.CoreModule.dll")->get("Component")->get<Method>("GetComponentInChildren", {"System.Type"});
                return method->invoke<T>(this, pClass->get_type());
            }

            template<typename T>
            auto get_component_in_parent(Class* pClass) -> T {
                static Method* method = get("UnityEngine.CoreModule.dll")->get("Component")->get<Method>("GetComponentInParent", {"System.Type"});
                return method->invoke<T>(this, pClass->get_type());
            }
        };

        struct Camera : Component {
            enum class Eye : int { Left, Right, Mono };

            static auto GetMain() -> Camera* {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Camera")->get<Method>("get_main");
                }
                if (method) {
                    return method->invoke<Camera*>();
                }
                return nullptr;
            }

            static auto GetCurrent() -> Camera* {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Camera")->get<Method>("get_current");
                }
                if (method) {
                    return method->invoke<Camera*>();
                }
                return nullptr;
            }

            static auto GetAllCount() -> int {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Camera")->get<Method>("get_allCamerasCount");
                }
                if (method) {
                    return method->invoke<int>();
                }
                return 0;
            }

            static auto GetAllCamera() -> std::vector<Camera*> {
                static Method* method;
                static std::shared_ptr<Class> klass;

                if (!method || !klass) {
                    method = get("UnityEngine.CoreModule.dll")->get("Camera")->get<Method>("GetAllCameras", {"*"});
                    klass = get("UnityEngine.CoreModule.dll")->get("Camera");
                }

                if (method && klass) {
                    if (const int count = GetAllCount(); count != 0) {
                        const auto array = Array<Camera*>::create(klass.get(), count);
                        method->invoke<int>(array);
                        return array->to_vector();
                    }
                }

                return {};
            }

            auto GetDepth() -> float {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Camera")->get<Method>("get_depth");
                }
                if (method) {
                    return method->invoke<float>(this);
                }
                return 0.0f;
            }

            auto SetDepth(const float depth) -> void {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Camera")->get<Method>("set_depth", {"*"});
                }
                if (method) {
                    return method->invoke<void>(this, depth);
                }
            }

            auto SetFoV(const float fov) -> void {
                static Method* method_fieldOfView;
                if (!method_fieldOfView) {
                    method_fieldOfView = get("UnityEngine.CoreModule.dll")->get("Camera")->get<Method>("set_fieldOfView", {"*"});
                }
                if (method_fieldOfView) {
                    return method_fieldOfView->invoke<void>(this, fov);
                }
            }

            auto GetFoV() -> float {
                static Method* method_fieldOfView;
                if (!method_fieldOfView) {
                    method_fieldOfView = get("UnityEngine.CoreModule.dll")->get("Camera")->get<Method>("get_fieldOfView");
                }
                if (method_fieldOfView) {
                    return method_fieldOfView->invoke<float>(this);
                }
                return 0.0f;
            }

            auto WorldToScreenPoint(const Vector3& position, const Eye eye = Eye::Mono) -> Vector3 {
                static Method* method;
                if (!method) {
                    if (mode_ == Mode::Mono) {
                        method = get("UnityEngine.CoreModule.dll")->get("Camera")->get<Method>("WorldToScreenPoint_Injected");
                    } else {
                        method = get("UnityEngine.CoreModule.dll")->get("Camera")->get<Method>("WorldToScreenPoint", {"*", "*"});
                    }
                }
                if (mode_ == Mode::Mono && method) {
                    constexpr Vector3 vec3{};
                    method->invoke<void>(this, position, eye, &vec3);
                    return vec3;
                }
                if (method) {
                    return method->invoke<Vector3>(this, position, eye);
                }
                return {};
            }

            auto ScreenToWorldPoint(const Vector3& position, const Eye eye = Eye::Mono) -> Vector3 {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Camera")->get<Method>(mode_ == Mode::Mono ? "ScreenToWorldPoint_Injected" : "ScreenToWorldPoint");
                }
                if (mode_ == Mode::Mono && method) {
                    constexpr Vector3 vec3{};
                    method->invoke<void>(this, position, eye, &vec3);
                    return vec3;
                }
                if (method) {
                    return method->invoke<Vector3>(this, position, eye);
                }
                return {};
            }

            auto CameraToWorldMatrix() -> Matrix4x4 {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Camera")->get<Method>(mode_ == Mode::Mono ? "get_cameraToWorldMatrix_Injected" : "get_cameraToWorldMatrix");
                }
                if (mode_ == Mode::Mono && method) {
                    Matrix4x4 matrix4{};
                    method->invoke<void>(this, &matrix4);
                    return matrix4;
                }
                if (method) {
                    return method->invoke<Matrix4x4>(this);
                }
                return {};
            }

            auto ScreenPointToRay(const Vector2& position, const Eye eye = Eye::Mono) -> Ray {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Camera")->get<Method>(mode_ == Mode::Mono ? "ScreenPointToRay_Injected" : "ScreenPointToRay");
                }
                if (mode_ == Mode::Mono && method) {
                    Ray ray{};
                    method->invoke<void>(this, position, eye, &ray);
                    return ray;
                }
                if (method) {
                    return method->invoke<Ray>(this, position, eye);
                }
                return {};
            }
        };

        struct Transform : Component {
            auto GetPosition() -> Vector3 {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Transform")->get<Method>("get_position_Injected");
                }
                constexpr Vector3 vec3{};
                method->invoke<void>(this, &vec3);
                return vec3;
            }

            auto SetPosition(const Vector3& position) -> void {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Transform")->get<Method>("set_position_Injected");
                }
                return method->invoke<void>(this, &position);
            }

            auto GetRight() -> Vector3 {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Transform")->get<Method>("get_right");
                }
                if (method) {
                    return method->invoke<Vector3>(this);
                }
                return {};
            }

            auto SetRight(const Vector3& value) -> void {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Transform")->get<Method>("set_right");
                }
                if (method) {
                    return method->invoke<void>(this, value);
                }
            }

            auto GetUp() -> Vector3 {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Transform")->get<Method>("get_up");
                }
                if (method) {
                    return method->invoke<Vector3>(this);
                }
                return {};
            }

            auto SetUp(const Vector3& value) -> void {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Transform")->get<Method>("set_up");
                }
                if (method) {
                    return method->invoke<void>(this, value);
                }
            }

            auto GetForward() -> Vector3 {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Transform")->get<Method>("get_forward");
                }
                if (method) {
                    return method->invoke<Vector3>(this);
                }
                return {};
            }

            auto SetForward(const Vector3& value) -> void {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Transform")->get<Method>("set_forward");
                }
                if (method) {
                    return method->invoke<void>(this, value);
                }
            }

            auto GetRotation() -> Quaternion {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Transform")->get<Method>(mode_ == Mode::Mono ? "get_rotation_Injected" : "get_rotation");
                }
                if (mode_ == Mode::Mono && method) {
                    constexpr Quaternion vec3{};
                    method->invoke<void>(this, &vec3);
                    return vec3;
                }
                if (method) {
                    return method->invoke<Quaternion>(this);
                }
                return {};
            }

            auto SetRotation(const Quaternion& position) -> void {
                static Method* method = get("UnityEngine.CoreModule.dll")->get("Transform")->get<Method>(mode_ == Mode::Mono ? "set_rotation_Injected" : "set_rotation");
                return mode_ == Mode::Mono ? method->invoke<void>(this, &position) : method->invoke<void>(this, position);
            }

            auto GetLocalPosition() -> Vector3 {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Transform")->get<Method>(mode_ == Mode::Mono ? "get_localPosition_Injected" : "get_localPosition");
                }
                if (mode_ == Mode::Mono && method) {
                    constexpr Vector3 vec3{};
                    method->invoke<void>(this, &vec3);
                    return vec3;
                }
                if (method) {
                    return method->invoke<Vector3>(this);
                }
                return {};
            }

            auto SetLocalPosition(const Vector3& position) -> void {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Transform")->get<Method>(mode_ == Mode::Mono ? "set_localPosition_Injected" : "set_localPosition");
                }
                if (mode_ == Mode::Mono && method) {
                    return method->invoke<void>(this, &position);
                }
                if (method) {
                    return method->invoke<void>(this, position);
                }
            }

            auto GetLocalRotation() -> Quaternion {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Transform")->get<Method>(mode_ == Mode::Mono ? "get_localRotation_Injected" : "get_localRotation");
                }
                if (mode_ == Mode::Mono && method) {
                    constexpr Quaternion vec3{};
                    method->invoke<void>(this, &vec3);
                    return vec3;
                }
                if (method) {
                    return method->invoke<Quaternion>(this);
                }
                return {};
            }

            auto SetLocalRotation(const Quaternion& position) -> void {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Transform")->get<Method>(mode_ == Mode::Mono ? "set_localRotation_Injected" : "set_localRotation");
                }
                if (mode_ == Mode::Mono && method) {
                    return method->invoke<void>(this, &position);
                }
                if (method) {
                    return method->invoke<void>(this, position);
                }
            }

            auto GetLocalScale() -> Vector3 {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Transform")->get<Method>("get_localScale_Injected");
                }
                constexpr Vector3 vec3{};
                method->invoke<void>(this, &vec3);
                return vec3;
            }

            auto SetLocalScale(const Vector3& position) -> void {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Transform")->get<Method>(mode_ == Mode::Mono ? "set_localScale_Injected" : "set_localScale");
                }
                if (mode_ == Mode::Mono && method) {
                    return method->invoke<void>(this, &position);
                }
                if (method) {
                    return method->invoke<void>(this, position);
                }
            }

            auto GetChildCount() -> int {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Transform")->get<Method>("get_childCount");
                }
                if (method) {
                    return method->invoke<int>(this);
                }
                return 0;
            }

            auto GetChild(const int index) -> Transform* {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Transform")->get<Method>("GetChild");
                }
                if (method) {
                    return method->invoke<Transform*>(this, index);
                }
                return nullptr;
            }

            auto GetRoot() -> Transform* {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Transform")->get<Method>("GetRoot");
                }
                if (method) {
                    return method->invoke<Transform*>(this);
                }
                return nullptr;
            }

            auto GetParent() -> Transform* {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Transform")->get<Method>("GetParent");
                }
                if (method) {
                    return method->invoke<Transform*>(this);
                }
                return nullptr;
            }

            auto GetLossyScale() -> Vector3 {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Transform")->get<Method>(mode_ == Mode::Mono ? "get_lossyScale_Injected" : "get_lossyScale");
                }
                if (mode_ == Mode::Mono && method) {
                    constexpr Vector3 vec3{};
                    method->invoke<void>(this, &vec3);
                    return vec3;
                }
                if (method) {
                    return method->invoke<Vector3>(this);
                }
                return {};
            }

            auto TransformPoint(const Vector3& position) -> Vector3 {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Transform")->get<Method>(mode_ == Mode::Mono ? "TransformPoint_Injected" : "TransformPoint");
                }
                if (mode_ == Mode::Mono && method) {
                    constexpr Vector3 vec3{};
                    method->invoke<void>(this, position, &vec3);
                    return vec3;
                }
                if (method) {
                    return method->invoke<Vector3>(this, position);
                }
                return {};
            }

            auto LookAt(const Vector3& worldPosition) -> void {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Transform")->get<Method>("LookAt", {"Vector3"});
                }
                if (method) {
                    return method->invoke<void>(this, worldPosition);
                }
            }

            auto Rotate(const Vector3& eulers) -> void {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Transform")->get<Method>("Rotate", {"Vector3"});
                }
                if (method) {
                    return method->invoke<void>(this, eulers);
                }
            }
        };

        struct GameObject : UnityObject {
            static auto create(const std::string& _name) -> GameObject* {
                auto klass = get("UnityEngine.CoreModule.dll")->get("GameObject");
                if (!klass) {
                    return nullptr;
                }
                auto obj = klass->create<GameObject>();
                if (!obj) {
                    return nullptr;
                }
                static Method* method;
                if (!method) {
                    method = klass->get<Method>("Internal_CreateGameObject");
                }
                if (method) {
                    method->invoke<void, GameObject*, String*>(obj, String::create(_name));
                }
                return obj ? obj : nullptr;
            }

            static auto find_game_objects_with_tag(const std::string& name) -> std::vector<GameObject*> {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("GameObject")->get<Method>("FindGameObjectsWithTag");
                }
                if (method) {
                    const auto array = method->invoke<Array<GameObject*>*>(String::create(name));
                    return array->to_vector();
                }
                return {};
            }

            static auto find(const std::string& _name) -> GameObject* {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("GameObject")->get<Method>("Find");
                }
                if (method) {
                    return method->invoke<GameObject*>(String::create(_name));
                }
                return nullptr;
            }

            auto get_active() -> bool {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("GameObject")->get<Method>("get_active");
                }
                if (method) {
                    return method->invoke<bool>(this);
                }
                return false;
            }

            auto set_active(const bool value) -> void {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("GameObject")->get<Method>("set_active");
                }
                if (method) {
                    return method->invoke<void>(this, value);
                }
            }

            auto GetActiveSelf() -> bool {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("GameObject")->get<Method>("get_activeSelf");
                }
                if (method) {
                    return method->invoke<bool>(this);
                }
                return false;
            }

            auto GetActiveInHierarchy() -> bool {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("GameObject")->get<Method>("get_activeInHierarchy");
                }
                if (method) {
                    return method->invoke<bool>(this);
                }
                return false;
            }

            auto GetIsStatic() -> bool {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("GameObject")->get<Method>("get_isStatic");
                }
                if (method) {
                    return method->invoke<bool>(this);
                }
                return false;
            }

            auto GetTransform() -> Transform* {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("GameObject")->get<Method>("get_transform");
                }
                if (method) {
                    return method->invoke<Transform*>(this);
                }
                return nullptr;
            }

            auto GetTag() -> String* {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("GameObject")->get<Method>("get_tag");
                }
                if (method) {
                    return method->invoke<String*>(this);
                }
                return {};
            }

            template<typename T>
            auto GetComponent() -> T {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("GameObject")->get<Method>("GetComponent");
                }
                if (method) {
                    return method->invoke<T>(this);
                }
                return T();
            }

            template<typename T>
            auto GetComponent(Class* type) -> T {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("GameObject")->get<Method>("GetComponent", {"System.Type"});
                }
                if (method) {
                    return method->invoke<T>(this, type->get_type());
                }
                return T();
            }

            template<typename T>
            auto GetComponentInChildren(Class* type) -> T {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("GameObject")->get<Method>("GetComponentInChildren", {"System.Type"});
                }
                if (method) {
                    return method->invoke<T>(this, type->get_type());
                }
                return T();
            }

            template<typename T>
            auto GetComponentInParent(Class* type) -> T {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("GameObject")->get<Method>("GetComponentInParent", {"System.Type"});
                }
                if (method) {
                    return method->invoke<T>(this, type->get_type());
                }
                return T();
            }

            template<typename T>
            auto GetComponents(Class* type,
                               bool useSearchTypeAsArrayReturnType = false,
                               bool recursive = false,
                               bool includeInactive = true,
                               bool reverse = false,
                               List<T>* resultList = nullptr) -> std::vector<T> {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("GameObject")->get<Method>("GetComponentsInternal");
                }
                if (method) {
                    return method->invoke<Array<T>*>(this, type->get_type(), useSearchTypeAsArrayReturnType, recursive, includeInactive, reverse, resultList)->to_vector();
                }
                return {};
            }

            template<typename T>
            auto GetComponentsInChildren(Class* type, const bool includeInactive = false) -> std::vector<T> {
                return GetComponents<T>(type, false, true, includeInactive, false, nullptr);
            }

            template<typename T>
            auto GetComponentsInParent(Class* type, const bool includeInactive = false) -> std::vector<T> {
                return GetComponents<T>(type, false, true, includeInactive, true, nullptr);
            }
        };

        struct LayerMask : Object {
            int m_Mask;

            static auto NameToLayer(const std::string& layerName) -> int {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("LayerMask")->get<Method>("NameToLayer");
                }
                if (method) {
                    return method->invoke<int>(String::create(layerName));
                }
                return 0;
            }

            static auto LayerToName(const int layer) -> String* {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("LayerMask")->get<Method>("LayerToName");
                }
                if (method) {
                    return method->invoke<String*>(layer);
                }
                return {};
            }
        };

        struct Rigidbody : Component {
            auto GetDetectCollisions() -> bool {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.PhysicsModule.dll")->get("Rigidbody")->get<Method>("get_detectCollisions");
                }
                if (method) {
                    return method->invoke<bool>(this);
                }
                return false;
            }

            auto SetDetectCollisions(const bool value) -> void {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.PhysicsModule.dll")->get("Rigidbody")->get<Method>("set_detectCollisions");
                }
                if (method) {
                    return method->invoke<void>(this, value);
                }
            }

            auto GetVelocity() -> Vector3 {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.PhysicsModule.dll")->get("Rigidbody")->get<Method>(mode_ == Mode::Mono ? "get_velocity_Injected" : "get_velocity");
                }
                if (mode_ == Mode::Mono && method) {
                    Vector3 vector;
                    method->invoke<void>(this, &vector);
                    return vector;
                }
                if (method) {
                    return method->invoke<Vector3>(this);
                }
                return {};
            }

            auto SetVelocity(Vector3 value) -> void {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.PhysicsModule.dll")->get("Rigidbody")->get<Method>(mode_ == Mode::Mono ? "set_velocity_Injected" : "set_velocity");
                }
                if (mode_ == Mode::Mono && method) {
                    return method->invoke<void>(this, &value);
                }
                if (method) {
                    return method->invoke<void>(this, value);
                }
            }
        };

        struct Collider : Component {
            auto GetBounds() -> Bounds {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.PhysicsModule.dll")->get("Collider")->get<Method>("get_bounds_Injected");
                }
                if (method) {
                    Bounds bounds;
                    method->invoke<void>(this, &bounds);
                    return bounds;
                }
                return {};
            }
        };

        struct Mesh : UnityObject {
            auto GetBounds() -> Bounds {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Mesh")->get<Method>("get_bounds_Injected");
                }
                if (method) {
                    Bounds bounds;
                    method->invoke<void>(this, &bounds);
                    return bounds;
                }
                return {};
            }
        };

        struct CapsuleCollider : Collider {
            auto GetCenter() -> Vector3 {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.PhysicsModule.dll")->get("CapsuleCollider")->get<Method>("get_center");
                }
                if (method) {
                    return method->invoke<Vector3>(this);
                }
                return {};
            }

            auto GetDirection() -> Vector3 {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.PhysicsModule.dll")->get("CapsuleCollider")->get<Method>("get_direction");
                }
                if (method) {
                    return method->invoke<Vector3>(this);
                }
                return {};
            }

            auto GetHeightn() -> Vector3 {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.PhysicsModule.dll")->get("CapsuleCollider")->get<Method>("get_height");
                }
                if (method) {
                    return method->invoke<Vector3>(this);
                }
                return {};
            }

            auto GetRadius() -> Vector3 {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.PhysicsModule.dll")->get("CapsuleCollider")->get<Method>("get_radius");
                }
                if (method) {
                    return method->invoke<Vector3>(this);
                }
                return {};
            }
        };

        struct BoxCollider : Collider {
            auto GetCenter() -> Vector3 {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.PhysicsModule.dll")->get("BoxCollider")->get<Method>("get_center");
                }
                if (method) {
                    return method->invoke<Vector3>(this);
                }
                return {};
            }

            auto GetSize() -> Vector3 {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.PhysicsModule.dll")->get("BoxCollider")->get<Method>("get_size");
                }
                if (method) {
                    return method->invoke<Vector3>(this);
                }
                return {};
            }
        };

        struct Renderer : Component {
            auto GetBounds() -> Bounds {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Renderer")->get<Method>("get_bounds_Injected");
                }
                if (method) {
                    Bounds bounds;
                    method->invoke<void>(this, &bounds);
                    return bounds;
                }
                return {};
            }
        };

        struct Behaviour : Component {
            auto GetEnabled() -> bool {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Behaviour")->get<Method>("get_enabled");
                }
                if (method) {
                    return method->invoke<bool>(this);
                }
                return false;
            }

            auto SetEnabled(const bool value) -> void {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Behaviour")->get<Method>("set_enabled");
                }
                if (method) {
                    return method->invoke<void>(this, value);
                }
            }
        };

        struct MonoBehaviour : Behaviour {};

        struct Physics : Object {
            static auto Linecast(const Vector3& start, const Vector3& end) -> bool {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.PhysicsModule.dll")->get("Physics")->get<Method>("Linecast", {"*", "*"});
                }
                if (method) {
                    return method->invoke<bool>(start, end);
                }
                return false;
            }

            static auto Raycast(const Vector3& origin, const Vector3& direction, const float maxDistance) -> bool {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.PhysicsModule.dll")->get("Physics")->get<Method>("Raycast", {"UnityEngine.Vector3", "UnityEngine.Vector3", "System.Single"});
                }
                if (method) {
                    return method->invoke<bool>(origin, direction, maxDistance);
                }
                return false;
            }

            static auto Raycast(const Ray& origin, const RaycastHit* direction, const float maxDistance) -> bool {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.PhysicsModule.dll")->get("Physics")->get<Method>("Raycast", {"UnityEngine.Ray", "UnityEngine.RaycastHit&", "System.Single"});
                }
                if (method) {
                    return method->invoke<bool, Ray>(origin, direction, maxDistance);
                }
                return false;
            }

            static auto IgnoreCollision(Collider* collider1, Collider* collider2) -> void {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.PhysicsModule.dll")->get("Physics")->get<Method>("IgnoreCollision1", {"*", "*"});
                }
                if (method) {
                    return method->invoke<void>(collider1, collider2);
                }
            }
        };

        struct Animator : Behaviour {
            enum class HumanBodyBones : int {
                Hips,
                LeftUpperLeg,
                RightUpperLeg,
                LeftLowerLeg,
                RightLowerLeg,
                LeftFoot,
                RightFoot,
                Spine,
                Chest,
                UpperChest = 54,
                Neck       = 9,
                Head,
                LeftShoulder,
                RightShoulder,
                LeftUpperArm,
                RightUpperArm,
                LeftLowerArm,
                RightLowerArm,
                LeftHand,
                RightHand,
                LeftToes,
                RightToes,
                LeftEye,
                RightEye,
                Jaw,
                LeftThumbProximal,
                LeftThumbIntermediate,
                LeftThumbDistal,
                LeftIndexProximal,
                LeftIndexIntermediate,
                LeftIndexDistal,
                LeftMiddleProximal,
                LeftMiddleIntermediate,
                LeftMiddleDistal,
                LeftRingProximal,
                LeftRingIntermediate,
                LeftRingDistal,
                LeftLittleProximal,
                LeftLittleIntermediate,
                LeftLittleDistal,
                RightThumbProximal,
                RightThumbIntermediate,
                RightThumbDistal,
                RightIndexProximal,
                RightIndexIntermediate,
                RightIndexDistal,
                RightMiddleProximal,
                RightMiddleIntermediate,
                RightMiddleDistal,
                RightRingProximal,
                RightRingIntermediate,
                RightRingDistal,
                RightLittleProximal,
                RightLittleIntermediate,
                RightLittleDistal,
                LastBone = 55
            };

            auto GetBoneTransform(const HumanBodyBones humanBoneId) -> Transform* {
                static Method* method;

                if (!method) {
                    method = get("UnityEngine.AnimationModule.dll")->get("Animator")->get<Method>("GetBoneTransform");
                }
                if (method) {
                    return method->invoke<Transform*>(this, humanBoneId);
                }
                return nullptr;
            }
        };

        struct Time {
            static auto GetTime() -> float {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Time")->get<Method>("get_time");
                }
                if (method) {
                    return method->invoke<float>();
                }
                return 0.0f;
            }

            static auto GetDeltaTime() -> float {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Time")->get<Method>("get_deltaTime");
                }
                if (method) {
                    return method->invoke<float>();
                }
                return 0.0f;
            }

            static auto GetFixedDeltaTime() -> float {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Time")->get<Method>("get_fixedDeltaTime");
                }
                if (method) {
                    return method->invoke<float>();
                }
                return 0.0f;
            }

            static auto GetTimeScale() -> float {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Time")->get<Method>("get_timeScale");
                }
                if (method) {
                    return method->invoke<float>();
                }
                return 0.0f;
            }

            static auto SetTimeScale(const float value) -> void {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Time")->get<Method>("set_timeScale");
                }
                if (method) {
                    return method->invoke<void>(value);
                }
            }
        };

        struct Screen {
            static auto get_width() -> Int32 {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Screen")->get<Method>("get_width");
                }
                if (method) {
                    return method->invoke<int32_t>();
                }
                return 0;
            }

            static auto get_height() -> Int32 {
                static Method* method;
                if (!method) {
                    method = get("UnityEngine.CoreModule.dll")->get("Screen")->get<Method>("get_height");
                }
                if (method) {
                    return method->invoke<int32_t>();
                }
                return 0;
            }
        };

        template<typename Return, typename... Args>
        static auto invoke(void* address, Args... args) -> Return {
#if WINDOWS_MODE
            if (address != nullptr) {
                return reinterpret_cast<Return (*)(Args...)>(address)(args...);
            }
#elif LINUX_MODE || ANDROID_MODE || IOS_MODE || HARMONYOS_MODE
            if (address != nullptr)
                return ((Return (*)(Args...))(address))(args...);
#endif
            return Return();
        }
    };

private:
    inline static Mode mode_{};
    inline static void* hmodule_;
    inline static std::unordered_map<std::string, void*> address_{};
    inline static void* pDomain{};
};
#endif // UNITYRESOLVE_HPPs
