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

#include "test_ubse_storage.h"
#include "mockcpp/mockcpp.hpp"
#include "ubse_storage.h"
#include "ubse_storage_module.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"

namespace ubse::ldc::ut {
using namespace ubse::config;
using namespace ubse::context;
using namespace ubse::storage;

namespace {
int g_callbackCount = 0;

void callbackFunc(const std::string &keyPrefix, const std::string &key, const UbseByteBuffer &buff, void *ctx)
{
    g_callbackCount++;
    EXPECT_EQ("key", keyPrefix);
    EXPECT_EQ("_ssu", key);
    EXPECT_NE(nullptr, ctx);
}
}

void TestUbseStorage::SetUp()
{
    Test::SetUp();
    g_callbackCount = 0;
}

void TestUbseStorage::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * 测试对外接口存储接口
 * 测试步骤：
 * 1. 存储模块未初始化
 * 2. data为空
 * 3. 存储接口存储失败
 * 4. 存储接口存储成功
 * 预期结果：
 * 1. UBSE_ERROR_MODULE_LOAD_FAILED
 * 2. UBSE_ERROR_NULLPTR
 * 3. UBSE_ERROR
 * 4. UBSE_OK
 */
TEST_F(TestUbseStorage, UbseStoragePutData)
{
    std::shared_ptr<UbseStorageModule> nullModule = nullptr;
    std::shared_ptr<UbseStorageModule> module = std::make_shared<UbseStorageModule>();
    std::string value = "value";
    UbseByteBuffer data{ reinterpret_cast<uint8_t *>(value.data()), value.size(), nullptr };

    MOCKER(&UbseContext::GetModule<UbseStorageModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module))
        .then(returnValue(module))
        .then(returnValue(module));
    MOCKER(&UbseStorageModule::Put)
        .stubs()
        .will(returnObjectList(UBSE_ERROR, UBSE_OK));

    EXPECT_EQ(UBSE_ERROR_MODULE_LOAD_FAILED, UbseStoragePutData("key", "_ssu", &data));
    EXPECT_EQ(UBSE_ERROR_NULLPTR, UbseStoragePutData("key", "_ssu", nullptr));
    EXPECT_EQ(UBSE_ERROR, UbseStoragePutData("key", "_ssu", &data));
    EXPECT_EQ(UBSE_OK, UbseStoragePutData("key", "_ssu", &data));
    module.reset();
}

/*
 * 用例描述：
 * 测试对外接口查询接口
 * 测试步骤：
 * 1. 回调函数为空
 * 2. 存储模块未初始化
 * 3. 存储接口查询失败
 * 4. 存储接口查询成功
 * 预期结果：
 * 1. UBSE_ERROR_NULLPTR
 * 2. UBSE_ERROR_MODULE_LOAD_FAILED
 * 3. UBSE_ERROR
 * 4. UBSE_OK
 */
TEST_F(TestUbseStorage, UbseStorageQueryData)
{
    int ctx = 0;
    std::shared_ptr<UbseStorageModule> nullModule = nullptr;
    std::shared_ptr<UbseStorageModule> module = std::make_shared<UbseStorageModule>();

    MOCKER(&UbseContext::GetModule<UbseStorageModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module))
        .then(returnValue(module));
    MOCKER(&UbseStorageModule::Get)
        .stubs()
        .will(returnObjectList(UBSE_ERROR, UBSE_OK));

    EXPECT_EQ(UBSE_ERROR_NULLPTR, UbseStorageQueryData("key", "_ssu", &ctx, nullptr));
    EXPECT_EQ(UBSE_ERROR_MODULE_LOAD_FAILED, UbseStorageQueryData("key", "_ssu", &ctx, callbackFunc));
    EXPECT_EQ(UBSE_ERROR, UbseStorageQueryData("key", "_ssu", &ctx, callbackFunc));
    EXPECT_EQ(UBSE_OK, UbseStorageQueryData("key", "_ssu", &ctx, callbackFunc));
    EXPECT_EQ(1, g_callbackCount);
    module.reset();
}

/*
 * 用例描述：
 * 测试对外接口删除接口
 * 测试步骤：
 * 1. 存储模块未初始化
 * 2. 存储接口删除失败
 * 3. 存储接口删除成功
 * 预期结果：
 * 1. UBSE_ERROR_MODULE_LOAD_FAILED
 * 2. UBSE_ERROR
 * 3. UBSE_OK
 */
TEST_F(TestUbseStorage, UbseStorageDeleteData)
{
    std::shared_ptr<UbseStorageModule> nullModule = nullptr;
    std::shared_ptr<UbseStorageModule> module = std::make_shared<UbseStorageModule>();
    MOCKER(&UbseContext::GetModule<UbseStorageModule>)
        .stubs()
        .will(returnValue(nullModule))
        .then(returnValue(module));
    MOCKER(&UbseStorageModule::Delete)
        .stubs()
        .will(returnObjectList(UBSE_ERROR, UBSE_OK));

    EXPECT_EQ(UBSE_ERROR_MODULE_LOAD_FAILED, UbseStorageDeleteData("key", "_ssu"));
    EXPECT_EQ(UBSE_ERROR, UbseStorageDeleteData("key", "_ssu"));
    EXPECT_EQ(UBSE_OK, UbseStorageDeleteData("key", "_ssu"));
    module.reset();
}
}