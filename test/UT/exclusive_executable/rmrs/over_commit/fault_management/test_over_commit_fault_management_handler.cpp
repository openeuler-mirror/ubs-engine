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

#include <thread>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "mockcpp/mokc.h"
#include "ubse_def.h"
#include "over_commit_fault_management_handler.h"
#include "over_commit_fault_memid_helper.h"
#include "over_commit_fault_memid_module.h"
#include "over_commit_fault_node_module.h"
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace mempooling::over_commit {
using std::cout;
using std::endl;
class TestOverCommitFaultManagementHandler : public ::testing::Test {
public:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(TestOverCommitFaultManagementHandler, GetVmNumaInfoMapRecvHandler_Succeed)
{
    MOCKER_CPP(&OverCommitFaultMemIdModule::GetRemoteNumaVms,
               MpResult(*)(OverCommitFaultMemIdModule *, uint16_t, std::vector<VmNumaInfoWithSocket> &))
        .stubs()
        .with(any(), any(), any())
        .will(returnValue(MEM_POOLING_OK));

    UbseByteBuffer req;
    UbseByteBuffer resp;
    MpResult ret = OverCommitFaultManagementHandler::GetVmNumaInfoMapRecvHandler(req, resp);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultManagementHandler, GetVmNumaInfoMapRecvHandler_GetRemoteNumaVms_Failed)
{
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    VmNumaInfoWithSocket sockItem{0, 1, 2, 3, 4, 5, 6};
    vmNumaInfoWithSocketList.push_back(sockItem);

    MOCKER_CPP(&OverCommitFaultMemIdModule::GetRemoteNumaVms,
               MpResult(*)(OverCommitFaultMemIdModule *, uint16_t, std::vector<VmNumaInfoWithSocket> &))
        .stubs()
        .with(any(), any(), any())
        .will(returnValue(MEM_POOLING_ERROR));

    UbseByteBuffer req;
    UbseByteBuffer resp;
    MpResult ret = OverCommitFaultManagementHandler::GetVmNumaInfoMapRecvHandler(req, resp);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultManagementHandler, GetVmNumaInfoMapResHandler_Succeed)
{
    int ctx;
    UbseByteBuffer respData;
    uint32_t resCode = MEM_POOLING_OK;
    OverCommitFaultManagementHandler::GetVmNumaInfoMapResHandler(&ctx, respData, resCode);
    EXPECT_EQ(resCode, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultManagementHandler, GetVmNumaInfoMapResHandler_resCode_Failed)
{
    int ctx;
    UbseByteBuffer respData;
    uint32_t resCode = MEM_POOLING_ERROR;
    OverCommitFaultManagementHandler::GetVmNumaInfoMapResHandler(&ctx, respData, resCode);
    EXPECT_EQ(resCode, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultManagementHandler, MemIdExecuteRecvHandler_Succeed)
{
    MOCKER_CPP(&OverCommitFaultMemIdModule::MemIdExecute,
        MpResult (*)(OverCommitFaultMemIdExecuteParam))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    UbseByteBuffer req;
    UbseByteBuffer resp;
    uint32_t ret = OverCommitFaultManagementHandler::MemIdExecuteRecvHandler(req, resp);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultManagementHandler, MemIdExecuteRecvHandler_MemIdExecute_Failed)
{
    MOCKER_CPP(&OverCommitFaultMemIdModule::MemIdExecute,
        MpResult (*)(OverCommitFaultMemIdExecuteParam))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    UbseByteBuffer req;
    UbseByteBuffer resp;
    uint32_t ret = OverCommitFaultManagementHandler::MemIdExecuteRecvHandler(req, resp);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitFaultManagementHandler, MemIdExecuteResHandler_Succeed)
{
    uint32_t ctx = 0;
    UbseByteBuffer resp;
    resp.len = MEM_POOLING_ERROR;
    uint32_t resCode = MEM_POOLING_OK;
    OverCommitFaultManagementHandler::MemIdExecuteResHandler(&ctx, resp, resCode);
    EXPECT_EQ(ctx, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultManagementHandler, MemIdExecuteResHandler_Failed)
{
    uint32_t ctx = 1;
    UbseByteBuffer resp;
    resp.len = MEM_POOLING_ERROR;
    uint32_t resCode = MEM_POOLING_ERROR;
    OverCommitFaultManagementHandler::MemIdExecuteResHandler(&ctx, resp, resCode);
    EXPECT_EQ(ctx, MEM_POOLING_ERROR);
}

} // namespace mempooling::over_commit
