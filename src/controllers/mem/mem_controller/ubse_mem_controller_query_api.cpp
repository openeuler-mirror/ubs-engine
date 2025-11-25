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
#include "ubse_mem_controller_query_api.h"

#include "ubs_error.h"
#include "ubse_error.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_resource.h"
#include "ubse_mgr_configuration.h"
namespace ubse::mem::controller {
void UbseNodeNumaMemGet(const std::string &nodeId, std::vector<ubse::nodeController::UbseNumaInfo> &nodeNumaMemList)
{
    nodeNumaMemList.clear();
    auto nodeInfo = ubse::nodeController::UbseNodeController::GetInstance().GetAllNodes();
    if (nodeInfo.find(nodeId) == nodeInfo.end()) {
        return;
    }
    for (const auto &[locb, ubseNumaInfo] : nodeInfo[nodeId].numaInfos) {
        nodeNumaMemList.push_back(ubseNumaInfo);
    }
}

const size_t NAME_MAX_LEN = 47;
int32_t UbseMemFdGet(const std::string &name, def::UbseMemFdDesc &fdDesc)
{
    if (name.size() > NAME_MAX_LEN) {
        return -1;
    }
    election::UbseRoleInfo currentRoleInfo{};
    if (auto ret = UbseGetCurrentNodeInfo(currentRoleInfo); ret != UBSE_OK) {
        return ret;
    }
    NodeMemDebtInfoMap debtInfoMap{};
    auto res = GetNdeoMemDebtInfoMap(currentRoleInfo.nodeId, debtInfoMap);
    if (res != UBSE_OK) {
        return res;
    }
    bool found = false;
    for (const auto &[nodeId, debtInfos] : debtInfoMap) {
        auto iterator = debtInfos.fdImportObjMap.find(name);
        if (iterator == debtInfos.fdImportObjMap.end()) {
            continue;
        }
        // 找到目标对象，提取信息
        auto importObj = iterator->second;
        fdDesc.name = name;
        fdDesc.memIds.clear();
        for (const auto &obmmInfo : importObj.status.importResults) {
            fdDesc.memIds.push_back(obmmInfo.memId);
        }
        fdDesc.totalMemSize = importObj.req.size;
        fdDesc.unitSize = ubse::mem::strategy::MemMgrConfiguration::GetInstance().GetUnitSize();
        found = true;
        break; // 找到后立即退出循环
    }

    return found ? UBSE_OK : UBS_ENGINE_ERR_NOT_EXIST; // 根据查找结果返回
}

int32_t UbseMemFdList(std::vector<def::UbseMemFdDesc> &fdDescs)
{
    fdDescs.clear();
    election::UbseRoleInfo currentRoleInfo{};
    if (auto ret = UbseGetCurrentNodeInfo(currentRoleInfo); ret != UBSE_OK) {
        return ret;
    }
    NodeMemDebtInfoMap debtInfoMap{};
    auto res = GetNdeoMemDebtInfoMap(currentRoleInfo.nodeId, debtInfoMap);
    if (res != UBSE_OK) {
        return res;
    }
    for (const auto &[nodeId, debtInfos] : debtInfoMap) {
        for (const auto &[name, importObj] : debtInfos.fdImportObjMap) {
            def::UbseMemFdDesc fdDesc{};
            fdDesc.name = name;
            fdDesc.memIds.clear();
            for (const auto &obmmInfo : importObj.status.importResults) {
                fdDesc.memIds.push_back(obmmInfo.memId);
            }
            fdDesc.totalMemSize = importObj.req.size;
            fdDesc.unitSize = ubse::mem::strategy::MemMgrConfiguration::GetInstance().GetUnitSize();
            fdDescs.push_back(fdDesc);
        }
    }
    return UBSE_OK;
}

int32_t UbseMemNumaGet(const std::string &name, def::UbseMemNumaDesc &numaDesc)
{
    election::UbseRoleInfo currentRoleInfo{};
    if (auto ret = UbseGetCurrentNodeInfo(currentRoleInfo); ret != UBSE_OK) {
        return ret;
    }
    NodeMemDebtInfoMap debtInfoMap{};
    auto res = GetNdeoMemDebtInfoMap(currentRoleInfo.nodeId, debtInfoMap);
    if (res != UBSE_OK) {
        return res;
    }
    bool found = false;
    for (const auto &[nodeId, debtInfos] : debtInfoMap) {
        auto iterator = debtInfos.numaImportObjMap.find(name);
        if (iterator == debtInfos.numaImportObjMap.end()) {
            continue;
        }
        auto importObj = iterator->second;
        numaDesc.name = name;
        if (importObj.status.importResults.empty()) {
            return UBSE_ERROR;
        }
        numaDesc.numaId = importObj.status.importResults[0].numaId;
        found = true;
        break; // 找到后立即退出循环
    }

    return found ? UBSE_OK : UBS_ENGINE_ERR_NOT_EXIST; // 根据查找结果返回
}

int32_t UbseMemNumaList(std::vector<def::UbseMemNumaDesc> &numaDescs)
{
    numaDescs.clear();
    election::UbseRoleInfo currentRoleInfo{};
    if (auto ret = UbseGetCurrentNodeInfo(currentRoleInfo); ret != UBSE_OK) {
        return ret;
    }
    NodeMemDebtInfoMap debtInfoMap{};
    auto res = GetNdeoMemDebtInfoMap(currentRoleInfo.nodeId, debtInfoMap);
    if (res != UBSE_OK) {
        return res;
    }
    for (const auto &[nodeId, debtInfos] : debtInfoMap) {
        for (const auto &[name, importObj] : debtInfos.numaImportObjMap) {
            def::UbseMemNumaDesc numaDesc{};
            numaDesc.name = name;
            if (importObj.status.importResults.empty()) {
                return UBSE_ERROR;
            }
            numaDesc.numaId = importObj.status.importResults[0].numaId;
            numaDescs.push_back(numaDesc);
        }
    }
    return UBSE_OK;
}
} // namespace ubse::mem::controller