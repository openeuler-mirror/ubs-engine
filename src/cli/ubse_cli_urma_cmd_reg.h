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
    static UbseCliCommandInfo UbseCliQueryUrmaQos();
    static std::shared_ptr<UbseCliResultEcho> UbseQueryUrmaQosFunc(
        [[maybe_unused]] const std::map<std::string, std::string> &params);
    static std::shared_ptr<UbseCliResultEcho> UbseCliProcessUrmaQosTable(UbseDeSerialization &ubse_de_serial,
                                                                         uint32_t urma_size);
    static UbseCliCommandInfo UbseCliQueryUrmaDevInfo();
    static std::shared_ptr<UbseCliResultEcho> UbseCliRegUrmaModule::ParseAndValidateUrmaParams(
        const std::map<std::string, std::string> &params, uint32_t &nodeId, uint32_t &urmaType);
    static std::shared_ptr<UbseCliResultEcho> UbseQueryUrmaDevInfoFunc(
        [[maybe_unused]] const std::map<std::string, std::string> &params);
    static std::shared_ptr<UbseCliResultEcho> UbseCliProcessUrmaDevInfoTable(UbseDeSerialization &ubse_de_serial,
                                                                             uint32_t urma_size);
};
} // namespace ubse::cli::reg

#endif