/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_mem_sign_verifier.h"

#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_mem_sign_verifier.h"
#include "adapter_plugins/mmi/ubse_mmi_def.h"
#include "src/framework/vscok/ubse_vsock_client.h"

namespace ubse::mem_controller::ut {
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::mem::controller;
using namespace ubse::vsock;
using namespace ubse::adapter_plugins::mmi;

void TestUbseMemSignVerifier::SetUp()
{
    Test::SetUp();
}

void TestUbseMemSignVerifier::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemSignVerifier, Sign_Success)
{
    std::string type = "test_type";
    std::string signedData;
    std::string trustRingId;
    vsock::UbseSignRsp rsp;
    rsp.signedData = R"({"result":0,"signed_data":"test_signed_data","id":"test_trust_ring_id"})";
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(std::make_shared<UbseConfModule>()));
    MOCKER(&vsock::UbseVsockClient::UbseVsockSend).stubs().with(any(), outBound(rsp)).will(returnValue(UBSE_OK));
    auto ret = UbseMemSignVerifier::Sign(type, signedData, trustRingId);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemSignVerifier, Sign_VsockSendFail)
{
    std::string type = "test_type";
    std::string signedData;
    std::string trustRingId;
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(std::make_shared<UbseConfModule>()));
    MOCKER(&vsock::UbseVsockClient::UbseVsockSend).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbseMemSignVerifier::Sign(type, signedData, trustRingId);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemSignVerifier, SignAndVerify_Success)
{
    UbseExportSignReq signReq;
    signReq.type = "test_type";
    signReq.reqSignedData = "test_signed_data";
    signReq.trustRingId = "test_trust_ring_id";
    UbseMemObmmInfo obmmInfo;
    obmmInfo.desc.addr = 0x1000;
    obmmInfo.desc.length = 1024;
    obmmInfo.desc.tokenid = 1;
    signReq.exportObmmInfo.push_back(obmmInfo);
    std::vector<std::string> lendSignedDatas;
    vsock::UbseSignRsp rsp;
    rsp.signedData = R"({"result":0,"signed_data":"test_lend_signed_data","id":"test_trust_ring_id"})";
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(std::make_shared<UbseConfModule>()));
    MOCKER(&vsock::UbseVsockClient::UbseVsockSend).stubs().with(any(), outBound(rsp)).will(returnValue(UBSE_OK));
    auto ret = UbseMemSignVerifier::SignAndVerify(signReq, lendSignedDatas);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemSignVerifier, SignAndVerify_VsockSendFail)
{
    UbseExportSignReq signReq;
    signReq.type = "test_type";
    signReq.reqSignedData = "test_signed_data";
    signReq.trustRingId = "test_trust_ring_id";
    UbseMemObmmInfo obmmInfo;
    obmmInfo.desc.addr = 0x1000;
    obmmInfo.desc.length = 1024;
    obmmInfo.desc.tokenid = 1;
    signReq.exportObmmInfo.push_back(obmmInfo);
    std::vector<std::string> lendSignedDatas;
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(std::make_shared<UbseConfModule>()));
    MOCKER(&vsock::UbseVsockClient::UbseVsockSend).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbseMemSignVerifier::SignAndVerify(signReq, lendSignedDatas);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemSignVerifier, IsHighSafety_GetModuleNull)
{
    std::shared_ptr<UbseConfModule> nullModule;
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(nullModule));

    auto result = IsHighSafety();
    EXPECT_EQ(result, false);
}

TEST_F(TestUbseMemSignVerifier, IsHighSafety_GetConfFail)
{
    auto confModule = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(confModule));
    MOCKER(&UbseConfModule::GetConf<uint32_t>).stubs().will(returnValue(UBSE_ERROR));

    auto result = IsHighSafety();
    EXPECT_EQ(result, false);
}

TEST_F(TestUbseMemSignVerifier, IsHighSafety_Success)
{
    auto confModule = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(confModule));
    MOCKER(&UbseConfModule::GetConf<uint32_t>).stubs().will(returnValue(UBSE_OK));
    auto result = IsHighSafety();
    EXPECT_EQ(result, true);
}
} // namespace ubse::mem_controller::ut