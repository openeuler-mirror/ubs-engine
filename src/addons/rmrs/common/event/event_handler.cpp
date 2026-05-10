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

#include "event_handler.h"

#include <rapidjson/document.h>
#include "ubse_def.h"
#include "ubse_logger.h"
#include "ubse_storage.h"
#include "fault_memid_helper.h"
#include "fault_node_module.h"
#include "mp_configuration.h"
#include "mp_error.h"
#include "mp_json_util.h"
#include "mp_string_util.h"
#include "over_commit_fault_memid_helper.h"
#include "over_commit_fault_node_module.h"

static const std::string KEYPREFIX_SMAP = "mempooling";
static const int OVERCOMMIT_MODE = 0;
static const int MEMFRAGMENT_MODE = 1;

namespace mempooling {
namespace event {

using namespace ubse::log;
using namespace ubse::storage;

static void GetRunMode(const std::string& keyPrefix, const std::string& key, const UbseByteBuffer& buff, void* ctx)
{
    if (ctx == nullptr || buff.data == nullptr || buff.len != 1) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] GetRunMode invalid params.";

        return;
    }
    int& runMode = *(static_cast<int*>(ctx));
    runMode = static_cast<int>(buff.data[0]);
    return;
}

static MpResult CheckIsOverCommitMode(bool& isOverCommit)
{
    // 容器场景直接走超分
    auto sceneType = MpConfiguration::GetInstance().GetSceneType();
    if (sceneType == MpSceneType::CONTAINER_SCENE) {
        isOverCommit = true;
        return MEM_POOLING_OK;
    }

    int runMode = -1;
    MpResult ret = UbseStorageQueryData("mempooling", "_runMode", &runMode, GetRunMode);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] Failed to query runmode data.";
        return MEM_POOLING_ERROR;
    }
    if (runMode == -1) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] No runMode record.";
        return MEM_POOLING_ERROR;
    }

    if (runMode == OVERCOMMIT_MODE) {
        isOverCommit = true;
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] The fault handling mode is overCommit.";
    } else if (runMode == MEMFRAGMENT_MODE) {
        isOverCommit = false;
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] The fault handling mode is memFragment.";
    } else {
        return MEM_POOLING_ERROR;
    }

    return MEM_POOLING_OK;
}

static MpResult FaultMemIdManageHelper(bool isOverCommit, std::string importNodeIdStr, uint64_t importMemId,
                                       std::string eventMessage)
{
    if (isOverCommit) {
        MpResult res =
            mempooling::OverCommitFaultMemIdHelper::Instance().FaultMemIdManageHelper(importNodeIdStr, importMemId);
        if (res != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[OverCommit] MemIdFaultManage failed, eventMessage:" << eventMessage << ".";
        }
        return res;
    } else {
        MpResult res = FaultMemIdHelper::Instance().FaultMemIdManageHelper(importNodeIdStr, importMemId, false, false);
        if (res != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager][MemId] FaultMemIdManageHelper failed, eventMessage:" << eventMessage << ".";
        }
        return res;
    }
}

MpResult EventHandler::FragMentHandleFault(std::string nodeId) 
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] FragMentHandleFault start.";
    const uint32_t faultProcessTimeoutMs = MpConfiguration::GetInstance().GetFaultProcessTimeout();
    const auto startTime = std::chrono::steady_clock::now();

    while(true) {
        // =========基于不信任原则，获取账本并筛选合法条目
        std::vector<BorrowRecord> fragMentFaultBorrowRecords;
        MpResult res = UpdateBorrowRecordsWithFragMentFault(nodeId, fragMentFaultBorrowRecords);
        if (res != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager] UpdateBorrowRecordsWithFragMentFault failed.";
            return res;
        }
        // =========超时判断：计算已耗时，超时则退出轮询并返回错误=========
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
        if (elapsedMs >= faultProcessTimeoutMs)
        {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager] [FaultLentNode] ProcessBorrowOutNodeFault timeout! Elapsed: " << elapsedMs
                << "ms, Timeout threshold: " << faultProcessTimeoutMs << "ms, NodeId: " << nodeId;
            if (fragMentFaultBorrowRecords.empty()) {
                UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] FragMentHandleFault succeed."
                return MEM_POOLING_OK;
            }
            return MEM_POOLING_ERROR;
        }

        NodeType nodeType = NodeType::ABNORMAL;
        MpResult res = FaultNodeModule::Instance().DetermineNodeTypeFragment(nodeId, nodeType);
        if (res != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager] DetermineNodeType failed " << eventMessage << ".";
            continue;
        }
        if (nodeType == NodeType::BORROW_IN) {
            UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager] BORROW_IN Fault " << eventId << " is handled by MXE.";
        } else if (nodeType == NodeType::BORROW_OUT) {
            res = FaultNodeModule::Instance().ProcessBorrowOutNodeFault(nodeId, true);                 
            if (res != MEM_POOLING_OK) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[FaultManager] Process BORROW_OUT node fault failed, eventMessage:" << eventMessage << ".";
            }
        }
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] FragMentHandleFault end.";
    return MEM_POOLING_OK;
}

MpResult EventHandler::HandleAlarmRebootEvent(ALARM_FAULT_TYPE eventId, std::string eventMessage)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultManager] HandleAlarmRebootEvent recv " << eventId << " " << eventMessage << ".";
    // 读取超分/碎片场景
    bool isOverCommit = false;
    MpResult retRunMode = CheckIsOverCommitMode(isOverCommit);
    if (retRunMode != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] Failed to get runmode param. Skipping fault handling.";
        return MEM_POOLING_ERROR;
    }
    if (!isOverCommit) { // 碎片场景
        MpResult resFragMent = FaultNodeModule::Instance().FragMentHandleFault(nodeId);
        if (resFragMent != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] FragMentHandleFault failed.";
            return resFragMent;
        }
    } else { //超分场景
        std::string nodeId = eventMessage;
        // 调用节点级重启前置处理函数  成功返回0 失败返回1
        NodeType nodeType = NodeType::ABNORMAL;
        MpResult res = FaultNodeModule::Instance().DetermineNodeTypeOverCommit(nodeId, nodeType);
        if (res != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager] DetermineNodeType failed " << eventMessage << ".";
            return res;
        }
        if (nodeType == NodeType::BORROW_IN) {
            UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager] BORROW_IN Fault " << eventId << " is handled by MXE.";
            return MEM_POOLING_OK;
        }
        if (nodeType == NodeType::BORROW_OUT) {
            res = OverCommitFaultNodeModule::Instance().ProcessBorrowOutNodeFault(nodeId);               
            if (res != MEM_POOLING_OK) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[FaultManager] Process BORROW_OUT node fault failed, eventMessage:" << eventMessage << ".";
                return res;
            }
        }
    }
    return MEM_POOLING_OK;
}

MpResult EventHandler::HandleAlarmUceEvent(ALARM_FAULT_TYPE eventId, std::string eventMessage)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultManager] HandleAlarmUceEvent recv, eventId:" << eventId << " eventMessage:" << eventMessage << ".";
    rapidjson::Document doc;
    (void)doc.Parse(eventMessage.c_str());
    if (doc.HasParseError()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] HandleAlarmUceEvent eventMessage parse error, eventMessage:" << eventMessage << ".";
        return MEM_POOLING_ERROR;
    }
    std::string importNodeIdStr;
    std::string importMemIdStr;
    if (!JsonUtil::GetJsonStringValue(doc, "importNodeID", importNodeIdStr) ||
        !JsonUtil::RackMemGetJsonItemStr(doc, "importMemID", importMemIdStr)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] EventMessage GetJsonString error, eventMessage:" << eventMessage << ".";
        return MEM_POOLING_ERROR;
    }
    uint64_t importMemId = 0;
    auto ret = MpStringUtil::SafeStoull(importMemIdStr, importMemId);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] HandleAlarmUceEvent Stoull failed, importMemIdStr:" << importMemIdStr << ","
            << "ret:" << ret << ".";
        return MEM_POOLING_ERROR;
    }

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultManager] The recv, importMemId:" << importMemId << " importNodeIdStr:" << importNodeIdStr << ".";
    // 读取超分/碎片场景
    bool isOverCommit = false;
    MpResult retRunMode = CheckIsOverCommitMode(isOverCommit);
    if (retRunMode != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] Failed to get runmode param. Skipping fault handling.";
        return MEM_POOLING_ERROR;
    }
    // 调用memId级别故障处理函数
    return FaultMemIdManageHelper(isOverCommit, importNodeIdStr, importMemId, eventMessage);
}

MpResult EventHandler::HandlePanicEvent(ALARM_FAULT_TYPE eventId, std::string eventMessage)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultManager] HandlePanicEvent recv " << eventId << " " << eventMessage << ".";
    // 读取超分/碎片场景
    bool isOverCommit = false;
    MpResult retRunMode = CheckIsOverCommitMode(isOverCommit);
    if (retRunMode != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] Failed to get runmode param. Skipping fault handling.";
        return MEM_POOLING_ERROR;
    }

    if (!isOverCommit) { // 碎片场景
        MpResult resFragMent = FaultNodeModule::Instance().FragMentHandleFault(nodeId);
        if (resFragMent != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] FragMentHandleFault failed.";
            return resFragMent;
        }
    } else { //超分场景
        std::string nodeId = eventMessage;
        // 调用节点级重启前置处理函数  成功返回0 失败返回1
        NodeType nodeType = NodeType::ABNORMAL;
        MpResult res = FaultNodeModule::Instance().DetermineNodeTypeOverCommit(nodeId, nodeType);
        if (res == MEM_POOLING_ERROR || (nodeType != NodeType::BORROW_IN && nodeType != NodeType::BORROW_OUT)) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager] DetermineNodeType failed " << eventMessage << ",res=" << res << ".";
            return res;
        }

        MpResult ret;
        if (nodeType == NodeType::BORROW_IN) {
            UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager] BORROW_IN Fault " << eventId << " is handled by MXE.";
            return MEM_POOLING_OK;
        }
        if (nodeType == NodeType::BORROW_OUT) {
            ret = OverCommitFaultNodeModule::Instance().ProcessBorrowOutNodeFault(nodeId);
            if (ret != MEM_POOLING_OK) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[FaultManager] BORROW_OUT failed" << eventMessage << ".";
                return ret;
            }
        }
    }
    return MEM_POOLING_OK;
}

MpResult EventHandler::HandleAlarmKernelRebootEvent(ALARM_FAULT_TYPE eventId, std::string eventMessage)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultManager] HandleAlarmKernelRebootEvent recv " << eventId << " " << eventMessage << ".";
    bool isOverCommit = false;
    MpResult retRunMode = CheckIsOverCommitMode(isOverCommit);
    if (retRunMode != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] Failed to get runmode param. Skipping fault handling.";
        return MEM_POOLING_ERROR;
    }

    if (!isOverCommit) { // 碎片场景
        MpResult resFragMent = FaultNodeModule::Instance().FragMentHandleFault(nodeId);
        if (resFragMent != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] FragMentHandleFault failed.";
            return resFragMent;
        }
    } else { //超分场景
        std::string nodeId = eventMessage;
        // 调用节点级重启前置处理函数  成功返回0 失败返回1
        NodeType nodeType = NodeType::ABNORMAL;
        MpResult res = FaultNodeModule::Instance().DetermineNodeTypeOverCommit(nodeId, nodeType);
        if (res == MEM_POOLING_ERROR || (nodeType != NodeType::BORROW_IN && nodeType != NodeType::BORROW_OUT)) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager] DetermineNodeType failed " << eventMessage << ",res=" << res << ".";
            return res;
        }

        MpResult ret;
        if (nodeType == NodeType::BORROW_IN) {
            UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager] BORROW_IN Fault " << eventId << " is handled by MXE.";
            return MEM_POOLING_OK;
        }
        if (nodeType == NodeType::BORROW_OUT) {
            ret = OverCommitFaultNodeModule::Instance().ProcessBorrowOutNodeFault(nodeId);
            if (ret != MEM_POOLING_OK) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] BORROW_OUT failed" << eventMessage;
                return ret;
            }
        }
    }
    return MEM_POOLING_OK;
}

} // namespace event
} // namespace mempooling