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

#ifndef UBSE_CLI_URMA_CMD_REG_H
#define UBSE_CLI_URMA_CMD_REG_H

#include "ubse_cli_reg.h"
#include "ubse_cli_reg_builder.h"
#include "ubse_serial_util.h"

namespace ubse::cli::reg {
using namespace ubse::cli::framework;
using namespace ubse::serial;
class UbseCliRegUrmaModule : public UbseCliRegModule {
public:
    void UbseCliSignUp() override;

private:
    static UbseCliCommandInfo UbseCliCreateUrmaQos();
    static UbseCliCommandInfo UbseCliDeleteUrmaQos();
    static UbseCliCommandInfo UbseCliDisplayUrmaQos();
    static std::shared_ptr<UbseCliResultEcho> UbseCreateUrmaQosFunc(
        [[maybe_unused]] const std::map<std::string, std::string> &params);
    static std::shared_ptr<UbseCliResultEcho> UbseDeleteUrmaQosFunc(
        [[maybe_unused]] const std::map<std::string, std::string> &params);
    static std::shared_ptr<UbseCliResultEcho> UbseDisplayUrmaQosFunc(
        [[maybe_unused]] const std::map<std::string, std::string> &params);
    static std::shared_ptr<UbseCliResultEcho> UbseCliProcessUrmaQosTable(UbseDeSerialization &ubseDeSerial,
                                                                         uint32_t urmaSize);
    static bool IsPositiveInteger(const std::string &str);
    static std::shared_ptr<UbseCliResultEcho> ValidateSingleQosParam(
        const std::string &priToken, const std::string &cirToken,
        uint32_t &priority, uint32_t &bandwidth);
    static std::shared_ptr<UbseCliResultEcho> MapQosErrorToMessage(uint32_t ret);
    static std::shared_ptr<UbseCliResultEcho> ParseAndValidateQosParams(
        const std::string &priStr, const std::string &cirStr,
        std::vector<uint32_t> &priorities, std::vector<uint32_t> &bandwidths);
    static UbseCliCommandInfo UbseCliQueryUrmaDevInfo();
    static std::vector<std::string> ParseCommaSeparatedDeviceList(const std::string &deviceStr);
    static std::shared_ptr<UbseCliResultEcho> ParseAndValidateUrmaParams(
        const std::map<std::string, std::string> &params, uint32_t &nodeId, std::vector<std::string> &deviceNameList);
    static std::shared_ptr<UbseCliResultEcho> UbseQueryUrmaDevInfoFunc(
        [[maybe_unused]] const std::map<std::string, std::string> &params);
    static std::shared_ptr<UbseCliResultEcho> UbseCliProcessUrmaDevInfoTable(UbseDeSerialization &ubseDeSerial,
                                                                             uint32_t urmaSize);
};
}  // namespace ubse::cli::reg

#endif