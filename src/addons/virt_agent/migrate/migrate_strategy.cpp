/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "migrate_strategy.h"
#include <ubse_api_server.h>
#include <ubse_conf.h>
#include <ubse_error.h>
#include <ubse_logger.h>
#include <ubse_mem_controller.h>
#include <ubse_node.h>
#include "ham_make_decision_msg.h"
#include "ubs_virt_agent_object_def.h"
#include "vm_sdk_def.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using namespace api::server;
using namespace ubse::config;
using namespace ubse::log;
using namespace ubse::nodeController;
using namespace ubse::mem::controller;

VmResult VirtMigrateStrategy::Register()
{
    auto ret =
        RegisterIpcHandler(UBS_VA_VM_MIGRATE, UBS_VA_MAKE_DECISION, GetMigrateStrategy, UBS_VA_VM_MIGRATE_PERMISSION);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Registration of VirtMigrateStrategy failed, " << FormatRetCode(ret);
        return ret;
    }
    return VM_OK;
}

uint32_t VirtMigrateStrategy::GetMigrateStrategy(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    UBSE_LOG_DEBUG << "Ham make decision message start.";
    // Parsing input
    HamMakeDecisionMsg hamMakeDecisionMsg(req.buffer, req.length);
    auto ret = hamMakeDecisionMsg.Deserialize();
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Ham make decision message deserialize failed. " << FormatRetCode(ret);
        return ret;
    }
    InputParams inputParams = hamMakeDecisionMsg.GetInputParams();
    UBSE_LOG_DEBUG << "mem=" << inputParams.vmMemoryMB << ", uuid=" << inputParams.uuid
                   << ", destHostName=" << inputParams.destHostName << ", destNumaId=" << inputParams.destNumaId;
    // get decision
    uint32_t migrateStrategy = static_cast<uint32_t>(MigrateStrategy::MULTICOPY_MIGRATE_POLICY);
    ret = MakeMigrateStrategyDecision(inputParams.vmMemoryMB, inputParams.uuid, inputParams.destHostName,
                                      inputParams.destNumaId, &migrateStrategy);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Make migrate strategy decision failed. " << FormatRetCode(ret);
        return ret;
    }
    // build response
    UBSE_LOG_DEBUG << "Migrate decision result=" << migrateStrategy;
    UbseIpcMessage resp{};
    resp.length = sizeof(migrateStrategy);
    resp.buffer = new (std::nothrow) uint8_t[resp.length];
    if (resp.buffer == nullptr) {
        UBSE_LOG_ERROR << "UbseIpcMessage init failed.";
        return VM_ERROR;
    }
    ret = memcpy_s(resp.buffer, resp.length, &migrateStrategy, sizeof(migrateStrategy));
    if (ret != EOK) {
        SafeDeleteArray(resp.buffer);
        UBSE_LOG_ERROR << "Migrate strategy memcpy_s failed.";
        return VM_ERROR;
    }
    ret = SendResponse(VM_OK, context.requestId, resp);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Migrate decision send response send failed. " << FormatRetCode(ret);
        SafeDeleteArray(resp.buffer);
        return ret;
    }

    SafeDeleteArray(resp.buffer);
    UBSE_LOG_DEBUG << "Migrate decision send succeed.";
    return ret;
}

uint32_t VirtMigrateStrategy::GetMigrateOneCopyMemoryBound()
{
    // UB Scenario
    // Currently, the ubse is same
    // Obtain the VM specification size for OneCopy migration.
    // If the retrieval fails or the value is not within the valid range, use the default value.
    uint32_t migrateOneCopyMemoryBound;
    auto ret = UbseGetUInt("plugin_virt_agent", UB_VM_MEMORY_BOUNDARY_KEY, migrateOneCopyMemoryBound);
    if (ret != VM_OK || migrateOneCopyMemoryBound < MIN_VM_MEMORY_BOUNDARY ||
        migrateOneCopyMemoryBound > MAX_VM_MEMORY_BOUNDARY) {
        UBSE_LOG_WARN << "The value of the key does not exist or is invalid, key: " << UB_VM_MEMORY_BOUNDARY_KEY
                      << ", use default value: " << UB_VM_MEMORY_BOUNDARY;
        migrateOneCopyMemoryBound = UB_VM_MEMORY_BOUNDARY;
    }
    return migrateOneCopyMemoryBound;
}

uint32_t VirtMigrateStrategy::GetLocalMigrateInfo(MigrateInfoBase &migrateInfoLocal, const std::string &uuid)
{
    pid_t pid = MigrateInfoUtil::GetPidByVmUUID(uuid);
    if (pid < 0) {
        UBSE_LOG_DEBUG << "Get pid by uuid failed. uuid=" << uuid;
        return VM_ERROR;
    }

    auto ret = MigrateInfoUtil::GetNumaIdAndPageSizeByPid(pid, migrateInfoLocal);
    if (ret != VM_OK) {
        UBSE_LOG_DEBUG << "Get numa id and pagesize by pid failed. pid=" << pid;
        return VM_ERROR;
    }

    ret = MigrateInfoUtil::GetSocketIdByNumaId(migrateInfoLocal.numaId, &migrateInfoLocal.socketId);
    if (ret != VM_OK) {
        UBSE_LOG_DEBUG << "Get socket id by numa id failed. numaId=" << migrateInfoLocal.numaId;
        return VM_ERROR;
    }

    return VM_OK;
}

uint32_t VirtMigrateStrategy::GetRemoteMigrateInfo(MigrateInfoBase &migrateInfoRemote, const std::string &destHostName,
                                                   uint32_t destNumaId, const std::string &dstNid)
{
    std::vector<UbseNodeNumaInfo> numaNodeInfoList{};
    auto res = UbseGetNodeNumaInfoByNodeId(dstNid, numaNodeInfoList);
    if (res != UBSE_OK || numaNodeInfoList.empty()) {
        UBSE_LOG_ERROR << "UbseGetNodeNumaInfoByNodeId failed or empty numaNodeInfoList, res="
                       << static_cast<uint32_t>(res);
        return VM_ERROR;
    }

    migrateInfoRemote.dstNodeId = dstNid;
    migrateInfoRemote.numaId = destNumaId;
    bool found = false;
    for (const auto &numaNodeInfo : numaNodeInfoList) {
        if (numaNodeInfo.numaId == destNumaId) {
            migrateInfoRemote.socketId = numaNodeInfo.socketId;
            found = true;
            break;
        }
    }
    if (!found) {
        UBSE_LOG_ERROR << "Failed to find matching numaId: " << destNumaId;
        return VM_ERROR;
    }
    return VM_OK;
}

uint32_t VirtMigrateStrategy::GetMigrateInfo(MigrateInfoBase &migrateInfoLocal, MigrateInfoBase &migrateInfoRemote,
                                             const std::string &uuid, const std::string &destHostName,
                                             uint32_t destNumaId)
{
    // Obtain local migration information
    uint32_t ret = GetLocalMigrateInfo(migrateInfoLocal, uuid);
    if (ret != VM_OK) {
        return VM_ERROR;
    }

    std::unordered_map<std::string, std::vector<VmNodeData>> nodeData;
    ret = UbseVmGetNodeTopologyInfo(JumpCount::One, nodeData);
    if (ret != VM_OK) {
        return static_cast<uint32_t>(UbseVmResult::VM_GET_NODE_TOPOLOGY_INFO_FAILED);
    }

    std::string dstNid;
    for (const auto &[fst, snd] : nodeData) {
        for (const auto &elem : snd) {
            if (elem.hostname == destHostName) {
                dstNid = elem.nodeId;
                break;
            }
        }
    }

    if (dstNid.empty()) {
        return VM_ERROR;
    }

    return GetRemoteMigrateInfo(migrateInfoRemote, destHostName, destNumaId, dstNid);
}

uint32_t IsNeighborNode(const std::string &destHostName, const uint32_t &hugePageSize, uint32_t *migrateStrategy)
{
    // Obtain the topology information of the source node. If the retrieval fails, return directly.
    std::unordered_map<std::string, std::vector<VmNodeData>> nodeData;
    if (const auto ret = UbseVmGetNodeTopologyInfo(JumpCount::One, nodeData); ret != VM_OK) {
        return static_cast<uint32_t>(UbseVmResult::VM_GET_NODE_TOPOLOGY_INFO_FAILED);
    }
    bool isNeighborNode = false;
    for (const auto &[fst, snd] : nodeData) {
        for (const auto &elem : snd) {
            if (elem.hostname == destHostName) {
                isNeighborNode = true;
                break;
            }
        }
    }
    if (isNeighborNode && hugePageSize == HUGE_PAGE_SIZE) {
        *migrateStrategy = static_cast<uint32_t>(MigrateStrategy::HAM_MIGRATE_POLICY);
    }
    return VM_OK;
}

uint32_t IsBorrowCircle(const std::string &hostname, bool &isCircle)
{
    UbseRoleInfo srcNodeInfo;
    std::string dstNid;
    // Obtain the topology information of the source node. If the retrieval fails, return directly.
    std::unordered_map<std::string, std::vector<VmNodeData>> nodeData;
    auto ret = UbseVmGetNodeTopologyInfo(JumpCount::One, nodeData);
    if (ret != VM_OK) {
        return static_cast<uint32_t>(UbseVmResult::VM_GET_NODE_TOPOLOGY_INFO_FAILED);
    }
    for (const auto &[fst, snd] : nodeData) {
        for (const auto &elem : snd) {
            if (elem.hostname == hostname) {
                dstNid = elem.nodeId;
                break;
            }
        }
    }
    if (dstNid.empty()) {
        return static_cast<uint32_t>(UbseVmResult::VM_GET_HOSTNAME_NODEID_FAILED);
    }
    ret = UbseGetCurrentNodeInfo(srcNodeInfo);
    if (ret != VM_OK) {
        return static_cast<uint32_t>(UbseVmResult::VM_GET_CUR_NODE_INFO_FAILED);
    }

    auto ubseRet = UbseMemDebtCircleCheck(srcNodeInfo.nodeId, dstNid, isCircle);
    if (ubseRet != UBSE_OK) {
        // If the request construction fails, the ham migration method will not be selected.
        UBSE_LOG_ERROR << "UbseMemDebtCircleCheck failed. " << FormatRetCode(static_cast<uint32_t>(ubseRet));
        return static_cast<uint32_t>(ubseRet);
    }

    return VM_OK;
}

uint32_t IsSameRack(const std::string &destHostName, bool &isSameRack)
{
    std::vector<NodeInfo> nodeInfos;
    auto ret = UbseGetNodeInfos(nodeInfos);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Get UbseGetNodeInfos failed. " << FormatRetCode(ret);
        return ret;
    }
    for (auto &nodeInfo : nodeInfos) {
        if (nodeInfo.hostName == destHostName) {
            isSameRack = true;
            break;
        }
    }
    return VM_OK;
}

uint32_t VirtMigrateStrategy::MakeHamMigrateDecision(const std::string &uuid, const std::string &destHostName,
                                                     uint32_t destNumaId, uint32_t *migrateStrategy)
{
    // Obtain the scenario; if the acquisition fails or it is not ham migration scenario, disable ham migration.
    bool isEnableHamMigrate = false;
    auto hamMigrationRet = UbseGetBool("plugin_virt_agent", "mig.isEnableHamMigrate", isEnableHamMigrate);
    if (hamMigrationRet != VM_OK || !isEnableHamMigrate) {
        return VM_OK;
    }

    // Check whether the migration target node is borrowed for forming a loop.
    // If the query fails or a loop is formed, use the multicopy migration method.
    bool isCircle = true;
    auto ret = IsBorrowCircle(destHostName, isCircle);
    if (ret != VM_OK || isCircle) {
        UBSE_LOG_ERROR << "IsBorrowCircle failed. " << FormatRetCode(ret);
        return ret;
    }
    MigrateInfoBase migrateInfoLocal;
    MigrateInfoBase migrateInfoRemote;
    ret = GetMigrateInfo(migrateInfoLocal, migrateInfoRemote, uuid, destHostName, destNumaId);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "GetMigrateInfo failed. " << FormatRetCode(ret);
        return ret;
    }
    // Obtain node topology. If the query fails or involves a neighboring node,
    // and the virtual machine uses 2M huge pages, ham migration is used.
    ret = IsNeighborNode(destHostName, migrateInfoLocal.pageSize, migrateStrategy);
    if (migrateInfoLocal.socketId != migrateInfoRemote.socketId) {
        *migrateStrategy = static_cast<uint32_t>(MigrateStrategy::MULTICOPY_MIGRATE_POLICY);
        return VM_OK;
    }
    return ret;
}

uint32_t VirtMigrateStrategy::MakeMigrateStrategyDecision(uint32_t vmMemoryMB, const std::string &uuid,
                                                          const std::string &destHostName, uint32_t destNumaId,
                                                          uint32_t *migrateStrategy)
{
    if (migrateStrategy == nullptr) {
        return static_cast<uint32_t>(UbseVmResult::VM_MIGRATE_STRATEGY_NULL_POINTER);
    }
    if (uuid.empty()) {
        return static_cast<uint32_t>(UbseVmResult::VM_DEST_UUID_EMPTY);
    }
    // Check whether the target host name is empty.
    if (destHostName.empty()) {
        return static_cast<uint32_t>(UbseVmResult::VM_DEST_HOST_NAME_EMPTY);
    }
    bool isSameRack = false;
    auto ret = IsSameRack(destHostName, isSameRack);
    if (ret != VM_OK) {
        UBSE_LOG_ERROR << "Get UbseGetNodeInfos failed. " << FormatRetCode(ret);
        return ret;
    }
    if (!isSameRack) {
        *migrateStrategy = static_cast<uint32_t>(MigrateStrategy::CROSS_RACK_MULTICOPY_MIGRATE_POLICY);
        return VM_OK;
    }
    *migrateStrategy = static_cast<uint32_t>(MigrateStrategy::MULTICOPY_MIGRATE_POLICY);
    uint32_t migrateOneCopyMemoryBound = GetMigrateOneCopyMemoryBound();
    // If the VM specifications are smaller than those required for OneCopy migration, use the OneCopy migration method.
    if (vmMemoryMB <= migrateOneCopyMemoryBound) {
        *migrateStrategy = static_cast<uint32_t>(MigrateStrategy::ONECOPY_MIGRATE_POLICY);
        return VM_OK;
    }
    return MakeHamMigrateDecision(uuid, destHostName, destNumaId, migrateStrategy);
}
} // namespace vm