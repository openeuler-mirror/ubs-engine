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
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"
#define private public
#include "exporter.h"
#undef private
#include <stdexcept>
#include "LibvirtHelper.h"
#include "OsHelper.h"

#include <sys/resource.h>
#include <atomic>
#include <cstdlib>
#include <new>

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

using namespace mempooling::exportV2;
namespace mempooling::exportV2 {
VmDomainInfo genVmDomainInfo(const std::string& vmName);

class ExporterTest : public ::testing::Test {
    void SetUp() override
    {
        std::cout << "[ExporterTest SetUp Begin]" << std::endl;
        GlobalMockObject::verify();
        GlobalMockObject::reset();
        std::cout << "[ExporterTest SetUp End]" << std::endl;
    }
    void TearDown() override
    {
        std::cout << "[ExporterTest TearDown Begin]" << std::endl;
        GlobalMockObject::verify();
        GlobalMockObject::reset();
        std::cout << "[ExporterTest TearDown End]" << std::endl;
    }
};

TEST_F(ExporterTest, TestInit_01)
{
    Exporter::Options opt;

    MOCKER_CPP(&mempooling::exportV2::LibvirtHelper::Init, MpResult (*)()).stubs().will(returnValue(MEM_POOLING_OK));

    Exporter::inited_.store(true, std::memory_order_release);
    MpResult ret = Exporter::Init(opt);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(ExporterTest, TestInit_02)
{
    Exporter::Options opt;

    MOCKER_CPP(&mempooling::exportV2::LibvirtHelper::Init, MpResult (*)()).stubs().will(returnValue(MEM_POOLING_ERROR));
    Exporter::inited_.store(false, std::memory_order_release);
    MpResult ret = Exporter::Init(opt);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(ExporterTest, genVmDomainInfo_1)
{
    std::string vmName = "vm01";
    MOCKER(OsHelper::GetHostName).stubs().will(returnValue(MEM_POOLING_ERROR));
    MOCKER(OsHelper::GetVmPidByVmName).stubs().will(returnValue(MEM_POOLING_ERROR));
    MOCKER(OsHelper::GetProcessStartTimeByPid).stubs().will(returnValue(MEM_POOLING_ERROR));
    MOCKER(OsHelper::GetVmNumaInfoByPid).stubs().will(returnValue(MEM_POOLING_ERROR));
    MOCKER(OsHelper::GetSocketIdByNumaId).stubs().will(returnValue(MEM_POOLING_ERROR));
    MOCKER_CPP(&LibvirtHelper::GetDomainByName, MpResult (*)(std::string& hostName))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MOCKER_CPP(&LibvirtHelper::GetVmUuidByDomain, MpResult (*)(VirDomainPtr domain, std::string& uuidStr))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MOCKER_CPP(&LibvirtHelper::GetVmStateAndMaxMemByDomain, MpResult (*)(VirDomainPtr domain, VmDomainInfo& vmInfo))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    VmDomainInfo ret = Exporter::genVmDomainInfo(vmName);
    EXPECT_EQ(ret.metaData.name, vmName);
}

TEST_F(ExporterTest, genVmDomainInfo_2)
{
    std::string vmName = "vm01";
    MOCKER(OsHelper::GetHostName).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(OsHelper::GetVmPidByVmName).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(OsHelper::GetProcessStartTimeByPid).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(OsHelper::GetVmNumaInfoByPid).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(OsHelper::GetSocketIdByNumaId).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&LibvirtHelper::GetDomainByName, MpResult (*)(std::string& hostName))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MOCKER_CPP(&LibvirtHelper::GetVmUuidByDomain, MpResult (*)(VirDomainPtr domain, std::string& uuidStr))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MOCKER_CPP(&LibvirtHelper::GetVmStateAndMaxMemByDomain, MpResult (*)(VirDomainPtr domain, VmDomainInfo& vmInfo))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    VmDomainInfo ret = Exporter::genVmDomainInfo(vmName);
    EXPECT_EQ(ret.metaData.name, vmName);
}

TEST_F(ExporterTest, genVmDomainInfo_3)
{
    std::string vmName = "vm01";
    MOCKER(OsHelper::GetHostName).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(OsHelper::GetVmPidByVmName).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(OsHelper::GetProcessStartTimeByPid).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(OsHelper::GetVmNumaInfoByPid).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(OsHelper::GetSocketIdByNumaId).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&LibvirtHelper::GetDomainByName, MpResult (*)(std::string& hostName))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&LibvirtHelper::GetVmUuidByDomain, MpResult (*)(VirDomainPtr domain, std::string& uuidStr))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&LibvirtHelper::GetVmStateAndMaxMemByDomain, MpResult (*)(VirDomainPtr domain, VmDomainInfo& vmInfo))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));

    VmDomainInfo ret = Exporter::genVmDomainInfo(vmName);
    EXPECT_EQ(ret.metaData.name, vmName);
}
TEST_F(ExporterTest, genNumaInfo_1)
{
    std::uint16_t numaId = 1;
    NumaInfo ret;

    MOCKER(OsHelper::GetHostName).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(OsHelper::IsNumaLocal).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(OsHelper::GetMemInfoByNumaId).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(OsHelper::GetSocketIdByNumaId).stubs().will(returnValue(MEM_POOLING_OK));

    ret = Exporter::genNumaInfo(numaId);
    EXPECT_EQ(ret.metaData.numaId, numaId);
}

TEST_F(ExporterTest, genNumaInfo_2)
{
    std::uint16_t numaId = 1;
    NumaInfo ret;

    MOCKER(OsHelper::GetHostName).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(OsHelper::IsNumaLocal).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(OsHelper::GetMemInfoByNumaId).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(OsHelper::GetSocketIdByNumaId).stubs().will(returnValue(MEM_POOLING_OK));

    ret = Exporter::genNumaInfo(numaId);
    EXPECT_EQ(ret.metaData.socketId, -1);
}

TEST_F(ExporterTest, GetNumaInfoImmediately_ok)
{
    Exporter::inited_.store(true, std::memory_order_release);
    std::vector<NumaInfo> numaInfos;

    MOCKER(OsHelper::GetNumaSet).stubs().will(returnValue(MEM_POOLING_OK));

    MpResult ret = Exporter::GetNumaInfoImmediately(numaInfos);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

static MpResult ThrowGetNumaSetStd(std::vector<std::uint16_t>& /*numaSet*/)
{
    throw std::runtime_error("GetNumaSet failed");
}

static MpResult ThrowGetNumaSetUnknown(std::vector<std::uint16_t>& /*numaSet*/)
{
    throw 1; // 非 std::exception
}

TEST_F(ExporterTest, GetNumaInfoImmediately_throw_std_exception_in_subprocess)
{
    ASSERT_EXIT(
        []() {
            std::vector<NumaInfo> numaInfos;
            Exporter::inited_.store(true, std::memory_order_release);

            MOCKER_CPP(&mempooling::exportV2::OsHelper::GetNumaSet, MpResult (*)(std::vector<std::uint16_t>&))
                .stubs()
                .will(invoke(ThrowGetNumaSetStd));

            MpResult ret = Exporter::GetNumaInfoImmediately(numaInfos);
            std::exit(ret == MEM_POOLING_ERROR ? 0 : 1);
        }(),
        ::testing::ExitedWithCode(0), "");
}

TEST_F(ExporterTest, GetNumaInfoImmediately_throw_unknown_exception_in_subprocess)
{
    ASSERT_EXIT(
        []() {
            std::vector<NumaInfo> numaInfos;
            Exporter::inited_.store(true, std::memory_order_release);

            MOCKER_CPP(&mempooling::exportV2::OsHelper::GetNumaSet, MpResult (*)(std::vector<std::uint16_t>&))
                .stubs()
                .will(invoke(ThrowGetNumaSetUnknown));

            MpResult ret = Exporter::GetNumaInfoImmediately(numaInfos);

            std::exit(ret == MEM_POOLING_ERROR ? 0 : 1);
        }(),
        ::testing::ExitedWithCode(0), "");
}

static MpResult ThrowGetVmNameSetStd(std::vector<std::string>& /*vmNameSet*/)
{
    throw std::runtime_error("GetVmNameSet failed");
}

static MpResult ThrowGetVmNameSetUnknown(std::vector<std::string>& /*vmNameSet*/)
{
    throw 1;
}

TEST_F(ExporterTest, GetVmInfoImmediately_throw_std_exception_in_subprocess)
{
    ASSERT_EXIT(
        []() {
            std::vector<mempooling::exportV2::VmDomainInfo> vmInfos;
            mempooling::exportV2::Exporter::inited_.store(true, std::memory_order_release);

            MOCKER_CPP(&mempooling::exportV2::OsHelper::GetVmNameSet, MpResult (*)(std::vector<std::string>&))
                .stubs()
                .will(invoke(ThrowGetVmNameSetStd));

            MpResult ret = mempooling::exportV2::Exporter::GetVmInfoImmediately(vmInfos);
            std::exit(ret == MEM_POOLING_ERROR ? 0 : 1);
        }(),
        ::testing::ExitedWithCode(0), "");
}

TEST_F(ExporterTest, GetVmInfoImmediately_throw_unknown_exception_in_subprocess)
{
    ASSERT_EXIT(
        []() {
            std::vector<mempooling::exportV2::VmDomainInfo> vmInfos;
            mempooling::exportV2::Exporter::inited_.store(true, std::memory_order_release);

            MOCKER_CPP(&mempooling::exportV2::OsHelper::GetVmNameSet, MpResult (*)(std::vector<std::string>&))
                .stubs()
                .will(invoke(ThrowGetVmNameSetUnknown));

            MpResult ret = mempooling::exportV2::Exporter::GetVmInfoImmediately(vmInfos);
            std::exit(ret == MEM_POOLING_ERROR ? 0 : 1);
        }(),
        ::testing::ExitedWithCode(0), "");
}

static MpResult StubGetVmNameSet_AndClampMem(std::vector<std::string>& vmNameSet)
{
    vmNameSet.clear();
    vmNameSet.emplace_back("vm-test-0"); // 小集合，保证会进入 parallel_collect

    // 1) 先占一大块内存（尽量让后续分配更容易失败）
    //    用 static 保证在函数返回后仍然占着
    static void* hog = nullptr;
    if (!hog) {
        // 例如占 96MB（可根据你环境调大/调小）
        hog = std::malloc(96 * 1024 * 1024);
        if (hog)
            std::memset(hog, 0xA5, 96 * 1024 * 1024);
    }

    // 2) 再把地址空间限制到一个比较小的值（略高于当前占用即可）
    //    这样 parallel_collect 里的任何 new/malloc 更容易 bad_alloc
    struct rlimit rl;
    rl.rlim_cur = 128 * 1024 * 1024; // 128MB（按 hog 大小调：hog=96MB 就给 128MB）
    rl.rlim_max = 128 * 1024 * 1024;
    (void)setrlimit(RLIMIT_AS, &rl);

    return MEM_POOLING_OK;
}

TEST_F(ExporterTest, GetVmInfoImmediately_bad_alloc_in_parallel_collect_in_subprocess)
{
    ASSERT_EXIT(
        []() {
            using namespace mempooling::exportV2;

            // 让 Exporter 处于 inited 状态且线程池已创建
            Exporter::Options opt;
            opt.threads = 1;

            MOCKER_CPP(&mempooling::exportV2::LibvirtHelper::Init, MpResult (*)())
                .stubs()
                .will(returnValue(MEM_POOLING_OK));

            MpResult ir = Exporter::Init(opt);
            if (ir != MEM_POOLING_OK)
                std::exit(2);

            // GetVmNameSet：小集合 + 在返回前“挤爆/限死”内存
            MOCKER_CPP(&mempooling::exportV2::OsHelper::GetVmNameSet, MpResult (*)(std::vector<std::string>&))
                .stubs()
                .will(invoke(StubGetVmNameSet_AndClampMem));

            // 避免在这里分配导致提前失败：直接返回 OK
            MOCKER_CPP(&mempooling::exportV2::LibvirtHelper::CheckConnectAndReconnect, MpResult (*)())
                .stubs()
                .will(returnValue(MEM_POOLING_OK));

            std::vector<VmDomainInfo> vmInfos;
            MpResult ret = Exporter::GetVmInfoImmediately(vmInfos);

            // 期望：parallel_collect 内部某处分配触发 bad_alloc，被 GetVmInfoImmediately catch，
            // 返回 MEM_POOLING_ERROR
            std::exit(ret == MEM_POOLING_ERROR ? 0 : 1);
        }(),
        ::testing::ExitedWithCode(0), "");
}

} // namespace mempooling::exportV2