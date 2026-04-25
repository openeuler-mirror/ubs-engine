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

#include "test_ubse_storage_req_handler.h"
#include "mockcpp/mockcpp.hpp"
#include "ubse_error.h"
#include "ubse_storage_module.h"
#include "ubse_storage_req_handler.h"
#include "ubse_storage_req_simpo.h"
#include "ubse_storage_resp_simpo.h"

namespace ubse::ut::storage {
using namespace ubse::message;
using namespace ubse::storage;
using namespace ubse::storage::message;

void TestUbseStorageReqHandler::SetUp()
{
    Test::SetUp();
}

void TestUbseStorageReqHandler::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * 测试handler模块码和操作码
 * 测试步骤：
 * 1. 获取模块码
 * 2. 获取操作码
 * 预期结果：
 * 模块码和操作码符合预期
 */
TEST_F(TestUbseStorageReqHandler, GetCode)
{
    UbseStorageReqHandler handler;

    EXPECT_EQ(static_cast<uint16_t>(UbseModuleCode::STORAGE), handler.GetModuleCode());
    EXPECT_EQ(static_cast<uint16_t>(UbseStorageOpCode::STORAGE_REQ), handler.GetOpCode());
}

/*
 * 用例描述：
 * 测试请求消息转换失败
 * 测试步骤：
 * 1. 构造错误类型请求
 * 2. 调用Handle
 * 预期结果：
 * 返回UBSE_ERROR_NULLPTR
 */
TEST_F(TestUbseStorageReqHandler, HandleReqConvertFailed)
{
    UbseStorageReqHandler handler;
    UbseBaseMessagePtr req = new (std::nothrow) UbseStorageRespSimpo();
    UbseBaseMessagePtr rsp = new (std::nothrow) UbseStorageRespSimpo();

    EXPECT_EQ(UBSE_ERROR_NULLPTR, handler.Handle(req, rsp, nullptr));
}

/*
 * 用例描述：
 * 测试存储模块为空
 * 测试步骤：
 * 1. 构造合法请求和响应
 * 2. mock存储模块为空
 * 3. 调用Handle
 * 预期结果：
 * 返回UBSE_ERROR
 */
TEST_F(TestUbseStorageReqHandler, HandleStorageModuleNull)
{
    UbseStorageReqHandler handler;
    UbseStorageReq storageReq{ UbseStorageReqCmdType::GET, DEFAULT_DB_NAME, "key" };
    UbseBaseMessagePtr req = new (std::nothrow) UbseStorageReqSimpo(storageReq);
    UbseBaseMessagePtr rsp = new (std::nothrow) UbseStorageRespSimpo();
    std::shared_ptr<UbseStorageModule> nullModule = nullptr;

    MOCKER(&UbseStorageModule::GetStorageModule).stubs().will(returnValue(nullModule));

    EXPECT_EQ(UBSE_ERROR, handler.Handle(req, rsp, nullptr));
}

/*
 * 用例描述：
 * 测试响应消息转换失败
 * 测试步骤：
 * 1. 构造合法请求
 * 2. 构造错误类型响应
 * 3. 调用Handle
 * 预期结果：
 * 返回UBSE_ERROR_NULLPTR
 */
TEST_F(TestUbseStorageReqHandler, HandleRespConvertFailed)
{
    UbseStorageReqHandler handler;
    UbseStorageReq storageReq{ UbseStorageReqCmdType::GET, DEFAULT_DB_NAME, "key" };
    UbseBaseMessagePtr req = new (std::nothrow) UbseStorageReqSimpo(storageReq);
    UbseBaseMessagePtr rsp = new (std::nothrow) UbseStorageReqSimpo(storageReq);
    std::shared_ptr<UbseStorageModule> module = std::make_shared<UbseStorageModule>();

    MOCKER(&UbseStorageModule::GetStorageModule).stubs().will(returnValue(module));

    EXPECT_EQ(UBSE_ERROR_NULLPTR, handler.Handle(req, rsp, nullptr));
}

/*
 * 用例描述：
 * 测试未知请求类型
 * 测试步骤：
 * 1. 构造未知请求类型
 * 2. 调用Handle
 * 预期结果：
 * 返回UBSE_ERROR
 */
TEST_F(TestUbseStorageReqHandler, HandleUnknownCmdType)
{
    UbseStorageReqHandler handler;
    UbseStorageReq storageReq{ static_cast<UbseStorageReqCmdType>(3), DEFAULT_DB_NAME, "key" };
    UbseBaseMessagePtr req = new (std::nothrow) UbseStorageReqSimpo(storageReq);
    UbseBaseMessagePtr rsp = new (std::nothrow) UbseStorageRespSimpo();
    std::shared_ptr<UbseStorageModule> module = std::make_shared<UbseStorageModule>();

    MOCKER(&UbseStorageModule::GetStorageModule).stubs().will(returnValue(module));

    EXPECT_EQ(UBSE_ERROR, handler.Handle(req, rsp, nullptr));
}

/*
 * 用例描述：
 * 测试GET请求失败
 * 测试步骤：
 * 1. 构造GET请求
 * 2. mock Get失败
 * 3. 调用Handle
 * 预期结果：
 * 返回UBSE_ERROR
 */
TEST_F(TestUbseStorageReqHandler, HandleGetFailed)
{
    UbseStorageReqHandler handler;
    UbseStorageReq storageReq{ UbseStorageReqCmdType::GET, DEFAULT_DB_NAME, "key" };
    UbseBaseMessagePtr req = new (std::nothrow) UbseStorageReqSimpo(storageReq);
    UbseBaseMessagePtr rsp = new (std::nothrow) UbseStorageRespSimpo();
    std::shared_ptr<UbseStorageModule> module = std::make_shared<UbseStorageModule>();

    MOCKER(&UbseStorageModule::GetStorageModule).stubs().will(returnValue(module));
    MOCKER(&UbseStorageModule::Get).stubs().will(returnValue(UBSE_ERROR));

    EXPECT_EQ(UBSE_ERROR, handler.Handle(req, rsp, nullptr));
}

/*
 * 用例描述：
 * 测试GET请求成功
 * 测试步骤：
 * 1. 构造GET请求
 * 2. mock Get成功
 * 3. 调用Handle
 * 预期结果：
 * 返回UBSE_OK
 */
TEST_F(TestUbseStorageReqHandler, HandleGetSuccess)
{
    UbseStorageReqHandler handler;
    UbseStorageReq storageReq{ UbseStorageReqCmdType::GET, DEFAULT_DB_NAME, "key" };
    UbseBaseMessagePtr req = new (std::nothrow) UbseStorageReqSimpo(storageReq);
    UbseBaseMessagePtr rsp = new (std::nothrow) UbseStorageRespSimpo();
    std::shared_ptr<UbseStorageModule> module = std::make_shared<UbseStorageModule>();

    MOCKER(&UbseStorageModule::GetStorageModule).stubs().will(returnValue(module));
    MOCKER(&UbseStorageModule::Get).stubs().will(returnValue(UBSE_OK));

    EXPECT_EQ(UBSE_OK, handler.Handle(req, rsp, nullptr));

    auto resp = UbseBaseMessage::DeConvert<UbseStorageRespSimpo>(rsp);
    ASSERT_NE(nullptr, resp.Get());
    EXPECT_EQ(1, resp->GetStorageResp().kvs.size());
}
} // namespace ubse::ut::storage