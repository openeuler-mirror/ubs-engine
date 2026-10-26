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
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>
#include "ubse_security.h"
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#include "mp_file_util.h"

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

class TestVmNumaPtrace : public TestOsHelper {
protected:
    inline static bool enabled = false;
    inline static uint32_t enableResult = MEM_POOLING_OK;
    inline static uint32_t dropResult = MEM_POOLING_OK;
    inline static MpResult readResult = MEM_POOLING_OK;
    inline static MpResult localResult = MEM_POOLING_OK;
    inline static MpResult socketResult = MEM_POOLING_OK;
    inline static bool throwOnRead = false;
    inline static size_t readCalls = 0;
    inline static std::vector<bool> changes;
    inline static std::vector<std::thread::id> changeThreads;
    inline static std::vector<std::string> lines;

    static uint32_t ChangeCapability(bool isAdd)
    {
        changes.push_back(isAdd);
        changeThreads.push_back(std::this_thread::get_id());
        uint32_t ret = isAdd ? enableResult : dropResult;
        if (ret == MEM_POOLING_OK) {
            enabled = isAdd;
        }
        return ret;
    }

    static MEM_POOLING_RES ReadNumaMaps(const std::string& path, std::vector<std::string>& output)
    {
        EXPECT_TRUE(enabled);
        EXPECT_EQ(path, "/proc/123/numa_maps");
        EXPECT_EQ(changeThreads.front(), std::this_thread::get_id());
        ++readCalls;
        if (throwOnRead) {
            throw std::runtime_error("模拟读取异常");
        }
        output = lines;
        return readResult;
    }

    static MpResult QueryLocal(const uint16_t& numaId, bool& isLocal)
    {
        // NUMA 解析及 sysfs 查询阶段不应继续持有 PTRACE 权限。
        EXPECT_FALSE(enabled);
        isLocal = (numaId == 0);
        return localResult;
    }

    static MpResult QuerySocket(const uint16_t& numaId, int16_t& socketId)
    {
        EXPECT_FALSE(enabled);
        socketId = 7;
        return socketResult;
    }

    void SetUp() override
    {
        TestOsHelper::SetUp();
        enabled = false;
        enableResult = dropResult = readResult = localResult = socketResult = MEM_POOLING_OK;
        throwOnRead = false;
        readCalls = 0;
        changes.clear();
        changeThreads.clear();
        lines = {"1000 default file=/dev/hugepages/libvirt/qemu dirty=5 N0=2 N1=3 kernelpagesize_kB=2048"};
        MOCKER(ubse::security::ChangePtraceCapability).stubs().will(invoke(ChangeCapability));
        MOCKER(MpFileUtil::GetFileInfo).stubs().will(invoke(ReadNumaMaps));
        MOCKER(OsHelper::IsNumaLocal).stubs().will(invoke(QueryLocal));
        MOCKER(OsHelper::GetSocketIdByNumaId).stubs().will(invoke(QuerySocket));
    }

    void TearDown() override
    {
        const std::vector<bool> expected = enableResult == MEM_POOLING_OK ? std::vector<bool>{true, false} :
                                                                            std::vector<bool>{true};
        EXPECT_EQ(changes, expected);
        if (dropResult == MEM_POOLING_OK) {
            EXPECT_FALSE(enabled);
        }
        TestOsHelper::TearDown();
    }
};

TEST_F(TestVmNumaPtrace, CollectsNumaInfoAndDropsCapabilityBeforeParsing)
{
    VmDomainInfo info;
    ASSERT_EQ(OsHelper::GetVmNumaInfoByPid(123, info), MEM_POOLING_OK);
    EXPECT_EQ(readCalls, 1U);
    ASSERT_EQ(info.numaInfo.size(), 2U);
    EXPECT_EQ(info.numaInfo.at(0).usedMem, 4096);
    EXPECT_EQ(info.numaInfo.at(0).pageSize, 2048U);
    EXPECT_EQ(info.numaInfo.at(0).socketId, 7);
    EXPECT_EQ(info.numaInfo.at(1).usedMem, 6144);
    EXPECT_EQ(info.numaInfo.at(1).socketId, -1);
}

TEST_F(TestVmNumaPtrace, EnableFailureDoesNotReadFile)
{
    enableResult = MEM_POOLING_ERROR;
    VmDomainInfo info;
    EXPECT_EQ(OsHelper::GetVmNumaInfoByPid(123, info), MEM_POOLING_ERROR);
    EXPECT_EQ(readCalls, 0U);
    EXPECT_TRUE(info.numaInfo.empty());
}

TEST_F(TestVmNumaPtrace, FileFailureDropsCapability)
{
    readResult = MEM_POOLING_ERROR;
    VmDomainInfo info;
    EXPECT_EQ(OsHelper::GetVmNumaInfoByPid(123, info), MEM_POOLING_ERROR);
    EXPECT_EQ(readCalls, 1U);
    EXPECT_TRUE(info.numaInfo.empty());
}

TEST_F(TestVmNumaPtrace, ReadExceptionDropsCapability)
{
    throwOnRead = true;
    VmDomainInfo info;
    EXPECT_ANY_THROW(OsHelper::GetVmNumaInfoByPid(123, info));
    EXPECT_TRUE(info.numaInfo.empty());
}

TEST_F(TestVmNumaPtrace, DropFailureDoesNotParseOrReportSuccess)
{
    dropResult = MEM_POOLING_ERROR;
    VmDomainInfo info;
    EXPECT_EQ(OsHelper::GetVmNumaInfoByPid(123, info), MEM_POOLING_ERROR);
    EXPECT_TRUE(info.numaInfo.empty());
}

TEST_F(TestVmNumaPtrace, ReadExceptionStillAttemptsDropOnDropFailure)
{
    throwOnRead = true;
    dropResult = MEM_POOLING_ERROR;
    VmDomainInfo info;
    EXPECT_ANY_THROW(OsHelper::GetVmNumaInfoByPid(123, info));
}

TEST_F(TestVmNumaPtrace, EmptyNumaMapsReturnsErrorWithoutCapability)
{
    lines.clear();
    VmDomainInfo info;
    EXPECT_EQ(OsHelper::GetVmNumaInfoByPid(123, info), MEM_POOLING_ERROR);
}

TEST_F(TestVmNumaPtrace, LocalQueryFailureDoesNotLeakCapability)
{
    localResult = MEM_POOLING_ERROR;
    VmDomainInfo info;
    EXPECT_EQ(OsHelper::GetVmNumaInfoByPid(123, info), MEM_POOLING_ERROR);
}

TEST_F(TestVmNumaPtrace, SocketQueryFailureDoesNotLeakCapability)
{
    socketResult = MEM_POOLING_ERROR;
    VmDomainInfo info;
    EXPECT_EQ(OsHelper::GetVmNumaInfoByPid(123, info), MEM_POOLING_ERROR);
}

TEST_F(TestVmNumaPtrace, PageSizeConflictDoesNotLeakCapability)
{
    lines.push_back("2000 default file=/dev/hugepages/libvirt/qemu dirty=1 N0=1 kernelpagesize_kB=4096");
    VmDomainInfo info;
    EXPECT_EQ(OsHelper::GetVmNumaInfoByPid(123, info), MEM_POOLING_ERROR);
}

TEST_F(TestVmNumaPtrace, CapabilityChangesRunOnCollectingWorker)
{
    VmDomainInfo info;
    MpResult ret = MEM_POOLING_ERROR;
    std::thread::id workerId;
    std::thread worker([&] {
        workerId = std::this_thread::get_id();
        ret = OsHelper::GetVmNumaInfoByPid(123, info);
    });
    worker.join();
    EXPECT_EQ(ret, MEM_POOLING_OK);
    ASSERT_EQ(changeThreads.size(), 2U);
    EXPECT_EQ(changeThreads[0], workerId);
    EXPECT_EQ(changeThreads[1], workerId);
    EXPECT_NE(workerId, std::this_thread::get_id());
}

} // namespace mempooling::exportV2