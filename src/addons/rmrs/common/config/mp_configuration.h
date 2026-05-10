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

#ifndef MEMPOOLING_CONFIGURATION_H
#define MEMPOOLING_CONFIGURATION_H

#include <cstdint>
#include <string>
#include <vector>

namespace mempooling {

const std::string NODEID_KEY = "nodeId";
const std::string NODEIDS_KEY = "nodeIds";
const std::string RACK_VM_INFO_TYPY = "vm_info";
const std::string RACK_NODE_INFO_TYPY = "node_info";

class MpConfiguration;

const uint32_t MEM_SYS_NUM = 1024;
#define MP_MODULE_NAME (mempooling::MpConfiguration::GetInstance().GetModuleName())
#define MP_MODULE_CODE (mempooling::MpConfiguration::GetInstance().GetModuleCode())

enum class MpRequestState {
    SUCCESS,
    FAIL,
};

struct MpRequestResult {
    MpRequestState state;
    std::string msg;
};

enum class MpSceneType {
    CONTAINER_SCENE,
    VIRTUAL_SCENE
};

enum class IsSamePlane {
    NotSamePlane,
    SamePlane
};

enum class PageType {
    PAGE_4K,
    PAGE_2M,
};

class MpConfiguration {
public:
    static MpConfiguration& GetInstance();

    uint32_t Initialize(const uint16_t modCode);

    uint32_t LoadConfig();

    inline const char* GetModuleName()
    {
        return moduleName.c_str();
    }
    inline uint16_t GetModuleCode()
    {
        return moduleCode;
    }

    bool GetUcacheEnable() const;
    std::string GetNodeId() const;
    std::vector<std::string> GetNodeIds() const;
    MpSceneType GetSceneType() const;
    uint32_t GetIpcTimeLimit() const;

    inline PageType GetPageType()
    {
        return pageType;
    }

    inline MpSceneType GetMpSceneType()
    {
        return sceneType;
    }

    inline IsSamePlane GetSamePlane()
    {
        return isSamePlane;
    }

    inline bool GetMustSamePlane()
    {
        return mustSamePlane;
    }

    inline bool GetEnableBorrowSplit()
    {
        return enableBorrowSplit;
    }

    uint32_t GetFaultProcessTimeout()
    {
        return faultProcessTimeout;
    }

    inline bool GetMultiNumaScene()
    {
        return multiNumaScene;
    }

    inline long GetBasePageSize()
    {
        return basePageSize;
    }

private:
    void LoadUCacheConfig();
    void LoadMultiNumaSceneConfig();
    // 获取节点基本页配置
    void LoadBasePageType();
    // mempooling 插件配置
    std::string moduleName = "mempooling_plugin";
    uint16_t moduleCode = 0;

    // mempooling 采集配置
    uint32_t ipcTimeLimit{60};

    // 容器超分场景，是否开启PageCache迁移功能
    bool ucacheEnable = false;

    // 通用配置
    std::string nodeId{};
    std::string nodeIds{};
    MpSceneType sceneType = MpSceneType::VIRTUAL_SCENE;
    PageType pageType = PageType::PAGE_4K;
    IsSamePlane isSamePlane = IsSamePlane::SamePlane;
    long basePageSize{};

    bool multiNumaScene = false;
    bool mustSamePlane = true;
    bool enableBorrowSplit = true;
    uint32_t faultProcessTimeout{10};
};

} // namespace mempooling

#endif
