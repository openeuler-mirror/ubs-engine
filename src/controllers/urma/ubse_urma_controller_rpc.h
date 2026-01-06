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

#ifndef UBSE_URMA_CONTROLLER_RPC_H
#define UBSE_URMA_CONTROLLER_RPC_H
#include <cstdint>
#include <queue>
#include <set>
#include <string>
#include <thread>

#include "ubse_com.h"
#include "ubse_com_base.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_serial_util.h"
#include "ubse_urma_controller.h"
#include "ubse_urma_controller_manager.h"
#include "ubse_urma_def.h"

namespace ubse::urmaController {
using namespace ubse::com;
using namespace ubse::utils;
using namespace ubse::election;
using namespace ubse::urmaController;
using namespace ubse::context;
using namespace ubse::urma;

using HcomCbRecv = void *;
using HcomCbCtx = void *;

class UbseUrmaDevQueryMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

struct UrmaDevQueryRpcReq {
    uint32_t nodeId;
    uint32_t type;
};

struct UrmaDevQueryRpcRsp {
    std::vector<UbseUrmaInfoForQuery> urmaInfos;
};

class UrmaDevQueryReqSimpo : public UbseBaseMessage {
public:
    UrmaDevQueryReqSimpo() = default;
    explicit UrmaDevQueryReqSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    inline void SetUbseUrmaDevReq(UrmaDevQueryRpcReq request)
    {
        req = std::move(request);
    }

    inline UrmaDevQueryRpcReq GetUbseUrmaDevReq()
    {
        return req;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    UrmaDevQueryRpcReq req;
};
using UbseUrmaDevQueryReqPtr = Ref<UrmaDevQueryReqSimpo>;

class UrmaDevQueryRspSimpo : public UbseBaseMessage {
public:
    UrmaDevQueryRspSimpo() = default;
    explicit UrmaDevQueryRspSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    inline void SetUbseUrmaDevQueryRsp(UrmaDevQueryRpcRsp response)
    {
        rsp = std::move(response);
    }

    inline UrmaDevQueryRpcRsp GetUbseUrmaDevRsp()
    {
        return rsp;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    UrmaDevQueryRpcRsp rsp;
};
using UbseUrmaDevRspPtr = Ref<UrmaDevQueryRspSimpo>;

struct UrmaNotifyReq {
    std::string nodeId; // 有更新的nodeId
};

class UbseUrmaNotifyReqSimpo : public UbseBaseMessage {
public:
    UbseUrmaNotifyReqSimpo() = default;
    explicit UbseUrmaNotifyReqSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    inline void SetUrmaNotifyReq(UrmaNotifyReq request)
    {
        req = std::move(request);
    }

    inline UrmaNotifyReq GetUrmaNotifyReq()
    {
        return req;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    UrmaNotifyReq req;
};

using UbseUrmaNotifyReqPtr = Ref<UbseUrmaNotifyReqSimpo>;

class UbseUrmaNotifyRspSimpo : public UbseBaseMessage {
public:
    UbseUrmaNotifyRspSimpo() = default;
    explicit UbseUrmaNotifyRspSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }

    UbseResult Serialize() override;

    UbseResult Deserialize() override;
};
using UbseUrmaNotifyRspPtr = Ref<UbseUrmaNotifyRspSimpo>;

class UbseUrmaNotifyMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

struct UrmaQueryReq {
    std::string nodeId; // 请求nodeId
};

struct UrmaQueryRsp {
    UbseUrmaNodeInfo nodeInfo;
};

class UbseUrmaQueryReqSimpo : public UbseBaseMessage {
public:
    UbseUrmaQueryReqSimpo() = default;
    explicit UbseUrmaQueryReqSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    inline void SetUrmaQueryReq(UrmaQueryReq request)
    {
        req = std::move(request);
    }

    inline UrmaQueryReq GetUrmaQueryReq()
    {
        return req;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    UrmaQueryReq req;
};

using UbseUrmaQueryReqSimpoPtr = Ref<UbseUrmaQueryReqSimpo>;

class UbseUrmaQueryRspSimpo : public UbseBaseMessage {
public:
    UbseUrmaQueryRspSimpo() = default;
    explicit UbseUrmaQueryRspSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    inline void SetUbseQueryRsp(UrmaQueryRsp response)
    {
        rsp = std::move(response);
    }

    inline UrmaQueryRsp GetUbseUrmaQueryRsp()
    {
        return rsp;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    UrmaQueryRsp rsp;
};
using UbseUrmaQueryRspSimpoPtr = Ref<UbseUrmaQueryRspSimpo>;

class UbseUrmaQueryMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

struct ReportUrmaNodeInfoReq {
    std::string nodeId;
    UbseUrmaNodeInfo urmaNodeInfo;

    friend ubse::serial::UbseSerialization &operator<<(ubse::serial::UbseSerialization &out,
                                                       const ReportUrmaNodeInfoReq &obj)
    {
        out << obj.nodeId << obj.urmaNodeInfo;
        return out;
    }

    friend ubse::serial::UbseDeSerialization &operator>>(ubse::serial::UbseDeSerialization &in,
                                                         ReportUrmaNodeInfoReq &obj)
    {
        in >> obj.nodeId >> obj.urmaNodeInfo;
        return in;
    }
};

class UbseUrmaReportUrmaNodeInfoReqSimpo : public UbseBaseMessage {
public:
    UbseUrmaReportUrmaNodeInfoReqSimpo() = default;
    explicit UbseUrmaReportUrmaNodeInfoReqSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }

    inline void SetUbseUrmaNodeInfo(const ReportUrmaNodeInfoReq &info)
    {
        urmaNodeInfo = info;
    }

    inline void SetUbseUrmaNodeInfo(ReportUrmaNodeInfoReq &&info)
    {
        urmaNodeInfo = std::move(info);
    }

    inline ReportUrmaNodeInfoReq GetUbseUrmaNodeInfo()
    {
        return urmaNodeInfo;
    }

    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    ReportUrmaNodeInfoReq urmaNodeInfo;
};
using UbseUrmaReportUrmaNodeInfoReqSimpoPtr = Ref<UbseUrmaReportUrmaNodeInfoReqSimpo>;

class UbseUrmaReportUrmaNodeInfoRspSimpo : public UbseBaseMessage {
public:
    UbseUrmaReportUrmaNodeInfoRspSimpo() = default;
    explicit UbseUrmaReportUrmaNodeInfoRspSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }

    UbseResult Serialize() override;

    UbseResult Deserialize() override;
};
using UbseUrmaReportUrmaNodeInfoRspSimpoPtr = Ref<UbseUrmaReportUrmaNodeInfoRspSimpo>;

class UbseUrmaReportUrmaNodeInfoMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

UbseResult ReportUrmaNodeInfoToMaster(const std::string &nodeId, UbseUrmaNodeInfo &nodeInfo);
} // namespace ubse::urmaController
#endif