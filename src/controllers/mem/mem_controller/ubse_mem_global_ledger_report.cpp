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
#include <new>

#include "ubse_com_module.h"
#include "ubse_com_op_code.h"
#include "ubse_context.h"
#include "debt/ubse_mem_debt_info.h"
#include "message/ubse_mem_controller_serial.h"
#include "ubse_election_module.h"
#include "ubse_mem_global_ledger_summary_store.h"
#include "ubse_logger_module.h"
#include "ubse_node_controller.h"
#include "ubse_serial_util.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::com;
using namespace ubse::serial;
using ubse::election::MasterInfo;
using ubse::election::UbseElectionModule;
using ubse::log::FormatRetCode;

namespace {
constexpr size_t GLOBAL_LEDGER_MAX_SUMMARY_ITEMS = 4096;
constexpr size_t GLOBAL_LEDGER_MAX_NUMA_INFOS = 1024;

UbseResult GetGlobalMasterNodeId(std::string &globalMasterNodeId)
{
    auto electionModule = ubse::context::UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "get election module failed";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    MasterInfo masterInfo{};
    auto ret = electionModule->GetCurrentMaster(masterInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get current master failed, " << FormatRetCode(ret);
        return ret;
    }

    globalMasterNodeId = masterInfo.globalMasterId.empty() ? masterInfo.groupMasterId : masterInfo.globalMasterId;
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

void SerializeSummaryItem(UbseSerialization &out, const UbseGlobalLedgerSummaryItem &item)
{
    uint32_t state = static_cast<uint32_t>(item.state);
    out << item.name;
    SerializeNumaInfos(out, item.exportNumaInfos);
    SerializeNumaInfos(out, item.importNumaInfos);
    out << item.blockSize << state;
}

bool DeserializeSummaryItem(UbseDeSerialization &in, UbseGlobalLedgerSummaryItem &item)
{
    uint32_t state{};
    in >> item.name;
    if (!DeserializeNumaInfos(in, item.exportNumaInfos) || !DeserializeNumaInfos(in, item.importNumaInfos)) {
        return false;
    }
    in >> item.blockSize >> state;
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
    out << report.nodeId << report.sourceMasterNodeId << report.ledgerEpoch << report.reportTimestampMs;
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

class UbseGlobalLedgerSyncStateReportMessage final : public ubse::message::UbseBaseMessage {
public:
    void SetReport(UbseGlobalLedgerSyncStateReportReq report)
    {
        report_ = std::move(report);
    }

    const UbseGlobalLedgerSyncStateReportReq &GetReport() const
    {
        return report_;
    }

    UbseResult Serialize() override
    {
        UbseSerialization out;
        const uint32_t state = static_cast<uint32_t>(report_.state);
        out << report_.nodeId << state << report_.ledgerEpoch;
        if (!out.Check()) {
            UBSE_LOG_ERROR << "serialize global ledger sync state report req failed";
            return UBSE_ERROR_SERIALIZE_FAILED;
        }
        mOutputRawDataSize = out.GetLength();
        mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
        return UBSE_OK;
    }

    UbseResult Deserialize() override
    {
        if (mInputRawData == nullptr || mInputRawDataSize == 0) {
            UBSE_LOG_ERROR << "global ledger sync state report req is empty";
            return UBSE_ERROR_DESERIALIZE_FAILED;
        }

        uint32_t state{};
        UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
        in >> report_.nodeId >> state >> report_.ledgerEpoch;
        if (!in.Check()) {
            UBSE_LOG_ERROR << "deserialize global ledger sync state report req failed";
            return UBSE_ERROR_DESERIALIZE_FAILED;
        }
        if (state != static_cast<uint32_t>(UbseGlobalLedgerSyncState::SMOOTHING)) {
            UBSE_LOG_ERROR << "invalid global ledger sync state=" << state;
            return UBSE_ERROR_INVAL;
        }
        report_.state = static_cast<UbseGlobalLedgerSyncState>(state);
        return UBSE_OK;
    }

private:
    UbseGlobalLedgerSyncStateReportReq report_{};
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
        in >> report_.nodeId >> report_.sourceMasterNodeId >> report_.ledgerEpoch >> report_.reportTimestampMs;
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

class UbseGlobalLedgerSyncStateReportMessageHandler final : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const ubse::message::UbseBaseMessagePtr &req, const ubse::message::UbseBaseMessagePtr &rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override
    {
        (void)ctx;
        auto request = ubse::message::UbseBaseMessage::DeConvert<UbseGlobalLedgerSyncStateReportMessage>(req);
        auto response = ubse::message::UbseBaseMessage::DeConvert<UbseGlobalLedgerReportRespMessage>(rsp);
        if (request == nullptr || response == nullptr) {
            UBSE_LOG_ERROR << "convert global ledger sync state report message failed";
            return UBSE_ERROR_NULLPTR;
        }
        const auto ret = StoreGlobalLedgerSyncState(request->GetReport());
        response->SetRet(ret);
        return UBSE_OK;
    }

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_GLOBAL_LEDGER_SYNC_STATE);
    }

    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP);
    }
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
        const auto ret = StoreGlobalNodeLedgerSummary(request->GetReport());
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
UbseResult SendGlobalLedgerReport(uint16_t opCode, TReq &request, UbseResult &reportRet)
{
    std::string globalMasterNodeId;
    auto ret = GetGlobalMasterNodeId(globalMasterNodeId);
    if (ret != UBSE_OK) {
        return ret;
    }

    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "get com module failed";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    ubse::utils::Ref<UbseGlobalLedgerReportRespMessage> response =
        new (std::nothrow) UbseGlobalLedgerReportRespMessage();
    if (response == nullptr) {
        UBSE_LOG_ERROR << "new global ledger rpc response message failed";
        return UBSE_ERROR_NULLPTR;
    }

    SendParam sendParam{globalMasterNodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP), opCode};
    ret = comModule->RpcSend(sendParam, request, response, false);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "send global ledger report rpc failed, globalMasterNodeId=" << globalMasterNodeId
                       << ", opCode=" << opCode << ", " << FormatRetCode(ret);
        return ret;
    }
    reportRet = response->GetRet();
    return UBSE_OK;
}
} // namespace

UbseGlobalLedgerSummaryItem BuildShmSummaryItem(const std::string &name, const UbseMemAlgoResult &algoResult,
                                                UbseMemState state)
{
    UbseGlobalLedgerSummaryItem item{};
    item.name = name;
    item.exportNumaInfos = algoResult.exportNumaInfos;
    item.importNumaInfos = algoResult.importNumaInfos;
    item.blockSize = algoResult.blockSize;
    item.state = state;
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
        items.emplace(ledgerKey, BuildShmSummaryItem(obj.req.name, obj.algoResult, state));
    }
}

UbseResult StartGlobalLedgerSync(const UbseGlobalLedgerSyncStartReq &req)
{
    if (req.targetNodeId.empty()) {
        UBSE_LOG_ERROR << "targetNodeId is empty";
        return UBSE_ERROR_INVAL;
    }

    const auto allNodes = nodeController::UbseNodeController::GetInstance().GetAllNodes();
    if (allNodes.find(req.targetNodeId) == allNodes.end()) {
        UBSE_LOG_ERROR << "target node does not exist, targetNodeId=" << req.targetNodeId;
        return UBSE_ERR_NODE_NOT_EXIST;
    }
    return UBSE_OK;
}

UbseResult ReportGlobalLedgerSyncState(const std::string &nodeId, UbseGlobalLedgerSyncState state,
                                       uint64_t ledgerEpoch)
{
    if (nodeId.empty()) {
        UBSE_LOG_ERROR << "global ledger sync state report nodeId is empty";
        return UBSE_ERROR_INVAL;
    }
    if (state != UbseGlobalLedgerSyncState::SMOOTHING) {
        UBSE_LOG_ERROR << "global ledger sync state report only accepts SMOOTHING, nodeId=" << nodeId
                       << ", state=" << static_cast<uint32_t>(state);
        return UBSE_ERROR_INVAL;
    }
    const auto sourceMasterNodeId = nodeController::UbseNodeController::GetInstance().GetCurNode().nodeId;
    if (sourceMasterNodeId.empty()) {
        UBSE_LOG_ERROR << "current source master node id is empty";
        return UBSE_ERR_NODE_NOT_EXIST;
    }

    UbseGlobalLedgerSyncStateReportReq report{
        .nodeId = nodeId,
        .state = state,
        .ledgerEpoch = ledgerEpoch,
    };
    ubse::utils::Ref<UbseGlobalLedgerSyncStateReportMessage> request =
        new (std::nothrow) UbseGlobalLedgerSyncStateReportMessage();
    if (request == nullptr) {
        UBSE_LOG_ERROR << "new global ledger sync state report message failed";
        return UBSE_ERROR_NULLPTR;
    }
    request->SetReport(std::move(report));

    UbseResult reportRet = UBSE_OK;
    auto ret = SendGlobalLedgerReport(static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_GLOBAL_LEDGER_SYNC_STATE),
                                      request, reportRet);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "send global ledger sync state report failed, " << FormatRetCode(ret)
                       << ", state=" << static_cast<uint32_t>(state);
        return ret;
    }
    if (reportRet != UBSE_OK) {
        UBSE_LOG_ERROR << "global ledger sync state report rejected, " << FormatRetCode(reportRet)
                       << ", state=" << static_cast<uint32_t>(state);
        return reportRet;
    }
    UBSE_LOG_INFO << "Report global ledger sync state success, sourceMasterNodeId=" << sourceMasterNodeId
                  << ", nodeId=" << nodeId << ", state=" << static_cast<uint32_t>(state)
                  << ", ledgerEpoch=" << ledgerEpoch;
    return UBSE_OK;
}

UbseResult ReportGlobalLedgerSummary(const UbseGlobalNodeLedgerSummary &summary)
{
    if (summary.nodeId.empty()) {
        UBSE_LOG_ERROR << "global ledger summary report nodeId is empty";
        return UBSE_ERROR_INVAL;
    }

    ubse::utils::Ref<UbseGlobalLedgerSummaryReportMessage> request =
        new (std::nothrow) UbseGlobalLedgerSummaryReportMessage();
    if (request == nullptr) {
        UBSE_LOG_ERROR << "new global ledger summary report message failed";
        return UBSE_ERROR_NULLPTR;
    }
    request->SetReport(summary);

    UbseResult reportRet = UBSE_OK;
    auto ret = SendGlobalLedgerReport(static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_GLOBAL_LEDGER_SUMMARY_REPORT),
                                      request, reportRet);
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
                  << ", ledgerEpoch=" << summary.ledgerEpoch
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

    UbseComBaseMessageHandlerPtr syncStateHandler =
        new (std::nothrow) UbseGlobalLedgerSyncStateReportMessageHandler();
    if (syncStateHandler == nullptr) {
        UBSE_LOG_ERROR << "new global ledger sync state report handler failed";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = comModule->RegRpcService<UbseGlobalLedgerSyncStateReportMessage, UbseGlobalLedgerReportRespMessage>(
        syncStateHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "register global ledger sync state report handler failed, " << FormatRetCode(ret);
        return ret;
    }

    UbseComBaseMessageHandlerPtr summaryHandler = new (std::nothrow) UbseGlobalLedgerSummaryReportMessageHandler();
    if (summaryHandler == nullptr) {
        UBSE_LOG_ERROR << "new global ledger summary report handler failed";
        return UBSE_ERROR_NULLPTR;
    }
    ret = comModule->RegRpcService<UbseGlobalLedgerSummaryReportMessage, UbseGlobalLedgerReportRespMessage>(
        summaryHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "register global ledger summary report handler failed, " << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

UbseResult QueryGlobalShmNodeLedgerSummary(const std::string &targetNodeId, UbseGlobalNodeLedgerSummary &summary)
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
} // namespace ubse::mem::controller
