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

#include "hcom/hcom_service.h"

namespace ubse::ut::com {
using namespace ubse::com;
class TestChannel : public UBSHcomChannel {
public:
    TestChannel() {}
    ~TestChannel() override {}
    int32_t Send(const UBSHcomRequest &req, const Callback *done = nullptr) override {}
    int32_t Call(const UBSHcomRequest &req, UBSHcomResponse &rsp, const Callback *done = nullptr) override { return 0; }
    int32_t Reply(const UBSHcomReplyContext &ctx, const UBSHcomRequest &req, const Callback *done = nullptr) override {}
    int32_t CallWithHlc(const UBSHcomRequest& req, UBSHcomResponse& rsp, const Callback* done = nullptr) override
    {
        return 0;
    }
    int32_t ReplyWithHlc(const UBSHcomReplyContext& ctx, const UBSHcomRequest& req,
                         const Callback* done = nullptr) override
    {
        return 0;
    }
    int32_t Put(const UBSHcomOneSideRequest &req, const Callback *done = nullptr) override {}
    int32_t Get(const UBSHcomOneSideRequest &req, const Callback *done = nullptr) override {}
    int32_t Recv(const UBSHcomServiceContext &context, uintptr_t address, uint32_t size,
                 const Callback *done = nullptr) override
    {
    }
    int32_t PutV(const UBSHcomOneSideSglRequest &req, const Callback *done) {}
    int32_t GetV(const UBSHcomOneSideSglRequest &req, const Callback *done) {}
    uint64_t GetUpCtx() {}
    void SetUpCtx(uint64_t ctx) {}
    int32_t SetFlowControlConfig(const UBSHcomFlowCtrlOptions &opt) override {}
    void SetChannelTimeOut(int16_t oneSideTimeout, int16_t twoSideTimeout) override {}
    int32_t SetTwoSideThreshold(const UBSHcomTwoSideThreshold &threshold) override {}
    void SetTraceId(const std::string &traceId) override {}
    int32_t GetRemoteUdsIdInfo(UBSHcomNetUdsIdInfo &idInfo) override
    {
        idInfo.pid = 1;
        idInfo.gid = 1;
        idInfo.uid = 1;
        return 0;
    }
    int32_t SendFds(int fds[], uint32_t len) override {}
    int32_t ReceiveFds(int fds[], uint32_t len, int32_t timeoutSec) override {}
    auto SpliceMessage(const UBSHcomNetRequestContext &ctx,
                       bool isResp) -> std::tuple<SpliceMessageResultType, SerResult, std::string> override
    {
    }
    SerResult Initialize(std::vector<UBSHcomEndpointPtr> &ep, uintptr_t ctxMemPool, uintptr_t periodicMgr,
                          uintptr_t pgTable)
    {
    }
    SerResult Initialize(std::vector<UBSHcomEndpointPtr> &ep, uintptr_t ctxMemPool, uintptr_t periodicMgr,
                         uintptr_t pgTable, uint32_t ctxStoreCapacity = NN_NO2097152)
    {
    }
    void UnInitialize() override {}
    std::string ToString() override {}
    void SetUuid(const std::string &uuid) override {}
    void SetPayload(const std::string &payload) override {}
    void SetBrokenInfo(UBSHcomChannelBrokenPolicy policy, const UBSHcomServiceChannelBrokenHandler &broken) override {}
    void SetEpBroken(uint32_t index) override {}
    void SetChannelState(UBSHcomChannelState state) override {}
    inline void SetMultiRail(bool multiRail, uint32_t threshold) override {}
    inline void SetDriverNum(uint16_t driverNum) override {}
    inline void SetTotalBandWidth(uint32_t bandWidth) override {}
    bool AllEpBroken() override {}
    bool NeedProcessBroken() override {}
    void ProcessIoInBroken() override {}
    void InvokeChannelBrokenCb(UBSHcomChannelPtr &channel) override {}
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
    UBSHcomChannelCallBackType GetCallBackType() override {}
    void SetEnableMrCache(bool) override {}
};
}
#endif // UBS_ENGINE_TEST_UBSE_COM_MOCK_H