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

#include "src/framework/plugin_mgr/ubse_plugin_loader.h"
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

UbseResult UbseContext::ProcessNonCoreModule()
{
    auto ret = CreatePluginModules();
    if (ret != UBSE_OK) {
        std::cerr << "UbseContext::Run-Error: create plugin modules failed" << std::endl;
        return ret;
    }
    ret = InitAndStartNonCoreModules();
    if (ret != UBSE_OK) {
        std::cerr << "UbseContext::Run-Error: init and start non core modules failed" << std::endl;
        return ret;
    }
    return UBSE_OK;
}
UbseResult UbseContext::ProcessingCoreModule()
{
    auto ret = CreateCoreModules();
    if (ret != UBSE_OK) {
        std::cerr << "UbseContext::Run-Error: create core modules failed" << std::endl;
        return ret;
    }

    ret = InitAndStartCoreModules();
    if (ret != UBSE_OK) {
        std::cerr << "UbseContext::Run-Error: init and start core modules failed" << std::endl;
        return ret;
    }
    return UBSE_OK;
}
UbseResult UbseContext::Run(int argc, char *argv[], ProcessMode mode)
{
    std::cout << "UbseContext::Run-start ProcessMode: " << static_cast<int>(mode) << std::endl;
    auto startTime = std::chrono::system_clock::now();
    this->cmdArgc_ = argc;
    this->cmdArgv_ = argv;
    UbseResult ret = GetExecutablePath();
    if (ret != UBSE_OK) {
        std::cerr << "UbseContext::Run-Error: get run path failed" << std::endl;
        return ret;
    }
    SetProcessMode(mode);
    if (GetProcessMode() == ProcessMode::MANAGER) {
        ret = ParserArgs(argc, argv);
        if (ret != UBSE_OK) {
            std::cerr << "ParserArgs failed" << std::endl;
            return ret;
        }
    }

    // CORE 模块处理
    std::cout << "UbseContext::Run-Phase1: Creating and starting CORE modules." << std::endl;
    ret = ProcessingCoreModule();
    if (ret != UBSE_OK) {
        std::cerr << "UbseContext::Run-Error: ProcessingCoreModule failed" << std::endl;
        return ret;
    }
    // 插件扫描
    std::cout << "UbseContext::Run-Phase2: Loading plugins with admission check." << std::endl;
    plugin::UbsePluginLoader::GetInstance().DiscoverAndLoad();

    // Non CORE 模块处理
    std::cout << "UbseContext::Run-Phase3: Creating and starting Non CORE modules." << std::endl;
    ret = ProcessNonCoreModule();
    if (ret != UBSE_OK) {
        std::cerr << "UbseContext::Run-Error: ProcessNonCoreModule failed" << std::endl;
        return ret;
    }

    ret = RegisterArg();
    if (ret != UBSE_OK) {
        std::cerr << "RegisterArg failed" << std::endl;
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
UbseResult UbseContext::InitNonCoreModules(std::chrono::time_point<std::chrono::system_clock> &moduleStartTime)
{
    std::cout << "UbseContext::InitNonCoreModules-NonCORE" << std::endl;
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
            std::cerr << "UbseContext::InitNonCoreModules-Error: initializing module " << name << std::endl;
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
UbseResult UbseContext::StartNonCoreModules(std::chrono::time_point<std::chrono::system_clock> &moduleStartTime)
{
    std::cout << "UbseContext::StartNonCoreModules-NonCORE" << std::endl;
    for (const auto &[name, module] : sortedModules_) {
        if (g_globalStop.load()) {
            break;
        }
        auto it = registry_.find(name);
        if (it == registry_.end() || it->second.category == UbseModuleCategory::CORE) {
            continue;
        }
        auto ret = ubse::context::StartModule(name, module, moduleStartTime);
        if (ret != UBSE_OK) {
            std::cerr << "UbseContext::StartNonCoreModules-Error: starting module " << name << std::endl;
            return ret;
        }
        moduleMap_[name] = module;
    }
    return UBSE_OK;
}

UbseResult UbseContext::InitAndStartCoreModules()
{
    std::cout << "UbseContext::InitAndStartCoreModules-start" << std::endl;
    auto startTime = std::chrono::system_clock::now();
    auto moduleStartTime = startTime;

    if (auto ret = InitCoreModules(moduleStartTime); ret != UBSE_OK) {
        return ret;
    }
    if (auto ret = StartCoreModules(moduleStartTime); ret != UBSE_OK) {
        return ret;
    }
    auto endTime = std::chrono::system_clock::now();
    std::cout << "UbseContext::InitAndStartCoreModules-end. Total time: " << CountDuration(startTime, endTime) << "ms"
              << std::endl;
    return UBSE_OK;
}
UbseResult UbseContext::InitAndStartNonCoreModules()
{
    std::cout << "UbseContext::InitAndStartNonCoreModules-start" << std::endl;
    auto startTime = std::chrono::system_clock::now();
    auto moduleStartTime = startTime;

    if (auto ret = InitNonCoreModules(moduleStartTime); ret != UBSE_OK) {
        return ret;
    }

    if (auto ret = StartNonCoreModules(moduleStartTime); ret != UBSE_OK) {
        return ret;
    }
    auto endTime = std::chrono::system_clock::now();
    std::cout << "UbseContext::InitAndStartNonCoreModules-end. Total time: " << CountDuration(startTime, endTime)
              << "ms" << std::endl;
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
    plugin::UbsePluginLoader::GetInstance().UnloadAll();
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
UbseResult UbseContext::CreateCoreModules()
{
    registry_ = UbseModuleRegistry::GetInstance().TakeRegistry();
    std::vector<std::string> sorted{};
    try {
        ResolveActivation(UbseModuleCategory::CORE);
        sorted = TopologicalSort();
    } catch (const std::exception &e) {
        std::cerr << "UbseContext::CreateCoreModules-Error: " << e.what() << std::endl;
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    for (const auto &name : sorted) {
        if (moduleMap_.find(name) != moduleMap_.end()) {
            std::cerr << "UbseContext::CreateCoreModules-Warning: module " << name << " already exists" << std::endl;
            continue;
        }
        auto it = registry_.find(name);
        if (it == registry_.end() || !it->second.creator) {
            std::cerr << "UbseContext::CreateCoreModules-Error: failed to create module: " << name << std::endl;
            return UBSE_ERROR_MODULE_LOAD_FAILED;
        }
        auto module = it->second.creator();
        if (!module) {
            std::cerr << "UbseContext::CreateCoreModules-Error: failed to create module: " << name << std::endl;
            return UBSE_ERROR_MODULE_LOAD_FAILED;
        }
        sortedModules_.emplace_back(name, module);
        moduleMap_[name] = module;
    }

    std::cout << "UbseContext::CreateCoreModules-ModuleList: ";
    for (const auto &[name, _] : sortedModules_) {
        std::cout << "[" << name << "] ";
    }
    std::cout << std::endl;

    return UBSE_OK;
}

UbseResult UbseContext::CreatePluginModules()
{
    auto pluginRegistry = UbseModuleRegistry::GetInstance().TakeRegistry();
    for (auto &[name, entry] : pluginRegistry) {
        registry_[name] = std::move(entry);
    }

    std::vector<std::string> sorted{};
    try {
        ResolveActivation(UbseModuleCategory::PLUGIN);
        sorted = TopologicalSort();
    } catch (const std::exception &e) {
        std::cerr << "UbseContext::CreatePluginModules-Error: " << e.what() << std::endl;
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    for (const auto &name : sorted) {
        if (moduleMap_.find(name) != moduleMap_.end()) {
            std::cerr << "UbseContext::CreatePluginModules-Warning: module " << name << " already exists" << std::endl;
            continue;
        }
        auto it = registry_.find(name);
        if (it == registry_.end() || !it->second.creator) {
            std::cerr << "UbseContext::CreatePluginModules-Error: failed to create module: " << name << std::endl;
            return UBSE_ERROR_MODULE_LOAD_FAILED;
        }
        auto module = it->second.creator();
        if (!module) {
            std::cerr << "UbseContext::CreatePluginModules-Error: failed to create module: " << name << std::endl;
            return UBSE_ERROR_MODULE_LOAD_FAILED;
        }
        sortedModules_.emplace_back(name, module);
        moduleMap_[name] = module;
    }

    std::cout << "UbseContext::CreatePluginModules-ModuleList: ";
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

void UbseContext::ResolveActivation(UbseModuleCategory category)
{
    if (category == UbseModuleCategory::CORE) {
        activated_.clear();
    }

    for (const auto &[name, entry] : registry_) {
        if (entry.category == category) {
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

    std::function<void(const std::string &)> visit = [&](const std::string &name) {
        if (visited.find(name) != visited.end()) {
            return;
        }

        auto it = registry_.find(name);
        if (it != registry_.end()) {
            for (const auto &dep : it->second.dependencies) {
                if (activated_.find(dep) == activated_.end()) {
                    std::cerr << "UbseContext::TopologicalSort-Error: " << dep << " which is a dependency of " << name
                              << " is not activated" << std::endl;
                    throw std::runtime_error("Dependency not activated");
                }
                visit(dep);
            }
        }

        visited.insert(name);
        result.push_back(name);
    };

    for (const auto &name : activated_) {
        visit(name);
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
