/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
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
using HcomCbRecv = void *;
using HcomCbCtx = void *;

// 跨节点查询urma bounding设备信息
class UbseUrmaDevQueryMessageHandler : public ubse::com::UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                      ubse::com::UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

struct UrmaDevQueryRpcReq {
    uint32_t nodeId;
};

struct UrmaDevQueryRpcRsp {
    std::vector<UbseUrmaInfoForQuery> urmaInfos;
    uint32_t result;

    friend ubse::serial::UbseSerialization &operator<<(ubse::serial::UbseSerialization &serializer,
                                                       const UrmaDevQueryRpcRsp &info)
    {
        serializer << info.urmaInfos << info.result;
        return serializer;
    }

    friend ubse::serial::UbseDeSerialization &operator>>(ubse::serial::UbseDeSerialization &deserializer,
                                                         UrmaDevQueryRpcRsp &info)
    {
        deserializer >> info.urmaInfos >> info.result;
        return deserializer;
    }
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
using UbseUrmaDevQueryRspPtr = Ref<UrmaDevQueryRspSimpo>;

struct UrmaBrocastReq {
    std::map<std::string, uint64_t> urmaInfoTimestamps; // <nodeId, updateTimeStamp>

    friend ubse::serial::UbseSerialization &operator<<(ubse::serial::UbseSerialization &serializer,
                                                       const UrmaBrocastReq &req)
    {
        serializer << req.urmaInfoTimestamps;
        return serializer;
    }
    friend ubse::serial::UbseDeSerialization &operator>>(ubse::serial::UbseDeSerialization &deserializer,
                                                         UrmaBrocastReq &req)
    {
        deserializer >> req.urmaInfoTimestamps;
        return deserializer;
    }
};

class UbseUrmaBrocastReqSimpo : public UbseBaseMessage {
public:
    UbseUrmaBrocastReqSimpo() = default;
    explicit UbseUrmaBrocastReqSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    inline void SetUrmaNotifyReq(UrmaBrocastReq request)
    {
        req = std::move(request);
    }

    inline UrmaBrocastReq GetUrmaNotifyReq()
    {
        return req;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    UrmaBrocastReq req;
};

using UbseUrmaBrocastReqPtr = Ref<UbseUrmaBrocastReqSimpo>;

class UbseUrmaBrocastRspSimpo : public UbseBaseMessage {
public:
    UbseUrmaBrocastRspSimpo() = default;
    explicit UbseUrmaBrocastRspSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }

    UbseResult Serialize() override;

    UbseResult Deserialize() override;
};
using UbseUrmaBrocastRspPtr = Ref<UbseUrmaBrocastRspSimpo>;

class UbseUrmaNotifyMessageHandler : public ubse::com::UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                      ubse::com::UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

struct QueryUrmaInfoReq {
    std::vector<std::string> updateNodeIds;
};

struct QueryUrmaInfoRsp {
    std::vector<UbseUrmaNodeInfo> queryNodeInfos;
};

class UbseUrmaQueryReqSimpo : public UbseBaseMessage {
public:
    UbseUrmaQueryReqSimpo() = default;
    explicit UbseUrmaQueryReqSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    inline void SetUrmaQueryReq(QueryUrmaInfoReq request)
    {
        req = std::move(request);
    }

    inline QueryUrmaInfoReq GetUrmaQueryReq()
    {
        return req;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    QueryUrmaInfoReq req;
};

using UbseUrmaQueryReqSimpoPtr = Ref<UbseUrmaQueryReqSimpo>;

class UbseUrmaQueryRspSimpo : public UbseBaseMessage {
public:
    UbseUrmaQueryRspSimpo() = default;
    explicit UbseUrmaQueryRspSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    inline void SetUbseQueryRsp(QueryUrmaInfoRsp response)
    {
        rsp = std::move(response);
    }

    inline QueryUrmaInfoRsp GetUbseUrmaQueryRsp()
    {
        return rsp;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    QueryUrmaInfoRsp rsp;
};
using UbseUrmaQueryRspSimpoPtr = Ref<UbseUrmaQueryRspSimpo>;

class UbseUrmaQueryMessageHandler : public ubse::com::UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                      ubse::com::UbseComBaseMessageHandlerCtxPtr ctx) override;

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

class UbseUrmaReportUrmaNodeInfoMessageHandler : public ubse::com::UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                      ubse::com::UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

UbseResult ReportUrmaNodeInfoToMaster(const std::string &nodeId);

class UbseUrmaActivateUrmaInfoReqSimpo : public UbseBaseMessage {
public:
    UbseUrmaActivateUrmaInfoReqSimpo() = default;
    explicit UbseUrmaActivateUrmaInfoReqSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }

    inline void SetNodeId(const std::string &id)
    {
        nodeId = id;
    }

    inline void SetNodeId(std::string &&id)
    {
        nodeId = std::move(id);
    }

    inline std::string GetNodeId() const
    {
        return nodeId;
    }

    inline void SetUrmaName(const std::string &name)
    {
        urmaName = name;
    }

    inline void SetUrmaName(std::string &&name)
    {
        urmaName = std::move(name);
    }

    inline std::string GetUrmaName() const
    {
        return urmaName;
    }

    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    std::string nodeId;
    std::string urmaName;
};
using UbseUrmaActivateUrmaInfoReqSimpoPtr = Ref<UbseUrmaActivateUrmaInfoReqSimpo>;

class UbseUrmaActivateUrmaInfoRspSimpo : public UbseBaseMessage {
public:
    UbseUrmaActivateUrmaInfoRspSimpo() = default;
    explicit UbseUrmaActivateUrmaInfoRspSimpo(uint8_t *data, uint32_t size)
    {
        SetInputRawData(data, size);
    }

    UbseResult Serialize() override;

    UbseResult Deserialize() override;
};
using UbseUrmaActivateUrmaInfoRspSimpoPtr = Ref<UbseUrmaActivateUrmaInfoRspSimpo>;

class UbseUrmaActivateUrmaInfoMessageHandler : public ubse::com::UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                      ubse::com::UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};
} // namespace ubse::urmaController
#endif