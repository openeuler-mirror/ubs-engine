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
#include <set>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>

#include "ubse_module.h"
#include "ubse_common_def.h"
namespace ubse::context {
using namespace ubse::module;
using namespace ubse::common::def;
using ModulerCreatorFunc = std::function<std::shared_ptr<UbseModule>()>;

enum class ProcessMode {
    MANAGER, // manager启动
    CLI,     // cli启动
    DEFAULT  // 默认启动方式, manager启动
};

extern std::atomic<bool> g_globalStop;
extern std::condition_variable_any g_globalCv;

class UbseContext {
public:
    static UbseContext &GetInstance()
    {
        static UbseContext instance;
        return instance;
    }

    UbseContext(const UbseContext &) = delete;

    UbseContext &operator=(const UbseContext &) = delete;

    // 运行上下文
    UbseResult Run(int argc, char *argv[], ProcessMode = ProcessMode::MANAGER);
    // 停止上下文
    void Stop();

    template <typename T>
    std::shared_ptr<T> GetModule()
    {
        // 静态断言：确保类型T完整且是UbseModule派生类
        static_assert(sizeof(T) != 0, "Type is incomplete. Provide a full definition.");
        static_assert(std::is_base_of_v<UbseModule, T>, "GetModule must be used with UbseModule derived types");
        auto it = moduleMap_.find(T::kModuleName);
        if (it != moduleMap_.end()) {
            return std::dynamic_pointer_cast<T>(it->second);
        }
        return nullptr;
    }

    UbseResult GetArgStr(const std::string &argName, std::string &argValue);

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
    char **GetArgv() const
    {
        return cmdArgv_;
    }

    bool IsAllModulesReady() const;

    ~UbseContext() = default;

private:
    UbseContext() = default;

    UbseResult CreateModules();

    void SetProcessMode(ProcessMode mode);

    UbseResult GetExecutablePath();

    UbseResult InitModule(std::vector<std::pair<std::string, std::shared_ptr<UbseModule>>>& sortedModuleVec);

    UbseResult StartModule(std::vector<std::pair<std::string, std::shared_ptr<UbseModule>>>& sortedModuleVec);

    UbseResult StopModule(std::vector<std::pair<std::string, std::shared_ptr<UbseModule>>>& sortedModuleVec);

    UbseResult DestroyModule(std::vector<std::pair<std::string, std::shared_ptr<UbseModule>>>& sortedModuleVec);

    UbseResult RegisterArg();

    UbseResult ParserArgs(int argc, char *argv[]);

    UbseResult InitAndStartModule();
    UbseResult InitCoreModules(std::chrono::time_point<std::chrono::system_clock>& moduleStartTime);
    UbseResult StartCoreModules(std::chrono::time_point<std::chrono::system_clock>& moduleStartTime);
    UbseResult InitAndStartNonCoreModules(std::chrono::time_point<std::chrono::system_clock>& moduleStartTime);
    void StopNonCoreModules();
    void DestroyNonCoreModules();
    void StopCoreModules();
    void DestroyCoreModules();
    void ResolveActivation();
    void ActivateWithDependencies(const std::string& name);
    void ActivateWithDependenciesImpl(const std::string& name, std::set<std::string>& inStack);
    std::vector<std::string> TopologicalSort();
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
    char **cmdArgv_ = nullptr;
    std::string ubseRunPath_{};
    ProcessMode processMode_ = ProcessMode::DEFAULT;
    // 命令行参数
    std::unordered_map<std::string, std::string> argMap_{};
    std::map<std::string, UbseModuleEntry> registry_{};
    std::set<std::string> activated_{};
    std::vector<std::pair<std::string, std::shared_ptr<UbseModule>>> sortedModules_;
    std::map<std::string, std::shared_ptr<UbseModule>> moduleMap_;

    uint8_t workReadiness_ = 0;

    std::atomic<bool> allModulesReady_ = false;
};
} // namespace ubse::context
#endif // UBSE_CONTEXT_H
