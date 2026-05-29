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

#include "test_ubse_cli_cert_cmd_reg.h"
#include <securec.h>
#include <mockcpp/mockcpp.hpp>
#include "ubse_cert_cli_import.h"
#include "ubse_cli_cert_cmd_reg.h"
#include "ubse_error.h"
#include "ubse_serial_util.h"
#include "test_mock_invoke.h"
namespace ubse::ut::cli {
using namespace ubse::cli::framework;
using namespace ubse::serial;
using namespace ubse::cli::reg;
using namespace ubse::cli::cert;
void TestUbseCliCertCmdReg::SetUp() {}

void TestUbseCliCertCmdReg::TearDown() {}

TEST_F(TestUbseCliCertCmdReg, RegisterCertModule)
{
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UBSE_CLI_REGISTER_MODULE("CLI_CERT_MODULE", UbseCliRegCertModule);
    EXPECT_EQ(UbseCliModuleRegistry::GetInstance().creators_.size(), 1);
    UbseCliModuleRegistry::GetInstance().UbseCliCallAllModuleSignUp();
    EXPECT_EQ(UbseCliModuleRegistry::GetInstance().creators_.size(), 0);
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
}

TEST_F(TestUbseCliCertCmdReg, CertImportNoParams)
{
    std::map<std::string, std::string> map = {{"server-cert-file", "/filename"}, {"ca-cert-file", "/filename"}};
    EXPECT_NO_THROW(UbseCliRegCertModule::UbseCliCertImportFunc(map)->UbseCliDisplayResult());
    std::map<std::string, std::string> map1 = {{"server-cert-file", "/filename"}, {"server-key-file", "/filename"}};
    EXPECT_NO_THROW(UbseCliRegCertModule::UbseCliCertImportFunc(map1)->UbseCliDisplayResult());
    std::map<std::string, std::string> map2 = {{"server-key-file", "/filename"}, {"ca-cert-file", "/filename"}};
    EXPECT_NO_THROW(UbseCliRegCertModule::UbseCliCertImportFunc(map2)->UbseCliDisplayResult());
}

TEST_F(TestUbseCliCertCmdReg, CertImportFailed)
{
    std::map<std::string, std::string> map = {
        {"server-cert-file", "/filename"}, {"server-key-file", "/filename"}, {"ca-cert-file", "/filename"}};
    MOCKER(&ImportCertSet).stubs().will(invoke(mock_import_cert_set_failed));
    EXPECT_NO_THROW(UbseCliRegCertModule::UbseCliCertImportFunc(map)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliCertCmdReg, CertImportSuccess)
{
    std::map<std::string, std::string> map = {
        {"server-cert-file", "/filename"}, {"server-key-file", "/filename"}, {"ca-cert-file", "/filename"}};
    MOCKER(&ImportCertSet).stubs().will(invoke(mock_import_cert_set_success));
    EXPECT_NO_THROW(UbseCliRegCertModule::UbseCliCaCrlImportFunc(map)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliCertCmdReg, CaCrlImportNoParams)
{
    EXPECT_NO_THROW(UbseCliRegCertModule::UbseCliCaCrlImportFunc({})->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliCertCmdReg, CaCrlImportFailed)
{
    std::map<std::string, std::string> map = {{"ca-crl-file", "/filename"}};
    MOCKER(&ImportCaCrl).stubs().will(invoke(mock_import_ca_set_failed));
    EXPECT_NO_THROW(UbseCliRegCertModule::UbseCliCaCrlImportFunc(map)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliCertCmdReg, CaCrlImportSuccess)
{
    std::map<std::string, std::string> map = {{"ca-crl-file", "/filename"}};
    MOCKER(&ImportCaCrl).stubs().will(invoke(mock_import_ca_set_success));
    EXPECT_NO_THROW(UbseCliRegCertModule::UbseCliCaCrlImportFunc(map)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliCertCmdReg, DeleteCertFailed)
{
    MOCKER(&DeleteCertSet).stubs().will(invoke(mock_delete_cert_failed));
    EXPECT_NO_THROW(UbseCliRegCertModule::UbseCliCertDeleteFunc({})->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliCertCmdReg, DeleteCertSuccess)
{
    MOCKER(&DeleteCertSet).stubs().will(invoke(mock_delete_cert_success));
    EXPECT_NO_THROW(UbseCliRegCertModule::UbseCliCertDeleteFunc({})->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}
} // namespace ubse::ut::cli
