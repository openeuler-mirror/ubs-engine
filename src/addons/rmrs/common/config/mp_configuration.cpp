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

#include "mp_configuration.h"
#include <unistd.h>
#include <cstdint>
#include <vector>
#include "mp_error.h"

#include "ubse_conf.h"
#include "ubse_election.h"
#include "ubse_logger.h"

namespace mempooling {
using namespace ubse::config;
using namespace ubse::election;
using namespace ubse::log;

#define LOG_DEBUG UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_ERROR UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_INFO UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_WARN UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)

constexpr long FOUR_KB_TO_BYTES = 4096;
static MpConfiguration g_instance;
MpConfiguration& MpConfiguration::GetInstance()
{
    return g_instance;
}

uint32_t MpConfiguration::Initialize(const uint16_t modCode)
{
    moduleCode = modCode;
    LoadConfig();

    return MEM_POOLING_OK;
}

uint32_t MpConfiguration::LoadConfig()
{
    auto ret = UbseGetUInt("plugin_mempooling", "rmrs.ipc.timeout", ipcTimeLimit);
    if (ret != MEM_POOLING_OK) {
        LOG_WARN << "Get config failed, key=rmrs.ipc.timeout.";
    }
    uint32_t rawSceneType{2};
    ret = UbseGetUInt("plugin_virt_agent", "virt.sceneType", rawSceneType);
    if (ret != MEM_POOLING_OK) {
        LOG_WARN << "Get config failed, key=virt.sceneType.";
    }
    if (rawSceneType <= static_cast<uint32_t>(MpSceneType::VIRTUAL_SCENE)) {
        sceneType = static_cast<MpSceneType>(rawSceneType);
    } else {
        LOG_WARN << "Config virt.sceneType value is invalid , use default VIRTUAL_SCENE.";
        sceneType = MpSceneType::VIRTUAL_SCENE;
    }

    LOG_DEBUG << "Before, pageType=" << static_cast<int>(pageType) << ".";
    uint32_t rawPageType{2};
    ret = UbseGetUInt("plugin_virt_agent", "virt.pageType", rawPageType);
    if (ret != MEM_POOLING_OK) {
        LOG_WARN << "Get config failed, key=virt.pageType.";
    }
    if (rawPageType <= static_cast<uint32_t>(PageType::PAGE_2M)) {
        pageType = static_cast<PageType>(rawPageType);
    } else {
        LOG_WARN << "virt.pageType value is invalided rawPageType=" << rawPageType << " , use default VIRTUAL_SCENE.";
        pageType = PageType::PAGE_4K;
    }
    LOG_DEBUG << "After, pageType=" << static_cast<int>(pageType) << ".";

    ret = UbseGetBool("plugin_mempooling", "rmrs.fragment.mustSamePlane", mustSamePlane);
    if (ret != MEM_POOLING_OK) {
        LOG_WARN << "Get config failed, key=rmrs.fragment.mustSamePlane.";
    }
    LOG_DEBUG << "Param: mustSamePlane=" << mustSamePlane << " .";

    ret = UbseGetBool("plugin_mempooling", "rmrs.fragment.enableBorrowSplit", enableBorrowSplit);
    if (ret != MEM_POOLING_OK) {
        LOG_WARN << "Get config failed, key=rmrs.fragment.enableBorrowSplit.";
    }
    LOG_DEBUG << "Param: enableBorrowSplit=" << enableBorrowSplit << " .";

    ret = UbseGetUInt("plugin_mempooling", "fragment.faultProcessTimeout", faultProcessTimeout);
    if (ret != MEM_POOLING_OK) {
        LOG_WARN << "Get config failed, key=fragment.faultProcessTimeout.";
    }
    LOG_DEBUG << "Param: faultProcessTimeout=" << faultProcessTimeout << " ms.";

    LoadUCacheConfig();

    LoadMultiNumaSceneConfig();
    LoadBasePageType();
    return ret;
}

void MpConfiguration::LoadUCacheConfig()
{
    auto ret = UbseGetBool("plugin_mempooling", "ucache.enable", ucacheEnable);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Get config failed, key: ucache.enable, ret=" << ret << ", using default value false.";
        ucacheEnable = false;
    }
}

void MpConfiguration::LoadMultiNumaSceneConfig()
{
    auto ret = UbseGetBool("plugin_mempooling", "overcommit.enableMultiNuma", multiNumaScene);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Get config failed, key: overcommit.enableMultiNuma, ret=" << ret << ", using default value false.";
        multiNumaScene = false;
    }
}

void MpConfiguration::LoadBasePageType()
{
    long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0) {
        LOG_WARN << "Get PAGE_SIZE failed , fallback to 4K page.";
        basePageSize = FOUR_KB_TO_BYTES;
        return;
    }
    basePageSize = pageSize;
    LOG_DEBUG << "Get PAGE_SIZE success, basePageSize =" << basePageSize;
}

std::string MpConfiguration::GetNodeId() const
{
    std::string localNodeId = "";
    UbseRoleInfo currentNode;
    auto ret = UbseGetCurrentNodeInfo(currentNode);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "GetNodeId failed, UbseGetCurrentNodeInfo ret: " << ret;
        return localNodeId;
    }
    localNodeId = currentNode.nodeId;
    return localNodeId;
}

std::vector<std::string> MpConfiguration::GetNodeIds() const
{
    std::vector<UbseRoleInfo> roleInfos = {};
    std::vector<std::string> ids = {};
    auto ret = UbseGetAllNodeInfos(roleInfos);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "GetNodeIds failed, UbseGetAllNodeInfos ret: " << ret;
        return ids;
    }
    for (auto &role : roleInfos) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "NodeIds: " << role.nodeId;
        ids.push_back(role.nodeId);
    }
    return ids;
}


bool MpConfiguration::GetUcacheEnable() const
{
    return ucacheEnable;
}

MpSceneType MpConfiguration::GetSceneType() const
{
    return sceneType;
}

uint32_t MpConfiguration::GetIpcTimeLimit() const
{
    return ipcTimeLimit;
}

} // namespace mempooling
