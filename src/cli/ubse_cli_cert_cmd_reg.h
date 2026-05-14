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
#ifndef UBSE_CLI_CERT_CMD_REG_H
#define UBSE_CLI_CERT_CMD_REG_H

#include "ubse_cli_reg.h"
#include "ubse_cli_reg_builder.h"

namespace ubse::cli::reg {
using namespace ubse::cli::framework;
class UbseCliRegCertModule : public UbseCliRegModule {
public:
    void UbseCliSignUp() override;

private:
    UbseCliCommandInfo UbseCliChangeCaCrl();
    UbseCliCommandInfo UbseCliCreateCert();
    UbseCliCommandInfo UbseCliRemoveCert();
    static std::shared_ptr<UbseCliResultEcho> UbseCliCertImportFunc(
        [[maybe_unused]] const std::map<std::string, std::string>& params);
    static std::shared_ptr<UbseCliResultEcho> UbseCliCaCrlImportFunc(
        [[maybe_unused]] const std::map<std::string, std::string>& params);
    static std::shared_ptr<UbseCliResultEcho> UbseCliCertDeleteFunc(
        [[maybe_unused]] const std::map<std::string, std::string>& params);
};
} // namespace ubse::cli::reg
#endif
