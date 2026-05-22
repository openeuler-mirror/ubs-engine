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

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

#include <ubse_node_controller.h>
#include <ubse_obmm_executor.h>
#include <ubse_obmm_utils.h>
#include <ubse_security_module.h>

namespace ubse::ut::mmi {
using namespace ubse::mmi;
using namespace ubse::nodeController;
using namespace ubse::security;

class TestUbseObmmUnimport : public testing::Test {
    void SetUp() override
    {
        Test::SetUp();
        oldUnimportFunc_ = RmObmmExecutor::GetInstance().obmmUnimportFunc;
        RmObmmExecutor::GetInstance().obmmUnimportFunc = [](unsigned long id, unsigned long flags) {
            return 0;
        };

        struct sigaction sa {};
        sa.sa_handler = [](int) {
        };
        sa.sa_flags = 0;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGUSR1, &sa, &oldSa_);
    }

    void TearDown() override
    {
        RmObmmExecutor::GetInstance().obmmUnimportFunc = oldUnimportFunc_;
        sigaction(SIGUSR1, &oldSa_, nullptr);
        Test::TearDown();
        GlobalMockObject::verify();
    }

private:
    ObmmUnimportPtr oldUnimportFunc_{nullptr};
    struct sigaction oldSa_ {};
};

TEST_F(TestUbseObmmUnimport, UnImportWithTimeout_Success)
{
    auto ret = RmObmmExecutor::GetInstance().ObmmUnImport(1, 5000);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseObmmUnimport, UnImportWithTimeout_NullptrFunc)
{
    RmObmmExecutor::GetInstance().obmmUnimportFunc = nullptr;
    auto ret = RmObmmExecutor::GetInstance().ObmmUnImport(1, 5000);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseObmmUnimport, UnImportWithTimeout_ObmmError)
{
    RmObmmExecutor::GetInstance().obmmUnimportFunc = [](unsigned long id, unsigned long flags) {
        errno = EIO;
        return -1;
    };
    auto ret = RmObmmExecutor::GetInstance().ObmmUnImport(1, 5000);
    EXPECT_EQ(ret, UBSE_MMI_OBMM_OP_FAILED);
}

TEST_F(TestUbseObmmUnimport, UnImportWithTimeout_InterruptedBySignal)
{
    RmObmmExecutor::GetInstance().obmmUnimportFunc = [](unsigned long id, unsigned long flags) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        errno = EINTR;
        return -1;
    };

    auto ret = RmObmmExecutor::GetInstance().ObmmUnImport(42, 100);
    EXPECT_EQ(ret, UBSE_MMI_OBMM_OP_TIMEOUT);
}

TEST_F(TestUbseObmmUnimport, UnImportWithTimeout_ENOENT_ReturnsOk)
{
    RmObmmExecutor::GetInstance().obmmUnimportFunc = [](unsigned long id, unsigned long flags) {
        errno = ENOENT;
        return -1;
    };
    auto ret = RmObmmExecutor::GetInstance().ObmmUnImport(1, 5000);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseObmmUnimport, BatchUnImportWithTimeout_Success)
{
    std::vector<mem_id> memIds = {1, 2, 3};
    auto ret = RmObmmExecutor::GetInstance().ObmmUnImport(memIds, 5000);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseObmmUnimport, BatchUnImportWithTimeout_PartialTimeout)
{
    static int callCount = 0;
    callCount = 0;
    RmObmmExecutor::GetInstance().obmmUnimportFunc = [](unsigned long id, unsigned long flags) {
        callCount++;
        if (id == 2) {
            errno = EINTR;
            return -1;
        }
        return 0;
    };
    std::vector<mem_id> memIds = {1, 2, 3};
    auto ret = RmObmmExecutor::GetInstance().ObmmUnImport(memIds, 5000);
    EXPECT_EQ(ret, UBSE_MMI_OBMM_OP_TIMEOUT);
    EXPECT_EQ(callCount, 2);
}

TEST_F(TestUbseObmmUnimport, CalculateUnImportTimeoutDefault)
{
    RmObmmExecutor::offlineTimeoutConfigured_ = false;
    EXPECT_EQ(RmObmmExecutor::CalculateUnImportTimeout(128), 5128);
    EXPECT_EQ(RmObmmExecutor::CalculateUnImportTimeout(4096), 9096);
    EXPECT_EQ(RmObmmExecutor::CalculateUnImportTimeout(0), 5000);
}

TEST_F(TestUbseObmmUnimport, CalculateUnImportTimeoutConfigured)
{
    RmObmmExecutor::offlineTimeoutConfigured_ = true;
    RmObmmExecutor::offlineTimeoutMs_ = 100000;
    EXPECT_EQ(RmObmmExecutor::CalculateUnImportTimeout(128), 100000);
    EXPECT_EQ(RmObmmExecutor::CalculateUnImportTimeout(4096), 100000);
    EXPECT_EQ(RmObmmExecutor::CalculateUnImportTimeout(0), 100000);

    RmObmmExecutor::offlineTimeoutMs_ = 120000;
    EXPECT_EQ(RmObmmExecutor::CalculateUnImportTimeout(999), 120000);

    RmObmmExecutor::offlineTimeoutConfigured_ = false;
}

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
