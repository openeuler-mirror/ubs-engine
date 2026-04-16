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

#include "ubse_cli_mem_query.h"
#include <string>

#include "ubse_cli_buffer_guard.h"
#include "ubse_cli_constant.h"
#include "ubse_cli_mem_struct.h"
#include "ubse_cli_reg.h"
#include "ubse_cli_whitelist.h"
#include "ubse_error.h"
#include "ubse_ipc_client.h"
#include "ubse_ipc_common.h"
#include "ubse_mmi_def.h"
#include "ubse_pointer_process.h"

namespace ubse::cli::reg {
using namespace ubse::cli::framework;
using namespace ubse::mem::controller;
using namespace ubse::serial;
using namespace mem::controller::message;
using namespace ubse::adapter_plugins::mmi;

class UbseCliMemQuery::UbseCliMemQueryImpl {
public:
    template <typename T, uint16_t ModuleCode, uint16_t OpCode>
    bool UbseCliMemQueryRequest(const std::string &name, T &container)
    {
        UbseSerialization ubse_req_serial;
        ubse_req_serial << name;
        if (!ubse_req_serial.Check()) {
            errorMsg_ = data::error::REQUEST_INFO_SER_FAILED;
            return false;
        }
        ubse_api_buffer_t ubse_req_buffer{ ubse_req_serial.GetBuffer(),
            static_cast<uint32_t>(ubse_req_serial.GetLength()) };
        ubse_api_buffer_t ubse_res_buffer{};
        UbseCliBufferGuard ubseCliBufferGuard(ubse_res_buffer);
        if (uint32_t ret = ubse_invoke_call(ModuleCode, OpCode, &ubse_req_buffer, &ubse_res_buffer); ret != UBSE_OK) {
            errorCode_ = ret;
            errorMsg_ = std::string("ERROR: Internal error with error code " + std::to_string(ret) + ".");
            return false;
        }
        UbseDeSerialization ubse_de_serial(ubse_res_buffer.buffer, ubse_res_buffer.length);
        if (!container.Deserialize(ubse_de_serial)) {
            errorMsg_ = data::error::RES_INFO_DESER_FAILED;
            return false;
        }
        return true;
    }

    template <typename InfoType, uint16_t ModuleCode, uint16_t OpCode>
    std::shared_ptr<framework::UbseCliResultEcho> HandleQueryError(const std::string &name)
    {
        if (errorCode_ == UBSE_ERR_NOT_EXIST) {
            return UbseCliRegModule::UbseCliStringPromptReply(memory::error::MEM_QUERY_NOT_EXIST_IN_CREATING);
        } else {
            return UbseCliRegModule::UbseCliStringPromptReply(errorMsg_ +
                " The failure occurred during the query process.");
        }
    }

    template <typename InfoType, uint16_t ModuleCode, uint16_t OpCode>
    std::shared_ptr<framework::UbseCliResultEcho> HandleWaitCreatingState(const std::string &name)
    {
        InfoType info{};
        while (true) {
            if (!UbseCliMemQueryRequest<InfoType, ModuleCode, OpCode>(name, info)) {
                return HandleQueryError<InfoType, ModuleCode, OpCode>(name);
            }

            switch (info.state) {
                case UbseMemStage::UBSE_EXIST:
                    return UbseCliRegModule::UbseCliStringPromptReply(info.GetStringResult());
                case UbseMemStage::UBSE_ERR_WAIT_UNEXPORT:
                case UbseMemStage::UBSE_NOT_EXIST:
                    return UbseCliRegModule::UbseCliStringPromptReply(memory::error::MEM_QUERY_NOT_EXIST_IN_CREATING);
                case UbseMemStage::UBSE_ERR_ONLY_IMPORT:
                    return UbseCliRegModule::UbseCliStringPromptReply(info.GetStringResult() +
                        "\nbut export node is fault");
                case UbseMemStage::UBSE_CREATING:
                case UbseMemStage::UBSE_DELETING:
                    sleep(RETRY_WAIT_TIME);
                    continue;
                default:
                    return UbseCliRegModule::UbseCliStringPromptReply(memory::error::MEM_STATE_UNKNOWN_IN_CREATING);
            }
        }
    }

    template <typename InfoType, uint16_t ModuleCode, uint16_t OpCode>
    std::shared_ptr<framework::UbseCliResultEcho> HandleNonWaitCreatingState(const std::string &name)
    {
        InfoType info{};
        if (!UbseCliMemQueryRequest<InfoType, ModuleCode, OpCode>(name, info)) {
            return HandleQueryError<InfoType, ModuleCode, OpCode>(name);
        }
        return UbseCliRegModule::UbseCliStringPromptReply(info.GetStringResult());
    }

    template <typename InfoType, uint16_t ModuleCode, uint16_t OpCode>
    std::shared_ptr<framework::UbseCliResultEcho> UbseCliQueryMemByNameImpl(const std::string &name, WaitType waitType)
    {
        if (waitType == WaitType::WAIT_CREATING) {
            return HandleWaitCreatingState<InfoType, ModuleCode, OpCode>(name);
        } else {
            return HandleNonWaitCreatingState<InfoType, ModuleCode, OpCode>(name);
        }
    }

    bool ShmMemoryGet(const std::string &name, UbseMemShmInfo &shmDesc)
    {
        UbseSerialization serial;
        serial << name;
        if (!serial.Check()) {
            errorMsg_ = data::error::REQUEST_INFO_SER_FAILED;
            return false;
        }
        ubse_api_buffer_t reqBuffer{serial.GetBuffer(), static_cast<uint32_t>(serial.GetLength())};
        ubse_api_buffer_t resBuffer{};

        uint32_t ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_CLI_SHM_INFO_GET_BY_NAME, &reqBuffer, &resBuffer);
        UbseCliBufferGuard cliBufferGuard(resBuffer);
        if (ret != UBSE_OK) {
            errorCode_ = ret;
            errorMsg_ = std::string("ERROR: Internal error with error code " + std::to_string(ret) + ".");
            return false;
        }
        // 解序列化
        UbseDeSerialization deSerialization(resBuffer.buffer, resBuffer.length);
        if (!shmDesc.Deserialize(deSerialization)) {
            errorMsg_ = data::error::RES_INFO_DESER_FAILED;
            return false;
        }
        return true;
    }

    // 判断是否为 export 操作（create/delete）
    static inline bool IsExportOperation(UbseCliShmOperation op)
    {
        return op == UbseCliShmOperation::CREATE || op == UbseCliShmOperation::DELETE;
    }

    // 判断是否为删除类操作（delete/detach）
    static inline bool IsRemoveOperation(UbseCliShmOperation op)
    {
        return op == UbseCliShmOperation::DELETE || op == UbseCliShmOperation::DETACH;
    }

    // 处理 EXIST 状态
    static std::shared_ptr<UbseCliResultEcho> HandleExistState(const UbseMemShmInfo &desc, UbseCliShmOperation op)
    {
        bool isAttach = !IsExportOperation(op);
        return UbseCliRegModule::UbseCliStringPromptReply(desc.GetStringResult(isAttach));
    }

    // 处理 ERR_ONLY_IMPORT 状态（需要额外提示）
    static std::shared_ptr<UbseCliResultEcho> HandleErrOnlyImportState(const UbseMemShmInfo &desc,
                                                                       UbseCliShmOperation op)
    {
        bool isAttach = !IsExportOperation(op);
        std::string message = desc.GetStringResult(isAttach);
        message += "\nbut export node is faulty";
        return UbseCliRegModule::UbseCliStringPromptReply(message);
    }

    // 处理已归还状态（NOT_EXIST 或 ERR_WAIT_UNEXPORT）
    static std::shared_ptr<UbseCliResultEcho> HandleMemoryRemoved(UbseCliShmOperation op)
    {
        if (IsRemoveOperation(op)) {
            return UbseCliRegModule::UbseCliStringPromptReply(GetSuccessMessage(op));
        }
        return UbseCliRegModule::UbseCliStringPromptReply("Memory not exist");
    }

    // 获取操作对应的成功消息
    static std::string GetSuccessMessage(UbseCliShmOperation op)
    {
        switch (op) {
            case UbseCliShmOperation::CREATE:
                return "Create successfully";
            case UbseCliShmOperation::DELETE:
                return "Delete successfully";
            case UbseCliShmOperation::ATTACH:
                return "Attach successfully";
            case UbseCliShmOperation::DETACH:
                return "Detach successfully";
            default:
                return "Operation successfully";
        }
    }

    std::shared_ptr<framework::UbseCliResultEcho> UbseCliQueryShmMemByNameImpl(const std::string &name,
                                                                               UbseCliShmOperation operation)
    {
        while (true) {
            UbseMemShmInfo shmDesc;
            if (!ShmMemoryGet(name, shmDesc)) {
                // 处理获取失败
                if (errorCode_ == UBSE_ERR_NOT_EXIST) {
                    return UbseCliRegModule::UbseCliStringPromptReply(memory::error::MEM_QUERY_NOT_EXIST_IN_CREATING);
                } else {
                    return UbseCliRegModule::UbseCliStringPromptReply(
                        errorMsg_ + " The failure occurred during the query process.");
                }
            }
            // 获取当前状态并处理
            UbseMemStage state = IsExportOperation(operation) ? shmDesc.exportState : shmDesc.importState;
            switch (state) {
                case UbseMemStage::UBSE_EXIST:
                    return HandleExistState(shmDesc, operation);
                case UbseMemStage::UBSE_ERR_ONLY_IMPORT:
                    return HandleErrOnlyImportState(shmDesc, operation);
                case UbseMemStage::UBSE_CREATING:
                case UbseMemStage::UBSE_DELETING:
                    sleep(RETRY_WAIT_TIME);
                    break;
                case UbseMemStage::UBSE_ERR_WAIT_UNEXPORT:
                case UbseMemStage::UBSE_NOT_EXIST:
                    return HandleMemoryRemoved(operation);
                default:
                    return UbseCliRegModule::UbseCliStringPromptReply(memory::error::MEM_STATE_UNKNOWN_IN_CREATING);
            }
        }
    }

private:
    const int RETRY_WAIT_TIME = 5;

private:
    uint32_t errorCode_{ UBSE_OK };
    std::string errorMsg_{};
};

UbseCliMemQuery::UbseCliMemQuery()
{
    this->pImpl_ = SafeMakeUnique<UbseCliMemQueryImpl>();
}
UbseCliMemQuery::~UbseCliMemQuery() noexcept = default;

std::shared_ptr<framework::UbseCliResultEcho> UbseCliMemQuery::UbseCliGetNumaMemByName(const std::string &name,
    WaitType waitType)
{
    if (this->pImpl_ == nullptr) {
        return UbseCliRegModule::UbseCliStringPromptReply(systemd::error::ALLOCATION_ERROR);
    }
    return this->pImpl_->UbseCliQueryMemByNameImpl<UbseNumaInfo, static_cast<uint16_t>(UBSE_MEM),
        static_cast<uint16_t>(UBSE_MEM_CLI_NUMA_INFO_GET_BY_NAME)>(name, waitType);
}
std::shared_ptr<framework::UbseCliResultEcho> UbseCliMemQuery::UbseCliGetFdMemByName(const std::string &name,
    WaitType waitType)
{
    if (this->pImpl_ == nullptr) {
        return UbseCliRegModule::UbseCliStringPromptReply(systemd::error::ALLOCATION_ERROR);
    }
    return this->pImpl_->UbseCliQueryMemByNameImpl<UbseFdInfo, static_cast<uint16_t>(UBSE_MEM),
        static_cast<uint16_t>(UBSE_MEM_CLI_FD_INFO_GET_BY_NAME)>(name, waitType);
}


std::shared_ptr<framework::UbseCliResultEcho> UbseCliMemQuery::UbseCliGetShmMemByName(const std::string &name,
                                                                                      UbseCliShmOperation operation)
{
    if (this->pImpl_ == nullptr) {
        return UbseCliRegModule::UbseCliStringPromptReply(systemd::error::ALLOCATION_ERROR);
    }
    return this->pImpl_->UbseCliQueryShmMemByNameImpl(name, operation);
}

class UbseCliMemDisplayBorrowDetail::UbseCliMemDisplayBorrowDetailImpl {
public:
    struct ClusterInfo {
        std::string nodeInfo{};
        std::string role{};
        std::string bondingEid{};
        std::string guid{};
    };
    bool UbseCliGetIdsWithHostName(std::unordered_map<std::string, std::string> &node_id_with_hostname)
    {
        UbseSerialization ubse_req_serial(constant::REQUEST_BUFFER_CAPACITY);
        ubse_api_buffer_t ubse_req_buffer{ ubse_req_serial.GetBuffer(),
            static_cast<uint32_t>(ubse_req_serial.GetLength()) };
        ubse_api_buffer_t ubse_res_buffer{};
        uint32_t ret = ubse_invoke_call(UBSE_NODE, UBSE_CLUSTER_INFO, &ubse_req_buffer, &ubse_res_buffer);
        UbseCliBufferGuard ubseCliBufferGuard(ubse_res_buffer);
        if (ret != UBSE_OK) {
            errorMsg_ = "ERROR: Internal error with error code " + std::to_string(ret);
            return false;
        }
        UbseDeSerialization ubse_de_cluster_serial(ubse_res_buffer.buffer, ubse_res_buffer.length);
        size_t size = 0;
        ubse_de_cluster_serial >> size;
        if (!ubse_de_cluster_serial.Check()) {
            errorMsg_ = std::string(node::error::NODE_ATTRIBUTE_EMPTY);
            return false;
        }

        for (size_t i = 0; i < size; i++) {
            ClusterInfo clusterInfo;
            ubse_de_cluster_serial >> clusterInfo.nodeInfo >> clusterInfo.role >> clusterInfo.bondingEid >>
                clusterInfo.guid;
            if (!ubse_de_cluster_serial.Check()) {
                errorMsg_ = std::string(node::error::NODE_ATTRIBUTE_EMPTY);
                return false;
            }
            std::string::size_type openParen = clusterInfo.nodeInfo.find('(');
            std::string::size_type closeParen = clusterInfo.nodeInfo.find(')');
            if (openParen == std::string::npos || closeParen == std::string::npos) {
                continue;
            }
            std::string hostname = clusterInfo.nodeInfo.substr(0, openParen);
            std::string nodeId = clusterInfo.nodeInfo.substr(openParen + 1, closeParen - openParen - 1);
            UbseCliWhitelist ubseCliWhitelist;
            ubseCliWhitelist.UbseCliSetNoControlChars();
            if (!ubseCliWhitelist.UbseCliIsAllowed(hostname) || !ubseCliWhitelist.UbseCliIsAllowed(nodeId)) {
                continue;
            }
            if (!std::all_of(nodeId.begin(), nodeId.end(), ::isdigit)) {
                continue;
            }
            node_id_with_hostname.insert(std::make_pair(nodeId, hostname));
        }
        if (node_id_with_hostname.empty()) {
            errorMsg_ = std::string(node::error::NODE_ATTRIBUTE_EMPTY);
            return false;
        }
        return true;
    }

    bool UbseCliFetchDebtInfoByPage(const std::string &node_id, DebtFetchType type,
        std::vector<FlatDebtInformation> &borrow_account, std::vector<FlatDebtInformation> &lend_account,
        const Filter &filter)
    {
        for (int pageNum = ubse::common::def::NO_1; pageNum <= MAX_PAGE_NUM; pageNum++) {
            DebtFetchInfo debtFetchInfo{ node_id, filter.name,    AccountTypeUtil::StringToAccountType(filter.type),
                pageNum, EACH_PAGE_SIZE, type };
            UbseSerialization ubse_account_req_serial;
            if (!DebtFetchInfo::Serialize(ubse_account_req_serial, debtFetchInfo)) {
                errorMsg_ = std::string(data::error::REQUEST_INFO_SER_FAILED);
                return false;
            }
            ubse_api_buffer_t ubse_req_account_buffer{ ubse_account_req_serial.GetBuffer(),
                static_cast<uint32_t>(ubse_account_req_serial.GetLength()) };
            ubse_api_buffer_t ubse_res_account_buffer{};
            auto ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_CLI_BORROW_DETAIL_DEBT_PARTIAL_FETCH,
                &ubse_req_account_buffer, &ubse_res_account_buffer);
            UbseCliBufferGuard ubseCliBufferGuardForAccount(ubse_res_account_buffer);
            if (ret != UBSE_OK) {
                errorMsg_ = std::string("ERROR: Internal error with error code " + std::to_string(ret));
                return false;
            }
            UbseDeSerialization ubse_account_res_serial(ubse_res_account_buffer.buffer, ubse_res_account_buffer.length);
            PartialFetchRes partialFetchRes{};
            if (!PartialFetchRes::Deserialize(ubse_account_res_serial, partialFetchRes)) {
                errorMsg_ = std::string(data::error::RES_INFO_DESER_FAILED);
                return false;
            }
            if (debtFetchInfo.type == DebtFetchType::IMPORT) {
                borrow_account.insert(borrow_account.end(), std::make_move_iterator(partialFetchRes.flatDebt.begin()),
                    std::make_move_iterator(partialFetchRes.flatDebt.end()));
            } else {
                lend_account.insert(lend_account.end(), std::make_move_iterator(partialFetchRes.flatDebt.begin()),
                    std::make_move_iterator(partialFetchRes.flatDebt.end()));
            }
            if (!partialFetchRes.NeedToContinue) {
                break;
            }
        }
        return true;
    }

    bool UbseCliGetAllDebtInfo(std::unordered_map<std::string, std::string> &node_id_with_hostname,
        std::vector<FlatDebtInformation> &borrow_account, std::vector<FlatDebtInformation> &lend_account,
        const Filter &filter)
    {
        for (const auto &[node_id, hostname] : node_id_with_hostname) {
            for (DebtFetchType type : allDebtFetchTypes) {
                if (!UbseCliFetchDebtInfoByPage(node_id, type, borrow_account, lend_account, filter)) {
                    return false;
                }
            }
        }
        return true;
    }

    void UbseCliProcessExport(const std::string &type_name, const FlatDebtInformation &account,
        std::unordered_map<std::string, UbseBorrowDetailInfo> &ubse_borrow_details)
    {
        std::string key = account.name + "_" + account.importId + " " + type_name;
        ubse_borrow_details.insert({ key, UbseBorrowDetailInfo{ account.name, type_name, account.importId,
            account.lendId, account.numaLendInfos, "fault", "-" } });

        if (UbseMemStateMap.find(static_cast<UbseMemState>(account.status)) != UbseMemStateMap.end()) {
            ubse_borrow_details[key].status = UbseMemStateMap[static_cast<UbseMemState>(account.status)];
        }
    }

    void UbseCliProcessImport(const std::string &type_name, const FlatDebtInformation &account,
        std::unordered_map<std::string, UbseBorrowDetailInfo> &ubse_borrow_details)
    {
        std::string key = account.name + "_" + account.importId + " " + type_name;
        if (ubse_borrow_details.find(key) == ubse_borrow_details.end()) {
            ubse_borrow_details.insert({ key, UbseBorrowDetailInfo{ account.name, type_name, account.importId,
                account.lendId, account.numaLendInfos, "fault", account.handle } });
            return;
        }

        if (ubse_borrow_details[key].status == "single") {
            if (UbseMemStateMap.find(static_cast<ubse::adapter_plugins::mmi::UbseMemState>(account.status)) !=
                UbseMemStateMap.end()) {
                ubse_borrow_details[key].status = UbseMemStateMap[static_cast<UbseMemState>(account.status)];
            } else {
                ubse_borrow_details[key].status = "fault";
            }
            ubse_borrow_details[key].handle = account.handle;
        }
    }

    void ProcessLendAccount(const std::vector<FlatDebtInformation> &lend_account,
        std::unordered_map<std::string, UbseBorrowDetailInfo> &ubse_borrow_details,
        std::unordered_map<std::string, UbseShmAttachRecord> &shm_export_account)
    {
        for (const auto &account : lend_account) {
            if (AccountTypeUtil::AccountTypeToString(account.type).empty()) {
                continue;
            }
            if (account.type == AccountType::SHM) {
                std::string key = account.name + "_" + account.lendId + " S";
                shm_export_account.insert({ key,
                    { false, UbseBorrowDetailInfo{ account.name, "share", account.importId, account.lendId,
                    account.numaLendInfos, "fault", "-" } } });

                if (UbseMemStateMap.find(static_cast<ubse::adapter_plugins::mmi::UbseMemState>(account.status)) !=
                    UbseMemStateMap.end()) {
                    shm_export_account[key].ubseOneBorrowDetailInfo.status =
                        UbseMemStateMap[static_cast<ubse::adapter_plugins::mmi::UbseMemState>(account.status)];
                }
            } else {
                UbseCliProcessExport(AccountTypeUtil::AccountTypeToString(account.type), account, ubse_borrow_details);
            }
        }
    }

    void ProcessBorrowAccount(const std::vector<FlatDebtInformation> &borrow_account,
        std::unordered_map<std::string, UbseBorrowDetailInfo> &ubse_borrow_details,
        std::unordered_map<std::string, UbseShmAttachRecord> &shm_export_account)
    {
        for (const auto &account : borrow_account) {
            if (AccountTypeUtil::AccountTypeToString(account.type).empty()) {
                continue;
            }
            if (account.type == AccountType::SHM) {
                std::string key = account.name + "_" + account.lendId + " S";
                auto new_key = key + account.importId;
                if (shm_export_account.find(key) == shm_export_account.end()) {
                    ubse_borrow_details.insert({ new_key, UbseBorrowDetailInfo{ account.name, "share", account.importId,
                        account.lendId, account.numaLendInfos, "fault", account.handle } });
                    continue;
                }
                shm_export_account[key].hasAttached = true;
                shm_export_account[key].ubseOneBorrowDetailInfo.borrowNode = account.importId;
                shm_export_account[key].ubseOneBorrowDetailInfo.handle = account.handle;
                ubse_borrow_details.insert({ new_key, shm_export_account[key].ubseOneBorrowDetailInfo });

                if (shm_export_account[key].ubseOneBorrowDetailInfo.status == "single") {
                    ubse_borrow_details[new_key].status =
                        UbseMemStateMap.find(static_cast<UbseMemState>(account.status)) != UbseMemStateMap.end() ?
                        UbseMemStateMap[static_cast<UbseMemState>(account.status)] :
                        "fault";
                }
            } else {
                UbseCliProcessImport(AccountTypeUtil::AccountTypeToString(account.type), account, ubse_borrow_details);
            }
        }
    }

    std::string GetErrorMsg()
    {
        return errorMsg_;
    }

private:
    std::vector<DebtFetchType> allDebtFetchTypes = { DebtFetchType::EXPORT, DebtFetchType::IMPORT };
    std::unordered_map<UbseMemState, std::string> UbseMemStateMap = {
        { UBSE_MEM_EXPORT_RUNNING, "exporting" },      { UBSE_MEM_EXPORT_SUCCESS, "single" },
        { UBSE_MEM_EXPORT_DESTROYING, "unexporting" }, { UBSE_MEM_IMPORT_RUNNING, "importing" },
        { UBSE_MEM_IMPORT_SUCCESS, "done" },           { UBSE_MEM_IMPORT_DESTROYING, "unimporting" }
    };

private:
    std::string errorMsg_{};
};
UbseCliMemDisplayBorrowDetail::UbseCliMemDisplayBorrowDetail()
{
    this->pImpl_ = SafeMakeUnique<UbseCliMemDisplayBorrowDetailImpl>();
}
std::shared_ptr<framework::UbseCliResultEcho> UbseCliMemDisplayBorrowDetail::UbseCliQueryBorrowDetail(
    const Filter &filter)
{
    if (this->pImpl_ == nullptr) {
        return UbseCliRegModule::UbseCliStringPromptReply(systemd::error::ALLOCATION_ERROR);
    }
    std::unordered_map<std::string, std::string> node_id_with_hostname{};
    if (!this->pImpl_->UbseCliGetIdsWithHostName(node_id_with_hostname)) {
        return UbseCliRegModule::UbseCliStringPromptReply(this->pImpl_->GetErrorMsg());
    }
    std::vector<ubse::mem::controller::message::FlatDebtInformation> borrow_account{};
    std::vector<ubse::mem::controller::message::FlatDebtInformation> lend_account{};
    if (!this->pImpl_->UbseCliGetAllDebtInfo(node_id_with_hostname, borrow_account, lend_account, filter)) {
        return UbseCliRegModule::UbseCliStringPromptReply(this->pImpl_->GetErrorMsg());
    }
    std::unordered_map<std::string, UbseBorrowDetailInfo> ubse_borrow_details{};
    std::unordered_map<std::string, UbseShmAttachRecord> shm_export_account{};
    this->pImpl_->ProcessLendAccount(lend_account, ubse_borrow_details, shm_export_account);
    this->pImpl_->ProcessBorrowAccount(borrow_account, ubse_borrow_details, shm_export_account);
    for (const auto &[key, value] : shm_export_account) {
        if (value.hasAttached) {
            continue;
        }
        ubse_borrow_details.insert({ key + " single_export_shm", value.ubseOneBorrowDetailInfo });
    }
    if (ubse_borrow_details.empty()) {
        return UbseCliRegModule::UbseCliStringPromptReply(memory::info::MEM_BORROW_DETAIL_EMPTY);
    }
    return UbseCliRegModule::UbseCliVariableCelReply(
        UbseBorrowDetailInfo::GetVariableCellInfo(ubse_borrow_details, node_id_with_hostname));
}
UbseCliMemDisplayBorrowDetail::~UbseCliMemDisplayBorrowDetail() noexcept = default;
}; // namespace ubse::cli::reg
