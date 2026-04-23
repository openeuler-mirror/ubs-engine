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
#include "escape_algorithm_helper.h"
#include "vm_error.h"
#include "vm_configuration.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using namespace ubse::log;

EscapeAlgorithmHelper &EscapeAlgorithmHelper::GetInstance()
{
    static EscapeAlgorithmHelper gInstance;
    return gInstance;
}

VmResult EscapeAlgorithmHelper::Init()
{
    VmResult ret = EscapeAlgorithmModule::Init();
    if (ret != VM_OK) {
        return VM_ERROR;
    }
    EscapeAlgorithmInitFunc algorithmInitFunc = EscapeAlgorithmModule::GetEscapeAlgorithmInit();
    if (algorithmInitFunc == nullptr) {
        return VM_ERROR;
    }
    StrategyConfig strategyConf;
    GetStrategyConf(strategyConf);
    int res = algorithmInitFunc(strategyConf, EscapeAlgorithmModule::VmEscapeLog);
    if (res != 0) {
        UBSE_LOG_ERROR << "EscapeAlgorithmInit failed, " << FormatRetCode(res);
        return VM_ERROR;
    }
    return VM_OK;
}

void EscapeAlgorithmHelper::GetStrategyConf(StrategyConfig &strategyConf)
{
    auto &vmConfig = VmConfiguration::GetInstance();
    strategyConf.borrowWatermark = vmConfig.GetBorrowWatermark();
    strategyConf.lowWatermark = vmConfig.GetLowWatermark();
    strategyConf.highWatermark = vmConfig.GetHighWatermark();

    strategyConf.maxMemBorrow = vmConfig.GetMaxMemBorrow();
    strategyConf.maxMemPerBorrowBytes = vmConfig.GetMaxMemPerBorrowBytes();
    strategyConf.minMemPerBorrowBytes = vmConfig.GetMinMemPerBorrowBytes();
    strategyConf.maxPerTotalMemBorrowBytes = vmConfig.GetMaxPerTotalMemBorrowBytes();
    strategyConf.oomEventBorrowBytes = vmConfig.GetOomEventBorrowBytes();
}

void EscapeAlgorithmHelper::EscapeAlgorithmHandleClose()
{
    EscapeAlgorithmModule::CloseStrategyHandle();
}
} // namespace vm