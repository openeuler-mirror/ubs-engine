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
#include "over_commit_msg.h"

#include "ubse_def.h"
#include "ubse_com.h"
#include "mempooling_interface.h"
#include "mempooling_message.h"
#include "mp_error.h"
#include "numa_info.h"
#include "turbo_rmrs_interface.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

using namespace mempooling;
using namespace std;
using namespace ubse::com;
namespace mempooling::over_commit {
class TestOverCommitMsg : public ::testing::Test {
public:
    void SetUp() override
    {
        cout << "[TestOverCommitMsg SetUp Begin]" << endl;
        cout << "[TestOverCommitMsg SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[TestOverCommitMsg TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[TestOverCommitMsg TearDown End]" << endl;
    }
};

TEST_F(TestOverCommitMsg, GetVmNumaInfoMapRecvHandlerSuccess)
{
    MOCKER_CPP(&OverCommitMsg::GetLocalNumaVms, MpResult(*)(uint16_t, std::vector<VmNumaInfoWithSocket>&))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    OverCommitMsg overCommitMsg;
    UbseByteBuffer req;
    OverCommitFaultVmNumaInfoParam vmNumaInfoParam = {.remoteNumaId = 5};
    RmrsOutStream builder;
    builder << vmNumaInfoParam;
    req = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    UbseByteBuffer resp;
    auto ret = overCommitMsg.GetVmNumaInfoMapRecvHandler(req, resp);
    ASSERT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitMsg, GetVmNumaInfoMapRecvHandlerFail)
{
    MOCKER_CPP(&OverCommitMsg::GetLocalNumaVms, MpResult(*)(uint16_t, std::vector<VmNumaInfoWithSocket>&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    OverCommitMsg overCommitMsg;
    UbseByteBuffer req;
    OverCommitFaultVmNumaInfoParam vmNumaInfoParam = {.remoteNumaId = 5};
    RmrsOutStream builder;
    builder << vmNumaInfoParam;
    req = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    UbseByteBuffer resp;
    auto ret = overCommitMsg.GetVmNumaInfoMapRecvHandler(req, resp);
    ASSERT_EQ(ret, MEM_POOLING_ERROR);
}

MpResult MockGetVmInfoImmediately(std::vector<mempooling::exportV2::VmDomainInfo>& vmDomainInfos)
{
    mempooling::exportV2::VmDomainInfo vmDomainInfo;
    vmDomainInfo.metaData.pid = 0;
    mempooling::exportV2::VmDomainNumaInfo vmNumaInfo1 = {0, 2048, 10, 0, 1};
    vmDomainInfo.numaInfo[0] = vmNumaInfo1;
    mempooling::exportV2::VmDomainNumaInfo vmNumaInfo2 = {1, 2048, 10, -1, 0};
    vmDomainInfo.numaInfo[1] = vmNumaInfo2;
    vmDomainInfos.push_back(vmDomainInfo);
    return MEM_POOLING_OK;
}

MpResult MockGetNumaInfoImmediately(std::vector<mempooling::exportV2::NumaInfo>& numaInfos)
{
    mempooling::exportV2::NumaInfo numaInfo;
    numaInfo.metaData.numaId = 0;
    numaInfos.push_back(numaInfo);
    return MEM_POOLING_OK;
}

TEST_F(TestOverCommitMsg, GetLocalNumaVmsSuccess)
{
    OverCommitMsg overCommitMsg;
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult(*)(std::vector<mempooling::exportV2::VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(invoke(MockGetVmInfoImmediately));
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetNumaInfoImmediately,
               MpResult(*)(std::vector<mempooling::exportV2::NumaInfo> & numaInfos))
        .stubs()
        .will(invoke(MockGetNumaInfoImmediately));
    auto ret = overCommitMsg.GetLocalNumaVms(0, vmNumaInfoWithSocketList);
    ASSERT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitMsg, GetLocalNumaVmsFailWhenGetVMERROR)
{
    OverCommitMsg overCommitMsg;
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult(*)(std::vector<mempooling::exportV2::VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetNumaInfoImmediately,
               MpResult(*)(std::vector<mempooling::exportV2::NumaInfo> & numaInfos))
        .stubs()
        .will(invoke(MockGetNumaInfoImmediately));
    auto ret = overCommitMsg.GetLocalNumaVms(0, vmNumaInfoWithSocketList);
    ASSERT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitMsg, GetLocalNumaVmsFailWhenGetVMEmpty)
{
    OverCommitMsg overCommitMsg;
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult(*)(std::vector<mempooling::exportV2::VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetNumaInfoImmediately,
               MpResult(*)(std::vector<mempooling::exportV2::NumaInfo> & numaInfos))
        .stubs()
        .will(invoke(MockGetNumaInfoImmediately));
    auto ret = overCommitMsg.GetLocalNumaVms(0, vmNumaInfoWithSocketList);
    ASSERT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitMsg, GetLocalNumaVmsFailWhenGetNumaEmpty)
{
    OverCommitMsg overCommitMsg;
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult(*)(std::vector<mempooling::exportV2::VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(invoke(MockGetVmInfoImmediately));
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetNumaInfoImmediately,
               MpResult(*)(std::vector<mempooling::exportV2::NumaInfo> & numaInfos))
        .stubs()
        .will(returnValue(MEM_POOLING_OK));
    auto ret = overCommitMsg.GetLocalNumaVms(0, vmNumaInfoWithSocketList);
    ASSERT_EQ(ret, MEM_POOLING_OK);
}

TEST_F(TestOverCommitMsg, GetLocalNumaVmsFailWhenGetNumaERR)
{
    OverCommitMsg overCommitMsg;
    std::vector<VmNumaInfoWithSocket> vmNumaInfoWithSocketList;
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetVmInfoImmediately,
               MpResult(*)(std::vector<mempooling::exportV2::VmDomainInfo> & vmDomainInfos))
        .stubs()
        .will(invoke(MockGetVmInfoImmediately));
    MOCKER_CPP(&mempooling::exportV2::Exporter::GetNumaInfoImmediately,
               MpResult(*)(std::vector<mempooling::exportV2::NumaInfo> & numaInfos))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    auto ret = overCommitMsg.GetLocalNumaVms(0, vmNumaInfoWithSocketList);
    ASSERT_EQ(ret, MEM_POOLING_ERROR);
}

TEST_F(TestOverCommitMsg, TestGetVmNumaInfoMapResHandler)
{
    OverCommitMsg overCommitMsg;
    OverCommitFaultVmNumaInfoResult overCommitFaultVmNumaInfoResult;

    void* ptr = static_cast<void*>(&overCommitFaultVmNumaInfoResult);
    OverCommitFaultVmNumaInfoResult resp;
    RmrsOutStream builder;
    builder << resp;

    UbseByteBuffer respData = {.data = builder.GetBufferPointer()};
    overCommitMsg.GetVmNumaInfoMapResHandler(ptr, respData, 0);
}

TEST_F(TestOverCommitMsg, TestGetVmNumaInfoMapResHandlerFail)
{
    OverCommitMsg overCommitMsg;
    OverCommitFaultVmNumaInfoResult overCommitFaultVmNumaInfoResult;

    void* ptr = static_cast<void*>(&overCommitFaultVmNumaInfoResult);
    OverCommitFaultVmNumaInfoResult resp;
    RmrsOutStream builder;
    builder << resp;

    UbseByteBuffer respData = {.data = builder.GetBufferPointer()};
    overCommitMsg.GetVmNumaInfoMapResHandler(ptr, respData, 1);
}

static uint32_t MockRmrsNumaMemInfoCollectERR(const turbo::rmrs::NumaMemInfoCollectParam& numaMemInfoCollectParam,
                                              turbo::rmrs::ResponseInfoSimpo& responseInfoSimpo)
{
    return 1;
}

TEST_F(TestOverCommitMsg, TestNumaMemInfoCollectRecvHandlerFail)
{
    OverCommitMsg overCommitMsg;
    UbseByteBuffer req;
    UbseByteBuffer resp;
    message::MempoolingMessage::rmrsNumaMemInfoCollect = &MockRmrsNumaMemInfoCollectERR;
    auto ret = overCommitMsg.NumaMemInfoCollectRecvHandler(req, resp);
    ASSERT_EQ(ret, MEM_POOLING_ERROR);
}

static uint32_t MockRmrsNumaMemInfoCollectSuccess(const turbo::rmrs::NumaMemInfoCollectParam& numaMemInfoCollectParam,
                                                  turbo::rmrs::ResponseInfoSimpo& responseInfoSimpo)
{
    return 0;
}

TEST_F(TestOverCommitMsg, TestNumaMemInfoCollectRecvHandlerFailed)
{
    OverCommitMsg overCommitMsg;
    UbseByteBuffer req;
    UbseByteBuffer resp;
    message::MempoolingMessage::rmrsNumaMemInfoCollect = &MockRmrsNumaMemInfoCollectSuccess;
    auto ret = overCommitMsg.NumaMemInfoCollectRecvHandler(req, resp);
    ASSERT_EQ(ret, MEM_POOLING_ERROR);
}

uint32_t TestRackRpcSend1(const UbseComEndpoint& endpoint, const UbseByteBuffer& reqData, void* ctx,
                          const UbseComRespHandler& handler)
{
    if (ctx != nullptr) {
        *(uint32_t*)ctx = MEM_POOLING_ERROR; // 写到指针指向的内容里
    }
    return MEM_POOLING_ERROR; // RackRpcSend本身也返回错误
}

TEST_F(TestOverCommitMsg, GetVmNumaInfoMapRpcFail)
{
    OverCommitFaultMemIdExecuteParam param;
    std::string importNodeId = "1";

    MOCKER_CPP(&UbseRpcSend,
               uint32_t(*)(const UbseComEndpoint&, const UbseByteBuffer&, void*, const UbseComRespHandler&))
        .stubs()
        .will(invoke(TestRackRpcSend1));

    MpResult ret = OverCommitFaultMemIdModule::Instance().MemIdExecuteRpc(param, importNodeId);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}
} // namespace mempooling::over_commit