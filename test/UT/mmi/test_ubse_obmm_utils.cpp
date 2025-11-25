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


#include "test_ubse_obmm_utils.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_obmm_utils.h"

namespace ubse::ut::mmi {
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::mmi;
TEST_F(TestUbseObmmUtils, GetBlockSize_Success)
{
    uint64_t blockSize{0};
    std::shared_ptr<UbseConfModule> mock = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(mock));
    MOCKER(&UbseConfModule::GetConf<uint64_t>)
        .stubs()
        .with(_, _, outBound(static_cast<uint64_t>(128)))
        .will(returnValue(0));
    auto ret = RmObmmUtils::GetBlockSize(blockSize);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(blockSize, 1<<27);
}

TEST_F(TestUbseObmmUtils, GetBlockSize_Default)
{
    uint64_t blockSize{128};
    std::shared_ptr<UbseConfModule> mock = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(mock));
    MOCKER(&UbseConfModule::GetConf<uint64_t>).stubs().will(returnValue(1));
    auto ret = RmObmmUtils::GetBlockSize(blockSize);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(blockSize, 1<<27);
}

TEST_F(TestUbseObmmUtils, GetBlockSize_Fail)
{
    uint64_t blockSize{0};
    std::shared_ptr<UbseConfModule> mock = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(mock));
    MOCKER(&UbseConfModule::GetConf<uint64_t>)
        .stubs()
        .with(_, _, outBound(static_cast<uint64_t>(999)))
        .will(returnValue(0));
    auto ret = RmObmmUtils::GetBlockSize(blockSize);
    EXPECT_NE(ret, UBSE_OK);
}
}
