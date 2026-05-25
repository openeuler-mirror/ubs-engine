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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <thread>
#include "ubse_def.h"
#include "mockcpp/mokc.h"
#include "mp_mem_json_util.h"
#include "over_commit_fault_management_handler.h"
#include "over_commit_fault_memid_helper.h"
#include "over_commit_fault_memid_module.h"
#include "over_commit_fault_node_module.h"
#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace mempooling::over_commit {
using std::cout;
using std::endl;
class TestOverCommitFaultMemIdHelper : public ::testing::Test {
public:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(TestOverCommitFaultMemIdHelper, FaultMemIdManageHelper_Succeed)
{
    MOCKER_CPP(&OverCommitFaultMemIdModule::MemIdFaultManage, MpResult (*)(std::string, uint64_t))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    std::string importNodeId = "Node1";
    uint64_t importMemId = 0;
    MpResult ret = OverCommitFaultMemIdHelper::Instance().FaultMemIdManageHelper(importNodeId, importMemId);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitFaultMemIdHelper, FaultMemIdManageHelper_MemIdFaultManage_Failed)
{
    MOCKER_CPP(&OverCommitFaultMemIdModule::MemIdFaultManage, MpResult (*)(std::string, uint64_t))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    std::string importNodeId = "Node1";
    uint64_t importMemId = 0;
    MpResult ret = OverCommitFaultMemIdHelper::Instance().FaultMemIdManageHelper(importNodeId, importMemId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

} // namespace mempooling::over_commit
