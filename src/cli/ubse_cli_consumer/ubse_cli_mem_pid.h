/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef UBSE_CLI_MEM_PID_H
#define UBSE_CLI_MEM_PID_H

#include <memory>
#include "ubse_cli_res_builder.h"
#include "process_mem_pid_manager_def.h"

namespace ubse::cli::reg {
class UbseCliMemPid {
public:
    UbseCliMemPid();
    ~UbseCliMemPid() noexcept;

    std::shared_ptr<framework::UbseCliResultEcho> UbseCliSetPidThreshold(
        const process_mem::def::ProcessMemPidInfo& pidInfo);
    std::shared_ptr<framework::UbseCliResultEcho> UbseCliPrintPidInfo();
    std::shared_ptr<framework::UbseCliResultEcho> UbseCliUnsetPid(pid_t pid);

private:
    class UbseCliMemPidImpl;
    std::unique_ptr<UbseCliMemPidImpl> pImpl_;
};
} // namespace ubse::cli::reg
#endif
