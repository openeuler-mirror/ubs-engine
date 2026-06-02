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

#include <chrono>
#include <thread>

#include <rapidjson/document.h>
#include "ubse_def.h"
#include "ubse_logger.h"
#include "ubse_node_controller.h"
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

MpResult EventHandler::ResolveOverCommitMode(bool& isOverCommit, bool useSimplified)
{
    if (useSimplified) {
        bool isSimplified = MpConfiguration::GetInstance().GetFaultSimplified();
        if (isSimplified) {
            isOverCommit = true;
            return MEM_POOLING_OK;
        }
    }
    return CheckIsOverCommitMode(isOverCommit);
}

bool EventHandler::IsAllOtherNodesWorking(const std::string& nodeId)
{
    const auto allNodeInfo = UbseNodeController::GetInstance().GetAllNodes();
    for (const auto& [nId, nodeInfo] : allNodeInfo) {
        if (nId != nodeId && nodeInfo.clusterState != UbseNodeClusterState::UBSE_NODE_WORKING) {
            return false;
        }
    }
    return true;
}

void EventHandler::WaitForAllOtherNodesWorking(const std::string& nodeId, NodeType& nodeType)
{
    auto timeoutSec = MpConfiguration::GetInstance().GetIpcTimeLimit();
    auto startTime = std::chrono::steady_clock::now();
    constexpr auto kRetryInterval = std::chrono::seconds(1);

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultManager] Start retry waiting for all other nodes working, nodeId=" << nodeId
        << ", nodeType=" << static_cast<int>(nodeType) << ", timeout=" << timeoutSec << "s.";

    while (!IsAllOtherNodesWorking(nodeId)) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startTime);
        if (elapsed.count() >= static_cast<int64_t>(timeoutSec)) {
            UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager] Timeout waiting for all other nodes working, nodeId=" << nodeId
                << ", nodeType=" << static_cast<int>(nodeType) << ", timeout=" << timeoutSec
                << "s, continue processing.";
            break;
        }

        std::this_thread::sleep_for(kRetryInterval);

        MpResult res = FaultNodeModule::Instance().DetermineNodeTypeOverCommit(nodeId, nodeType);
        if (res != MEM_POOLING_OK) {
            UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager] DetermineNodeType failed during retry, nodeId=" << nodeId
                << ", will continue retrying.";
            continue;
        }
    }

    if (IsAllOtherNodesWorking(nodeId)) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] All other nodes working after retry, nodeId=" << nodeId
            << ", nodeType=" << static_cast<int>(nodeType) << ".";
    }
}

MpResult EventHandler::HandleOverCommitNodeFault(const std::string& nodeId, bool isSimplified)
{
    NodeType nodeType = NodeType::ABNORMAL;
    MpResult res = FaultNodeModule::Instance().DetermineNodeTypeOverCommit(nodeId, nodeType);
    if (res != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] DetermineNodeType failed, nodeId=" << nodeId << ".";
        return res;
    }

    // 当账本为空或存在其他节点不在工作状态时，持续尝试获取账本，直到其他节点都在工作状态
    if (nodeType == NodeType::NO_RECORD || !IsAllOtherNodesWorking(nodeId)) {
        WaitForAllOtherNodesWorking(nodeId, nodeType);
    }

    if (nodeType == NodeType::NO_RECORD) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] No debt record found after retry, nodeId=" << nodeId << ".";
        return MEM_POOLING_ERROR;
    }
    if (nodeType == NodeType::BORROW_IN) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] BORROW_IN Fault is handled by ubse, nodeId=" << nodeId << ".";
        return MEM_POOLING_OK;
    }
    if (nodeType == NodeType::BORROW_OUT) {
        MpResult ret = isSimplified ?
                           OverCommitFaultNodeModule::Instance().ProcessBorrowOutNodeFaultSimplified(nodeId) :
                           OverCommitFaultNodeModule::Instance().ProcessBorrowOutNodeFault(nodeId);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager] BORROW_OUT failed, nodeId=" << nodeId << ".";
            return ret;
        }
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

MpResult EventHandler::HandleAlarmRebootEvent(ALARM_FAULT_TYPE eventId, std::string eventMessage)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultManager] HandleAlarmRebootEvent recv " << eventId << " " << eventMessage << ".";
    bool isOverCommit = false;
    MpResult ret = ResolveOverCommitMode(isOverCommit, false);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] Failed to get runmode param. Skipping fault handling.";
        return MEM_POOLING_ERROR;
    }
    std::string nodeId = eventMessage;
    if (!isOverCommit) {
        MpResult resFragment = FaultNodeModule::Instance().FragmentHandleFault(nodeId);
        if (resFragment != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] FragmentHandleFault failed.";
        }
        return resFragment;
    }
    return HandleOverCommitNodeFault(nodeId, false);
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
    bool isOverCommit = false;
    bool isSimplified = MpConfiguration::GetInstance().GetFaultSimplified();
    MpResult ret = ResolveOverCommitMode(isOverCommit, isSimplified);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] Failed to get runmode param. Skipping fault handling.";
        return MEM_POOLING_ERROR;
    }
    std::string nodeId = eventMessage;
    if (!isOverCommit) {
        MpResult resFragment = FaultNodeModule::Instance().FragmentHandleFault(nodeId);
        if (resFragment != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] FragmentHandleFault failed.";
        }
        return resFragment;
    }
    return HandleOverCommitNodeFault(nodeId, isSimplified);
}

MpResult EventHandler::HandleAlarmKernelRebootEvent(ALARM_FAULT_TYPE eventId, std::string eventMessage)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultManager] HandleAlarmKernelRebootEvent recv " << eventId << " " << eventMessage << ".";
    bool isOverCommit = false;
    bool isSimplified = MpConfiguration::GetInstance().GetFaultSimplified();
    MpResult ret = ResolveOverCommitMode(isOverCommit, isSimplified);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] Failed to get runmode param. Skipping fault handling.";
        return MEM_POOLING_ERROR;
    }
    std::string nodeId = eventMessage;
    if (!isOverCommit) {
        MpResult resFragment = FaultNodeModule::Instance().FragmentHandleFault(nodeId);
        if (resFragment != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] FragmentHandleFault failed.";
        }
        return resFragment;
    }
    return HandleOverCommitNodeFault(nodeId, isSimplified);
}

} // namespace event
} // namespace mempooling