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

#include "mempooling_interface.h"
#include "mem_manager.h"
#include "mempool_borrow_module.h"
#include "rmrs_resource_query.h"
#include "mempool_migrate_helper.h"
#include "turbo_rmrs_interface.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)
namespace mempooling {

using namespace mempooling::outinterface;

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
    MOCKER_CPP(&AntiNode::Update, MpResult(*)(
        const std::map<std::string, std::vector<std::string>> &nodeAntiMap))
        .stubs()
        .will(returnValue(0));
    std::map<std::string, std::vector<std::string>> nodeAntiMap;
    nodeAntiMap["a"].push_back("a0");
    auto ret = UBSRMRSUpdateAntiNode(nodeAntiMap);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSUpdateAntiNodeFailed)
{
    MOCKER_CPP(&AntiNode::Update, MpResult(*)(
        const std::map<std::string, std::vector<std::string>> &nodeAntiMap))
        .stubs()
        .will(returnValue(0));
    std::map<std::string, std::vector<std::string>> nodeAntiMap;
    auto ret = UBSRMRSUpdateAntiNode(nodeAntiMap);
    EXPECT_EQ(ret, 1);
}

MpResult MemBorrowStrategyForTest(const mempooling::SrcMemoryBorrowParam &srcParam, const uint64_t borrowSize,
    mempooling::MemBorrowStrategyResult &borrowStrategyResult)
{
    mempooling::DestMemoryBorrowParam param;
    borrowStrategyResult.destParam.push_back(param);
    return MEM_POOLING_OK;
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSMemBorrowStrategySucceed)
{
    MOCKER_CPP(
        &MempoolBorrowModule::MemBorrowStrategy,
        MpResult(*)(const mempooling::SrcMemoryBorrowParam &, const uint64_t, mempooling::MemBorrowStrategyResult &))
        .stubs()
        .will(returnValue(0));
    mempooling::outinterface::SrcMemoryBorrowParam outSrcParam;
    uint64_t borrowSize;
    mempooling::outinterface::MemBorrowStrategyResult outBorrowStrategyResult;
    auto ret = UBSRMRSMemBorrowStrategy(outSrcParam, borrowSize, outBorrowStrategyResult);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSMemBorrowExecuteSucceed)
{
    MOCKER_CPP(&MempoolBorrowModule::MemBorrowExecute, MpResult(*)(const mempooling::SrcMemoryBorrowParam &,
        const std::vector<mempooling::DestMemoryBorrowParam> &, mempooling::MemBorrowExecuteResult &))
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
    MOCKER_CPP(&MempoolBorrowModule::MemBorrowExecute, MpResult(*)(mempooling::SrcMemoryBorrowParam &,
        const std::vector<mempooling::DestMemoryBorrowParam> &, mempooling::MemBorrowExecuteResult &))
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

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSMigrateStrategySucceed)
{
    MOCKER_CPP(&migrate::MempoolMigrateModule::MigrateStrategy, MpResult(*)(std::string,
        std::vector<mempooling::VMPresetParam>, std::uint64_t, turbo::rmrs::MigrateStrategyResult &))
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

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSMigrateExecuteSucceed)
{
    MOCKER_CPP(
        &migrate::MempoolMigrateExecute::MigrateExecute,
        MpResult(*)(std::string, std::vector<turbo::rmrs::VMMigrateOutParam>, uint64_t, std::vector<std::string>))
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

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSMemFreeSucceed)
{
    MOCKER_CPP(&MempoolBorrowModule::MemFree, MpResult(*)(std::string))
        .stubs()
        .will(returnValue(0));
    std::string nodeId{"Node0"};
    auto ret = UBSRMRSMemFree(nodeId);
    EXPECT_EQ(ret, 1);
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSMemBorrowRollbackSucceed)
{
    MOCKER_CPP(mempooling::RpcMemBorrowRollback, MpResult(*)(std::string, const std::vector<std::string> &))
        .stubs()
        .will(returnValue(0));
    std::string borrowInNode{"Node0"};
    std::vector<std::string> borrowIds{"Node1"};
    auto ret = UBSRMRSMemBorrowRollback(borrowInNode, borrowIds);
    EXPECT_EQ(ret, 1);
}

MpResult HelpGetVmInfoListOnNodeForTest(std::vector<mempooling::exportV2::VmDomainInfo> &vmDomainInfos)
{
    mempooling::exportV2::VmDomainInfo vmDomain;
    vmDomainInfos.push_back(vmDomain);
    return MEM_POOLING_OK;
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSGetVmInfoListOnNodeSucceed)
{
    MOCKER_CPP(mempooling::HelpGetVmInfoListOnNode, MpResult(*)(std::vector<mempooling::exportV2::VmDomainInfo> &))
        .stubs()
        .will(invoke(HelpGetVmInfoListOnNodeForTest));
    std::vector<mempooling::outinterface::VmDomainInfo> outVmDomainInfos;
    auto ret = UBSRMRSGetVmInfoListOnNode(outVmDomainInfos);
    EXPECT_EQ(ret, 0);
}

MpResult HelpGetNumaInfoListOnNodeForTest(std::vector<mempooling::exportV2::NumaInfo> &numaInfos)
{
    mempooling::exportV2::NumaInfo numa;
    numaInfos.push_back(numa);
    return MEM_POOLING_OK;
}

TEST_F(TestRackMempoolingInterfaceAdapt, UBSRMRSGetNumaInfoListOnNodeSucceed)
{
    MOCKER_CPP(mempooling::HelpGetNumaInfoListOnNode, MpResult(*)(std::vector<mempooling::exportV2::NumaInfo> &))
        .stubs()
        .will(invoke(HelpGetNumaInfoListOnNodeForTest));
    std::vector<mempooling::outinterface::NumaInfo> outNumaInfos;
    auto ret = UBSRMRSGetNumaInfoListOnNode(outNumaInfos);
    EXPECT_EQ(ret, 0);
}

MpResult HelpGetContainerPidNumaInfoByLocalNodeTest(const std::string &srcNid, const std::vector<pid_t> &pidList,
                                                    std::vector<mempooling::RmrsPidInfo> &pidInfos)
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
         MpResult(*)(const std::string &, const std::vector<pid_t> &, std::vector<mempooling::RmrsPidInfo> &))
        .stubs()
        .will(invoke(HelpGetContainerPidNumaInfoByLocalNodeTest));
    mempooling::outinterface::SrcMemoryBorrowParam srcParam;
    std::vector<pid_t> pids;
    pids.push_back(1234);
    std::vector<mempooling::outinterface::PidInfo> pidInfos;
    auto ret = UBSRMRSPidNumaInfoCollect(srcParam, pids, pidInfos);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

}