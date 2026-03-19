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

#include "test_ubse_mem_validator.h"
#include "ubse_mem_validator.h"

namespace ubse::mem_scheduler::ut {
using namespace ubse::mem::strategy;

void TestMemValidator::SetUp()
{
    Test::SetUp();
}

void TestMemValidator::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestMemValidator, TestCheckLendNodeIsInCandidatelistOK)
{
    UbseMemValidator validator;
    validator.candidateNodeList_ = {"1", "2", "3"};
    validator.exportNodeId_ = "1";
    auto ret = validator.CheckLendNodeIsInCandidatelist();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestMemValidator, TestCheckLendNodeIsInCandidatelistERROR)
{
    UbseMemValidator validator;
    validator.candidateNodeList_ = {"1", "2", "3"};
    validator.exportNodeId_ = "4";
    auto ret = validator.CheckLendNodeIsInCandidatelist();
    EXPECT_EQ(ret, UBSE_ERROR);
}

} // namespace ubse::mem_scheduler::ut