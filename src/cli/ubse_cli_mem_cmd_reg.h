/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_CLI_MEM_CMD_REG_H
#define UBSE_CLI_MEM_CMD_REG_H

#include "ubse_cli_reg.h"
#include "ubse_cli_reg_builder.h"
#include "ubse_serial_util.h"
namespace ubse::cli::reg {
using ubse::cli::framework::UbseCliCommandInfo;
using ubse::cli::framework::UbseCliResultEcho;
using ubse::serial::UbseDeSerialization;
class UbseCliRegMemModule : public UbseCliRegModule {
public:
    void UbseCliSignUp() override;

private:
    static UbseCliCommandInfo UbseCliQueryMem();
    static std::shared_ptr<UbseCliResultEcho> UbseCliMemQueryFunc(
        [[maybe_unused]] const std::map<std::string, std::string>& params);

    static UbseCliCommandInfo UbseCliCheckMemoryStatus();
    static std::shared_ptr<UbseCliResultEcho> UbseCliQueryNodeLendInfo();
    static std::shared_ptr<UbseCliResultEcho> UbseCliQueryNodeBorrowInfo();
    static std::shared_ptr<UbseCliResultEcho> UbseCliCheckMemoryStatusFunc(
        [[maybe_unused]] const std::map<std::string, std::string>& params);

    static std::shared_ptr<UbseCliResultEcho> UbseCliMemoryStatusData(UbseDeSerialization& ubse_de_serial, size_t size);
    static std::shared_ptr<UbseCliResultEcho> UbseCliProcessNumaStatusData(UbseDeSerialization& deSerialization,
                                                                           size_t numaInfoSize);
    static std::shared_ptr<UbseCliResultEcho> UbseCliQueryNumaStatus();
    static std::shared_ptr<UbseCliResultEcho> QueryMemConfig();

    UbseCliCommandInfo CreateMemory();

    UbseCliCommandInfo DeleteMemory();

    UbseCliCommandInfo ShmMemoryDetach();

    UbseCliCommandInfo ShmMemoryAttach();

    UbseCliCommandInfo ChangeMemory();

    UbseCliCommandInfo RemoveMemory();

    static UbseCliCommandInfo DisplayProcessMem();
    static std::shared_ptr<UbseCliResultEcho> DisplayProcessMemFunc(
        [[maybe_unused]] const std::map<std::string, std::string>& params);

    static std::shared_ptr<UbseCliResultEcho> DeleteMemoryFunc(const std::map<std::string, std::string>& params);

    static std::shared_ptr<UbseCliResultEcho> ShmMemoryAttachFunc(const std::map<std::string, std::string>& params);

    static std::shared_ptr<UbseCliResultEcho> ShmMemoryDetachFunc(const std::map<std::string, std::string>& params);

    static std::shared_ptr<UbseCliResultEcho> CreateMemoryFunc(const std::map<std::string, std::string>& params);

    static std::shared_ptr<UbseCliResultEcho> PidSetThresholdFunc(const std::map<std::string, std::string>& params);

    static std::shared_ptr<UbseCliResultEcho> PidUnSetFunc(const std::map<std::string, std::string>& params);
};
bool CheckName(const std::string& name);
} // namespace ubse::cli::reg
#endif