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
#include <iostream>
#include <filesystem>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#define private public
#include "OsHelper.h"
#undef private

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace mempooling::exportV2 {
namespace fs = std::filesystem;

class TestOsHelper : public ::testing::Test {
protected:
    TestOsHelper() {}
    virtual void SetUp()
    {
        GlobalMockObject::reset();
    }
    virtual void TearDown()
    {
        GlobalMockObject::verify();
        GlobalMockObject::reset();
    }
};

TEST(TestOsHelper, GetNumaSetShouldReturnOk)
{
    std::vector<std::uint16_t> numaSet;
    auto result = OsHelper::GetNumaSet(numaSet);
    EXPECT_EQ(result, MEM_POOLING_OK);
}

TEST(TestOsHelper, GetHostNameShouldReturnOk)
{
    std::string hostName;
    auto result = OsHelper::GetHostName(hostName);
    EXPECT_EQ(result, MEM_POOLING_OK);
}

TEST(TestOsHelper, IsNumaLocalShouldReturnErrorWhenNumaIdInvalid)
{
    uint16_t numaId = UINT16_MAX;
    bool isLocal;
    auto result = OsHelper::IsNumaLocal(numaId, isLocal);
    EXPECT_EQ(result, MEM_POOLING_ERROR);
}

TEST(TestOsHelper, GetSocketIdByNumaIdShouldReturnErrorWhenNumaIdInvalid)
{
    uint16_t numaId = UINT16_MAX;
    int16_t socketId;
    auto result = OsHelper::GetSocketIdByNumaId(numaId, socketId);
    EXPECT_EQ(result, MEM_POOLING_ERROR);
}

TEST(TestOsHelper, GetSocketIdByNumaIdShouldReturnOkWhenNumaIdValid)
{
    uint16_t numaId = 0;
    int16_t socketId;
    auto result = OsHelper::GetSocketIdByNumaId(numaId, socketId);
    EXPECT_EQ(result, MEM_POOLING_OK);
}

TEST(TestOsHelper, GetMemInfoByNumaIdShouldReturnOkWhenNumaIdValid)
{
    uint16_t numaId = 0;
    NumaInfo info;
    auto result = OsHelper::GetMemInfoByNumaId(numaId, info);
    EXPECT_EQ(result, MEM_POOLING_OK);
}

TEST(TestOsHelper, GetMemInfoByNumaIdShouldReturnErrorWhenNumaIdInvalid)
{
    uint16_t numaId = UINT16_MAX;
    NumaInfo info;
    auto result = OsHelper::GetMemInfoByNumaId(numaId, info);
    EXPECT_EQ(result, MEM_POOLING_ERROR);
}

TEST(TestOsHelper, GetProcessStartTimeByPidShouldReturnErrorWhenPidInvalid)
{
    pid_t pid = INT8_MIN;
    time_t startTime;
    auto result = OsHelper::GetProcessStartTimeByPid(pid, startTime);
    EXPECT_EQ(result, MEM_POOLING_ERROR);
}

TEST(TestOsHelper, GetProcessStartTimeByPidShouldReturnOkWhenPidValid)
{
    pid_t pid = 1;
    time_t startTime;
    auto result = OsHelper::GetProcessStartTimeByPid(pid, startTime);
    EXPECT_EQ(result, MEM_POOLING_OK);
}

TEST(TestOsHelper, GetVmNumaInfoByPidShouldReturnErrorWhenPidInvalid)
{
    pid_t pid = INT8_MIN;
    VmDomainInfo info;
    auto result = OsHelper::GetVmNumaInfoByPid(pid, info);
    EXPECT_EQ(result, MEM_POOLING_ERROR);
}

TEST(TestOsHelper, GetVmNumaInfoByPidShouldReturnErrorWhenPidPathInvalid)
{
    pid_t pid = 1;
    VmDomainInfo info;
    auto result = OsHelper::GetVmNumaInfoByPid(pid, info);
    EXPECT_EQ(result, MEM_POOLING_ERROR);
}

} // namespace mempooling::exportV2