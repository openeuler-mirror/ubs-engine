/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#ifndef UBSE_MEM_SEI_DEGRADE_H
#define UBSE_MEM_SEI_DEGRADE_H

#include <mutex>

#include "ubse_common_def.h"

namespace ubse::mem::controller {
using common::def::UbseResult;

class UbseMemSeiDegradeManager {
public:
    static UbseMemSeiDegradeManager& GetInstance()
    {
        static UbseMemSeiDegradeManager instance;
        return instance;
    }

    UbseMemSeiDegradeManager(const UbseMemSeiDegradeManager& other) = delete;
    UbseMemSeiDegradeManager(UbseMemSeiDegradeManager&& other) = delete;
    UbseMemSeiDegradeManager& operator=(const UbseMemSeiDegradeManager& other) = delete;
    UbseMemSeiDegradeManager& operator=(UbseMemSeiDegradeManager&& other) noexcept = delete;

    void Init();
    bool IsSeiEnabled() const
    {
        return seiEnable_;
    }
    void TryEnableSei();
    void TryDisableSei();
    void RecoverSeiState(); // 依赖 UbseMemSeiDegradeManager::Init() 已执行
    void Reset();

private:
    UbseMemSeiDegradeManager() = default;

    UbseResult WriteSeiFile(bool enable);
    size_t GetTotalImportCount();

    static constexpr const char* SEI_SYSCTL_CMD = "sudo /usr/sbin/sysctl -w kernel.arm64_sync_sei=";
    static constexpr int SEI_WRITE_MAX_RETRY = 3;
    static constexpr int SEI_WRITE_RETRY_DELAY_US = 50000;

    // seiEnable_ 仅 Init() 写入，其后仅读取，依赖 happens-before 保证可见性
    bool seiEnable_{false};
    bool seiOpened_{false};
    std::mutex seiOpMutex_;
};

void AsyncTryEnableSei();
void AsyncTryDisableSei();

} // namespace ubse::mem::controller

#endif // UBSE_MEM_SEI_DEGRADE_H
