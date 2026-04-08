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
#include <optional>
#include "ubse_error.h"
#include "ubse_env_util.h"
namespace ubse::context {

std::atomic<bool> g_globalStop{false};
std::condition_variable_any g_globalCv;

SceneType GetSceneType()
{
    static std::optional<SceneType> sceneType;
    if (!sceneType.has_value()) {
        auto kindArg = ubse::utils::GetEnv<std::string>("SCENE_TYPE", {});
        sceneType = kindArg == "ai" ? SceneType::AI : SceneType::COMMON;
    }
    return *sceneType;
}
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

    // 创建模块
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

UbseResult UbseContext::InitAndStartModule()
{
    auto ret = InitModule(sortedBaseModules_);
    if (ret != UBSE_OK) {
        std::cerr << "UbseContext::InitBaseModule-Error: initializing base module failed." << std::endl;
        return ret;
    }
    ret = StartModule(sortedBaseModules_);
    if (ret != UBSE_OK) {
        std::cerr << "UbseContext::StartBaseModule-Error: starting base module failed." << std::endl;
        return ret;
    }

    ret = InitModule(sortedModules_);
    if (ret != UBSE_OK) {
        std::cerr << "UbseContext::InitModule-Error: initializing module failed." << std::endl;
        return ret;
    }
    ret = StartModule(sortedModules_);
    if (ret != UBSE_OK) {
        std::cerr << "UbseContext::StartModule-Error: starting module failed." << std::endl;
        return ret;
    }
    return UBSE_OK;
}

UbseResult UbseContext::InitModule(
    std::vector<std::pair<std::type_index, std::shared_ptr<UbseModule>>> &sortedModuleVec)
{
    std::cout << "UbseContext::InitModule-start" << std::endl;
    auto startTime = std::chrono::system_clock::now();
    auto moduleStartTime = std::chrono::system_clock::now();
    for (const auto &it : sortedModuleVec) {
        if (!g_globalStop.load()) {
            auto ret = ubse::context::InitModule(Demangle(it.first.name()), it.second, moduleStartTime);
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

UbseResult UbseContext::StartModule(
    std::vector<std::pair<std::type_index, std::shared_ptr<UbseModule>>> &sortedModuleVec)
{
    std::cout << "UbseContext::StartModule-start" << std::endl;
    auto startTime = std::chrono::system_clock::now();
    auto moduleStartTime = startTime;
    for (const auto &it : sortedModuleVec) {
        if (!g_globalStop.load()) {
            auto ret = ubse::context::StartModule(Demangle(it.first.name()), it.second, moduleStartTime);
            if (ret != UBSE_OK) {
                return ret;
            }
            moduleMap_[it.first] = it.second;
        }
    }
    auto endTime = std::chrono::system_clock::now();
    std::cout << "UbseContext::StartModule-end. Total start time: " << CountDuration(startTime, endTime) << "ms"
              << std::endl;
    return UBSE_OK;
}

UbseResult UbseContext::StopModule(
    std::vector<std::pair<std::type_index, std::shared_ptr<UbseModule>>> &sortedModuleVec)
{
    std::cout << "UbseContext::StopModule-start" << std::endl;
    for (auto it = sortedModuleVec.rbegin(); it != sortedModuleVec.rend(); ++it) {
        try {
            moduleMap_.erase(it->first);
            it->second->Stop();
        } catch (const std::exception &e) {
            std::cerr << "UbseContext::StopModule-Error: stopping module " << Demangle(it->first.name()) << ": "
                      << e.what() << std::endl;
        }
        std::cout << "UbseContext::StopModule-Module: " << Demangle(it->first.name()) << std::endl;
    }
    std::cout << "UbseContext::StopModule-end" << std::endl;
    return UBSE_OK;
}

UbseResult UbseContext::DestroyModule(
    std::vector<std::pair<std::type_index, std::shared_ptr<UbseModule>>> &sortedModuleVec)
{
    std::cout << "UbseContext::DestroyModule-start" << std::endl;
    for (auto it = sortedModuleVec.rbegin(); it != sortedModuleVec.rend(); ++it) {
        try {
            it->second->UnInitialize();
        } catch (const std::exception &e) {
            std::cerr << "UbseContext::DestroyModule-Error: destroying module " << Demangle(it->first.name()) << ": "
                      << e.what() << std::endl;
        }
        std::cout << "UbseContext::DestroyModule-Module: " << Demangle(it->first.name()) << std::endl;
    }
    sortedModuleVec.clear();
    std::cout << "UbseContext::DestroyModule-end" << std::endl;
    return UBSE_OK;
}

void UbseContext::Stop()
{
    allModulesReady_.store(false);
    g_globalStop.store(true);
    std::cout << "UbseContext::Stop-start, allModulesReady=" << allModulesReady_.load()
              << ", globalStop=" << g_globalStop.load() << std::endl;
    // 停止业务模块
    (void)StopModule(sortedModules_);
    (void)DestroyModule(sortedModules_);

    // 停止基础模块
    (void)StopModule(sortedBaseModules_);
    (void)DestroyModule(sortedBaseModules_);
    std::cout << "UbseContext::Stop-end" << std::endl;
}

UbseResult UbseContext::RegisterArg()
{
    std::cout << "UbseContext::RegisterArg-start" << std::endl;
    for (const auto &it : moduleMap_) {
        try {
            it.second->RegArgs();
        } catch (const std::exception &e) {
            std::cerr << "UbseContext::RegisterArg-Error: registering arguments for module "
                      << Demangle(it.first.name()) << ": " << e.what() << std::endl;
            return UBSE_ERROR_PARSE_ARGS_FAILED;
        }
    }
    std::cout << "UbseContext::RegisterArg-end" << std::endl;
    return UBSE_OK;
}

UbseResult UbseContext::ParserArgs(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg[0] != '-') {
            std::cerr << "UbseContext::ParserArgs-Error: parsing arguments: " << arg << std::endl;
            return UBSE_ERROR_PARSE_ARGS_FAILED;
        }

        std::string key = arg.substr(1); // 去掉'-'，得到参数名
        if (key.empty()) {
            std::cerr << "UbseContext::ParserArgs-Error: parsing arguments " << arg << std::endl;
            return UBSE_ERROR_PARSE_ARGS_FAILED;
        }
        std::string value;
        if (i + 1 < argc && argv[i + 1][0] != '-') {
            value = argv[++i]; // 获取下一个非'-'的值
        }
        argMap_[key] = value;
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
    // 创建基础模块
    std::cout << "UbseContext::CreateBaseModules-start" << std::endl;
    UbseResult res = CreateModules(baseModuleCreatorMap_, sortedBaseModules_);
    if (res != UBSE_OK) {
        return res;
    }
    std::cout << "UbseContext::CreateBaseModules-end" << std::endl;

    // 创建业务模块
    std::cout << "UbseContext::CreateModules-start" << std::endl;
    res = CreateModules(moduleCreatorMap_, sortedModules_);
    if (res != UBSE_OK) {
        return res;
    }
    std::cout << "UbseContext::CreateModules-end" << std::endl;
    return UBSE_OK;
}

UbseResult UbseContext::CreateModules(
    const std::unordered_map<std::type_index, ModuleEntry> &creatorMap,
    std::vector<std::pair<std::type_index, std::shared_ptr<UbseModule>>> &sortedModuleVec)
{
    if (!sortedModuleVec.empty()) {
        return UBSE_OK;
    }
    std::vector<std::type_index> sortedModuleName;
    try {
        sortedModuleName = TopologicalSort(creatorMap);
    } catch (const std::exception &e) {
        std::cerr << "UbseContext::CreateModules-Error: " << e.what() << std::endl;
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    for (const std::type_index &moduleName : sortedModuleName) {
        auto creatorIt = creatorMap.find(moduleName);
        if (creatorIt == creatorMap.end()) {
            std::cerr << "UbseContext::CreateModules-Error: creator for module " << Demangle(moduleName.name())
                      << " not found" << std::endl;
        }
        try {
            auto module = creatorIt->second.creator();
            sortedModuleVec.emplace_back(moduleName, module);
            moduleMap_[moduleName] = module;
        } catch (const std::exception &e) {
            std::cerr << "UbseContext::CreateModules-Error: creating module " << Demangle(moduleName.name()) << ": "
                      << e.what() << std::endl;
            return UBSE_ERROR_MODULE_LOAD_FAILED;
        }
    }
    std::cout << "UbseContext::CreateModules-ModuleList: ";
    for (const auto &it : sortedModuleName) {
        std::cout << "[moduleName: " << Demangle(it.name()) << "] ";
    }
    std::cout << std::endl;
    return UBSE_OK;
}

std::vector<std::type_index> UbseContext::TopologicalSort(
    const std::unordered_map<std::type_index, ModuleEntry> &creatorMap)
{
    std::vector<std::type_index> sorted;
    std::unordered_set<std::type_index> visited;
    std::unordered_set<std::type_index> visiting;

    for (const auto &[name, entry] : creatorMap) {
        if (visited.find(name) == visited.end()) {
            if (!TopologicalSortUtil(creatorMap, name, visited, visiting, sorted)) {
                throw std::runtime_error("Cyclic dependency detected");
            }
        }
    }
    return sorted;
}

bool UbseContext::TopologicalSortUtil(const std::unordered_map<std::type_index, ModuleEntry> &creatorMap,
                                      const std::type_index &moduleName, std::unordered_set<std::type_index> &visited,
                                      std::unordered_set<std::type_index> &visiting,
                                      std::vector<std::type_index> &sorted)
{
    if (visiting.find(moduleName) != visiting.end()) {
        return false;
    }

    if (visited.find(moduleName) != visited.end()) {
        return true;
    }

    visiting.insert(moduleName);

    const auto &entry = creatorMap.at(moduleName);
    for (const auto &dependency : entry.dependencies) {
        if (creatorMap.find(dependency) == creatorMap.end()) {
            std::cerr << "UbseContext::CreateModules-Error: " << Demangle(dependency.name())
                      << " which is a dependency of " << Demangle(moduleName.name()) << " is not registered"
                      << std::endl;
            throw std::runtime_error("Dependency not registered");
        }
        if (!TopologicalSortUtil(creatorMap, dependency, visited, visiting, sorted)) {
            return false;
        }
    }

    visiting.erase(moduleName);
    visited.insert(moduleName);
    sorted.push_back(moduleName);

    return true;
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
