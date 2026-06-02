/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_MODULE_H
#define UBSE_MODULE_H

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
// ============================================================
// CORE_MODULE_IMPL — type form, auto-convert via kModuleName
// Usage: CORE_MODULE_IMPL(CoreModuleA)
//        CORE_MODULE_IMPL(CoreModuleB, CoreModuleA)
// ============================================================
#define CORE_MODULE_IMPL(CLASS, ...)                                                                    \
    namespace {                                                                                         \
    struct CLASS##Registrar {                                                                           \
        CLASS##Registrar()                                                                              \
        {                                                                                               \
            ubse::module::UbseModuleRegistry::GetInstance().RegisterCoreModule<CLASS, ##__VA_ARGS__>(); \
        }                                                                                               \
    };                                                                                                  \
    static CLASS##Registrar CLASS##registrar;                                                           \
    }

// ============================================================
// OPTIONAL_MODULE_IMPL — type form, auto-convert via kModuleName
// Usage: OPTIONAL_MODULE_IMPL(OptionalModuleX)
//        OPTIONAL_MODULE_IMPL(OptionalModuleY, OptionalModuleX)
// ============================================================
#define OPTIONAL_MODULE_IMPL(CLASS, ...)                                                                    \
    namespace {                                                                                             \
    struct CLASS##Registrar {                                                                               \
        CLASS##Registrar()                                                                                  \
        {                                                                                                   \
            ubse::module::UbseModuleRegistry::GetInstance().RegisterOptionalModule<CLASS, ##__VA_ARGS__>(); \
        }                                                                                                   \
    };                                                                                                      \
    static CLASS##Registrar CLASS##registrar;                                                               \
    }

// ============================================================
// PLUGIN_MODULE_IMPL — enum form, compile-time checked
// Usage: PLUGIN_MODULE_IMPL(PluginAlpha, ModuleId::OptionalModuleX)
//        PLUGIN_MODULE_IMPL(PluginBeta, ModuleId::OptionalModuleY, ModuleId::OptionalModuleX)
// ============================================================

#define PLUGIN_MODULE_IMPL(CLASS, MODULE_LIST)                                                    \
    extern "C" {                                                                                  \
    void __attribute__((constructor)) CLASS##_plugin_init()                                       \
    {                                                                                             \
        ubse::module::UbseModuleRegistry::GetInstance().RegisterPluginModule<CLASS>(MODULE_LIST); \
    }                                                                                             \
    }
namespace ubse::module {
enum class UbseModuleCategory {
    CORE,
    OPTIONAL,
    PLUGIN
};
enum class UbseOptionModule {
    UbseMmiModule,
    UbseLcneModule,
    SysSentryModule,
    UbseUrmaUvsModule,
    UbseNodeControllerModule,
    UbseUrmaControllerModule,
    UbseComModule,
    UbseElectionModule,
    UbseHttpModule,
    UbseStorageModule,
    UbseRasModule,
    UbseNodeMgrModule
};

inline std::string UbseModuleIdToString(UbseOptionModule id)
{
    switch (id) {
        case UbseOptionModule::UbseMmiModule:
            return "UbseMmiModule";
        case UbseOptionModule::UbseLcneModule:
            return "UbseLcneModule";
        case UbseOptionModule::SysSentryModule:
            return "SysSentryModule";
        case UbseOptionModule::UbseUrmaUvsModule:
            return "UbseUrmaUvsModule";
        case UbseOptionModule::UbseNodeControllerModule:
            return "UbseNodeControllerModule";
        case UbseOptionModule::UbseUrmaControllerModule:
            return "UbseUrmaControllerModule";
        case UbseOptionModule::UbseComModule:
            return "UbseComModule";
        case UbseOptionModule::UbseElectionModule:
            return "UbseElectionModule";
        case UbseOptionModule::UbseHttpModule:
            return "UbseHttpModule";
        case UbseOptionModule::UbseStorageModule:
            return "UbseStorageModule";
        case UbseOptionModule::UbseRasModule:
            return "UbseRasModule";
        case UbseOptionModule::UbseNodeMgrModule:
            return "UbseNodeMgrModule";
        default:
            return "UnKnown";
    }
}

class UbseModule {
public:
    virtual ~UbseModule() = default;
    virtual uint32_t Initialize() = 0;

    virtual void UnInitialize() = 0;

    virtual uint32_t Start() = 0;

    virtual void Stop() = 0;

    virtual void RegArgs() {};
    virtual std::string Name() const = 0;
    template <typename T>
    static std::shared_ptr<UbseModule> CreateModule()
    {
        return std::make_shared<T>();
    }
};
struct UbseModuleEntry {
    std::function<std::shared_ptr<UbseModule>()> creator;
    std::vector<std::string> dependencies;
    UbseModuleCategory category;
};

class UbseModuleRegistry {
public:
    static UbseModuleRegistry &GetInstance()
    {
        static UbseModuleRegistry instance;
        return instance;
    }

    template <typename T, typename... Deps>
    void RegisterCoreModule()
    {
        std::string moduleName = T::kModuleName;
        if (moduleName.empty()) {
            return;
        }
        if (registry_.find(moduleName) != registry_.end()) {
            return;
        }
        UbseModuleEntry entry;
        entry.creator = []() {
            return std::make_shared<T>();
        };
        entry.category = UbseModuleCategory::CORE;
        if constexpr (sizeof...(Deps) > 0) {
            entry.dependencies = {Deps::kModuleName...};
        }
        registry_[moduleName] = entry;
    }

    template <typename T, typename... Deps>
    void RegisterOptionalModule()
    {
        std::string moduleName = T::kModuleName;
        if (moduleName.empty()) {
            return;
        }
        if (registry_.find(moduleName) != registry_.end()) {
            return;
        }
        UbseModuleEntry entry;
        entry.creator = []() {
            return std::make_shared<T>();
        };
        entry.category = UbseModuleCategory::OPTIONAL;

        if constexpr (sizeof...(Deps) > 0) {
            entry.dependencies = {Deps::kModuleName...};
        }
        registry_[moduleName] = entry;
    }
    template <typename T, typename Container>
    void RegisterPluginModule(const Container &deps)
    {
        std::string moduleName = T::kModuleName;
        if (moduleName.empty()) {
            return;
        }
        if (registry_.find(moduleName) != registry_.end()) {
            return;
        }

        UbseModuleEntry entry;
        entry.creator = []() {
            return std::make_shared<T>();
        };
        entry.category = UbseModuleCategory::PLUGIN;

        // 这里的迭代器逻辑完全兼容 std::array 和 std::vector
        for (const auto &dep : deps) {
            entry.dependencies.push_back(UbseModuleIdToString(dep));
        }

        registry_[moduleName] = std::move(entry); // 使用 move 提升性能
    }
    void UnregisterPluginModules()
    {
        std::vector<std::string> toRemove;
        for (const auto &[name, entry] : registry_) {
            if (entry.category == UbseModuleCategory::PLUGIN) {
                toRemove.push_back(name);
            }
        }
        for (const auto &name : toRemove) {
            registry_.erase(name);
            std::cout << "[UbseModuleRegistry] Unregistered plugin: " << name << std::endl;
        }
    }
    std::map<std::string, UbseModuleEntry> TakeRegistry()
    {
        return std::move(registry_);
    }

private:
    UbseModuleRegistry() = default;
    std::map<std::string, UbseModuleEntry> registry_{};
};
} // namespace ubse::module
#endif // UBSE_MODULE_H
