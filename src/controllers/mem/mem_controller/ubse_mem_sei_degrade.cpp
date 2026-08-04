/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include "ubse_mem_sei_degrade.h"

#include <sys/wait.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

#include "ubse_conf.h"
#include "ubse_logger.h"
#include "ubse_mem_util.h"
#include "debt/ubse_mem_debt_ledger.h"

namespace ubse::mem::controller {
using namespace ubse::log;
using namespace ubse::config;
using namespace ubse::mem::util;
using namespace ubse::mem::controller::debt;

UBSE_DEFINE_THIS_MODULE("ubse");

void UbseMemSeiDegradeManager::Init()
{
    uint32_t ret = UbseGetBool("ubse.memory", "sei.enable", seiEnable_);
    UBSE_LOG_INFO << "[MEM_CONTROLLER] SEI degrade manager initialized, enable=" << (seiEnable_ ? "true" : "false");
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[MEM_CONTROLLER] Failed to read sei.enable config, ret=" << ret << ", using default: false";
    }
}

void UbseMemSeiDegradeManager::Reset()
{
    // UT调用,提供可测试性,业务代码未调用
    seiEnable_ = false;
    seiOpened_ = false;
}

void UbseMemSeiDegradeManager::TryEnableSei()
{
    std::lock_guard<std::mutex> lock(seiOpMutex_);
    if (seiOpened_) {
        return;
    }
    seiOpened_ = true;

    if (WriteSeiFile(true) == UBSE_OK) {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] SEI degrade enabled (write 1)";
    } else {
        seiOpened_ = false;
    }
}

void UbseMemSeiDegradeManager::TryDisableSei()
{
    std::lock_guard<std::mutex> lock(seiOpMutex_);
    if (!seiOpened_) {
        return;
    }

    size_t count = GetTotalImportCount();
    if (count > 0) {
        return;
    }

    if (WriteSeiFile(false) == UBSE_OK) {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] SEI degrade disabled (write 0)";
        seiOpened_ = false;
    }
}

// RecoverSeiState 仅进程恢复时调用（LoadLocalAllObjs 回调），此时单线程初始化无并发。
void UbseMemSeiDegradeManager::RecoverSeiState()
{
    if (!seiEnable_) {
        return;
    }

    size_t count = GetTotalImportCount();
    bool shouldEnable = (count > 0);

    if (WriteSeiFile(shouldEnable) == UBSE_OK) {
        seiOpened_ = shouldEnable;
        UBSE_LOG_INFO << "[MEM_CONTROLLER] SEI degrade state recovered, count=" << count
                      << ", state=" << (shouldEnable ? "enabled" : "disabled");
    }
}

UbseResult UbseMemSeiDegradeManager::WriteSeiFile(bool enable)
{
    const char* value = enable ? "1" : "0"; // 仅可为 "0" 或 "1"
    std::string cmd = std::string(SEI_SYSCTL_CMD) + value + " 2>&1";

    for (int retry = 0; retry < SEI_WRITE_MAX_RETRY; ++retry) {
        FILE* pipe = popen(cmd.c_str(), "r");
        if (pipe == nullptr) {
            UBSE_LOG_WARN << "[MEM_CONTROLLER] popen sysctl failed, retry=" << retry + 1 << "/" << SEI_WRITE_MAX_RETRY
                          << ", errno=" << strerror(errno);
            std::this_thread::sleep_for(std::chrono::microseconds(SEI_WRITE_RETRY_DELAY_US));
            continue;
        }

        char buf[128];
        std::string output;
        while (fgets(buf, sizeof(buf), pipe) != nullptr) {
            output += buf;
        }

        int status = pclose(pipe);
        if (status == 0) {
            UBSE_LOG_INFO << "[MEM_CONTROLLER] Write arm64_sync_sei success via sysctl, value=" << value
                          << ", retry=" << retry + 1;
            return UBSE_OK;
        }

        int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        if (retry < SEI_WRITE_MAX_RETRY - 1) {
            UBSE_LOG_WARN << "[MEM_CONTROLLER] Failed to write arm64_sync_sei via sysctl, retry=" << retry + 1 << "/"
                          << SEI_WRITE_MAX_RETRY << ", exit=" << exitCode << ", output=" << output;
            std::this_thread::sleep_for(std::chrono::microseconds(SEI_WRITE_RETRY_DELAY_US));
        } else {
            UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to write arm64_sync_sei after " << SEI_WRITE_MAX_RETRY
                           << " retries via sysctl, value=" << value << ", exit=" << exitCode << ", output=" << output;
        }
    }

    return UBSE_ERROR;
}

size_t UbseMemSeiDegradeManager::GetTotalImportCount()
{
    std::string curNodeId = GetCurNodeId();
    return UbseMemDebtLedger::GetInstance().GetNodeImportCount(curNodeId);
}

void AsyncTryEnableSei()
{
    if (!UbseMemSeiDegradeManager::GetInstance().IsSeiEnabled()) {
        return;
    }
    auto resourceExecutor = util::GetExecutor("ubseSeiExecutor");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_WARN << "[MEM_CONTROLLER] GetExecutor null, SEI enable fallback sync";
        UbseMemSeiDegradeManager::GetInstance().TryEnableSei();
        return;
    }
    // 内存借用性能敏感,sei耗时长且执行结果不影响借用流程,使用异步执行,不阻塞主流程;执行线程数为1,保证FIFO顺序,避免SEI状态与实际导入数不一致
    if (!resourceExecutor->Execute([]() { UbseMemSeiDegradeManager::GetInstance().TryEnableSei(); })) {
        UBSE_LOG_WARN << "[MEM_CONTROLLER] SEI enable task enqueue failed, fallback sync";
        UbseMemSeiDegradeManager::GetInstance().TryEnableSei();
    }
}

void AsyncTryDisableSei()
{
    if (!UbseMemSeiDegradeManager::GetInstance().IsSeiEnabled()) {
        return;
    }
    auto resourceExecutor = util::GetExecutor("ubseSeiExecutor");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_WARN << "[MEM_CONTROLLER] GetExecutor null, SEI disable fallback sync";
        UbseMemSeiDegradeManager::GetInstance().TryDisableSei();
        return;
    }
    if (!resourceExecutor->Execute([]() { UbseMemSeiDegradeManager::GetInstance().TryDisableSei(); })) {
        UBSE_LOG_WARN << "[MEM_CONTROLLER] SEI disable task enqueue failed, fallback sync";
        UbseMemSeiDegradeManager::GetInstance().TryDisableSei();
    }
}

} // namespace ubse::mem::controller
