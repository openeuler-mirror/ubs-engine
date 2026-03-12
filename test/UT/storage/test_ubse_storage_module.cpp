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

#include "test_ubse_storage_module.h"

#include <sys/stat.h>
#include <cstdlib>
#include <filesystem>
#include <mockcpp/mockcpp.hpp>

#include "ubse_storage_module.cpp"
#include "ubse_context.h"
#include "ubse_storage_req_simpo.h"
#include "ubse_storage_resp_simpo.h"

namespace ubse::ut::storage {
using namespace ubse::storage;
using namespace ubse::storage::message;

const std::string UBSE_DATA_PATH = "/var/lib/ubse/data";

void TestUbseStorageModule::SetUp()

{
    Test::SetUp();
    std::filesystem::create_directories(UBSE_DATA_PATH);
}

void TestUbseStorageModule::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
    std::filesystem::remove_all(UBSE_DATA_PATH);
}

TEST_F(TestUbseStorageModule, IsDirectoryExistsFalse)
{
    MOCKER(&stat).stubs().will(returnValue(1));
    EXPECT_EQ(false, IsDirectoryExists(""));
}

TEST_F(TestUbseStorageModule, CreateDirectory)
{
    EXPECT_EQ(UBSE_OK, CreateDirectory("/dir/subdir"));
    std::string command = "rm -rf /dir";
    system(command.c_str());
}

TEST_F(TestUbseStorageModule, CreateDirectoryMkdirFailFirst)
{
    MOCKER(&mkdir).stubs().will(returnValue(-1));
    EXPECT_EQ(UBSE_ERROR, CreateDirectory("/dir/subdir"));
}

TEST_F(TestUbseStorageModule, CreateDirectoryMkdirFailSecond)
{
    MOCKER(&mkdir).stubs().will(returnValue(-1));
    EXPECT_EQ(UBSE_ERROR, CreateDirectory("/"));
}

TEST_F(TestUbseStorageModule, CheckDirectoryPermissionStatFail)
{
    MOCKER(&stat).stubs().will(returnValue(1));
    EXPECT_EQ(false, CheckDirectoryPermission("/", DIR_MODE));
}

TEST_F(TestUbseStorageModule, InitializeFailWithCreateDirectoryFail)
{
    MOCKER(&IsDirectoryExists).stubs().will(returnValue(false));
    MOCKER(&CreateDirectory).stubs().will(returnValue(UBSE_ERROR));
    UbseStorageModule storageModule{};
    EXPECT_EQ(UBSE_ERROR, storageModule.Initialize());
}

TEST_F(TestUbseStorageModule, InitializeFailWithCheckDirectoryPermissonFail)
{
    MOCKER(&IsDirectoryExists).stubs().will(returnValue(true));
    MOCKER(&CheckDirectoryPermission).stubs().will(returnValue(false));
    UbseStorageModule storageModule{};
    EXPECT_EQ(UBSE_ERROR, storageModule.Initialize());
}

TEST_F(TestUbseStorageModule, UnInitializeOkWithCLI)
{
    UbseStorageModule storageModule{};
    storageModule.UnInitialize();
}

TEST_F(TestUbseStorageModule, StartFailWithRegRemoteReqHandler)
{
    MOCKER(&UbseStorageModule::Impl::RegRemoteReqHandler).stubs().will(returnValue(UBSE_ERROR));
    UbseStorageModule storageModule{};
    EXPECT_EQ(UBSE_ERROR, storageModule.Start());
}

TEST_F(TestUbseStorageModule, ResultFreeSuccess)
{
    UbseStorageModule storageModule{};
    KV kv{};
    std::vector<KV> kvVec;
    EXPECT_NO_THROW(storageModule.ResultFree(kv));
    EXPECT_NO_THROW(storageModule.ResultFree(kvVec));
}

/*
 * 用例描述：
 * 测试存储模块存储接口
 * 测试步骤：
   1. Put数据
   2. Put已存在Key
 * 预期结果：
 * 1. Put数据成功，并且Get出的数据一致
 * 2. Put数据成功，并且覆盖原有value
 */
TEST_F(TestUbseStorageModule, PutSuccess)
{
    std::string key = "key";
    std::string value = "value";
    auto ret = module->Put("default", key, reinterpret_cast<uint8_t *>(value.data()), value.size());
    ASSERT_EQ(UBSE_OK, ret);
    KV kv;
    ret = module->Get("default", key, kv);
    ASSERT_EQ(UBSE_OK, ret);
    std::string retValue = std::string(reinterpret_cast<char *>(kv.value), kv.valueLen);
    ASSERT_EQ(value, retValue);
    module->ResultFree(kv);

    std::string value1 = "value1";
    ret = module->Put("default", key, reinterpret_cast<uint8_t *>(value1.data()), value1.size());
    ASSERT_EQ(UBSE_OK, ret);
    ret = module->Get("default", key, kv);
    ASSERT_EQ(UBSE_OK, ret);
    std::string retValue1 = std::string(reinterpret_cast<char *>(kv.value), kv.valueLen);
    ASSERT_EQ(value1, retValue1);
    module->ResultFree(kv);

    ret = module->Delete("default", key);
    ASSERT_EQ(UBSE_OK, ret);
}

/*
 * 用例描述：
 * 测试存储模块查询接口
 * 测试步骤：
   1. 数据库客户端为空
   2. 数据库查询成功
 * 预期结果：
 * 1. UBSE_ERROR_NULLPTR
 * 2. UBSE_OK
 */
TEST_F(TestUbseStorageModule, GetSuccessWhenKeyNotExists)
{
    KV kv;
    auto ret = module->Get("default", "test", kv);
    ASSERT_EQ(UBSE_OK, ret);
    ASSERT_EQ(nullptr, kv.value);
    ASSERT_EQ(NO_0, kv.valueLen);
}

UbseResult TestHandler(const std::vector<KV> &kvs)
{
    for (auto kv : kvs) {
        std::string respStr(reinterpret_cast<char *>(kv.value), kv.valueLen);
        if (kv.key == "key") {
            EXPECT_EQ("value", respStr);
        }
    }
    return UBSE_OK;
}

/*
 * 用例描述：
 * 测试从主节点精确查询数据
 * 测试步骤：
   1. 查询指定key
 * 预期结果：
 * 查询成功，并且返回值符合预期
 */
TEST_F(TestUbseStorageModule, RemoteGet)
{
    std::string key = "key";
    std::string value = "value";
    MOCKER(&UbseStorageModule::GetStorageModule).stubs().will(returnValue(module));
    auto ret = module->Put("default", key, reinterpret_cast<uint8_t *>(value.data()), value.size());
    ASSERT_EQ(UBSE_OK, ret);
    MOCKER(&UbseStorageModule::Impl::RpcSend).stubs().will(returnValue(UBSE_OK));
    std::string masterNode = "1";
    MOCKER(&UbseStorageModule::Impl::GetMasterNode).stubs().will(returnValue(masterNode));
    ret = module->RemoteGet("default", "key", TestHandler);
    ASSERT_EQ(UBSE_OK, ret);
    ret = module->Delete("default", key);
    ASSERT_EQ(UBSE_OK, ret);
}

/*
 * 用例描述：
 * 测试注册是处理函数失败
 * 测试步骤：
   1. 调用注册处理函数接口
 * 预期结果：
 * 注册失败
 */
TEST_F(TestUbseStorageModule, RegRemoteReqHandlerFail)
{
    EXPECT_EQ(UBSE_ERROR_NULLPTR, UbseStorageModule::Impl::RegRemoteReqHandler());
    auto comModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseStorageModule::Impl::GetUbseComModule).stubs().will(returnValue(comModule));
    EXPECT_EQ(UBSE_ERROR, UbseStorageModule::Impl::RegRemoteReqHandler());
}

/*
 * 用例描述：
 * 测试获取主节点ID失败
 * 测试步骤：
 * 1. 调用接口
 * 预期结果：
 * 1. 获取UbseRoleModule失败
 * 2. 获取MasterNodeId失败
 */
TEST_F(TestUbseStorageModule, GetMasterNodeFail)
{
    UbseRoleInfo roleInfo{};
    roleInfo.nodeId = "test";
    MOCKER(UbseGetMasterInfo).stubs().with(outBound(roleInfo)).will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ("", UbseStorageModule::Impl::GetMasterNode());
    EXPECT_EQ("test", UbseStorageModule::Impl::GetMasterNode());
}

/*
 * 用例描述：
 * 测试获取通信模块失败
 * 测试步骤：
 * 1. 调用接口
 * 预期结果：
 * 1. 获取UbseComModule失败
 */
TEST_F(TestUbseStorageModule, GetUbseComModuleFail)
{
    EXPECT_EQ(nullptr, UbseStorageModule::Impl::GetUbseComModule());
}

/*
 * 用例描述：
 * 测试获取存储模块失败
 * 测试步骤：
 * 1. 调用接口
 * 预期结果：
 * 1. 获取UbseComModule失败
 */
TEST_F(TestUbseStorageModule, GetUbseStorageModuleFail)
{
    EXPECT_EQ(nullptr, UbseStorageModule::GetStorageModule());
}

/*
 * 用例描述：
 * 测试消息发送失败
 * 测试步骤：
 * 1. 调用接口
 * 预期结果：
 * 1. 消息发送失败
 */
TEST_F(TestUbseStorageModule, RpcSendFail)
{
    EXPECT_EQ(nullptr, UbseStorageModule::Impl::GetUbseComModule());
    auto comModule = std::make_shared<UbseComModule>();
    MOCKER(&UbseStorageModule::Impl::GetUbseComModule).stubs().will(returnValue(comModule));
    SendParam sendParam("", static_cast<uint16_t>(UbseModuleCode::STORAGE),
                        static_cast<uint16_t>(UbseStorageOpCode::STORAGE_REQ));
    UbseBaseMessagePtr requestSimpoPtr = new (std::nothrow) storage::message::UbseStorageReqSimpo();
    UbseBaseMessagePtr responseSimpoPtr = new (std::nothrow) storage::message::UbseStorageRespSimpo();
    EXPECT_NE(nullptr, requestSimpoPtr.Get());
    EXPECT_NE(nullptr, responseSimpoPtr.Get());
    EXPECT_EQ(UBSE_ERROR, UbseStorageModule::Impl::RpcSend(sendParam, requestSimpoPtr, responseSimpoPtr));
}

/*
 * 用例描述：
 * 获取存储路径
 * 测试步骤：
 * 1. 调用接口
 * 预期结果：
 * 1. 存储路径符合预期
 */
TEST_F(TestUbseStorageModule, GetDbStorageDir)
{
    EXPECT_EQ(DB_STORE_DIR, UbseStorageModule::Impl::GetDbStorageDir());
}
}