/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <memory>
#include <gmock/gmock.h>
#include "gtest/gtest.h"

#include "ubse_com.h"
#include "fault_memid_helper.h"
#include "fault_node_module.h"
#include "mempool_migrate_helper.h"
#include "mempool_migrate_module.h"
#include "mempooling_message.h"
#include "mockcpp/mokc.h"
#include "mp_error.h"
#include "mp_heartbeat_monitor.h"
#include "mp_sync_data_helper.h"
#include "rmrs_serialize.h"
#include "turbo_def.h"
#include "over_commit_serializer.h"

using namespace std;
using namespace ubse::com;
using namespace mempooling::migrate;
using namespace rmrs;
using namespace rmrs::serialize;

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace mempooling::message {

class MockRackRegRpcService {
public:
    MOCK_METHOD(MpResult, UbseRegRpcService, (const UbseComEndpoint &endpoint, UbseComServiceHandler &handler), ());
};

// 测试类
class TestMemPoolingMessage : public ::testing::Test {
public:
    UbseByteBuffer *CreateMockUbseByteBuffer(uint32_t len)
    {
        UbseByteBuffer *buffer = new UbseByteBuffer();
        buffer->len = len;
        buffer->data = new uint8_t[len];
        return buffer;
    }

protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        cout << "[Phase SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }
};

TEST_F(TestMemPoolingMessage, InitSucceed)
{
    MOCKER_CPP(&mempooling::message::MempoolingMessage::InitOSTurboIpcClient, uint32_t(*)())
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MpResult res = MempoolingMessage::Init();
    ASSERT_EQ(res, 0);
}

uint32_t osturboFunctionCallerMock(const std::string &function, const TurboByteBuffer &params, TurboByteBuffer &result)
{
    return 0;
}
uint32_t osturboSetIpcTimeLimitMock(uint32_t timeLimit)
{
    return 0;
}

TEST_F(TestMemPoolingMessage, InitOSTurboIpcClientSucceed)
{
    static int dummy_handle;
    MOCKER_CPP(dlopen, void *(*)(const char *, int)).stubs().will(returnValue(reinterpret_cast<void *>(&dummy_handle)));

    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue(reinterpret_cast<void *>(osturboFunctionCallerMock)))
        .then(returnValue(reinterpret_cast<void *>(osturboSetIpcTimeLimitMock)));

    uint32_t ret = MempoolingMessage::InitOSTurboIpcClient();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemPoolingMessage, InitOSTurboIpcClient_dlopen_Failed)
{
    MOCKER_CPP(dlopen, void *(*)(const char *, int)).stubs().will(returnValue((void*)nullptr));

    uint32_t ret = MempoolingMessage::InitOSTurboIpcClient();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolingMessage, InitOSTurboIpcClient_dlsym_Failed)
{
    static int dummy_handle;
    MOCKER_CPP(dlopen, void *(*)(const char *, int)).stubs().will(returnValue(reinterpret_cast<void *>(&dummy_handle)));

    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue((void*)nullptr));

    uint32_t ret = MempoolingMessage::InitOSTurboIpcClient();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestMemPoolingMessage, InitOSTurboIpcClient_dlsym_Failed2)
{
    static int dummy_handle;
    MOCKER_CPP(dlopen, void *(*)(const char *, int)).stubs().will(returnValue(reinterpret_cast<void *>(&dummy_handle)));

    MOCKER_CPP(dlsym, void *(*)(void *, const char *))
        .stubs()
        .will(returnValue(reinterpret_cast<void *>(osturboFunctionCallerMock)))
        .then(returnValue((void*)nullptr));

    uint32_t ret = MempoolingMessage::InitOSTurboIpcClient();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}
TEST_F(TestMemPoolingMessage, MpMessageSubModuleInitSuccess)
{
    MOCKER_CPP(&MempoolingMessage::InitOSTurboIpcClient, uint32_t(*)())
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    std::unique_ptr<MpMessageSubModule> mp = std::make_unique<MpMessageSubModule>();
    auto ret = mp->Init();
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestMemPoolingMessage, MpMessageSubModuleInitFailure)
{
    MOCKER_CPP(&MempoolingMessage::InitOSTurboIpcClient, uint32_t(*)())
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    std::unique_ptr<MpMessageSubModule> mp = std::make_unique<MpMessageSubModule>();
    auto ret = mp->Init();
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

} // namespace mempooling::message
