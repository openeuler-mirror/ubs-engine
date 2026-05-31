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

#ifndef UBSE_CONTEXT_H
#define UBSE_CONTEXT_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>

#include "ubse_module.h"

// 所有UbseModule的扩展类cpp文件都需要使用该宏提供方法实现体,并完成向上下文中注册类信息
#define DYNAMIC_CREATE(MODULE_NAME, ...)                                                      \
    static UbseResult g_tmp_##MODULE_NAME =                                                   \
        ubse::context::UbseContext::GetInstance().RegisterModule<MODULE_NAME, ##__VA_ARGS__>( \
            ubse::module::UbseModule::CreateModule<MODULE_NAME>)

#define CONDITION_DYNAMIC_CREATE(CON, MODULE_NAME, ...)                                             \
    static UbseResult g_tmp_##MODULE_NAME =                                                         \
        CON ? ubse::context::UbseContext::GetInstance().RegisterModule<MODULE_NAME, ##__VA_ARGS__>( \
                  ubse::module::UbseModule::CreateModule<MODULE_NAME>) :                            \
              UBSE_OK

#define BASE_DYNAMIC_CREATE(MODULE_NAME, ...)                                                     \
    static UbseResult g_tmp_##MODULE_NAME =                                                       \
        ubse::context::UbseContext::GetInstance().RegisterBaseModule<MODULE_NAME, ##__VA_ARGS__>( \
            UbseModule::CreateModule<MODULE_NAME>)
#define CONDITION_BASE_DYNAMIC_CREATE(CON, MODULE_NAME, ...)                                            \
    static UbseResult g_tmp_##MODULE_NAME =                                                             \
        CON ? ubse::context::UbseContext::GetInstance().RegisterBaseModule<MODULE_NAME, ##__VA_ARGS__>( \
                  ubse::module::UbseModule::CreateModule<MODULE_NAME>) :                                \
              UBSE_OK
namespace ubse::context {
using ubse::common::def::UbseResult;
using ubse::module::UbseModule;

using ModulerCreatorFunc = std::function<std::shared_ptr<UbseModule>()>;

enum class ProcessMode
{
    MANAGER, // manager启动
    CLI,     // cli启动
    DEFAULT  // 默认启动方式, manager启动
};

extern std::atomic<bool> g_globalStop;
extern std::condition_variable_any g_globalCv;

enum class SceneType
{
    AI,
    COMMON
};

SceneType GetSceneType();

class UbseContext {
public:
    static UbseContext& GetInstance()
    {
        static UbseContext instance;
        return instance;
    }

    UbseContext(const UbseContext&) = delete;

    UbseContext& operator=(const UbseContext&) = delete;

    // 运行上下文
    UbseResult Run(int argc, char* argv[], ProcessMode = ProcessMode::MANAGER);

    template <typename T, typename... Dependencies>
    UbseResult RegisterModule(const ModulerCreatorFunc& creator)
    {
        // 静态断言：确保类型T完整且是UbseModule派生类
        static_assert(sizeof(T) != 0, "Type is incomplete. Provide a full definition.");
        static_assert(std::is_base_of_v<UbseModule, T>, "GetModule must be used with UbseModule derived types");
        // 依赖项检查: 处理0个或多个依赖
        if constexpr (sizeof...(Dependencies) > 0) {
            // 使用折叠表达式检查每个依赖项类型
            (CheckDependencyType<Dependencies>(), ...);
        }
        moduleCreatorMap_[typeid(T)] = {creator, {typeid(Dependencies)...}};
        return 0;
    }

    template <typename T, typename... Dependencies>
    UbseResult RegisterBaseModule(const ModulerCreatorFunc& creator)
    {
        // 静态断言：确保类型T完整且是UbseModule派生类
        static_assert(sizeof(T) != 0, "Type is incomplete. Provide a full definition.");
        static_assert(std::is_base_of_v<UbseModule, T>, "GetModule must be used with UbseModule derived types");
        // 依赖项检查: 处理0个或多个依赖
        if constexpr (sizeof...(Dependencies) > 0) {
            // 使用折叠表达式检查每个依赖项类型
            (CheckDependencyType<Dependencies>(), ...);
        }
        baseModuleCreatorMap_[typeid(T)] = {creator, {typeid(Dependencies)...}};
        return 0;
    }

    // 停止上下文
    void Stop();

    template <typename T>
    std::shared_ptr<T> GetModule()
    {
        // 静态断言：确保类型T完整且是UbseModule派生类
        static_assert(sizeof(T) != 0, "Type is incomplete. Provide a full definition.");
        static_assert(std::is_base_of_v<UbseModule, T>, "GetModule must be used with UbseModule derived types");
        const std::type_index moduleType(typeid(T));
        try {
            auto it = moduleMap_.find(moduleType);
            if (it != moduleMap_.end()) {
                return std::dynamic_pointer_cast<T>(it->second);
            }
        } catch (...) {
            return nullptr;
        }
        return nullptr;
    }

    UbseResult GetArgStr(const std::string& argName, std::string& argValue);

    ProcessMode GetProcessMode() const;

    std::string GetUbseRunPath() const;

    UbseResult SetWorkReadiness(uint8_t currentStatus);

    uint8_t GetWorkReadiness() const;

    // 获取 cmdArgc
    int GetArgc() const
    {
        return cmdArgc_;
    }

    // 获取 cmdArgv
    char** GetArgv() const
    {
        return cmdArgv_;
    }

    bool IsAllModulesReady() const;

    ~UbseContext() = default;

private:
    struct ModuleEntry {
        ModulerCreatorFunc creator;
        std::vector<std::type_index> dependencies;
    };

    UbseContext() = default;

    UbseResult CreateModules();

    UbseResult CreateModules(const std::unordered_map<std::type_index, ModuleEntry>& creatorMap,
                             std::vector<std::pair<std::type_index, std::shared_ptr<UbseModule>>>& sortedModuleVec);

    void SetProcessMode(ProcessMode mode);

    std::vector<std::type_index> TopologicalSort(const std::unordered_map<std::type_index, ModuleEntry>& creatorMap);

    bool TopologicalSortUtil(const std::unordered_map<std::type_index, ModuleEntry>& creatorMap,
                             const std::type_index& moduleName, std::unordered_set<std::type_index>& visited,
                             std::unordered_set<std::type_index>& visiting, std::vector<std::type_index>& sorted);

    UbseResult GetExecutablePath();

    UbseResult InitModule(std::vector<std::pair<std::type_index, std::shared_ptr<UbseModule>>>& sortedModuleVec);

    UbseResult StartModule(std::vector<std::pair<std::type_index, std::shared_ptr<UbseModule>>>& sortedModuleVec);

    UbseResult StopModule(std::vector<std::pair<std::type_index, std::shared_ptr<UbseModule>>>& sortedModuleVec);

    UbseResult DestroyModule(std::vector<std::pair<std::type_index, std::shared_ptr<UbseModule>>>& sortedModuleVec);

    UbseResult RegisterArg();

    UbseResult ParserArgs(int argc, char* argv[]);

    UbseResult InitAndStartModule();

    // 通用类型检查函数
    template <typename U>
    constexpr void CheckDependencyType()
    {
        // 1. 完整性检查
        static_assert(sizeof(U) != 0, "Dependency type is incomplete. Provide a full definition.");

        // 2. 必须是UbseModule派生类
        static_assert(std::is_base_of_v<UbseModule, U>, "All dependencies must derive from UbseModule");

        // 3. 不能是抽象类
        static_assert(!std::is_abstract_v<U>, "Dependency cannot be abstract class");
    }

    int cmdArgc_ = 0;
    char** cmdArgv_ = nullptr;
    std::string ubseRunPath_{};
    ProcessMode processMode_ = ProcessMode::DEFAULT;
    // 命令行参数
    std::unordered_map<std::string, std::string> argMap_{};
    std::unordered_map<std::type_index, ModuleEntry> moduleCreatorMap_{};
    std::unordered_map<std::type_index, ModuleEntry> baseModuleCreatorMap_{};
    std::vector<std::pair<std::type_index, std::shared_ptr<UbseModule>>> sortedModules_{};
    std::vector<std::pair<std::type_index, std::shared_ptr<UbseModule>>> sortedBaseModules_{};
    std::unordered_map<std::type_index, std::shared_ptr<UbseModule>> moduleMap_{};

    uint8_t workReadiness_ = 0;

    std::atomic<bool> allModulesReady_ = false;
};
} // namespace ubse::context
#endif // UBSE_CONTEXT_H
