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

#ifndef UBS_ENGINE_TEST_UBSE_COM_MOCK_H
#define UBS_ENGINE_TEST_UBSE_COM_MOCK_H

#include "hcom_service.h"

class TestChannel : public HcomChannel {
public:
    TestChannel() {}
    ~TestChannel() override {}
    int32_t Send(const Request &req, const Callback *done = nullptr) override {}
    int32_t Call(const Request &req, Response &rsp, const Callback *done = nullptr) override {}
    int32_t Reply(const ReplyContext &ctx, const Request &req, const Callback *done = nullptr) override {}
    int32_t Put(const OneSideRequest &req, const Callback *done = nullptr) override {}
    int32_t Get(const OneSideRequest &req, const Callback *done = nullptr) override {}
    int32_t Recv(const HcomServiceContext &context, uintptr_t address, uint32_t size,
                 const Callback *done = nullptr) override
    {
    }
    int32_t SetFlowControlConfig(const FlowCtrlOptions &opt) override {}
    void SetChannelTimeOut(int16_t oneSideTimeout, int16_t twoSideTimeout) override {}
    int32_t SetTwoSideThreshold(const TwoSideThreshold &threshold) override {}
    void SetTraceId(const std::string &traceId) override {}
    int32_t GetRemoteUdsIdInfo(NetUdsIdInfo &idInfo) override
    {
        idInfo.pid = 1;
        idInfo.gid = 1;
        idInfo.uid = 1;
        return 0;
    }
    int32_t SendFds(int fds[], uint32_t len) override {}
    int32_t ReceiveFds(int fds[], uint32_t len, int32_t timeoutSec) override {}
    auto SpliceMessage(const NetRequestContext &ctx,
                       bool isResp) -> std::tuple<SpliceMessageResultType, SerResult, std::string> override
    {
    }

    SerResult Initialize(std::vector<HcomEndpointPtr> &ep, uintptr_t ctxMemPool, uintptr_t periodicMgr,
                         uintptr_t pgTable) override
    {
    }
    void UnInitialize() override {}
    std::string ToString() override {}
    void SetUuid(const std::string &uuid) override {}
    void SetPayload(const std::string &payload) override {}
    void SetBrokenInfo(HcomChannelBrokenPolicy policy, const HcomServiceChannelBrokenHandler &broken) override {}
    void SetEpBroken(uint32_t index) override {}
    void SetChannelState(ChannelState state) override {}
    inline void SetMultiRail(bool multiRail, uint32_t threshold) override {}
    inline void SetDriverNum(uint16_t driverNum) override {}
    inline void SetTotalBandWidth(uint32_t bandWidth) override {}
    bool AllEpBroken() override {}
    bool NeedProcessBroken() override {}
    void ProcessIoInBroken() override {}
    void InvokeChannelBrokenCb(HcomChannelPtr &channel) override {}
    uint64_t GetId() override
    {
        return 1;
    }
    std::string GetUuid() override {}
    uintptr_t GetTimerList() override {}
    uint32_t GetLocalIp() override {}
    std::string GetPeerConnectPayload() override
    {
        return "";
    }
    uint16_t GetDelayEraseTime() override {}
    HcomServiceCtxStore *GetCtxStore() override {}
    HcomChannelCallBackType GetCallBackType() override {}
};

#endif // UBS_ENGINE_TEST_UBSE_COM_MOCK_H