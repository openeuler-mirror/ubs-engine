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

#include "test_ubse_urma_controller_api.h"
#include <netinet/in.h>
#include <securec.h>
#include "ubse_api_server_module.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_serial_util.h"
#include "ubse_urma_controller.h"
#include "ubse_urma_controller_api.h"
#include "ubse_urma_controller_rpc.h"

namespace ubse::urmaControllerApi::ut {
using namespace ubse::urmaController;
using namespace ubse::context;
using namespace api::server;
using namespace ubse::com;
using namespace ubse::election;
using namespace ubse::serial;

TEST_F(TestUbseUrmaControllerApi, Register)
{
    std::shared_ptr<UbseApiServerModule> nullModule = nullptr;
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module));
    EXPECT_EQ(UbseUrmaControllerApi::Register(), UBSE_ERROR_NULLPTR);

    MOCKER_CPP(&UbseApiServerModule::RegisterIpcHandler)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseUrmaControllerApi::Register(), UBSE_ERROR);
    EXPECT_EQ(UbseUrmaControllerApi::Register(), UBSE_OK);
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaBandWidthSet)
{
    UbseIpcMessage req;
    UbseRequestContext context;
    uint32_t minBandWidth = 80;
    uint32_t maxBandWidth = 160;
    req.length = sizeof("urma1") + sizeof(minBandWidth) + sizeof(maxBandWidth);
    req.buffer = (uint8_t *)malloc(req.length);
    uint8_t *buffer = req.buffer;
    strcpy_s((char *)buffer, req.length, "urma1");
    buffer += sizeof("urma1");
    *(uint32_t *)buffer = htonl(minBandWidth);
    buffer += sizeof(minBandWidth);
    *(uint32_t *)buffer = htonl(maxBandWidth);
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UrmaController::UbseUrmaBandWidthSet).stubs().will(returnValue(static_cast<uint32_t>(UBSE_OK)));
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(module));
    uint32_t ret = UbseUrmaControllerApi::UbseUrmaBandWidthSet(req, context);
    free(req.buffer);
    EXPECT_EQ(UBSE_OK, ret);
    GlobalMockObject::verify();
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaBandWidthSet_fail)
{
    UbseIpcMessage req;
    UbseRequestContext context;
    uint32_t minBandWidth = 80;
    uint32_t maxBandWidth = 160;
    req.length = sizeof("urma1") + sizeof(minBandWidth) + sizeof(maxBandWidth);
    req.buffer = (uint8_t *)malloc(req.length);
    uint8_t *buffer = req.buffer;
    strcpy_s((char *)buffer, req.length, "urma1");
    buffer += sizeof("urma1");
    *(uint32_t *)buffer = htonl(minBandWidth);
    buffer += sizeof(minBandWidth);
    *(uint32_t *)buffer = htonl(maxBandWidth);
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR));
    MOCKER(&UrmaController::UbseUrmaBandWidthSet).stubs().will(returnValue(static_cast<uint32_t>(UBSE_OK)));
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(module));
    uint32_t ret = UbseUrmaControllerApi::UbseUrmaBandWidthSet(req, context);
    EXPECT_EQ(UBSE_ERROR, ret);
    GlobalMockObject::verify();
    MOCKER(&UrmaController::UbseUrmaBandWidthSet).stubs().will(returnValue(static_cast<uint32_t>(UBSE_OK)));
    std::shared_ptr<UbseApiServerModule> nullModule = nullptr;
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(nullModule));
    ret = UbseUrmaControllerApi::UbseUrmaBandWidthSet(req, context);
    EXPECT_EQ(UBSE_ERROR_NULLPTR, ret);
    GlobalMockObject::verify();
    MOCKER(&UrmaController::UbseUrmaBandWidthSet).stubs().will(returnValue(static_cast<uint32_t>(UBSE_ERROR_NOT_EXIST)));
    ret = UbseUrmaControllerApi::UbseUrmaBandWidthSet(req, context);
    EXPECT_EQ(UBSE_ERROR_NOT_EXIST, ret);
    free(req.buffer);
    GlobalMockObject::verify();
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaBandWidthReset)
{
    UbseIpcMessage req;
    UbseRequestContext context;
    req.length = sizeof("urma1");
    req.buffer = (uint8_t *)"urma1";
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UrmaController::UbseUrmaBandWidthReset).stubs().will(returnValue(static_cast<uint32_t>(UBSE_OK)));
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(module));
    uint32_t ret = UbseUrmaControllerApi::UbseUrmaBandWidthReset(req, context);
    EXPECT_EQ(UBSE_OK, ret);
    GlobalMockObject::verify();
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaBandWidthReset_fail)
{
    UbseIpcMessage req;
    UbseRequestContext context;
    req.length = sizeof("urma1");
    req.buffer = (uint8_t *)"urma1";
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR));
    MOCKER(&UrmaController::UbseUrmaBandWidthReset).stubs().will(returnValue(static_cast<uint32_t>(UBSE_OK)));
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(module));
    uint32_t ret = UbseUrmaControllerApi::UbseUrmaBandWidthReset(req, context);
    EXPECT_EQ(UBSE_ERROR, ret);
    GlobalMockObject::verify();
    MOCKER(&UrmaController::UbseUrmaBandWidthReset).stubs().will(returnValue(static_cast<uint32_t>(UBSE_OK)));
    std::shared_ptr<UbseApiServerModule> nullModule = nullptr;
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(nullModule));
    ret = UbseUrmaControllerApi::UbseUrmaBandWidthReset(req, context);
    EXPECT_EQ(UBSE_ERROR_NULLPTR, ret);
    GlobalMockObject::verify();
    MOCKER(&UrmaController::UbseUrmaBandWidthReset).stubs().will(returnValue(static_cast<uint32_t>(UBSE_ERROR_NOT_EXIST)));
    ret = UbseUrmaControllerApi::UbseUrmaBandWidthReset(req, context);
    EXPECT_EQ(UBSE_ERROR_NOT_EXIST, ret);
    GlobalMockObject::verify();
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaBandWidthGet)
{
    UbseIpcMessage req;
    UbseRequestContext context;
    req.length = sizeof("urma1");
    req.buffer = (uint8_t *)malloc(req.length);
    uint8_t *buffer = req.buffer;
    strcpy_s((char *)buffer, req.length, "urma1");
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UrmaController::UbseUrmaBandWidthGet).stubs().will(returnValue(static_cast<uint32_t>(UBSE_OK)));
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(module));
    uint32_t ret = UbseUrmaControllerApi::UbseUrmaBandWidthGet(req, context);
    EXPECT_EQ(UBSE_OK, ret);
    free(req.buffer);
    GlobalMockObject::verify();
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaBandWidthGet_fail)
{
    UbseIpcMessage req;
    UbseRequestContext context;
    req.length = sizeof("urma1");
    req.buffer = (uint8_t *)malloc(req.length);
    uint8_t *buffer = req.buffer;
    strcpy_s((char *)buffer, req.length, "urma1");
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UrmaController::UbseUrmaBandWidthGet).stubs().will(returnValue(static_cast<uint32_t>(UBSE_OK)));
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(module));
    uint32_t ret = UbseUrmaControllerApi::UbseUrmaBandWidthGet(req, context);
    EXPECT_EQ(UBSE_OK, ret);
    GlobalMockObject::verify();
    MOCKER(&UrmaController::UbseUrmaBandWidthGet).stubs().will(returnValue(static_cast<uint32_t>(UBSE_OK)));
    std::shared_ptr<UbseApiServerModule> nullModule = nullptr;
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(nullModule));
    ret = UbseUrmaControllerApi::UbseUrmaBandWidthGet(req, context);
    EXPECT_EQ(UBSE_ERROR_NULLPTR, ret);
    GlobalMockObject::verify();
    MOCKER(&UrmaController::UbseUrmaBandWidthGet).stubs().will(returnValue(static_cast<uint32_t>(UBSE_ERROR_NOT_EXIST)));
    ret = UbseUrmaControllerApi::UbseUrmaBandWidthGet(req, context);
    EXPECT_EQ(UBSE_ERROR_NOT_EXIST, ret);
    free(req.buffer);
    GlobalMockObject::verify();
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaBandWidthCliGet)
{
    UbseRequestContext context;
    uint32_t nodeId = 0;
    UbseSerialization ubse_req_serial;
    ubse_req_serial << nodeId << "urma1";
    UbseIpcMessage req{ubse_req_serial.GetBuffer(), static_cast<uint32_t>(ubse_req_serial.GetLength())};
    UbseRoleInfo currentNodeInfo{};
    currentNodeInfo.nodeId = "0";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(currentNodeInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UrmaController::UbseUrmaBandWidthGet).stubs().will(returnValue(static_cast<uint32_t>(UBSE_OK)));
    std::shared_ptr<UbseApiServerModule> module = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(module));
    uint32_t ret = UbseUrmaControllerApi::UbseUrmaBandWidthCliGet(req, context);
    EXPECT_EQ(UBSE_OK, ret);
    GlobalMockObject::verify();
}

} // namespace ubse::urmaControllerApi::ut
