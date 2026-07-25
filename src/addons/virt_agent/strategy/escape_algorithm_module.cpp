/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "escape_algorithm_module.h"
#include <dlfcn.h>
#include "vm_configuration.h"
namespace vm {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using namespace ubse::log;

void* EscapeAlgorithmModule::algorithmHandle = nullptr;
EscapeAlgorithmInitFunc EscapeAlgorithmModule::escapeAlgorithmInitFunc = nullptr;
EscapeAlgorithmFunc EscapeAlgorithmModule::escapeAlgorithmFunc = nullptr;

VmResult EscapeAlgorithmModule::Init()
{
    std::string fileName = VmConfiguration::GetInstance().GetEscapeAlgorithmDir();
    if (fileName[0] != '/') {
        UBSE_LOG_ERROR << "The path of escapeAlgorithm is not an absolute path.";
        return VM_ERROR;
    }
    algorithmHandle = dlopen(fileName.c_str(), RTLD_LAZY);
    if (algorithmHandle == nullptr) {
        UBSE_LOG_ERROR << "Failed to load " << fileName << ", error: " << dlerror();
        return VM_ERROR;
    }
    return VM_OK;
}

EscapeAlgorithmInitFunc EscapeAlgorithmModule::GetEscapeAlgorithmInit()
{
    if (escapeAlgorithmInitFunc != nullptr) {
        return escapeAlgorithmInitFunc;
    }
    escapeAlgorithmInitFunc = reinterpret_cast<EscapeAlgorithmInitFunc>(dlsym(algorithmHandle, "EscapeAlgorithmInit"));
    if (escapeAlgorithmInitFunc == nullptr) {
        UBSE_LOG_ERROR << "Failed to get EscapeAlgorithmInit ptr, " << dlerror();
        return nullptr;
    }
    return escapeAlgorithmInitFunc;
}

EscapeAlgorithmFunc EscapeAlgorithmModule::GetStrategyAlgorithm()
{
    if (escapeAlgorithmFunc != nullptr) {
        return escapeAlgorithmFunc;
    }
    escapeAlgorithmFunc = reinterpret_cast<EscapeAlgorithmFunc>(dlsym(algorithmHandle, "EscapeAlgorithm"));
    if (escapeAlgorithmFunc == nullptr) {
        UBSE_LOG_ERROR << "Failed to get EscapeAlgorithm ptr, " << dlerror();
        return nullptr;
    }
    return escapeAlgorithmFunc;
}

void EscapeAlgorithmModule::CloseStrategyHandle()
{
    if (algorithmHandle != nullptr && dlclose(algorithmHandle) != 0) {
        UBSE_LOG_ERROR << "Failed to close StrategyAlgorithmHandle, error: " << dlerror();
    }
    escapeAlgorithmInitFunc = nullptr;
    escapeAlgorithmFunc = nullptr;
    algorithmHandle = nullptr;
}

void EscapeAlgorithmModule::VmEscapeLog(int level, const char* str)
{
    if (str == nullptr) {
        str = "";
    }
    switch (level) {
        case static_cast<uint32_t>(UbseLogLevel::DEBUG):
            UBSE_LOG_DEBUG << str;
            break;
        case static_cast<uint32_t>(UbseLogLevel::INFO):
            UBSE_LOG_INFO << str;
            break;
        case static_cast<uint32_t>(UbseLogLevel::WARN):
            UBSE_LOG_WARN << str;
            break;
        case static_cast<uint32_t>(UbseLogLevel::ERROR):
        default:
            UBSE_LOG_ERROR << str;
    }
}
} // namespace vm