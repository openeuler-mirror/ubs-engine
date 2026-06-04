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

#include "ubse_context.h"

#include <cxxabi.h>
#include <climits>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

#include "module_registry/ubse_plugin_loader.h"
#include "ubse_error.h"
namespace ubse::context {

std::atomic<bool> g_globalStop{false};
std::condition_variable_any g_globalCv;

std::string Demangle(const std::string &mangledName)
{
    int status = 0;
    auto demangled = abi::__cxa_demangle(mangledName.c_str(), nullptr, nullptr, &status);
    // 使用智能指针自动释放内存
    std::unique_ptr<char, void (*)(void *)> guard(demangled, std::free);
    if (status != 0) {
        return mangledName; // 返回原始名称
    }

    return demangled ? std::string(demangled) : mangledName;
}

inline long CountDuration(const std::chrono::time_point<std::chrono::system_clock> &start,
                          const std::chrono::time_point<std::chrono::system_clock> &end)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

UbseResult UbseContext::Run(int argc, char *argv[], ProcessMode mode)
{
    std::cout << "UbseContext::Run-start ProcessMode: " << static_cast<int>(mode) << std::endl;
    auto startTime = std::chrono::system_clock::now();
    this->cmdArgc_ = argc;
    this->cmdArgv_ = argv;
    // 获取运行路径
    UbseResult ret = GetExecutablePath();
    if (ret != UBSE_OK) {
        std::cerr << "UbseContext::Run-Error: get run path failed" << std::endl;
        return ret;
    }
    SetProcessMode(mode);
    std::cout << "UbseContext::Run-Loading plugins." << std::endl;
    UbsePluginLoader::GetInstance().DiscoverAndLoad();

    ret = CreateModules();
    if (ret != UBSE_OK) {
        std::cerr << "UbseContext::Run-Error: create modules failed" << std::endl;
        return ret;
    }
    // 注册参数
    ret = RegisterArg();
    if (ret != UBSE_OK) {
        std::cerr << "RegisterArg failed" << std::endl;
        return ret;
    }

    // 解析参数
    if (GetProcessMode() == ProcessMode::MANAGER) {
        ret = ParserArgs(argc, argv);
        if (ret != UBSE_OK) {
            std::cerr << "ParserArgs failed" << std::endl;
            return ret;
        }
    }

    // 初始化和启动模块
    ret = InitAndStartModule();
    if (ret != UBSE_OK) {
        return ret;
    }
    allModulesReady_.store(true);
    auto endTime = std::chrono::system_clock::now();
    std::cout << "UbseContext::Run-end. Total time: " << CountDuration(startTime, endTime) << "ms" << std::endl;
    return UBSE_OK;
}

UbseResult InitModule(const std::string &moduleName, std::shared_ptr<UbseModule> module,
                      std::chrono::system_clock::time_point &moduleStartTime)
{
    UbseResult res = module->Initialize();
    if (res != UBSE_OK) {
        std::cerr << "UbseContext::InitModule-Error: initializing module " << moduleName << " Error code: " << res
                  << std::endl;
        return res;
    }
    auto moduleEndTime = std::chrono::system_clock::now();
    std::cout << "UbseContext::InitModule-Module: " << moduleName
              << " initialized. InitTime: " << CountDuration(moduleStartTime, moduleEndTime) << "ms" << std::endl;
    moduleStartTime = moduleEndTime;
    return UBSE_OK;
}

UbseResult StartModule(const std::string &moduleName, std::shared_ptr<UbseModule> module,
                       std::chrono::system_clock::time_point &moduleStartTime)
{
    UbseResult res = module->Start();
    if (res != UBSE_OK) {
        std::cerr << "UbseContext::StartModule-Error: starting module " << moduleName << " Error code: " << res
                  << std::endl;
        return res;
    }
    auto moduleEndTime = std::chrono::system_clock::now();
    std::cout << "UbseContext::StartModule-Module: " << moduleName
              << " started. StartTime: " << CountDuration(moduleStartTime, moduleEndTime) << "ms" << std::endl;
    moduleStartTime = moduleEndTime;
    return UBSE_OK;
}

UbseResult UbseContext::InitCoreModules(std::chrono::time_point<std::chrono::system_clock> &moduleStartTime)
{
    std::cout << "UbseContext::InitCoreModules-CORE" << std::endl;
    for (const auto &[name, module] : sortedModules_) {
        if (g_globalStop.load()) {
            break;
        }
        auto it = registry_.find(name);
        if (it == registry_.end() || it->second.category != UbseModuleCategory::CORE) {
            continue;
        }
        auto ret = ubse::context::InitModule(name, module, moduleStartTime);
        if (ret != UBSE_OK) {
            std::cerr << "UbseContext::InitCoreModules-Error: initializing module " << name << std::endl;
            return ret;
        }
    }
    return UBSE_OK;
}

UbseResult UbseContext::StartCoreModules(std::chrono::time_point<std::chrono::system_clock> &moduleStartTime)
{
    std::cout << "UbseContext::StartCoreModules-CORE" << std::endl;
    for (const auto &[name, module] : sortedModules_) {
        if (g_globalStop.load()) {
            break;
        }
        auto it = registry_.find(name);
        if (it == registry_.end() || it->second.category != UbseModuleCategory::CORE) {
            continue;
        }
        auto ret = ubse::context::StartModule(name, module, moduleStartTime);
        if (ret != UBSE_OK) {
            std::cerr << "UbseContext::StartCoreModules-Error: starting module " << name << std::endl;
            return ret;
        }
        moduleMap_[name] = module;
    }
    return UBSE_OK;
}

UbseResult UbseContext::InitAndStartNonCoreModules(std::chrono::time_point<std::chrono::system_clock> &moduleStartTime)
{
    std::cout << "UbseContext::InitAndStartNonCoreModules-OPTIONAL/PLUGIN" << std::endl;
    for (const auto &[name, module] : sortedModules_) {
        if (g_globalStop.load()) {
            break;
        }
        auto it = registry_.find(name);
        if (it == registry_.end() || it->second.category == UbseModuleCategory::CORE) {
            continue;
        }
        auto ret = ubse::context::InitModule(name, module, moduleStartTime);
        if (ret != UBSE_OK) {
            std::cerr << "UbseContext::InitAndStartNonCoreModules-Error: initializing module " << name << std::endl;
            return ret;
        }
        ret = ubse::context::StartModule(name, module, moduleStartTime);
        if (ret != UBSE_OK) {
            std::cerr << "UbseContext::InitAndStartNonCoreModules-Error: starting module " << name << std::endl;
            return ret;
        }
        moduleMap_[name] = module;
    }
    return UBSE_OK;
}

UbseResult UbseContext::InitAndStartModule()
{
    std::cout << "UbseContext::InitAndStartModule-start" << std::endl;
    auto startTime = std::chrono::system_clock::now();
    auto moduleStartTime = startTime;

    if (auto ret = InitCoreModules(moduleStartTime); ret != UBSE_OK) {
        return ret;
    }
    if (auto ret = StartCoreModules(moduleStartTime); ret != UBSE_OK) {
        return ret;
    }
    if (auto ret = InitAndStartNonCoreModules(moduleStartTime); ret != UBSE_OK) {
        return ret;
    }

    auto endTime = std::chrono::system_clock::now();
    std::cout << "UbseContext::InitAndStartModule-end. Total time: " << CountDuration(startTime, endTime) << "ms"
              << std::endl;
    return UBSE_OK;
}

UbseResult UbseContext::InitModule(std::vector<std::pair<std::string, std::shared_ptr<UbseModule>>> &sortedModuleVec)
{
    std::cout << "UbseContext::InitModule-start" << std::endl;
    auto startTime = std::chrono::system_clock::now();
    auto moduleStartTime = startTime;
    for (const auto &[name, module] : sortedModuleVec) {
        if (!g_globalStop.load()) {
            auto ret = ubse::context::InitModule(name, module, moduleStartTime);
            if (ret != UBSE_OK) {
                return ret;
            }
        }
    }
    auto endTime = std::chrono::system_clock::now();
    std::cout << "UbseContext::InitModule-end. Total init time: " << CountDuration(startTime, endTime) << "ms"
              << std::endl;
    return UBSE_OK;
}

UbseResult UbseContext::StartModule(std::vector<std::pair<std::string, std::shared_ptr<UbseModule>>> &sortedModuleVec)
{
    std::cout << "UbseContext::StartModule-start" << std::endl;
    auto startTime = std::chrono::system_clock::now();
    auto moduleStartTime = startTime;
    for (const auto &[name, module] : sortedModuleVec) {
        if (!g_globalStop.load()) {
            auto ret = ubse::context::StartModule(name, module, moduleStartTime);
            if (ret != UBSE_OK) {
                return ret;
            }
        }
    }
    auto endTime = std::chrono::system_clock::now();
    std::cout << "UbseContext::StartModule-end. Total start time: " << CountDuration(startTime, endTime) << "ms"
              << std::endl;
    return UBSE_OK;
}

UbseResult UbseContext::StopModule(std::vector<std::pair<std::string, std::shared_ptr<UbseModule>>> &sortedModuleVec)
{
    std::cout << "UbseContext::StopModule-start" << std::endl;
    for (auto it = sortedModuleVec.rbegin(); it != sortedModuleVec.rend(); ++it) {
        try {
            moduleMap_.erase(it->first);
            it->second->Stop();
        } catch (const std::exception &e) {
            std::cerr << "UbseContext::StopModule-Error: stopping module " << it->first << ": " << e.what()
                      << std::endl;
        }
        std::cout << "UbseContext::StopModule-Module: " << it->first << std::endl;
    }
    std::cout << "UbseContext::StopModule-end" << std::endl;
    return UBSE_OK;
}

UbseResult UbseContext::DestroyModule(std::vector<std::pair<std::string, std::shared_ptr<UbseModule>>> &sortedModuleVec)
{
    std::cout << "UbseContext::DestroyModule-start" << std::endl;
    for (auto it = sortedModuleVec.rbegin(); it != sortedModuleVec.rend(); ++it) {
        try {
            it->second->UnInitialize();
        } catch (const std::exception &e) {
            std::cerr << "UbseContext::DestroyModule-Error: destroying module " << it->first << ": " << e.what()
                      << std::endl;
        }
        std::cout << "UbseContext::DestroyModule-Module: " << it->first << std::endl;
    }
    sortedModuleVec.clear();
    std::cout << "UbseContext::DestroyModule-end" << std::endl;
    return UBSE_OK;
}

void UbseContext::StopNonCoreModules()
{
    std::cout << "UbseContext::Stop-NonCORE" << std::endl;
    for (auto it = sortedModules_.rbegin(); it != sortedModules_.rend(); ++it) {
        auto entryIt = registry_.find(it->first);
        if (entryIt == registry_.end() || entryIt->second.category == UbseModuleCategory::CORE) {
            continue;
        }
        try {
            moduleMap_.erase(it->first);
            it->second->Stop();
        } catch (const std::exception &e) {
            std::cerr << "UbseContext::StopModule-Error: stopping module " << it->first << ": " << e.what()
                      << std::endl;
        }
        std::cout << "UbseContext::StopModule-Module: " << it->first << std::endl;
    }
}

void UbseContext::DestroyNonCoreModules()
{
    std::cout << "UbseContext::Destroy-NonCORE" << std::endl;
    for (auto it = sortedModules_.rbegin(); it != sortedModules_.rend(); ++it) {
        auto entryIt = registry_.find(it->first);
        if (entryIt == registry_.end() || entryIt->second.category == UbseModuleCategory::CORE) {
            continue;
        }
        try {
            it->second->UnInitialize();
        } catch (const std::exception &e) {
            std::cerr << "UbseContext::DestroyModule-Error: destroying module " << it->first << ": " << e.what()
                      << std::endl;
        }
        std::cout << "UbseContext::DestroyModule-Module: " << it->first << std::endl;

        if (entryIt->second.category == UbseModuleCategory::PLUGIN) {
            it->second.reset();
            registry_.erase(it->first);
            activated_.erase(it->first);
            std::cout << "[UbseContext] Unregistered plugin: " << it->first << std::endl;
        }
    }
}

void UbseContext::StopCoreModules()
{
    std::cout << "UbseContext::Stop-CORE" << std::endl;
    for (auto it = sortedModules_.rbegin(); it != sortedModules_.rend(); ++it) {
        auto entryIt = registry_.find(it->first);
        if (entryIt == registry_.end() || entryIt->second.category != UbseModuleCategory::CORE) {
            continue;
        }
        try {
            moduleMap_.erase(it->first);
            it->second->Stop();
        } catch (const std::exception &e) {
            std::cerr << "UbseContext::StopModule-Error: stopping module " << it->first << ": " << e.what()
                      << std::endl;
        }
        std::cout << "UbseContext::StopModule-Module: " << it->first << std::endl;
    }
}

void UbseContext::DestroyCoreModules()
{
    std::cout << "UbseContext::Destroy-CORE" << std::endl;
    for (auto it = sortedModules_.rbegin(); it != sortedModules_.rend(); ++it) {
        auto entryIt = registry_.find(it->first);
        if (entryIt == registry_.end() || entryIt->second.category != UbseModuleCategory::CORE) {
            continue;
        }
        try {
            it->second->UnInitialize();
        } catch (const std::exception &e) {
            std::cerr << "UbseContext::Stop-Error: destroying module " << it->first << ": " << e.what() << std::endl;
        }
        std::cout << "UbseContext::DestroyModule-Module: " << it->first << std::endl;
    }
}

void UbseContext::Stop()
{
    allModulesReady_.store(false);
    g_globalStop.store(true);
    std::cout << "UbseContext::Stop-start, allModulesReady=" << allModulesReady_.load()
              << ", globalStop=" << g_globalStop.load() << std::endl;

    StopNonCoreModules();
    DestroyNonCoreModules();
    UbsePluginLoader::GetInstance().UnloadAll();
    StopCoreModules();
    DestroyCoreModules();

    sortedModules_.clear();
    registry_.clear();
    activated_.clear();
    moduleMap_.clear();
    std::cout << "UbseContext::Stop-end" << std::endl;
}

UbseResult UbseContext::RegisterArg()
{
    std::cout << "UbseContext::RegisterArg-start" << std::endl;
    for (const auto &[name, module] : moduleMap_) {
        try {
            module->RegArgs();
        } catch (const std::exception &e) {
            std::cerr << "UbseContext::RegisterArg-Error: registering arguments for module " << name << ": " << e.what()
                      << std::endl;
            return UBSE_ERROR_PARSE_ARGS_FAILED;
        }
    }
    std::cout << "UbseContext::RegisterArg-end" << std::endl;
    return UBSE_OK;
}

UbseResult UbseContext::ParserArgs(int argc, char *argv[])
{
    int i = 1;
    while (i < argc) {
        std::string arg = argv[i];
        if (arg[0] != '-') {
            std::cerr << "UbseContext::ParserArgs-Error: parsing arguments: " << arg << std::endl;
            return UBSE_ERROR_PARSE_ARGS_FAILED;
        }

        std::string key = arg.substr(1);
        if (key.empty()) {
            std::cerr << "UbseContext::ParserArgs-Error: parsing arguments " << arg << std::endl;
            return UBSE_ERROR_PARSE_ARGS_FAILED;
        }
        std::string value;
        if (i + 1 < argc && argv[i + 1][0] != '-') {
            value = argv[i + 1];
            ++i;
        }
        argMap_[key] = value;
        ++i;
    }
    std::cout << "UbseContext::ParserArgs-Args: ";
    for (const auto &it : argMap_) {
        std::cout << "[" << it.first << ": " << it.second << "] ";
    }
    std::cout << std::endl;
    return UBSE_OK;
}

UbseResult UbseContext::GetArgStr(const std::string &argName, std::string &argValue)
{
    auto it = argMap_.find(argName);
    if (it != argMap_.end()) {
        argValue = it->second;
        return UBSE_OK;
    }
    return UBSE_ERROR_PARSE_ARGS_FAILED;
}

UbseResult UbseContext::CreateModules()
{
    if (!sortedModules_.empty()) {
        return UBSE_OK;
    }
    registry_ = UbseModuleRegistry::GetInstance().TakeRegistry();
    std::vector<std::string> sorted{};
    try {
        // 解析激活：CORE 无条件激活，PLUGIN 无条件激活，OPTIONAL 按需激活
        ResolveActivation();
        // 拓扑排序：返回模块名称列表，CORE 在前，其余按依赖顺序
        sorted = TopologicalSort();
    } catch (const std::exception &e) {
        std::cerr << "UbseContext::CreateModules-Error: " << e.what() << std::endl;
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    // 按排序顺序创建模块实例
    for (const auto &name : sorted) {
        auto it = registry_.find(name);
        if (it == registry_.end() || !it->second.creator) {
            std::cerr << "UbseContext::CreateModules-Error: failed to create module: " << name << std::endl;
            return UBSE_ERROR_MODULE_LOAD_FAILED;
        }
        auto module = it->second.creator();
        if (!module) {
            std::cerr << "UbseContext::CreateModules-Error: failed to create module: " << name << std::endl;
            return UBSE_ERROR_MODULE_LOAD_FAILED;
        }
        sortedModules_.emplace_back(name, module);
        moduleMap_[name] = module;
    }

    std::cout << "UbseContext::CreateModules-ModuleList: ";
    for (const auto &[name, _] : sortedModules_) {
        std::cout << "[" << name << "] ";
    }
    std::cout << std::endl;

    return UBSE_OK;
}
void UbseContext::ResolveActivation()
{
    activated_.clear();

    for (const auto &[name, entry] : registry_) {
        if (entry.category == UbseModuleCategory::CORE) {
            ActivateWithDependencies(name);
        }
    }

    for (const auto &[name, entry] : registry_) {
        if (entry.category == UbseModuleCategory::PLUGIN) {
            ActivateWithDependencies(name);
        }
    }
}

void UbseContext::ActivateWithDependencies(const std::string &name)
{
    std::set<std::string> inStack;
    ActivateWithDependenciesImpl(name, inStack);
}

void UbseContext::ActivateWithDependenciesImpl(const std::string &name, std::set<std::string> &inStack)
{
    if (activated_.find(name) != activated_.end()) {
        return;
    }

    auto it = registry_.find(name);
    if (it == registry_.end()) {
        std::cerr << "[UbseContext] Warning: Module not found in registry: " << name << std::endl;
        return;
    }

    if (inStack.find(name) != inStack.end()) {
        std::string errorMsg = "[UbseContext] Error: Circular dependency detected at: " + name;
        std::cerr << errorMsg << std::endl;
        throw std::runtime_error(errorMsg);
    }

    inStack.insert(name);

    for (const auto &dep : it->second.dependencies) {
        ActivateWithDependenciesImpl(dep, inStack);
    }

    inStack.erase(name);

    activated_.insert(name);

    const char *catStr = "";
    switch (it->second.category) {
        case UbseModuleCategory::CORE:
            catStr = "CORE";
            break;
        case UbseModuleCategory::OPTIONAL:
            catStr = "OPTIONAL";
            break;
        case UbseModuleCategory::PLUGIN:
            catStr = "PLUGIN";
            break;
    }
    std::cout << "[UbseContext] Activated: " << name << " [" << catStr << "]" << std::endl;
}
std::vector<std::string> UbseContext::TopologicalSort()
{
    std::vector<std::string> result;
    std::set<std::string> visited;
    std::set<std::string> inStack;

    std::function<bool(const std::string &)> visit = [&](const std::string &name) -> bool {
        if (visited.find(name) != visited.end()) {
            return true;
        }
        if (inStack.find(name) != inStack.end()) {
            std::cerr << "[UbseContext] Error: Circular dependency detected at: " << name << std::endl;
            return false;
        }

        inStack.insert(name);

        auto it = registry_.find(name);
        if (it != registry_.end()) {
            for (const auto &dep : it->second.dependencies) {
                if (activated_.find(dep) == activated_.end()) {
                    std::cerr << "UbseContext::CreateModules-Error: " << dep << " which is a dependency of " << name
                              << " is not registered" << std::endl;
                    throw std::runtime_error("Dependency not registered");
                }
                if (!visit(dep)) {
                    return false;
                }
            }
        }

        inStack.erase(name);
        visited.insert(name);
        result.push_back(name);
        return true;
    };

    for (const auto &name : activated_) {
        if (!visit(name)) {
            return {};
        }
    }

    std::vector<std::string> coreModules;
    std::vector<std::string> otherModules;
    for (const auto &name : result) {
        auto it = registry_.find(name);
        if (it != registry_.end() && it->second.category == UbseModuleCategory::CORE) {
            coreModules.push_back(name);
        } else {
            otherModules.push_back(name);
        }
    }

    result.clear();
    result.insert(result.end(), coreModules.begin(), coreModules.end());
    result.insert(result.end(), otherModules.begin(), otherModules.end());

    return result;
}
ProcessMode UbseContext::GetProcessMode() const
{
    if (processMode_ == ProcessMode::DEFAULT) {
        return ProcessMode::MANAGER;
    }
    return processMode_;
}

void UbseContext::SetProcessMode(ProcessMode mode)
{
    if (processMode_ != ProcessMode::DEFAULT) {
        return;
    }
    processMode_ = mode;
}

UbseResult UbseContext::GetExecutablePath()
{
    char result[PATH_MAX] = {0};
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    if (count == -1) {
        // 读取符号链接失败
        std::cerr << "UbseContext::GetExecutablePath-Error: readlink failed" << std::endl;
        return UBSE_ERROR;
    }
    std::string executablePath = std::string(result, count);
    // 规范化路径
    std::error_code ec;
    auto canonPath = std::filesystem::canonical(executablePath, ec);
    if (ec) {
        std::cerr << "Invalid path: " << executablePath << " - " << ec.message() << std::endl;
        return UBSE_ERROR;
    }
    // 检查是否为常规文件
    if (!std::filesystem::is_regular_file(canonPath)) {
        std::cerr << "Error: " << canonPath << " is not a regular file" << std::endl;
        return UBSE_ERROR;
    }
    auto parentPath = canonPath.parent_path();
    if (parentPath.empty()) { // 处理根目录情况
        ubseRunPath_ = "/";
    } else {
        ubseRunPath_ = parentPath.string();
    }

    std::cout << "UbseContext::GetExecutablePath-RunPath: " << ubseRunPath_ << std::endl;
    return UBSE_OK;
}

std::string UbseContext::GetUbseRunPath() const
{
    return ubseRunPath_;
}

uint8_t UbseContext::GetWorkReadiness() const
{
    return workReadiness_;
}

UbseResult UbseContext::SetWorkReadiness(uint8_t currentStatus)
{
    if (currentStatus > 1) {
        std::cerr << "UbseContext::SetWorkReadiness-Error: invalid status: " << currentStatus << std::endl;
        return UBSE_ERROR;
    }
    workReadiness_ = currentStatus;
    return UBSE_OK;
}

bool UbseContext::IsAllModulesReady() const
{
    return allModulesReady_.load();
}
} // namespace ubse::context
