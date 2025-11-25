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

#include "ubse_com_def.h"

#include <ubse_conf_module.h>
#include <fstream>
#include <regex>

#include "crc/ubse_crc.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_lcne_module.h"
#include "ubse_logger_inner.h"
#include "ubse_pointer_process.h"
#include "ubse_str_util.h"
#include "ubse_topology_interface.h"
#include "ubse_net_util.h"

namespace ubse::com {
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_COM_MID)

using namespace ubse::utils;
using namespace ubse::log;
UbseEngineType UbseComEngineInfo::GetEngineType() const
{
    return engineType;
}

void UbseComEngineInfo::SetEngineType(UbseEngineType type)
{
    UbseComEngineInfo::engineType = type;
}

UbseProtocol UbseComEngineInfo::GetProtocol() const
{
    return protocol;
}

void UbseComEngineInfo::SetProtocol(UbseProtocol proto)
{
    UbseComEngineInfo::protocol = proto;
}

UbseWorkerMode UbseComEngineInfo::GetWorkerMode() const
{
    return workerMode;
}

void UbseComEngineInfo::SetWorkerMode(UbseWorkerMode mode)
{
    UbseComEngineInfo::workerMode = mode;
}

const std::string &UbseComEngineInfo::GetNodeId() const
{
    return nodeId;
}

void UbseComEngineInfo::SetNodeId(const std::string &id)
{
    UbseComEngineInfo::nodeId = id;
}

const std::string &UbseComEngineInfo::GetWorkGroup() const
{
    return workGroup;
}

void UbseComEngineInfo::SetWorkGroup(const std::string &group)
{
    UbseComEngineInfo::workGroup = group;
}

const std::string &UbseComEngineInfo::GetName() const
{
    return name;
}

void UbseComEngineInfo::SetName(const std::string &engineName)
{
    UbseComEngineInfo::name = engineName;
}

void UbseComEngineInfo::SetLogFunc(UbseComLogFunc func)
{
    UbseComEngineInfo::logFunc = func;
}

void UbseComEngineInfo::SetHeartBeatTimeOut(const int16_t time)
{
    UbseComEngineInfo::heartBeatTimeout = time;
}

int16_t UbseComEngineInfo::GetHeartBeatTimeOut() const
{
    return heartBeatTimeout;
}

void UbseComEngineInfo::SetHcomHbTimeOut(uint16_t time)
{
    hcomHbTimeout = time;
}

uint16_t UbseComEngineInfo::GetHcomHbTimeOut() const
{
    return hcomHbTimeout;
}

void UbseComEngineInfo::SetTimeOut(const int16_t time)
{
    UbseComEngineInfo::timeout = time;
}

int16_t UbseComEngineInfo::GetTimeOut() const
{
    return timeout;
}

UbseComLogFunc UbseComEngineInfo::GetLogFunc() const
{
    return logFunc;
}

const std::pair<std::string, uint16_t> &UbseComEngineInfo::GetIpInfo() const
{
    return ipInfo;
}

void UbseComEngineInfo::SetIpInfo(const std::pair<std::string, uint16_t> &ipInfo)
{
    UbseComEngineInfo::ipInfo = ipInfo;
}

const std::pair<std::string, uint16_t> &UbseComEngineInfo::GetUdsInfo() const
{
    return udsInfo;
}

void UbseComEngineInfo::SetUdsInfo(const std::pair<std::string, uint16_t> &udsInfo)
{
    UbseComEngineInfo::udsInfo = udsInfo;
}

const uint32_t &UbseComEngineInfo::GetSendReceiveSegCount() const
{
    return sendReceiveSegCount;
}

void UbseComEngineInfo::SetSendReceiveSegCount(uint32_t segCount)
{
    UbseComEngineInfo::sendReceiveSegCount = segCount;
}

bool UbseComEngineInfo::IsUds() const
{
    return protocol == UbseProtocol::UDS;
}

bool UbseComEngineInfo::IsServerSide() const
{
    return engineType == UbseEngineType::SERVER;
}

const IsReconnectHook &UbseComEngineInfo::GetReConnectHook() const
{
    return reconnectHook;
}

void UbseComEngineInfo::SetReconnectHook(IsReconnectHook reconnectHook)
{
    UbseComEngineInfo::reconnectHook = reconnectHook;
}

const ShouldDoReconnectCb &UbseComEngineInfo::GetShouldDoReconnectCb() const
{
    return shouldDoReconnectCb;
}

void UbseComEngineInfo::SetShouldDoReconnectCb(ShouldDoReconnectCb shouldDoReconnectCb)
{
    UbseComEngineInfo::shouldDoReconnectCb = shouldDoReconnectCb;
}

const QueryEidByNodeIdCb &UbseComEngineInfo::GetQueryEidByNodeIdCb() const
{
    return queryEidByNodeIdCb;
}

void UbseComEngineInfo::SetQueryEidByNodeIdCb(QueryEidByNodeIdCb queryEidByNodeIdCb)
{
    UbseComEngineInfo::queryEidByNodeIdCb = queryEidByNodeIdCb;
}

const UBSHcomNetCipherSuite &UbseComEngineInfo::GetCipherSuite() const
{
    return cipherSuite;
}

void UbseComEngineInfo::SetCipherSuite(const UBSHcomNetCipherSuite &suite)
{
    cipherSuite = suite;
}

const std::string &UbseComChannelConnectInfo::GetIp() const
{
    return ip;
}

void UbseComChannelConnectInfo::SetIp(const std::string &ipAddr)
{
    UbseComChannelConnectInfo::ip = ipAddr;
}

uint16_t UbseComChannelConnectInfo::GetPort() const
{
    return port;
}

void UbseComChannelConnectInfo::SetPort(uint16_t conPort)
{
    UbseComChannelConnectInfo::port = conPort;
}

uint16_t UbseComChannelConnectInfo::GetLinkNum() const
{
    return linkNum;
}

void UbseComChannelConnectInfo::SetLinkNum(uint16_t num)
{
    UbseComChannelConnectInfo::linkNum = num;
}

bool UbseComChannelConnectInfo::IsUds() const
{
    return isUds;
}

void UbseComChannelConnectInfo::SetIsUds(bool uds)
{
    UbseComChannelConnectInfo::isUds = uds;
}

const std::string &UbseComChannelConnectInfo::GetRemoteNodeId() const
{
    return remoteNodeId;
}

void UbseComChannelConnectInfo::SetRemoteNodeId(const std::string &remoteNodeIdSet)
{
    UbseComChannelConnectInfo::remoteNodeId = remoteNodeIdSet;
}

const std::string &UbseComChannelConnectInfo::GetCurNodeId() const
{
    return curNodeId;
}

void UbseComChannelConnectInfo::SetCurNodeId(const std::string &nodeId)
{
    UbseComChannelConnectInfo::curNodeId = nodeId;
}

bool UbseComChannelInfo::IsServerSide() const
{
    return isServer;
}

void UbseComChannelInfo::SetIsServer(bool isServerSide)
{
    UbseComChannelInfo::isServer = isServerSide;
}

const UBSHcomChannelPtr &UbseComChannelInfo::GetChannel() const
{
    return channel;
}

void UbseComChannelInfo::SetChannel(const UBSHcomChannelPtr &ch)
{
    UbseComChannelInfo::channel = ch;
}

const UbseComChannelConnectInfo &UbseComChannelInfo::GetConnectInfo() const
{
    return connectInfo;
}

void UbseComChannelInfo::SetConnectInfo(const UbseComChannelConnectInfo &conInfo)
{
    UbseComChannelInfo::connectInfo = conInfo;
}

UbseChannelType UbseComChannelInfo::GetChannelType() const
{
    return channelType;
}

void UbseComChannelInfo::SetChannelType(UbseChannelType chType)
{
    UbseComChannelInfo::channelType = chType;
}

const std::string &UbseComChannelInfo::GetEngineName() const
{
    return engineName;
}

void UbseComChannelInfo::SetEngineName(const std::string &name)
{
    UbseComChannelInfo::engineName = name;
}
std::string UbseComChannelInfo::ConvertUbseComChannelInfoToString()
{
    std::string infoStr = "engine Name: " + engineName + "; ";
    infoStr = infoStr + "channel type: " + std::to_string(static_cast<int>(channelType)) + "; ";
    infoStr = infoStr + "channel id: " + std::to_string(channel->GetId()) + "; ";
    infoStr = infoStr + "cur node id: " + connectInfo.GetCurNodeId() + "; ";
    infoStr = infoStr + "remote node id: " + connectInfo.GetRemoteNodeId() + "; ";
    return infoStr;
}

const std::string PAYLOAD_SPLIT_SEP = "@";

UbseChannelType StringToChannelType(const std::string &type)
{
    if (type == "Normal") {
        return UbseChannelType::NORMAL;
    }
    if (type == "Emergency") {
        return UbseChannelType::EMERGENCY;
    }
    if (type == "Heartbeat") {
        return UbseChannelType::HEARTBEAT;
    }
    return UbseChannelType::SINGLE_SIDE;
}

std::string ChannelTypeToString(UbseChannelType type)
{
    if (type == UbseChannelType::NORMAL) {
        return "Normal";
    }
    if (type == UbseChannelType::EMERGENCY) {
        return "Emergency";
    }
    if (type == UbseChannelType::HEARTBEAT) {
        return "Heartbeat";
    }
    return "SingleSide";
}

UbseReplyResult StringToUbseReplyResult(const std::string &result)
{
    if (result == "ERR") {
        return UbseReplyResult::ERR;
    }
    if (result == "ERR_NO_HANDLER") {
        return UbseReplyResult::ERR_NO_HANDLER;
    }
    if (result == "ERR_NO_REPLY") {
        return UbseReplyResult::ERR_NO_REPLY;
    }
    return UbseReplyResult::OK;
}

std::string UbseReplyResultToString(UbseReplyResult result)
{
    if (result == UbseReplyResult::OK) {
        return "OK";
    }
    if (result == UbseReplyResult::ERR) {
        return "ERR";
    }
    if (result == UbseReplyResult::ERR_NO_HANDLER) {
        return "ERR_NO_HANDLER";
    }
    if (result == UbseReplyResult::ERR_NO_REPLY) {
        return "ERR_NO_REPLY";
    }
    return "OK";
}

std::string ChannelTypeToPayload(const std::string &id, UbseChannelType type)
{
    return id + PAYLOAD_SPLIT_SEP + ChannelTypeToString(type);
}

uint16_t UbseComMessageHead::GetOpCode() const
{
    return opCode;
}

void UbseComMessageHead::SetOpCode(uint16_t transOpCode)
{
    UbseComMessageHead::opCode = transOpCode;
}

uint16_t UbseComMessageHead::GetModuleCode() const
{
    return moduleCode;
}

void UbseComMessageHead::SetModuleCode(uint16_t transModuleCode)
{
    UbseComMessageHead::moduleCode = transModuleCode;
}

uint32_t UbseComMessageHead::GetBodyLen() const
{
    return bodyLen;
}

void UbseComMessageHead::SetBodyLen(uint32_t transBodyLen)
{
    UbseComMessageHead::bodyLen = transBodyLen;
}

uint32_t UbseComMessageHead::GetCrc() const
{
    return crc;
}

void UbseComMessageHead::SetCrc(uint32_t dataCrc)
{
    crc = dataCrc;
}

UbseComMessagePtr UbseComMessageCtx::GetMessage() const
{
    return message;
}

void UbseComMessageCtx::SetMessage(UbseComMessagePtr ubseComMessage)
{
    UbseComMessageCtx::message = ubseComMessage;
}

uintptr_t UbseComMessageCtx::GetRspCtx() const
{
    return rspCtx;
}

void UbseComMessageCtx::SetRspCtx(uintptr_t transRspCtx)
{
    UbseComMessageCtx::rspCtx = transRspCtx;
}

uint64_t UbseComMessageCtx::GetChannelId() const
{
    return channelId;
}

void UbseComMessageCtx::SetChannelId(uint64_t netChannel)
{
    UbseComMessageCtx::channelId = netChannel;
}

const std::string &UbseComMessageCtx::GetSrcId() const
{
    return srcId;
}

void UbseComMessageCtx::SetSrcId(const std::string &msgSrcId)
{
    UbseComMessageCtx::srcId = msgSrcId;
}

const std::string &UbseComMessageCtx::GetDstId() const
{
    return dstId;
}

void UbseComMessageCtx::SetDstId(const std::string &id)
{
    UbseComMessageCtx::dstId = id;
}

UbseComMessageCtx::UbseComMessageCtx(UbseComMessagePtr message, std::string srcId, std::string dstId,
                                     UbseChannelType channelType)
    : message(message),
      srcId(std::move(srcId)),
      dstId(std::move(dstId)),
      channelType(channelType)
{
}

void UbseComMessageCtx::SetChannelType(UbseChannelType chType)
{
    channelType = chType;
}

UbseChannelType UbseComMessageCtx::GetChannelType()
{
    return channelType;
}

const std::string &UbseComMessageCtx::GetEngineName() const
{
    return engineName;
}

void UbseComMessageCtx::SetEngineName(const std::string &egName)
{
    engineName = egName;
}

const UbseUdsIdInfo &UbseComMessageCtx::GetUdsInfo() const
{
    return udsInfo;
}

void UbseComMessageCtx::SetUdsInfo(const UbseUdsIdInfo &uds)
{
    UbseComMessageCtx::udsInfo = uds;
}

void UbseComMessageCtx::FreeMessage()
{
    UbseComMessage::FreeMessage(message);
}

void UbseComMessage::SetMessageHead(UbseComMessageHead &msgHead)
{
    head = msgHead;
}

const UbseComMessageHead &UbseComMessage::GetMessageHead() const
{
    return head;
}

UbseComMessagePtr UbseComMessage::AllocMessage(uint32_t len)
{
    uint64_t sumLen = sizeof(UbseComMessageHead) + len;
    auto msg = new (std::nothrow) uint8_t[sumLen];
    return msg;
}

void UbseComMessage::FreeMessage(UbseComMessagePtr &msg)
{
    SafeDeleteArray(msg);
}

UbseResult UbseComMessage::SetMessageBody(const uint8_t *data, uint32_t len)
{
    if (UBSE_LIKELY((len != 0) && data != nullptr)) {
        uint8_t *buff = static_cast<uint8_t *>(static_cast<void *>(this)) + sizeof(UbseComMessageHead);
        auto ret = memcpy_s(buff, len, data, len);
        if (UBSE_UNLIKELY(ret != EOK)) {
            return ret;
        }
    }
    head.SetBodyLen(len);
    return UBSE_OK;
}

UbseComMessagePtr UbseComMessage::GetMessageBody()
{
    return body;
}

uint32_t UbseComMessage::GetMessageBodyLen()
{
    return head.GetBodyLen();
}

UbseComMessagePtr TransRequestMsg(const UbseBaseMessagePtr &requestMsg, const uint16_t &opCode,
                                  const uint16_t moduleCode)
{
    if (requestMsg == nullptr) {
        UBSE_LOG_ERROR << "request is nullptr.";
        return nullptr;
    }
    UbseResult ret = requestMsg->Serialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "request serialize failed.";
        return nullptr;
    }
    uint32_t bodyLen = requestMsg->SerializedDataSize();
    UbseComMessagePtr msg = UbseComMessage::AllocMessage(bodyLen);
    if (msg == nullptr) {
        UBSE_LOG_ERROR << "trans req message alloc failed.";
        return nullptr;
    }

    UbseComMessageHead msgHead{};
    msgHead.SetOpCode(opCode);
    msgHead.SetModuleCode(moduleCode);
    msgHead.SetBodyLen(bodyLen);
    auto reqMsgData = requestMsg->SerializedData();
    auto crc = CrcUtil::SoftCrc32(reqMsgData, bodyLen, 1);
    msgHead.SetCrc(crc);
    auto ucMsg = static_cast<UbseComMessage *>(static_cast<void *>(msg));
    ucMsg->SetMessageHead(msgHead);
    ret = ucMsg->SetMessageBody(reqMsgData, bodyLen);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "set req message body failed.";
        UbseComMessage::FreeMessage(msg);
        return nullptr;
    }
    return msg;
}

UbseResult TransResponse(const UbseBaseMessagePtr &respMsg, UbseComDataDesc &retData, bool withCopy)
{
    if (respMsg == nullptr) {
        UBSE_LOG_ERROR << "response is nullptr.";
        return UBSE_ERROR;
    }
    UbseResult ret = respMsg->SetInputRawData(retData.data, retData.len, withCopy);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "response set data fail," << FormatRetCode(ret);
        return ret;
    }
    ret = respMsg->Deserialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "response deserialize fail," << FormatRetCode(ret);
        return ret;
    }
    ret = respMsg->CheckMsgBody();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "check response msg body error," << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

std::pair<std::string, UbseChannelType> SplitPayload(const std::string &payload)
{
    std::vector<std::string> payloadPair;
    ubse::utils::Split(payload, PAYLOAD_SPLIT_SEP, payloadPair);
    if (payloadPair.size() <= 1) {
        return std::make_pair(payload, UbseChannelType::SINGLE_SIDE);
    }
    return std::make_pair(payloadPair[0], StringToChannelType(payloadPair[1]));
}

UbseComMessage *GetMessageFromNetServiceContext(UBSHcomServiceContext &context)
{
    return static_cast<UbseComMessage *>(context.MessageData());
}

bool CheckMessageBodyLen(UBSHcomServiceContext &context, UbseComMessage &msg)
{
    return context.MessageDataLen() == (sizeof(UbseComMessage) + msg.GetMessageBodyLen());
}

uint64_t GetChannelIdFromNetServiceContext(UBSHcomServiceContext &context)
{
    return context.Channel()->GetId();
}

void GetUdsInfoFromNetServiceContext(UBSHcomServiceContext &context, UbseUdsIdInfo &udsIdInfo)
{
    UBSHcomNetUdsIdInfo uds;
    auto ret = context.Channel()->GetRemoteUdsIdInfo(uds);
    if (UBSE_RESULT_OK(ret)) {
        UBSE_LOG_DEBUG << "Uds info is " << uds.pid << "," << uds.uid << "," << uds.gid;
        udsIdInfo.pid = uds.pid;
        udsIdInfo.uid = uds.uid;
        udsIdInfo.gid = uds.gid;
    }
}

struct UbseIpV4Addr {
    uint8_t addr[4]; // 4个字符存储ipv4地址
};

struct UbseIpV6Addr {
    uint8_t addr[16]; // 16个字符存储ipv6地址
};

enum class UbseIpType {
    UBSE_IP_V4 = 0,
    UBSE_IP_V6
};

struct UbseIpAddr {
    UbseIpType type;
    union {
        UbseIpV4Addr ipv4;
        UbseIpV6Addr ipv6;
    };
};

bool IsSpecialIP(const std::string &ip)
{
    // 排除特殊 IP 地址：0.0.0.0, 127.x.x.x, 169.254.x.x
    return std::regex_match(ip, std::regex(R"(^(0\.0\.0\.0|127\..*|169\.254\..*)$)"));
}

uint32_t UbseNodeTelemetryGetIpInfo(std::vector<std::string> &ipInfos)
{
    std::ifstream file("/proc/net/fib_trie");
    if (!file.is_open()) {
        UBSE_LOG_ERROR << "Error opening file /proc/net/fib_trie";
        return UBSE_ERROR;
    }

    std::string line;
    std::unordered_set<std::string> uniqueIPs;
    std::regex ipRegex(R"(\b(\d+\.\d+\.\d+\.\d+)\b)"); // 匹配 IP 地址的正则表达式

    while (std::getline(file, line)) {
        std::smatch match;
        if (std::regex_search(line, match, ipRegex)) {
            std::string ip = match.str(1); // 正则匹配第2项
            if (!IsSpecialIP(ip)) {        // 排除特殊 IP 地址
                uniqueIPs.insert(ip);
            }
        }
    }
    file.close();
    ipInfos.insert(ipInfos.end(), uniqueIPs.begin(), uniqueIPs.end());
    return UBSE_OK;
}

UbseResult CollectIpList(std::vector<UbseIpAddr> &ubseNodeInfo)
{
    std::vector<std::string> ipInfos{};
    auto ret = UbseNodeTelemetryGetIpInfo(ipInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "collect ip list failed, " << FormatRetCode(ret);
        return ret;
    }
    ubseNodeInfo.resize(ipInfos.size());
    for (size_t i = 0; i < ubseNodeInfo.size(); i++) {
        if (!UbseNetUtil::Ipv4StringToArr(ipInfos[i], ubseNodeInfo[i].ipv4.addr)) {
            UBSE_LOG_WARN << "parse ip list failed, " << FormatRetCode(ret);
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

std::unordered_map<std::string, std::string> GetClusterIpListFromConf()
{
    std::unordered_map<std::string, std::string> ipMap;
    std::vector<std::string> ips;
    auto ubseConfModule = context::UbseContext::GetInstance().GetModule<config::UbseConfModule>();
    if (ubseConfModule == nullptr) {
        UBSE_LOG_ERROR << "Get config info failed";
        return ipMap;
    }
    std::string defaultVal;
    auto ret = ubseConfModule->GetConf<std::string>("ubse.rpc", "cluster.ipList", defaultVal);
    if (ret != UBSE_OK || defaultVal.empty()) {
        UBSE_LOG_WARN << "Unable to get cluster.ipList config," << FormatRetCode(ret) << " ,use default tcp";
        return ipMap;
    }
    std::vector<std::string> ipRangeVec;
    ubse::utils::Split(defaultVal, ",", ipRangeVec);
    for (auto& range : ipRangeVec) {
        if (range.find('-') != std::string::npos) {
            UbseNetUtil::ParseIpRangeToList(range, ips);
        } else if (UbseNetUtil::ValidIpv4Addr(range) || UbseNetUtil::ValidIpv6Addr(range)) {
            ips.emplace_back(range);
        } else {
            UBSE_LOG_WARN << "Invalid ip range:" << range;
        }
    }
    std::sort(ips.begin(), ips.end());
    for (int i = 0; i < ips.size(); ++i) {
        UBSE_LOG_INFO << "Put ip " << ips[i] << ", id " << i;
        ipMap.emplace(ips[i], std::to_string(i + 1));
    }
    return ipMap;
}

void GetTcpInfo(UbseComTcpStr &info)
{
    // 获取节点信息
    mti::MtiNodeInfo ubseNodeInfo{};
    auto module = context::UbseContext::GetInstance().GetModule<mti::UbseLcneModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "lcne module not init";
        return;
    }
    auto ret = module->UbseGetLocalNodeInfo(ubseNodeInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get local node info failed, " << FormatRetCode(ret);
        return;
    }
    UBSE_LOG_INFO << "collect node id=" << ubseNodeInfo.nodeId;
    info.nodeId = ubseNodeInfo.nodeId;
    // 获取ip信息
    std::vector<UbseIpAddr> ipList;
    ret = CollectIpList(ipList);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get local ip list failed, " << FormatRetCode(ret);
        return;
    }
    auto ipMap = GetClusterIpListFromConf();
    for (auto ip : ipList) {
        std::string ipStr;
        if (ip.type == UbseIpType::UBSE_IP_V4) {
            ipStr = UbseNetUtil::Ipv4ArrToString(ip.ipv4.addr);
        } else {
            ipStr = UbseNetUtil::Ipv6ArrToString(ip.ipv6.addr);
        }
        UBSE_LOG_INFO << "Ip str is " << ipStr;
        auto it = ipMap.find(ipStr);
        if (it == ipMap.end()) {
            continue;
        }
        info.comIp = it->first;
}
}
} // namespace ubse::com
