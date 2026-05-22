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
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#include "mem_manager.h"
#include "mempool_borrow_module.h"
#include "mempool_migrate_helper.h"
#include "mempooling_interface.h"
#include "mp_configuration.h"
#include "rmrs_resource_query.h"
#include "turbo_rmrs_interface.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)
namespace mempooling {

using namespace mempooling::outinterface;
using namespace std;

// 测试类
class TestRackMempoolingInterfaceAdapt : public ::testing::Test {
public:
    void SetUp() override
    {
        std::cout << "[TestRackMempoolingInterfaceAdapt SetUp Begin]" << std::endl;
        std::cout << "[TestRackMempoolingInterfaceAdapt SetUp End]" << std::endl;
    }
    void TearDown() override
    {
        std::cout << "[TestRackMempoolingInterfaceAdapt TearDown Begin]" << std::endl;
        GlobalMockObject::verify();
        std::cout << "[TestRackMempoolingInterfaceAdapt TearDown End]" << std::endl;
    }
};

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSUpdateAntiNodeSucceed)
{
    MOCKER_CPP(&AntiNode::Update, MpResult (*)(const std::map<std::string, std::vector<std::string>>& nodeAntiMap))
        .stubs()
        .will(returnValue(0));
    std::map<std::string, std::vector<std::string>> nodeAntiMap;
    nodeAntiMap["a"].push_back("a0");
    auto ret = UBSRMRSUpdateAntiNode(nodeAntiMap);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSUpdateAntiNodeFailed)
{
    MOCKER_CPP(&AntiNode::Update, MpResult (*)(const std::map<std::string, std::vector<std::string>>& nodeAntiMap))
        .stubs()
        .will(returnValue(0));
    std::map<std::string, std::vector<std::string>> nodeAntiMap;
    auto ret = UBSRMRSUpdateAntiNode(nodeAntiMap);
    EXPECT_EQ(ret, 1);
}

MpResult MemBorrowStrategyForTest(const mempooling::SrcMemoryBorrowParam& srcParam, const uint64_t borrowSize,
                                  mempooling::MemBorrowStrategyResult& borrowStrategyResult)
{
    mempooling::DestMemoryBorrowParam param;
    borrowStrategyResult.destParam.push_back(param);
    return MEM_POOLING_OK;
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSMemBorrowStrategyFailed1)
{
    MOCKER_CPP(
        &MempoolBorrowModule::MemBorrowStrategy,
        MpResult (*)(const mempooling::SrcMemoryBorrowParam&, const uint64_t, mempooling::MemBorrowStrategyResult&))
        .stubs()
        .will(returnValue(0));
    mempooling::outinterface::SrcMemoryBorrowParam outSrcParam;
    uint64_t borrowSize;
    mempooling::outinterface::MemBorrowStrategyResult outBorrowStrategyResult;
    auto ret = UBSRMRSMemBorrowStrategy(outSrcParam, borrowSize, outBorrowStrategyResult);
    EXPECT_EQ(ret, 1);
}

std::string MockTestGetNodeId()
{
    std::string nodeId = "1";
    return nodeId;
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSMemBorrowStrategySucceed)
{
    MOCKER_CPP(&MpConfiguration::GetNodeId, std::string (*)()).stubs().will(invoke(MockTestGetNodeId));
    MOCKER_CPP(
        &MempoolBorrowModule::MemBorrowStrategy,
        MpResult (*)(const mempooling::SrcMemoryBorrowParam&, const uint64_t, mempooling::MemBorrowStrategyResult&))
        .stubs()
        .will(returnValue(0));
    mempooling::outinterface::SrcMemoryBorrowParam outSrcParam;
    outSrcParam.srcNid = "1";
    uint64_t borrowSize;
    mempooling::outinterface::MemBorrowStrategyResult outBorrowStrategyResult;
    auto ret = UBSRMRSMemBorrowStrategy(outSrcParam, borrowSize, outBorrowStrategyResult);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSMemBorrowExecuteFailed)
{
    MOCKER_CPP(&MpConfiguration::GetNodeId, std::string (*)()).stubs().will(invoke(MockTestGetNodeId));
    MOCKER_CPP(&MempoolBorrowModule::MemBorrowExecute,
               MpResult (*)(const mempooling::SrcMemoryBorrowParam&,
                            const std::vector<mempooling::DestMemoryBorrowParam>&, mempooling::MemBorrowExecuteResult&))
        .stubs()
        .will(returnValue(0));
    mempooling::outinterface::SrcMemoryBorrowParam outSrcParam;
    std::vector<mempooling::outinterface::DestMemoryBorrowParam> outDestParam;
    mempooling::outinterface::MemBorrowExecuteResult outBorrowExecuteResult;
    mempooling::outinterface::DestMemoryBorrowParam param;
    outDestParam.push_back(param);
    auto ret = UBSRMRSMemBorrowExecute(outSrcParam, outDestParam, outBorrowExecuteResult);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSMemBorrowExecuteSucceed)
{
    MOCKER_CPP(&MpConfiguration::GetNodeId, std::string (*)()).stubs().will(invoke(MockTestGetNodeId));
    MOCKER_CPP(&MempoolBorrowModule::MemBorrowExecute,
               MpResult (*)(const mempooling::SrcMemoryBorrowParam&,
                            const std::vector<mempooling::DestMemoryBorrowParam>&, mempooling::MemBorrowExecuteResult&))
        .stubs()
        .will(returnValue(0));
    mempooling::outinterface::SrcMemoryBorrowParam outSrcParam;
    outSrcParam.srcNid = "1";
    std::vector<mempooling::outinterface::DestMemoryBorrowParam> outDestParam;
    outDestParam.emplace_back(mempooling::outinterface::DestMemoryBorrowParam{"2", 0, 1, {0}, {512 * 1024}});
    outDestParam.emplace_back(mempooling::outinterface::DestMemoryBorrowParam{"3", 1, 1, {2}, {512 * 1024}});
    mempooling::outinterface::MemBorrowExecuteResult outBorrowExecuteResult;
    mempooling::outinterface::DestMemoryBorrowParam param;
    outDestParam.push_back(param);
    auto ret = UBSRMRSMemBorrowExecute(outSrcParam, outDestParam, outBorrowExecuteResult);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSMemBorrowExecuteFailed1)
{
    mempooling::outinterface::SrcMemoryBorrowParam outSrcParam;
    std::vector<mempooling::outinterface::DestMemoryBorrowParam> outDestParam;
    mempooling::outinterface::MemBorrowExecuteResult outBorrowExecuteResult;
    auto ret = UBSRMRSMemBorrowExecute(outSrcParam, outDestParam, outBorrowExecuteResult);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSMemBorrowExecuteFailed2)
{
    MOCKER_CPP(&MempoolBorrowModule::MemBorrowExecute,
               MpResult (*)(mempooling::SrcMemoryBorrowParam&, const std::vector<mempooling::DestMemoryBorrowParam>&,
                            mempooling::MemBorrowExecuteResult&))
        .stubs()
        .will(returnValue(1));
    mempooling::outinterface::SrcMemoryBorrowParam outSrcParam;
    std::vector<mempooling::outinterface::DestMemoryBorrowParam> outDestParam;
    mempooling::outinterface::MemBorrowExecuteResult outBorrowExecuteResult;

    mempooling::outinterface::DestMemoryBorrowParam param;
    outDestParam.push_back(param);
    auto ret = UBSRMRSMemBorrowExecute(outSrcParam, outDestParam, outBorrowExecuteResult);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSMigrateStrategyFailed1)
{
    MOCKER_CPP(&migrate::MempoolMigrateModule::MigrateStrategy,
               MpResult (*)(std::string, std::vector<mempooling::VMPresetParam>, std::uint64_t,
                            turbo::rmrs::MigrateStrategyResult&))
        .stubs()
        .will(returnValue(0));

    std::string borrowInNode{"Node1"};
    std::vector<mempooling::outinterface::VMPresetParam> outVmInfoList;
    mempooling::outinterface::VMPresetParam param;
    outVmInfoList.push_back(param);
    uint64_t borrowSize;
    mempooling::outinterface::MigrateStrategyResult outMigrateStrategyResult;

    auto ret = UBSRMRSMigrateStrategy(borrowInNode, outVmInfoList, borrowSize, outMigrateStrategyResult);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSMigrateStrategySucceed)
{
    MOCKER_CPP(&migrate::MempoolMigrateModule::MigrateStrategy,
               MpResult (*)(std::string, std::vector<mempooling::VMPresetParam>, std::uint64_t,
                            turbo::rmrs::MigrateStrategyResult&))
        .stubs()
        .will(returnValue(0));
    std::string nodeId = "Node1";
    MOCKER_CPP(&MpConfiguration::GetNodeId, std::string (*)()).stubs().will(returnValue(nodeId));
    std::string borrowInNode{nodeId};
    std::vector<mempooling::outinterface::VMPresetParam> outVmInfoList;
    mempooling::outinterface::VMPresetParam param;
    outVmInfoList.push_back(param);
    uint64_t borrowSize;
    mempooling::outinterface::MigrateStrategyResult outMigrateStrategyResult;

    auto ret = UBSRMRSMigrateStrategy(borrowInNode, outVmInfoList, borrowSize, outMigrateStrategyResult);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSMigrateExecuteFailed)
{
    MOCKER_CPP(
        &migrate::MempoolMigrateExecute::MigrateExecute,
        MpResult (*)(std::string, std::vector<turbo::rmrs::VMMigrateOutParam>, uint64_t, std::vector<std::string>))
        .stubs()
        .will(returnValue(0));
    std::string borrowInNode{"Node0"};
    std::vector<mempooling::outinterface::VMMigrateOutParam> outVmInfoList;
    mempooling::outinterface::VMMigrateOutParam param;
    outVmInfoList.push_back(param);
    uint64_t waitingTime;
    std::vector<std::string> borrowIdList;
    borrowIdList.push_back("Node0");
    auto ret = UBSRMRSMigrateExecute(borrowInNode, outVmInfoList, waitingTime, borrowIdList);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSMigrateExecuteSucceed)
{
    MOCKER_CPP(
        &migrate::MempoolMigrateExecute::MigrateExecute,
        MpResult (*)(std::string, std::vector<turbo::rmrs::VMMigrateOutParam>, uint64_t, std::vector<std::string>))
        .stubs()
        .will(returnValue(0));

    std::string nodeId = "Node0";
    MOCKER_CPP(&MpConfiguration::GetNodeId, std::string (*)()).stubs().will(returnValue(nodeId));
    std::string borrowInNode{nodeId};
    std::vector<mempooling::outinterface::VMMigrateOutParam> outVmInfoList;
    mempooling::outinterface::VMMigrateOutParam param;
    outVmInfoList.push_back(param);
    uint64_t waitingTime = 100;
    std::vector<std::string> borrowIdList;
    borrowIdList.push_back(nodeId);
    auto ret = UBSRMRSMigrateExecute(borrowInNode, outVmInfoList, waitingTime, borrowIdList);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSMemFreeFailed)
{
    MOCKER_CPP(&MempoolBorrowModule::MemFree, MpResult (*)(std::string)).stubs().will(returnValue(0));
    std::string nodeId{"Node0"};
    auto ret = UBSRMRSMemFree(nodeId);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSMemFreeSucceed)
{
    MOCKER_CPP(&MpConfiguration::GetNodeId, std::string (*)()).stubs().will(invoke(MockTestGetNodeId));
    MOCKER_CPP(&MempoolBorrowModule::MemFree, MpResult (*)(std::string)).stubs().will(returnValue(0));
    std::string nodeId{"1"};
    auto ret = UBSRMRSMemFree(nodeId);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSMemBorrowRollbackSucceed)
{
    MOCKER_CPP(&MpConfiguration::GetNodeId, std::string (*)()).stubs().will(invoke(MockTestGetNodeId));
    MOCKER_CPP(mempooling::RpcMemBorrowRollback, MpResult (*)(std::string, const std::vector<std::string>&))
        .stubs()
        .will(returnValue(0));
    std::string borrowInNode{"1"};
    std::vector<std::string> borrowIds{"Node1"};
    auto ret = UBSRMRSMemBorrowRollback(borrowInNode, borrowIds);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSMemBorrowRollbackFailed)
{
    MOCKER_CPP(mempooling::RpcMemBorrowRollback, MpResult (*)(std::string, const std::vector<std::string>&))
        .stubs()
        .will(returnValue(0));
    std::string borrowInNode{"Node0"};
    std::vector<std::string> borrowIds{"Node1"};
    auto ret = UBSRMRSMemBorrowRollback(borrowInNode, borrowIds);
    EXPECT_EQ(ret, 1);
}

MpResult HelpGetVmInfoListOnNodeForTest(std::vector<mempooling::exportV2::VmDomainInfo>& vmDomainInfos)
{
    mempooling::exportV2::VmDomainInfo vmDomain;
    vmDomainInfos.push_back(vmDomain);
    return MEM_POOLING_OK;
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSGetVmInfoListOnNodeSucceed)
{
    MOCKER_CPP(mempooling::HelpGetVmInfoListOnNode, MpResult (*)(std::vector<mempooling::exportV2::VmDomainInfo>&))
        .stubs()
        .will(invoke(HelpGetVmInfoListOnNodeForTest));
    std::vector<mempooling::outinterface::VmDomainInfo> outVmDomainInfos;
    auto ret = UBSRMRSGetVmInfoListOnNode(outVmDomainInfos);
    EXPECT_EQ(ret, 0);
}

MpResult HelpGetNumaInfoListOnNodeForTest(std::vector<mempooling::exportV2::NumaInfo>& numaInfos)
{
    mempooling::exportV2::NumaInfo numa;
    numaInfos.push_back(numa);
    return MEM_POOLING_OK;
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSGetNumaInfoListOnNodeSucceed)
{
    MOCKER_CPP(mempooling::HelpGetNumaInfoListOnNode, MpResult (*)(std::vector<mempooling::exportV2::NumaInfo>&))
        .stubs()
        .will(invoke(HelpGetNumaInfoListOnNodeForTest));
    std::vector<mempooling::outinterface::NumaInfo> outNumaInfos;
    auto ret = UBSRMRSGetNumaInfoListOnNode(outNumaInfos);
    EXPECT_EQ(ret, 0);
}

MpResult HelpGetContainerPidNumaInfoByLocalNodeTest(const std::string& srcNid, const std::vector<pid_t>& pidList,
                                                    std::vector<mempooling::RmrsPidInfo>& pidInfos)
{
    mempooling::RmrsPidInfo info;
    info.pid = 1234;
    info.localNumaIds.push_back(0);
    info.totalLocalUsedMem = 1024;
    info.totalRemoteUsedMem = 0;
    pidInfos.push_back(info);
    return MEM_POOLING_OK;
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSPidNumaInfoCollectSucceed)
{
    MOCKER_CPP(ResourceQuery::HelpGetContainerPidNumaInfoByLocalNode,
               MpResult (*)(const std::string&, const std::vector<pid_t>&, std::vector<mempooling::RmrsPidInfo>&))
        .stubs()
        .will(invoke(HelpGetContainerPidNumaInfoByLocalNodeTest));
    mempooling::outinterface::SrcMemoryBorrowParam srcParam;
    std::vector<pid_t> pids;
    pids.push_back(1234);
    std::vector<mempooling::outinterface::PidInfo> pidInfos;
    auto ret = UBSRMRSPidNumaInfoCollect(srcParam, pids, pidInfos);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSSetRunModeSucceed)
{
    MOCKER_CPP(mempooling::smap::MpSmapHelper::SetRunModeAndWrite, MpResult (*)(int)).stubs().will(returnValue(0));
    int runMode = 0;
    auto ret = UBSRMRSSetRunMode(runMode);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSSetRunModeSucceed1)
{
    MOCKER_CPP(mempooling::smap::MpSmapHelper::SetRunModeAndWrite, MpResult (*)(int)).stubs().will(returnValue(0));
    int runMode = 1;
    auto ret = UBSRMRSSetRunMode(runMode);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSSetRunModeFailed)
{
    MOCKER_CPP(mempooling::smap::MpSmapHelper::SetRunModeAndWrite, MpResult (*)(int)).stubs().will(returnValue(1));
    int runMode = 0;
    auto ret = UBSRMRSSetRunMode(runMode);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSSmapAddProcessTrackingSucceed)
{
    MOCKER_CPP(mempooling::smap::MpSmapHelper::SmapAddProcessTrackingHelper,
               MpResult (*)(const std::vector<pid_t>&, const std::vector<uint32_t>&, int, const std::vector<uint32_t>&))
        .stubs()
        .will(returnValue(0));
    std::vector<pid_t> pidVec{3};
    std::vector<uint32_t> scanTimeVec{3};
    int scanType = 0;
    std::vector<uint32_t> durationVec{3};
    auto ret = UBSRMRSSmapAddProcessTracking(pidVec, scanTimeVec, scanType, durationVec);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSSmapAddProcessTrackingFailed1)
{
    MOCKER_CPP(mempooling::smap::MpSmapHelper::SmapAddProcessTrackingHelper,
               MpResult (*)(const std::vector<pid_t>&, const std::vector<uint32_t>&, int, const std::vector<uint32_t>&))
        .stubs()
        .will(returnValue(0));
    std::vector<pid_t> pidVec;
    std::vector<uint32_t> scanTimeVec;
    int scanType;
    std::vector<uint32_t> durationVec(0, 10);
    auto ret = UBSRMRSSmapAddProcessTracking(pidVec, scanTimeVec, scanType, durationVec);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSSmapQueryFreqSucceed)
{
    MOCKER_CPP(mempooling::smap::MpSmapHelper::QueryVMFreqArray, int (*)(int, uint16_t*, uint32_t, uint32_t&, int))
        .stubs()
        .will(returnValue(0));
    std::vector<pid_t> pidVec;
    std::vector<uint32_t> scanTimeVec;
    int scanType;
    std::vector<uint32_t> durationVec(0, 10);
    pid_t pid;
    std::vector<uint16_t> dataVec = {1, 2, 3};
    uint32_t lengthIn = 4;
    uint32_t lengthOut = 3;
    int dataSource;
    auto ret = UBSRMRSSmapQueryFreq(pid, dataVec, lengthIn, lengthOut, dataSource);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSSmapRemoveProcessTrackingSucceed)
{
    MOCKER_CPP(mempooling::smap::MpSmapHelper::SmapRemoveProcessTrackingHelper, int (*)(const std::vector<pid_t>&, int))
        .stubs()
        .will(returnValue(0));
    std::vector<pid_t> pidVec = {10086, 10010};
    int flags;
    auto ret = UBSRMRSSmapRemoveProcessTracking(pidVec, flags);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

} // namespace mempooling