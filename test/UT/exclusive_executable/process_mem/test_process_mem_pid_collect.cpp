/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_process_mem_pid_collect.h"

namespace ubse::ut::process_mem {
using namespace ::process_mem::collect;

void TestProcessMemPidCollect::SetUp() {}

void TestProcessMemPidCollect::TearDown() {}

TEST_F(TestProcessMemPidCollect, RegisterCollectHandlerSuccess)
{
    auto& collector = ProcessMemPidCollect::GetInstance();
    NumaMemDistributionCollectHandler handler = [](CollectInfoMap) {
    };
    auto ret = collector.RegisterCollectHandler("test_handler", handler);
    EXPECT_EQ(ret, UBSE_OK);
    collector.UnRegisterCollectHandler("test_handler");
}

TEST_F(TestProcessMemPidCollect, RegisterCollectHandlerNullHandler)
{
    auto& collector = ProcessMemPidCollect::GetInstance();
    NumaMemDistributionCollectHandler nullHandler;
    auto ret = collector.RegisterCollectHandler("null_handler", nullHandler);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestProcessMemPidCollect, RegisterCollectHandlerOverwrite)
{
    auto& collector = ProcessMemPidCollect::GetInstance();
    NumaMemDistributionCollectHandler handler1 = [](CollectInfoMap) {
    };
    NumaMemDistributionCollectHandler handler2 = [](CollectInfoMap) {
    };

    auto ret = collector.RegisterCollectHandler("overwrite_handler", handler1);
    EXPECT_EQ(ret, UBSE_OK);

    ret = collector.RegisterCollectHandler("overwrite_handler", handler2);
    EXPECT_EQ(ret, UBSE_OK);

    collector.UnRegisterCollectHandler("overwrite_handler");
}

TEST_F(TestProcessMemPidCollect, UnRegisterCollectHandlerNotExist)
{
    auto& collector = ProcessMemPidCollect::GetInstance();
    EXPECT_NO_THROW(collector.UnRegisterCollectHandler("non_existent_handler"));
}

TEST_F(TestProcessMemPidCollect, CollectProcessNumaMemDistributionInvalidPid)
{
    auto& collector = ProcessMemPidCollect::GetInstance();
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    auto ret = collector.CollectProcessNumaMemDistribution(-1, numaMemDistribution);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidCollect, CollectProcessNumaMemDistributionNonExistentPid)
{
    auto& collector = ProcessMemPidCollect::GetInstance();
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    auto ret = collector.CollectProcessNumaMemDistribution(99999999, numaMemDistribution);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidCollect, RegisterMultipleHandlers)
{
    auto& collector = ProcessMemPidCollect::GetInstance();
    NumaMemDistributionCollectHandler handler1 = [](CollectInfoMap) {
    };
    NumaMemDistributionCollectHandler handler2 = [](CollectInfoMap) {
    };

    auto ret1 = collector.RegisterCollectHandler("multi_handler_1", handler1);
    auto ret2 = collector.RegisterCollectHandler("multi_handler_2", handler2);
    EXPECT_EQ(ret1, UBSE_OK);
    EXPECT_EQ(ret2, UBSE_OK);

    collector.UnRegisterCollectHandler("multi_handler_1");
    collector.UnRegisterCollectHandler("multi_handler_2");
}

TEST_F(TestProcessMemPidCollect, UnRegisterAndReRegister)
{
    auto& collector = ProcessMemPidCollect::GetInstance();
    NumaMemDistributionCollectHandler handler = [](CollectInfoMap) {
    };

    auto ret = collector.RegisterCollectHandler("reregister_handler", handler);
    EXPECT_EQ(ret, UBSE_OK);

    collector.UnRegisterCollectHandler("reregister_handler");

    ret = collector.RegisterCollectHandler("reregister_handler", handler);
    EXPECT_EQ(ret, UBSE_OK);

    collector.UnRegisterCollectHandler("reregister_handler");
}
} // namespace ubse::ut::process_mem
