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

#include "test_ubse_obmm_executor.h"

#include <ubse_node_controller.h>
#include <ubse_obmm_executor.h>
#include <ubse_obmm_utils.h>

namespace ubse::ut::mmi {
using namespace ubse::mmi;
using namespace ubse::nodeController;

TEST_F(TestUbseObmmExecutor, Init_Success)
{
    auto ret = RmObmmExecutor::GetInstance().Init();
    EXPECT_NE(ret, UBSE_OK);

    ret = RmObmmExecutor::GetInstance().Init();
    EXPECT_NE(ret, UBSE_OK);

    MOCKER(&RmObmmUtils::GetPreOnlineSwitch).stubs().will(returnValue(UBSE_OK));
    MOCKER(&RmObmmExecutor::DlOpenLib).stubs().will(returnValue(UBSE_ERROR));
    ret = RmObmmExecutor::GetInstance().Init();
    EXPECT_NE(ret, UBSE_OK);

    MOCKER(&RmObmmExecutor::DlOpenLib).reset();
    MOCKER(&RmObmmExecutor::DlOpenLib).stubs().will(returnValue(UBSE_OK));
    ret = RmObmmExecutor::GetInstance().Init();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseObmmExecutor, Exit_Success)
{
    auto ret = RmObmmExecutor::GetInstance().Exit();
    EXPECT_EQ(ret, UBSE_OK);
}
} // namespace ubse::ut::mmi
