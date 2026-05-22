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

#include "rack_mempooling_plugin.h"

#include <dlfcn.h>
#include <gmock/gmock.h>
#include <securec.h>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "ubse_def.h"
#include "event_listener.h"
#include "fault_memid_helper.h"
#include "fault_node_module.h"
#include "mem_borrow_executor.h"
#include "mempool_borrow_module.h"
#include "mempool_migrate_helper.h"
#include "mempool_migrate_module.h"
#include "mempooling_message.h"
#include "mp_configuration.h"
#include "mp_error.h"
#include "mp_heartbeat_monitor.h"
#include "mp_module.h"
#include "mp_smap_controller.h"
#include "mp_smap_helper.h"
#include "over_commit_election_handler.h"
#include "over_commit_msg_handler.h"
#include "over_commit_serializer.h"
#include "rmrs_serialize.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

// mock
void* dlsym(void* __restrict __handle, const char* __restrict __name);
void* dlopen(const char* __file, int __mode);
int dlclose(void* __handle);

using namespace std;
using namespace mempooling;
using namespace rmrs::serialize;

namespace mempooling::MempoolingMessage {

// 测试类
class TestRackMempoolingPlugin : public ::testing::Test {
public:
    // mempooling::HostVmDomainInfo hostVmDomainInfo;
    // std::vector<mempooling::HostVmDomainInfo> hostVmDomainInfos;
    void SetUp() override
    {
        cout << "[TestRackMempoolingPlugin SetUp Begin]" << endl;
        cout << "[TestRackMempoolingPlugin SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[TestRackMempoolingPlugin TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[TestRackMempoolingPlugin TearDown End]" << endl;
    }

public:
    UbseByteBuffer* CreateMockUbseByteBuffer(uint32_t len)
    {
        UbseByteBuffer* buffer = new UbseByteBuffer();
        buffer->len = len;
        buffer->data = new uint8_t[len];
        return buffer;
    }
};

TEST_F(TestRackMempoolingPlugin, UbsePluginInitFailed1)
{
    MOCKER_CPP(&MpConfiguration::Initialize, uint32_t (*)(const uint16_t)).stubs().will(returnValue(1));
    auto ret = UbsePluginInit(0);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestRackMempoolingPlugin, UbsePluginInitFailed2)
{
    MOCKER_CPP(&MpConfiguration::Initialize, uint32_t (*)(const uint16_t)).stubs().will(returnValue(0));
    MOCKER_CPP(&mempooling::message::MempoolingMessage::Init, uint32_t (*)())
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    auto ret = UbsePluginInit(0);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestRackMempoolingPlugin, UbsePluginDeInit)
{
    MOCKER(static_cast<int (*)(void*)>(dlclose)).stubs().will(returnValue(0));
    UbsePluginDeInit();
}

} // namespace mempooling::MempoolingMessage