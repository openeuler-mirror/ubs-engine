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
#include <mockcpp/mockcpp.hpp>
#include "gtest/gtest.h"

#include <rmrs_serialize.h>
#include <securec.h>
#include "ubse_com.h"
#include "ubse_event.h"
#include "ubse_pointer_process.h"
#include "event_handler.h"
#include "mempooling_message.h"
#include "mp_configuration.h"
#include "mp_error.h"
#include "mp_json_util.h"
#include "mp_string_util.h"
#include "numa_memInfo_send.h"
#include "over_commit_msg_handler.h"
#include "over_commit_serializer.h"

#define private public
#include "rmrs_resource_query.h"
#undef private

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace mempooling {
using namespace ubse::log;
using namespace mempooling::event;

MpResult FillNumaInfo(mempooling::NumaMetaData& numaInfo, JSON_MAP numaInfoStrMap);

// 测试类
class TestResourceQuery : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

TEST_F(TestResourceQuery, FillNumaInfo_EmptyMap)
{
    NumaMetaData numaInfo{};
    JSON_MAP numaInfoStrMapEmpty{};

    uint32_t ret = FillNumaInfo(numaInfo, numaInfoStrMapEmpty);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestResourceQuery, FillNumaInfo_OnlyMemTotal)
{
    NumaMetaData numaInfo{};
    JSON_MAP numaInfoStrMap = {{"MemTotal", "0"}};

    uint32_t ret = FillNumaInfo(numaInfo, numaInfoStrMap);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestResourceQuery, FillNumaInfo_MemTotalMemFree)
{
    NumaMetaData numaInfo{};
    JSON_MAP numaInfoStrMap = {{"MemTotal", "0"}, {"MemFree", "0"}};

    uint32_t ret = FillNumaInfo(numaInfo, numaInfoStrMap);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestResourceQuery, FillNumaInfo_WithHugePagesTotal)
{
    NumaMetaData numaInfo{};
    JSON_MAP numaInfoStrMap = {{"MemTotal", "0"}, {"MemFree", "0"}, {"HugePages_Total", "0"}};

    uint32_t ret = FillNumaInfo(numaInfo, numaInfoStrMap);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestResourceQuery, FillNumaInfo_WithHugePagesFree)
{
    NumaMetaData numaInfo{};
    JSON_MAP numaInfoStrMap = {{"MemTotal", "0"}, {"MemFree", "0"}, {"HugePages_Total", "0"}, {"HugePages_Free", "0"}};

    uint32_t ret = FillNumaInfo(numaInfo, numaInfoStrMap);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestResourceQuery, FillNumaInfo_WithSocketId)
{
    NumaMetaData numaInfo{};
    JSON_MAP numaInfoStrMap = {
        {"MemTotal", "0"}, {"MemFree", "0"}, {"HugePages_Total", "0"}, {"HugePages_Free", "0"}, {"SocketId", "0"}};

    uint32_t ret = FillNumaInfo(numaInfo, numaInfoStrMap);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestResourceQuery, HelpGetNumaMemInfoCollect_SendFail)
{
    std::string srcNid = "0";
    int numaId = 0;
    NumaMetaData numaInfo{};

    MOCKER_CPP(&over_commit::NumaMemInfoSend::SendMsg, MpResult (*)()).stubs().will(returnValue(MEM_POOLING_ERROR));

    uint32_t ret = ResourceQuery::HelpGetNumaMemInfoCollect(srcNid, numaId, numaInfo);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestResourceQuery, HelpGetNumaMemInfoCollect_SendOK_JsonFail)
{
    std::string srcNid = "0";
    int numaId = 0;
    NumaMetaData numaInfo{};

    MOCKER_CPP(&over_commit::NumaMemInfoSend::SendMsg, MpResult (*)()).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(&JsonUtil::RackMemConvertJsonStr2Map).stubs().will(returnValue(false));

    uint32_t ret = ResourceQuery::HelpGetNumaMemInfoCollect(srcNid, numaId, numaInfo);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestResourceQuery, HelpGetNumaMemInfoCollect_SendOK_JsonOK_FillFail)
{
    std::string srcNid = "0";
    int numaId = 0;
    NumaMetaData numaInfo{};

    MOCKER_CPP(&over_commit::NumaMemInfoSend::SendMsg, MpResult (*)()).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(&JsonUtil::RackMemConvertJsonStr2Map).stubs().will(returnValue(true));
    MOCKER(&FillNumaInfo).stubs().will(returnValue(MEM_POOLING_ERROR));

    uint32_t ret = ResourceQuery::HelpGetNumaMemInfoCollect(srcNid, numaId, numaInfo);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestResourceQuery, HelpGetNumaMemInfoCollect_Success)
{
    std::string srcNid = "0";
    int numaId = 0;
    NumaMetaData numaInfo{};

    MOCKER_CPP(&over_commit::NumaMemInfoSend::SendMsg, MpResult (*)()).stubs().will(returnValue(MEM_POOLING_OK));
    MOCKER(&JsonUtil::RackMemConvertJsonStr2Map).stubs().will(returnValue(true));
    MOCKER(&FillNumaInfo).stubs().will(returnValue(MEM_POOLING_OK));

    uint32_t ret = ResourceQuery::HelpGetNumaMemInfoCollect(srcNid, numaId, numaInfo);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

MpResult ExporterGetVmInfoImmediatelySuccessMock(std::vector<mempooling::exportV2::VmDomainInfo>& vmDomainInfos)
{
    return MEM_POOLING_OK;
}

MpResult ExporterGetVmInfoImmediatelyThrowMock(std::vector<mempooling::exportV2::VmDomainInfo>& vmDomainInfos)
{
    throw std::runtime_error("Mock Exception for GetVmInfoImmediately");
}

MpResult ExporterGetNumaInfoImmediatelySuccessMock(std::vector<mempooling::exportV2::NumaInfo>& numaInfos)
{
    return MEM_POOLING_OK;
}

MpResult ExporterGetNumaInfoImmediatelyThrowMock(std::vector<mempooling::exportV2::NumaInfo>& numaInfos)
{
    throw std::runtime_error("Mock Exception for GetNumaInfoImmediately");
}

MpResult ExporterInitMockFail()
{
    return MEM_POOLING_ERROR;
}

uint32_t UbseRpcSendMockOk(const ubse::com::UbseComEndpoint& endpoint, const UbseByteBuffer& req, void* ctx,
                           const UbseComRespHandler& handler)
{
    if (ctx != nullptr) {
        auto* res = static_cast<turbo::rmrs::PidNumaInfoCollectResult*>(ctx);
        turbo::rmrs::PidInfo info;
        info.pid = 1234;
        info.totalLocalUsedMem = 1000;
        info.totalRemoteUsedMem = 2000;
        info.localNumaIds.push_back(0);

        turbo::rmrs::MetaNumaInfo meta;
        meta.numaId = 1;
        meta.numaUsedMem = 2000;
        meta.isLocalNuma = false;
        meta.socketId = 0;
        info.metaNumaInfos.push_back(meta);

        res->pidInfoList.push_back(info);
    }
    return MEM_POOLING_OK;
}

uint32_t UbseRpcSendMockInvalidPid(const ubse::com::UbseComEndpoint& endpoint, const UbseByteBuffer& req, void* ctx,
                                   const UbseComRespHandler& handler)
{
    if (ctx != nullptr) {
        auto* res = static_cast<turbo::rmrs::PidNumaInfoCollectResult*>(ctx);
        turbo::rmrs::PidInfo info;
        info.pid = -1; // Trigger exception branch
        res->pidInfoList.push_back(info);
    }
    return MEM_POOLING_OK;
}

MpResult PidNumaInfoCollectHandlerMockSuccess(const turbo::rmrs::PidNumaInfoCollectParam& param,
                                              turbo::rmrs::PidNumaInfoCollectResult& result)
{
    turbo::rmrs::PidInfo info;
    info.pid = 5678;
    info.totalLocalUsedMem = 1024;
    info.totalRemoteUsedMem = 1024;
    info.localNumaIds.push_back(0);

    turbo::rmrs::MetaNumaInfo meta;
    meta.numaId = 2;
    meta.numaUsedMem = 1024;
    meta.isLocalNuma = false;
    meta.socketId = 1;
    info.metaNumaInfos.push_back(meta);

    result.pidInfoList.push_back(info);
    return MEM_POOLING_OK;
}

// =========================================================================
// Test case area
// =========================================================================

// 1. Test HelpGetVmInfoListOnNode
TEST_F(TestResourceQuery, HelpGetVmInfoListOnNode_Succeed)
{
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult (*)(std::vector<mempooling::exportV2::VmDomainInfo>&))
        .stubs()
        .will(invoke(ExporterGetVmInfoImmediatelySuccessMock));

    std::vector<mempooling::exportV2::VmDomainInfo> vmDomainInfos;
    auto ret = HelpGetVmInfoListOnNode(vmDomainInfos);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestResourceQuery, HelpGetVmInfoListOnNode_Exception)
{
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult (*)(std::vector<mempooling::exportV2::VmDomainInfo>&))
        .stubs()
        .will(invoke(ExporterGetVmInfoImmediatelyThrowMock)); // Trigger std::exception catch branch
    std::vector<mempooling::exportV2::VmDomainInfo> vmDomainInfos;
    auto ret = HelpGetVmInfoListOnNode(vmDomainInfos);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

// 2. Test HelpGetNumaInfoListOnNode
TEST_F(TestResourceQuery, HelpGetNumaInfoListOnNode_Succeed)
{
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetNumaInfoImmediately,
               MpResult (*)(std::vector<mempooling::exportV2::NumaInfo>&))
        .stubs()
        .will(invoke(ExporterGetNumaInfoImmediatelySuccessMock));

    std::vector<mempooling::exportV2::NumaInfo> numaInfos;
    auto ret = HelpGetNumaInfoListOnNode(numaInfos);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestResourceQuery, HelpGetNumaInfoListOnNode_Exception)
{
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetNumaInfoImmediately,
               MpResult (*)(std::vector<mempooling::exportV2::NumaInfo>&))
        .stubs()
        .will(invoke(ExporterGetNumaInfoImmediatelyThrowMock));

    std::vector<mempooling::exportV2::NumaInfo> numaInfos;
    auto ret = HelpGetNumaInfoListOnNode(numaInfos);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

// 3. Test ConvertMetaNumaInfos (Pure data conversion logic, no stubbing required)
TEST_F(TestResourceQuery, ConvertMetaNumaInfos_Succeed)
{
    std::vector<turbo::rmrs::MetaNumaInfo> turboMetaInfos;
    turbo::rmrs::MetaNumaInfo inMeta;
    inMeta.numaId = 1;
    inMeta.numaUsedMem = 2048;
    inMeta.isLocalNuma = true;
    inMeta.socketId = 0;
    turboMetaInfos.push_back(inMeta);

    RmrsPidInfo pidInfo;
    ResourceQuery rq;
    auto ret = rq.ConvertMetaNumaInfos(turboMetaInfos, pidInfo);

    EXPECT_EQ(ret, MEM_POOLING_OK);
    ASSERT_EQ(pidInfo.metaNumaInfos.size(), 1);
    EXPECT_EQ(pidInfo.metaNumaInfos[0].numaId, 1);
    EXPECT_EQ(pidInfo.metaNumaInfos[0].numaUsedMem, 2048);
}

// 4. Test HelpGetContainerPidNumaInfo
TEST_F(TestResourceQuery, HelpGetContainerPidNumaInfo_EmptyList_ReturnOK)
{
    ResourceQuery rq;
    std::vector<pid_t> pidList; // Empty list returns directly
    std::vector<RmrsPidInfo> pidInfos;

    auto ret = rq.HelpGetContainerPidNumaInfo("node_1", pidList, pidInfos);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestResourceQuery, HelpGetContainerPidNumaInfo_Succeed)
{
    MOCKER_CPP(&UbseRpcSend,
               uint32_t (*)(const ubse::com::UbseComEndpoint&, const UbseByteBuffer&, void*, const UbseComRespHandler&))
        .stubs()
        .will(invoke(UbseRpcSendMockOk));

    ResourceQuery rq;
    std::vector<pid_t> pidList = {1234};
    std::vector<RmrsPidInfo> pidInfos;

    auto ret = rq.HelpGetContainerPidNumaInfo("node_1", pidList, pidInfos);

    EXPECT_EQ(ret, MEM_POOLING_OK);
    ASSERT_EQ(pidInfos.size(), 1);
    EXPECT_EQ(pidInfos[0].pid, 1234);
    EXPECT_EQ(pidInfos[0].metaNumaInfos[0].numaId, 1);
}

TEST_F(TestResourceQuery, HelpGetContainerPidNumaInfo_InvalidPid_Fail)
{
    MOCKER_CPP(&UbseRpcSend, uint32_t (*)(const ubse::com::UbseComEndpoint&, const UbseByteBuffer&, void*,
                                          const UbseComRespHandler&))
        .stubs()
        .will(invoke(UbseRpcSendMockInvalidPid)); // Trigger pid == -1

    ResourceQuery rq;
    std::vector<pid_t> pidList = {1234};
    std::vector<RmrsPidInfo> pidInfos;

    auto ret = rq.HelpGetContainerPidNumaInfo("node_1", pidList, pidInfos);

    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestResourceQuery, HelpGetContainerPidNumaInfo_RpcFail)
{
    MOCKER_CPP(&UbseRpcSend, uint32_t (*)(const ubse::com::UbseComEndpoint&, const UbseByteBuffer&, void*,
                                          const UbseComRespHandler&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR)); // Simulate RPC failure

    ResourceQuery rq;
    std::vector<pid_t> pidList = {1234};
    std::vector<RmrsPidInfo> pidInfos;

    auto ret = rq.HelpGetContainerPidNumaInfo("node_1", pidList, pidInfos);

    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

// 5. Test HelpGetContainerPidNumaInfoByLocalNode
TEST_F(TestResourceQuery, HelpGetContainerPidNumaInfoByLocalNode_EmptyList_ReturnOK)
{
    ResourceQuery rq;
    std::vector<pid_t> pidList; // Empty list returns directly
    std::vector<RmrsPidInfo> pidInfos;

    auto ret = rq.HelpGetContainerPidNumaInfoByLocalNode("node_1", pidList, pidInfos);
    EXPECT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestResourceQuery, HelpGetContainerPidNumaInfoByLocalNode_Succeed)
{
    MOCKER_CPP(&PidNumaInfoCollectHandler,
               MpResult (*)(const turbo::rmrs::PidNumaInfoCollectParam&, turbo::rmrs::PidNumaInfoCollectResult&))
        .stubs()
        .will(invoke(PidNumaInfoCollectHandlerMockSuccess));

    ResourceQuery rq;
    std::vector<pid_t> pidList = {5678};
    std::vector<RmrsPidInfo> pidInfos;

    auto ret = rq.HelpGetContainerPidNumaInfoByLocalNode("node_1", pidList, pidInfos);

    EXPECT_EQ(ret, MEM_POOLING_OK);
    ASSERT_EQ(pidInfos.size(), 1);
    EXPECT_EQ(pidInfos[0].pid, 5678);
    // Check if there is remoteNumaId assignment specific to container scenario
    EXPECT_EQ(pidInfos[0].remoteNumaId, 2);
}

TEST_F(TestResourceQuery, HelpGetContainerPidNumaInfoByLocalNode_HandlerFail)
{
    MOCKER_CPP(&PidNumaInfoCollectHandler,
               MpResult (*)(const turbo::rmrs::PidNumaInfoCollectParam&, turbo::rmrs::PidNumaInfoCollectResult&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR)); // Simulate direct processing failure
    ResourceQuery rq;
    std::vector<pid_t> pidList = {5678};
    std::vector<RmrsPidInfo> pidInfos;

    auto ret = rq.HelpGetContainerPidNumaInfoByLocalNode("node_1", pidList, pidInfos);

    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

} // namespace mempooling