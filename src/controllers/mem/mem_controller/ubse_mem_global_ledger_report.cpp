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

#include "ubse_mem_global_ledger_report.h"

#include <algorithm>
#include <limits>
#include <new>
#include <securec.h>

#include "debt/ubse_mem_debt_info.h"
#include "message/ubse_mem_controller_serial.h"
#include "ubse_com_module.h"
#include "ubse_com_op_code.h"
#include "ubse_context.h"
#include "ubse_election_module.h"
#include "ubse_logger_module.h"
#include "ubse_mem_controller_ledger.h"
#include "ubse_mem_global_ledger_summary_store.h"
#include "ubse_node_controller.h"
#include "ubse_serial_util.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::com;
using namespace ubse::serial;
using ubse::election::Node;
using ubse::election::UbseElectionModule;
using ubse::log::FormatRetCode;
using namespace ubse::nodeController;

namespace {
constexpr size_t GLOBAL_LEDGER_MAX_SUMMARY_ITEMS = 4096;
constexpr size_t GLOBAL_LEDGER_MAX_NUMA_INFOS = 1024;
constexpr size_t GLOBAL_LEDGER_MAX_MEMIDS = 1024;
constexpr size_t GLOBAL_LEDGER_MAX_NODELIST = 128;

UbseResult GetGlobalMasterNodeId(std::string &globalMasterNodeId)
{
    auto electionModule = ubse::context::UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "get election module failed";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    Node masterNode{};
    auto ret = electionModule->UbseGetMasterNode(masterNode);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get current master failed, " << FormatRetCode(ret);
        return ret;
    }

    globalMasterNodeId = masterNode.id;
    if (globalMasterNodeId.empty()) {
        UBSE_LOG_ERROR << "global master node id is empty";
        return UBSE_ERR_NODE_NOT_EXIST;
    }
    return UBSE_OK;
}

void SerializeNumaInfos(UbseSerialization &out, const std::vector<UbseMemDebtNumaInfo> &infos)
{
    out << right_v<size_t>(infos.size());
    for (const auto &info : infos) {
        ubse::mem::serial::UbseMemDebtNumaInfoSerialization(out, info);
    }
}

bool DeserializeNumaInfos(UbseDeSerialization &in, std::vector<UbseMemDebtNumaInfo> &infos)
{
    size_t size{};
    in >> size;
    if (!in.Check() || size > GLOBAL_LEDGER_MAX_NUMA_INFOS) {
        UBSE_LOG_ERROR << "invalid global ledger numa info size=" << size;
        return false;
    }
    infos.resize(size);
    for (size_t i = 0; i < size; ++i) {
        if (!ubse::mem::serial::UbseMemDebtNumaInfoDeserialization(in, infos[i])) {
            UBSE_LOG_ERROR << "deserialize global ledger numa info failed, index=" << i;
            return false;
        }
    }
    return true;
}

void SerializeMemIds(UbseSerialization &out, const std::vector<uint16_t> &memids)
{
    out << right_v<size_t>(memids.size());
    for (const auto memid : memids) {
        out << memid;
    }
}

bool DeserializeMemIds(UbseDeSerialization &in, std::vector<uint16_t> &memids)
{
    size_t size{};
    in >> size;
    if (!in.Check() || size > GLOBAL_LEDGER_MAX_MEMIDS) {
        UBSE_LOG_ERROR << "invalid global ledger memid size=" << size;
        return false;
    }
    memids.resize(size);
    for (size_t i = 0; i < size; ++i) {
        in >> memids[i];
        if (!in.Check()) {
            UBSE_LOG_ERROR << "deserialize global ledger memid failed, index=" << i;
            return false;
        }
    }
    return true;
}

void SerializeFaultTypes(UbseSerialization &out, const std::vector<UbMemFaultType> &faultTypes)
{
    out << right_v<size_t>(faultTypes.size());
    for (const auto faultType : faultTypes) {
        uint32_t val = static_cast<uint32_t>(faultType);
        out << val;
    }
}

bool DeserializeFaultTypes(UbseDeSerialization &in, std::vector<UbMemFaultType> &faultTypes)
{
    size_t size{};
    in >> size;
    if (!in.Check() || size > GLOBAL_LEDGER_MAX_MEMIDS) {
        UBSE_LOG_ERROR << "invalid global ledger fault type size=" << size;
        return false;
    }
    faultTypes.resize(size);
    for (size_t i = 0; i < size; ++i) {
        uint32_t val{};
        in >> val;
        if (!in.Check() || val > static_cast<uint32_t>(UB_MEM_HEALTHY)) {
            UBSE_LOG_ERROR << "deserialize global ledger fault type failed, index=" << i;
            return false;
        }
        faultTypes[i] = static_cast<UbMemFaultType>(val);
    }
    return true;
}

void SerializeNodeList(UbseSerialization &out, const std::vector<std::string> &nodelist)
{
    out << right_v<size_t>(nodelist.size());
    for (const auto &nodeId : nodelist) {
        out << nodeId;
    }
}

bool DeserializeNodeList(UbseDeSerialization &in, std::vector<std::string> &nodelist)
{
    size_t size{};
    in >> size;
    if (!in.Check() || size > GLOBAL_LEDGER_MAX_NODELIST) {
        UBSE_LOG_ERROR << "invalid global ledger nodelist size=" << size;
        return false;
    }
    nodelist.resize(size);
    for (size_t i = 0; i < size; ++i) {
        in >> nodelist[i];
        if (!in.Check()) {
            UBSE_LOG_ERROR << "deserialize global ledger nodelist failed, index=" << i;
            return false;
        }
    }
    return true;
}

void SerializeUsrInfo(UbseSerialization &out, const uint8_t *usrInfo, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        out << usrInfo[i];
    }
}

bool DeserializeUsrInfo(UbseDeSerialization &in, uint8_t *usrInfo, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        in >> usrInfo[i];
    }
    return in.Check();
}

void SerializeSummaryItem(UbseSerialization &out, const UbseGlobalLedgerSummaryItem &item)
{
    uint32_t state = static_cast<uint32_t>(item.state);
    out << item.name;
    SerializeNumaInfos(out, item.numaInfos);
    out << item.blockSize << state;
    ubse::mem::serial::UbseUdsInfoSerialization(out, item.userInfo);
    SerializeMemIds(out, item.memids);
    SerializeFaultTypes(out, item.faultTypes);
    SerializeNodeList(out, item.nodelist);
    SerializeUsrInfo(out, item.usrInfo, UBSE_MAX_USR_INFO_LEN);
    out << item.shmAnonymous;
}

bool DeserializeSummaryItem(UbseDeSerialization &in, UbseGlobalLedgerSummaryItem &item)
{
    uint32_t state{};
    in >> item.name;
    if (!DeserializeNumaInfos(in, item.numaInfos)) {
        return false;
    }
    in >> item.blockSize >> state;
    if (!ubse::mem::serial::UbseUdsInfoDeserialization(in, item.userInfo) || !DeserializeMemIds(in, item.memids) ||
        !DeserializeFaultTypes(in, item.faultTypes) || !DeserializeNodeList(in, item.nodelist) ||
        !DeserializeUsrInfo(in, item.usrInfo, UBSE_MAX_USR_INFO_LEN)) {
        return false;
    }
    in >> item.shmAnonymous;
    if (!in.Check() || state > static_cast<uint32_t>(UBSE_MEM_IMPORT_DESTROYED)) {
        UBSE_LOG_ERROR << "deserialize global ledger summary item failed, state=" << state;
        return false;
    }
    item.state = static_cast<UbseMemState>(state);
    return true;
}

void SerializeSummaryMap(UbseSerialization &out, const UbseGlobalLedgerSummaryMap &items)
{
    out << right_v<size_t>(items.size());
    for (const auto &[key, item] : items) {
        out << key;
        SerializeSummaryItem(out, item);
    }
}

bool DeserializeSummaryMap(UbseDeSerialization &in, UbseGlobalLedgerSummaryMap &items)
{
    size_t size{};
    in >> size;
    if (!in.Check() || size > GLOBAL_LEDGER_MAX_SUMMARY_ITEMS) {
        UBSE_LOG_ERROR << "invalid global ledger summary item size=" << size;
        return false;
    }
    items.clear();
    for (size_t i = 0; i < size; ++i) {
        std::string key;
        UbseGlobalLedgerSummaryItem item{};
        in >> key;
        if (!DeserializeSummaryItem(in, item)) {
            UBSE_LOG_ERROR << "deserialize global ledger summary item failed, index=" << i;
            return false;
        }
        items.emplace(std::move(key), std::move(item));
    }
    return in.Check();
}

void SerializeSummaryReportReq(UbseSerialization &out, const UbseGlobalLedgerSummaryReportReq &report)
{
    out << report.nodeId << report.sourceMasterNodeId;
    SerializeSummaryMap(out, report.shmSummary.importItems);
    SerializeSummaryMap(out, report.shmSummary.exportItems);
}

class UbseGlobalLedgerReportRespMessage final : public ubse::message::UbseBaseMessage {
public:
    void SetRet(UbseResult ret)
    {
        ret_ = ret;
    }

    UbseResult GetRet() const
    {
        return ret_;
    }

    UbseResult Serialize() override
    {
        UbseSerialization out;
        out << ret_;
        if (!out.Check()) {
            UBSE_LOG_ERROR << "serialize global ledger report resp failed";
            return UBSE_ERROR_SERIALIZE_FAILED;
        }
        mOutputRawDataSize = out.GetLength();
        mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
        return UBSE_OK;
    }

    UbseResult Deserialize() override
    {
        if (mInputRawData == nullptr || mInputRawDataSize == 0) {
            UBSE_LOG_ERROR << "global ledger report resp is empty";
            return UBSE_ERROR_NULLPTR;
        }
        UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
        in >> ret_;
        if (!in.Check()) {
            UBSE_LOG_ERROR << "deserialize global ledger report resp failed";
            return UBSE_ERROR_DESERIALIZE_FAILED;
        }
        return UBSE_OK;
    }

private:
    UbseResult ret_{UBSE_OK};
};

class UbseGlobalLedgerSummaryReportMessage final : public ubse::message::UbseBaseMessage {
public:
    void SetReport(UbseGlobalLedgerSummaryReportReq report)
    {
        report_ = std::move(report);
    }

    const UbseGlobalLedgerSummaryReportReq &GetReport() const
    {
        return report_;
    }

    UbseResult Serialize() override
    {
        UbseSerialization out;
        SerializeSummaryReportReq(out, report_);
        if (!out.Check()) {
            UBSE_LOG_ERROR << "serialize global ledger summary report req failed, nodeId=" << report_.nodeId;
            return UBSE_ERROR_SERIALIZE_FAILED;
        }
        mOutputRawDataSize = out.GetLength();
        mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
        return UBSE_OK;
    }

    UbseResult Deserialize() override
    {
        if (mInputRawData == nullptr || mInputRawDataSize == 0) {
            UBSE_LOG_ERROR << "global ledger summary report req is empty";
            return UBSE_ERROR_DESERIALIZE_FAILED;
        }
        UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
        in >> report_.nodeId >> report_.sourceMasterNodeId;
        if (!DeserializeSummaryMap(in, report_.shmSummary.importItems) ||
            !DeserializeSummaryMap(in, report_.shmSummary.exportItems) || !in.Check()) {
            UBSE_LOG_ERROR << "deserialize global ledger summary report req failed";
            return UBSE_ERROR_DESERIALIZE_FAILED;
        }
        return UBSE_OK;
    }

private:
    UbseGlobalLedgerSummaryReportReq report_{};
};

class UbseGlobalLedgerSummaryReportMessageHandler final : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const ubse::message::UbseBaseMessagePtr &req, const ubse::message::UbseBaseMessagePtr &rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override
    {
        (void)ctx;
        auto request = ubse::message::UbseBaseMessage::DeConvert<UbseGlobalLedgerSummaryReportMessage>(req);
        auto response = ubse::message::UbseBaseMessage::DeConvert<UbseGlobalLedgerReportRespMessage>(rsp);
        if (request == nullptr || response == nullptr) {
            UBSE_LOG_ERROR << "convert global ledger summary report message failed";
            return UBSE_ERROR_NULLPTR;
        }
        const auto ret = UbseGlobalLedgerSummaryStore::GetInstance().PutNodeSummary(request->GetReport());
        response->SetRet(ret);
        return UBSE_OK;
    }

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_GLOBAL_LEDGER_SUMMARY_REPORT);
    }

    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP);
    }
};

template <typename TReq>
UbseResult SendGlobalLedgerReportToNode(const std::string &globalMasterNodeId, uint16_t opCode, TReq &request,
                                        UbseResult &reportRet)
{
    if (globalMasterNodeId.empty()) {
        UBSE_LOG_ERROR << "global master node id is empty";
        return UBSE_ERR_NODE_NOT_EXIST;
    }

    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "get com module failed";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    ubse::utils::Ref<UbseGlobalLedgerReportRespMessage> response = new (std::nothrow)
        UbseGlobalLedgerReportRespMessage();
    if (response == nullptr) {
        UBSE_LOG_ERROR << "new global ledger rpc response message failed";
        return UBSE_ERROR_NULLPTR;
    }

    SendParam sendParam{globalMasterNodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP), opCode};
    auto ret = comModule->RpcSend(sendParam, request, response, false);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "send global ledger report rpc failed, globalMasterNodeId=" << globalMasterNodeId
                       << ", opCode=" << opCode << ", " << FormatRetCode(ret);
        return ret;
    }
    reportRet = response->GetRet();
    return UBSE_OK;
}

} // namespace

std::vector<uint16_t> BuildImportMemIds(const std::vector<UbseMemImportResult> &importResults)
{
    std::vector<uint16_t> memids;
    memids.reserve(importResults.size());
    for (const auto &result : importResults) {
        if (result.memId > std::numeric_limits<uint16_t>::max()) {
            UBSE_LOG_WARN << "global ledger import memId exceeds uint16_t, memId=" << result.memId;
        }
        memids.emplace_back(static_cast<uint16_t>(result.memId));
    }
    return memids;
}

std::vector<uint16_t> BuildExportMemIds(const std::vector<UbseMemObmmInfo> &exportObmmInfo)
{
    std::vector<uint16_t> memids;
    memids.reserve(exportObmmInfo.size());
    for (const auto &info : exportObmmInfo) {
        if (info.memId > std::numeric_limits<uint16_t>::max()) {
            UBSE_LOG_WARN << "global ledger export memId exceeds uint16_t, memId=" << info.memId;
        }
        memids.emplace_back(static_cast<uint16_t>(info.memId));
    }
    return memids;
}

UbseGlobalLedgerSummaryItem BuildShmSummaryItem(const UbseMemShareBorrowImportObj &obj)
{
    UbseGlobalLedgerSummaryItem item{};
    item.name = obj.req.name;
    item.blockSize = obj.algoResult.blockSize;
    item.state = obj.status.state;
    item.numaInfos = obj.algoResult.importNumaInfos;
    item.userInfo = obj.req.udsInfo;
    if (memcpy_s(item.usrInfo, UBSE_MAX_USR_INFO_LEN, obj.req.usrInfo, UBSE_MAX_USR_INFO_LEN) != EOK) {
        UBSE_LOG_WARN << "copy usrInfo failed when build shm import summary, name=" << obj.req.name;
    }
    item.memids = BuildImportMemIds(obj.status.importResults);
    item.nodelist.reserve(obj.req.shmRegion.nodelist.size());
    for (const auto &node : obj.req.shmRegion.nodelist) {
        item.nodelist.emplace_back(node.nodeId);
    }
    return item;
}

UbseGlobalLedgerSummaryItem BuildShmSummaryItem(const UbseMemShareBorrowExportObj &obj)
{
    UbseGlobalLedgerSummaryItem item{};
    item.name = obj.req.name;
    item.blockSize = obj.algoResult.blockSize;
    item.state = obj.status.state;
    item.numaInfos = obj.algoResult.exportNumaInfos;
    item.userInfo = obj.req.udsInfo;
    if (memcpy_s(item.usrInfo, UBSE_MAX_USR_INFO_LEN, obj.req.usrInfo, UBSE_MAX_USR_INFO_LEN) != EOK) {
        UBSE_LOG_WARN << "copy usrInfo failed when build shm export summary, name=" << obj.req.name;
    }
    item.shmAnonymous = obj.req.shmAnonymous;
    item.memids = BuildExportMemIds(obj.status.exportObmmInfo);
    item.faultTypes.reserve(obj.status.exportObmmInfo.size());
    for (const auto &info : obj.status.exportObmmInfo) {
        item.faultTypes.push_back(info.memIdStatus);
    }
    item.nodelist.reserve(obj.req.shmRegion.nodelist.size());
    for (const auto &node : obj.req.shmRegion.nodelist) {
        item.nodelist.emplace_back(node.nodeId);
    }
    return item;
}

bool IsDestroyedState(UbseMemState state)
{
    return state == UBSE_MEM_IMPORT_DESTROYED || state == UBSE_MEM_EXPORT_DESTROYED;
}

template <typename ObjMap>
void AppendShmItems(const ObjMap &debtMap, UbseGlobalLedgerSummaryMap &items)
{
    for (const auto &[ledgerKey, obj] : debtMap) {
        const auto state = obj.status.state;
        if (IsDestroyedState(state)) {
            continue;
        }
        items.emplace(ledgerKey, BuildShmSummaryItem(obj));
    }
}

static UbseResult ReportGlobalLedgerSummaryToTarget(const UbseGlobalNodeLedgerSummary &summary,
                                                    const std::string &globalMasterNodeId)
{
    if (summary.nodeId.empty()) {
        UBSE_LOG_ERROR << "global ledger summary report nodeId is empty";
        return UBSE_ERROR_INVAL;
    }

    ubse::utils::Ref<UbseGlobalLedgerSummaryReportMessage> request = new (std::nothrow)
        UbseGlobalLedgerSummaryReportMessage();
    if (request == nullptr) {
        UBSE_LOG_ERROR << "new global ledger summary report message failed";
        return UBSE_ERROR_NULLPTR;
    }
    request->SetReport(summary);

    UbseResult reportRet = UBSE_OK;
    const auto opCode = static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_GLOBAL_LEDGER_SUMMARY_REPORT);
    auto ret = SendGlobalLedgerReportToNode(globalMasterNodeId, opCode, request, reportRet);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "send global ledger summary report failed, nodeId=" << summary.nodeId << ", "
                       << FormatRetCode(ret);
        return ret;
    }
    if (reportRet != UBSE_OK) {
        UBSE_LOG_ERROR << "global ledger summary report rejected, nodeId=" << summary.nodeId << ", "
                       << FormatRetCode(reportRet);
        return reportRet;
    }
    UBSE_LOG_INFO << "Report global ledger summary success, nodeId=" << summary.nodeId
                  << ", shmImportItems=" << summary.shmSummary.importItems.size()
                  << ", shmExportItems=" << summary.shmSummary.exportItems.size();
    return UBSE_OK;
}

UbseResult RegGlobalLedgerReportRpcHandlers()
{
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "get com module failed";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    UbseComBaseMessageHandlerPtr summaryHandler = new (std::nothrow) UbseGlobalLedgerSummaryReportMessageHandler();
    if (summaryHandler == nullptr) {
        UBSE_LOG_ERROR << "new global ledger summary report handler failed";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = comModule->RegRpcService<UbseGlobalLedgerSummaryReportMessage, UbseGlobalLedgerReportRespMessage>(
        summaryHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "register global ledger summary report handler failed, " << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

static UbseResult QueryGlobalShmNodeLedgerSummary(const std::string &targetNodeId, UbseGlobalNodeLedgerSummary &summary)
{
    summary = {};
    if (targetNodeId.empty()) {
        UBSE_LOG_ERROR << "targetNodeId is empty";
        return UBSE_ERROR_INVAL;
    }

    const auto allNodes = nodeController::UbseNodeController::GetInstance().GetAllNodes();
    if (allNodes.find(targetNodeId) == allNodes.end()) {
        UBSE_LOG_ERROR << "target node does not exist, targetNodeId=" << targetNodeId;
        return UBSE_ERR_NODE_NOT_EXIST;
    }

    summary.nodeId = targetNodeId;
    summary.sourceMasterNodeId = nodeController::UbseNodeController::GetInstance().GetCurNode().nodeId;
    const auto debtInfo = GetNoDeletedNodeMemDebtInfoById(targetNodeId);
    AppendShmItems(debtInfo.shareImportObjMap, summary.shmSummary.importItems);
    AppendShmItems(debtInfo.shareExportObjMap, summary.shmSummary.exportItems);
    return UBSE_OK;
}

static UbseResult ReportNodeLedgerSummaryToTarget(const std::string &nodeId, const std::string &globalMasterNodeId)
{
    UbseGlobalNodeLedgerSummary summary{};
    auto ret = QueryGlobalShmNodeLedgerSummary(nodeId, summary);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "query global ledger summary failed, nodeId=" << nodeId << ", " << FormatRetCode(ret);
        return ret;
    }

    ret = ReportGlobalLedgerSummaryToTarget(summary, globalMasterNodeId);
    if (ret == UBSE_OK) {
        UBSE_LOG_INFO << "report global ledger summary success, nodeId=" << nodeId;
    } else {
        UBSE_LOG_ERROR << "report global ledger summary failed, nodeId=" << nodeId << ", " << FormatRetCode(ret);
    }
    return ret;
}

UbseResult ReportNodeLedgerSummary(const std::string &nodeId)
{
    std::string globalMasterNodeId;
    auto ret = GetGlobalMasterNodeId(globalMasterNodeId);
    if (ret != UBSE_OK) {
        return ret;
    }
    return ReportNodeLedgerSummary(nodeId, globalMasterNodeId);
}

UbseResult ReportNodeLedgerSummary(const std::string &nodeId, const std::string &globalMasterNodeId)
{
    if (nodeId.empty()) {
        UBSE_LOG_ERROR << "nodeId is empty";
        return UBSE_ERROR_INVAL;
    }
    if (globalMasterNodeId.empty()) {
        UBSE_LOG_ERROR << "globalMasterNodeId is empty";
        return UBSE_ERR_NODE_NOT_EXIST;
    }
    return ReportNodeLedgerSummaryToTarget(nodeId, globalMasterNodeId);
}
} // namespace ubse::mem::controller
