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

#include "test_ubse_urma_controller_api.h"
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "ubse_api_server_def.h"
#include "ubse_api_server_module.h"
#include "ubse_common_def.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_pack_util.h"
#include "ubse_serial_util.h"
#include "ubse_urma_controller.h"
#include "ubse_urma_controller_api.h"
#include "ubse_urma_controller_rpc.h"
#include "ubse_urma_def.h"

namespace ubse::urmaController {

size_t UbseStringCalcSize(const std::string& str, size_t maxLen);
UbseResult LocalDevPack(std::vector<std::string>& nameInfos, std::vector<uint32_t> status,
                        std::vector<uint64_t>& hwResIds, api::server::UbseIpcMessage& response);
uint32_t ParseUrmaDevGetRequest(const api::server::UbseIpcMessage& req, uint32_t& nodeId,
                                std::vector<std::string>& deviceNameList);
UbseResult AllocRspPack(ubse::urma::UbseUrmaDevPath& pathInfos, api::server::UbseIpcMessage& response);

} // namespace ubse::urmaController

namespace ubse::urmaControllerApi::ut {

using namespace ubse::urmaController;
using namespace ubse::urma;
using namespace ubse::serial;
using namespace ubse::context;
using namespace api::server;

TEST_F(TestUbseUrmaControllerApi, UbseStringCalcSize_NormalString)
{
    std::string str = "hello";
    size_t ret = UbseStringCalcSize(str, 32);
    EXPECT_EQ(ret, sizeof(uint32_t) + str.size());
}

TEST_F(TestUbseUrmaControllerApi, UbseStringCalcSize_EmptyString)
{
    std::string str;
    size_t ret = UbseStringCalcSize(str, 32);
    EXPECT_EQ(ret, sizeof(uint32_t));
}

TEST_F(TestUbseUrmaControllerApi, UbseStringCalcSize_ExceedsMax)
{
    std::string str = "hello world";
    size_t ret = UbseStringCalcSize(str, 5);
    EXPECT_EQ(ret, sizeof(uint32_t) + 5);
}

TEST_F(TestUbseUrmaControllerApi, LocalDevPack_SizeMismatch)
{
    std::vector<std::string> nameInfos = {"dev1"};
    std::vector<uint32_t> status;
    std::vector<uint64_t> hwResIds;
    UbseIpcMessage response = {nullptr, 0};

    auto ret = LocalDevPack(nameInfos, status, hwResIds, response);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseUrmaControllerApi, LocalDevPack_SuccessWithEmptyData)
{
    std::vector<std::string> nameInfos;
    std::vector<uint32_t> status;
    std::vector<uint64_t> hwResIds;
    UbseIpcMessage response = {nullptr, 0};

    auto ret = LocalDevPack(nameInfos, status, hwResIds, response);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_NE(response.buffer, nullptr);
    EXPECT_GT(response.length, 0);
    delete[] response.buffer;
}

TEST_F(TestUbseUrmaControllerApi, LocalDevPack_SuccessWithData)
{
    std::vector<std::string> nameInfos = {"urma_dev_0", "urma_dev_1"};
    std::vector<uint32_t> status = {0, 1};
    std::vector<uint64_t> hwResIds = {100, 200};
    UbseIpcMessage response = {nullptr, 0};

    auto ret = LocalDevPack(nameInfos, status, hwResIds, response);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_NE(response.buffer, nullptr);
    EXPECT_GT(response.length, 0);
    delete[] response.buffer;
}

TEST_F(TestUbseUrmaControllerApi, LocalDevPack_StatusSizeMismatch)
{
    std::vector<std::string> nameInfos = {"dev1"};
    std::vector<uint32_t> status = {0, 1};
    std::vector<uint64_t> hwResIds = {100};
    UbseIpcMessage response = {nullptr, 0};

    auto ret = LocalDevPack(nameInfos, status, hwResIds, response);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseUrmaControllerApi, ParseUrmaDevGetRequest_NullBuffer)
{
    UbseIpcMessage req = {nullptr, 0};
    uint32_t nodeId;
    std::vector<std::string> deviceNameList;

    auto ret = ParseUrmaDevGetRequest(req, nodeId, deviceNameList);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseUrmaControllerApi, ParseUrmaDevGetRequest_DeviceListTooLarge)
{
    UbseSerialization serial;
    uint32_t nodeId = 1;
    uint32_t deviceListSize = 2000;
    serial << nodeId << deviceListSize;
    UbseIpcMessage req = {serial.GetBuffer(), static_cast<uint32_t>(serial.GetLength())};

    uint32_t outNodeId;
    std::vector<std::string> deviceNameList;
    auto ret = ParseUrmaDevGetRequest(req, outNodeId, deviceNameList);
    EXPECT_EQ(ret, UBSE_ERROR_DESERIALIZE_FAILED);
}

TEST_F(TestUbseUrmaControllerApi, ParseUrmaDevGetRequest_Success)
{
    UbseSerialization serial;
    uint32_t nodeId = 5;
    uint32_t deviceListSize = 2;
    serial << nodeId << deviceListSize;
    serial << std::string("urma_dev_0") << std::string("urma_dev_1");
    UbseIpcMessage req = {serial.GetBuffer(), static_cast<uint32_t>(serial.GetLength())};

    uint32_t outNodeId;
    std::vector<std::string> deviceNameList;
    auto ret = ParseUrmaDevGetRequest(req, outNodeId, deviceNameList);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(outNodeId, 5);
    ASSERT_EQ(deviceNameList.size(), 2);
    EXPECT_EQ(deviceNameList[0], "urma_dev_0");
    EXPECT_EQ(deviceNameList[1], "urma_dev_1");
}

TEST_F(TestUbseUrmaControllerApi, ParseUrmaDevGetRequest_EmptyDeviceList)
{
    UbseSerialization serial;
    uint32_t nodeId = 3;
    uint32_t deviceListSize = 0;
    serial << nodeId << deviceListSize;
    UbseIpcMessage req = {serial.GetBuffer(), static_cast<uint32_t>(serial.GetLength())};

    uint32_t outNodeId;
    std::vector<std::string> deviceNameList;
    auto ret = ParseUrmaDevGetRequest(req, outNodeId, deviceNameList);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(outNodeId, 3);
    EXPECT_TRUE(deviceNameList.empty());
}

TEST_F(TestUbseUrmaControllerApi, AllocRspPack_VfePathCountMismatch)
{
    UbseUrmaDevPath pathInfos;
    pathInfos.vfePaths = {"path0"};
    pathInfos.bondingPath = "bond_path";
    pathInfos.bondingEid = "bond_eid";
    UbseIpcMessage response = {nullptr, 0};

    auto ret = AllocRspPack(pathInfos, response);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerApi, AllocRspPack_Success)
{
    UbseUrmaDevPath pathInfos;
    pathInfos.vfePaths = {"vfe_path_0", "vfe_path_1"};
    pathInfos.bondingPath = "bonding_path";
    pathInfos.bondingEid = "bonding_eid";
    UbseIpcMessage response = {nullptr, 0};

    auto ret = AllocRspPack(pathInfos, response);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_NE(response.buffer, nullptr);
    EXPECT_GT(response.length, 0);
    delete[] response.buffer;
}

TEST_F(TestUbseUrmaControllerApi, Register_NullApiServerModule)
{
    std::shared_ptr<UbseApiServerModule> nullMod;
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(nullMod));
    auto ret = UbseUrmaControllerApi::Register();
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseUrmaControllerApi, Register_RegisterIpcHandlerFails)
{
    auto apiServerModule = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiServerModule));
    MOCKER_CPP(&UbseApiServerModule::RegisterIpcHandler).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbseUrmaControllerApi::Register();
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerApi, Register_Success)
{
    auto apiServerModule = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiServerModule));
    MOCKER_CPP(&UbseApiServerModule::RegisterIpcHandler).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseUrmaControllerApi::Register();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaDevGet_NullBuffer)
{
    UbseIpcMessage req = {nullptr, 0};
    UbseRequestContext ctx = {};
    auto ret = UbseUrmaControllerApi::UbseUrmaDevGet(req, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaDevGet_GetLocalDevInfoFails)
{
    UbseSerialization serial;
    UbseIpcMessage req = {serial.GetBuffer(), static_cast<uint32_t>(serial.GetLength())};
    UbseRequestContext ctx = {};

    MOCKER_CPP(&UrmaController::UbseGetLocalUrmaDevInfo).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbseUrmaControllerApi::UbseUrmaDevGet(req, ctx);
    EXPECT_EQ(ret, UBSE_ERR_NOT_EXIST);
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaDevGet_NullApiServerModule)
{
    UbseSerialization serial;
    UbseIpcMessage req = {serial.GetBuffer(), static_cast<uint32_t>(serial.GetLength())};
    UbseRequestContext ctx = {};

    MOCKER_CPP(&UrmaController::UbseGetLocalUrmaDevInfo).stubs().will(returnValue(UBSE_OK));
    std::shared_ptr<UbseApiServerModule> nullMod;
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(nullMod));
    auto ret = UbseUrmaControllerApi::UbseUrmaDevGet(req, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaDevGet_Success)
{
    UbseSerialization serial;
    UbseIpcMessage req = {serial.GetBuffer(), static_cast<uint32_t>(serial.GetLength())};
    UbseRequestContext ctx = {};
    ctx.requestId = 42;

    auto apiServerModule = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UrmaController::UbseGetLocalUrmaDevInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiServerModule));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseUrmaControllerApi::UbseUrmaDevGet(req, ctx);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaDevFree_NullBuffer)
{
    UbseIpcMessage req = {nullptr, 0};
    UbseRequestContext ctx = {};
    auto ret = UbseUrmaControllerApi::UbseUrmaDevFree(req, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaDevFree_NameTooLong)
{
    std::string longName(33, 'x');
    auto buffer = new uint8_t[longName.size() + 1];
    memcpy(buffer, longName.c_str(), longName.size() + 1);
    UbseIpcMessage req = {buffer, static_cast<uint32_t>(longName.size() + 1)};
    UbseRequestContext ctx = {};

    auto ret = UbseUrmaControllerApi::UbseUrmaDevFree(req, ctx);
    EXPECT_EQ(ret, UBSE_ERROR);
    delete[] buffer;
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaDevFree_FreeDevFails)
{
    std::string devName("test_dev");
    auto buffer = new uint8_t[devName.size() + 1];
    memcpy(buffer, devName.c_str(), devName.size() + 1);
    UbseIpcMessage req = {buffer, static_cast<uint32_t>(devName.size() + 1)};
    UbseRequestContext ctx = {};

    MOCKER_CPP(&UrmaController::UbseFreeUrmaDev).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbseUrmaControllerApi::UbseUrmaDevFree(req, ctx);
    EXPECT_EQ(ret, UBSE_ERR_NOT_EXIST);
    delete[] buffer;
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaDevFree_NullApiServerModule)
{
    std::string devName("test_dev");
    auto buffer = new uint8_t[devName.size() + 1];
    memcpy(buffer, devName.c_str(), devName.size() + 1);
    UbseIpcMessage req = {buffer, static_cast<uint32_t>(devName.size() + 1)};
    UbseRequestContext ctx = {};

    MOCKER_CPP(&UrmaController::UbseFreeUrmaDev).stubs().will(returnValue(UBSE_OK));
    std::shared_ptr<UbseApiServerModule> nullMod;
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(nullMod));
    auto ret = UbseUrmaControllerApi::UbseUrmaDevFree(req, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
    delete[] buffer;
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaDevFree_Success)
{
    std::string devName("test_dev");
    auto buffer = new uint8_t[devName.size() + 1];
    memcpy(buffer, devName.c_str(), devName.size() + 1);
    UbseIpcMessage req = {buffer, static_cast<uint32_t>(devName.size() + 1)};
    UbseRequestContext ctx = {};
    ctx.requestId = 42;

    auto apiServerModule = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UrmaController::UbseFreeUrmaDev).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiServerModule));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseUrmaControllerApi::UbseUrmaDevFree(req, ctx);
    EXPECT_EQ(ret, UBSE_OK);
    delete[] buffer;
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaDevAlloc_NullBuffer)
{
    UbseIpcMessage req = {nullptr, 0};
    UbseRequestContext ctx = {};
    auto ret = UbseUrmaControllerApi::UbseUrmaDevAlloc(req, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaDevAlloc_NameTooLong)
{
    std::string longName(33, 'x');
    auto buffer = new uint8_t[longName.size() + 1];
    memcpy(buffer, longName.c_str(), longName.size() + 1);
    UbseIpcMessage req = {buffer, static_cast<uint32_t>(longName.size() + 1)};
    UbseRequestContext ctx = {};

    auto ret = UbseUrmaControllerApi::UbseUrmaDevAlloc(req, ctx);
    EXPECT_EQ(ret, UBSE_ERROR);
    delete[] buffer;
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaDevAlloc_AllocDevFails)
{
    std::string devName("test_dev");
    auto buffer = new uint8_t[devName.size() + 1];
    memcpy(buffer, devName.c_str(), devName.size() + 1);
    UbseIpcMessage req = {buffer, static_cast<uint32_t>(devName.size() + 1)};
    UbseRequestContext ctx = {};

    MOCKER_CPP(&UrmaController::UbseAllocUrmaDev).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbseUrmaControllerApi::UbseUrmaDevAlloc(req, ctx);
    EXPECT_EQ(ret, UBSE_ERR_NOT_EXIST);
    delete[] buffer;
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaDevAlloc_AllocRspPackFails)
{
    std::string devName("test_dev");
    auto buffer = new uint8_t[devName.size() + 1];
    memcpy(buffer, devName.c_str(), devName.size() + 1);
    UbseIpcMessage req = {buffer, static_cast<uint32_t>(devName.size() + 1)};
    UbseRequestContext ctx = {};

    auto apiServerModule = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UrmaController::UbseAllocUrmaDev).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiServerModule));
    MOCKER_CPP(AllocRspPack).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbseUrmaControllerApi::UbseUrmaDevAlloc(req, ctx);
    EXPECT_EQ(ret, UBSE_ERROR);
    delete[] buffer;
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaDevAlloc_NullApiServerModule)
{
    std::string devName("test_dev");
    auto buffer = new uint8_t[devName.size() + 1];
    memcpy(buffer, devName.c_str(), devName.size() + 1);
    UbseIpcMessage req = {buffer, static_cast<uint32_t>(devName.size() + 1)};
    UbseRequestContext ctx = {};

    MOCKER_CPP(&UrmaController::UbseAllocUrmaDev).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(AllocRspPack).stubs().will(returnValue(UBSE_OK));
    std::shared_ptr<UbseApiServerModule> nullMod;
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(nullMod));
    auto ret = UbseUrmaControllerApi::UbseUrmaDevAlloc(req, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
    delete[] buffer;
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaDevAlloc_SendResponseFails)
{
    std::string devName("test_dev");
    auto buffer = new uint8_t[devName.size() + 1];
    memcpy(buffer, devName.c_str(), devName.size() + 1);
    UbseIpcMessage req = {buffer, static_cast<uint32_t>(devName.size() + 1)};
    UbseRequestContext ctx = {};
    ctx.requestId = 42;

    auto apiServerModule = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UrmaController::UbseAllocUrmaDev).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(AllocRspPack).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiServerModule));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbseUrmaControllerApi::UbseUrmaDevAlloc(req, ctx);
    EXPECT_EQ(ret, UBSE_ERROR);
    delete[] buffer;
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaDevAlloc_Success)
{
    std::string devName("test_dev");
    auto buffer = new uint8_t[devName.size() + 1];
    memcpy(buffer, devName.c_str(), devName.size() + 1);
    UbseIpcMessage req = {buffer, static_cast<uint32_t>(devName.size() + 1)};
    UbseRequestContext ctx = {};
    ctx.requestId = 42;

    auto apiServerModule = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UrmaController::UbseAllocUrmaDev).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(AllocRspPack).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiServerModule));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseUrmaControllerApi::UbseUrmaDevAlloc(req, ctx);
    EXPECT_EQ(ret, UBSE_OK);
    delete[] buffer;
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaCliDevGet_NullBuffer)
{
    UbseIpcMessage req = {nullptr, 0};
    UbseRequestContext ctx = {};
    auto ret = UbseUrmaControllerApi::UbseUrmaCliDevGet(req, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaCliDevGet_QueryFails)
{
    UbseSerialization serial;
    uint32_t nodeId = 1;
    uint32_t deviceListSize = 0;
    serial << nodeId << deviceListSize;
    UbseIpcMessage req = {serial.GetBuffer(), static_cast<uint32_t>(serial.GetLength())};
    UbseRequestContext ctx = {};

    MOCKER_CPP(&UrmaController::UbseGetUrmaDevInfoByNodeId).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbseUrmaControllerApi::UbseUrmaCliDevGet(req, ctx);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaCliDevGet_Success)
{
    UbseSerialization serial;
    uint32_t nodeId = 1;
    uint32_t deviceListSize = 1;
    serial << nodeId << deviceListSize << std::string("urma_dev_0");
    UbseIpcMessage req = {serial.GetBuffer(), static_cast<uint32_t>(serial.GetLength())};
    UbseRequestContext ctx = {};
    ctx.requestId = 42;

    auto apiServerModule = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UrmaController::UbseGetUrmaDevInfoByNodeId).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiServerModule));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseUrmaControllerApi::UbseUrmaCliDevGet(req, ctx);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaCliDevGet_QueryFailsAfterParse)
{
    UbseSerialization serial;
    uint32_t nodeId = 1;
    uint32_t deviceListSize = 2;
    serial << nodeId << deviceListSize;
    serial << std::string("dev0") << std::string("dev1");
    UbseIpcMessage req = {serial.GetBuffer(), static_cast<uint32_t>(serial.GetLength())};
    UbseRequestContext ctx = {};

    MOCKER_CPP(&UrmaController::UbseGetUrmaDevInfoByNodeId).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbseUrmaControllerApi::UbseUrmaCliDevGet(req, ctx);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaCliDevActivate_NullBuffer)
{
    UbseIpcMessage req = {nullptr, 0};
    UbseRequestContext ctx = {};
    auto ret = UbseUrmaControllerApi::UbseUrmaCliDevActivate(req, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaCliDevActivate_DeserializeFails)
{
    auto buffer = new uint8_t[4]{};
    UbseIpcMessage req = {buffer, 4};
    UbseRequestContext ctx = {};

    auto ret = UbseUrmaControllerApi::UbseUrmaCliDevActivate(req, ctx);
    EXPECT_EQ(ret, UBSE_ERROR_DESERIALIZE_FAILED);
    delete[] buffer;
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaCliDevActivate_ActivateFails)
{
    UbseSerialization serial;
    serial << std::string("node_1") << std::string("urma_dev_0");
    UbseIpcMessage req = {serial.GetBuffer(), static_cast<uint32_t>(serial.GetLength())};
    UbseRequestContext ctx = {};

    MOCKER_CPP(&UrmaController::UbseUrmaCliDevActivate).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbseUrmaControllerApi::UbseUrmaCliDevActivate(req, ctx);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerApi, UbseUrmaCliDevActivate_Success)
{
    UbseSerialization serial;
    serial << std::string("node_1") << std::string("urma_dev_0");
    UbseIpcMessage req = {serial.GetBuffer(), static_cast<uint32_t>(serial.GetLength())};
    UbseRequestContext ctx = {};
    ctx.requestId = 42;

    auto apiServerModule = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UrmaController::UbseUrmaCliDevActivate).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiServerModule));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseUrmaControllerApi::UbseUrmaCliDevActivate(req, ctx);
    EXPECT_EQ(ret, UBSE_OK);
}

} // namespace ubse::urmaControllerApi::ut
