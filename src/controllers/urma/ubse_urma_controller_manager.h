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

#ifndef UBSE_URMA_CONTROLLER_MANAGER_H
#define UBSE_URMA_CONTROLLER_MANAGER_H
#include <cstdint>
#include <functional>
#include <map>

#include "ubse_common_def.h"
#include "ubse_mti_interface.h"
#include "ubse_urma_def.h"
#include "ubse_urma_uvs_module.h"
#include "lock/ubse_lock.h"

namespace ubse::urmaController {
using common::def::UbseResult;
using ubse::adapter_plugins::mti::UbseMtiFeInfo;
using ubse::urma::FeTopoType;
using ubse::urma::UbseUrmaInfo;
using ubse::urma::UbseUrmaNodeInfo;
using ubse::urma::UbseUrmaUvsNodeInfo;
using ubse::urma::UrmaDevState;

class UbseUrmaControllerManager {
public:
    static UbseUrmaControllerManager& GetInstance()
    {
        static UbseUrmaControllerManager instance;
        return instance;
    }
    // 通过RPC通信获取其余节点的nodeInfo后，通过此方法存储
    void InsertNewNodeInfo(const std::string& nodeId, UbseUrmaNodeInfo& insertNodeInfo);
    // 更新对应节点的fe信息，计算出urmaInfo设备信息
    UbseResult ConstructNewUrmaInfo(const std::string& nodeId, std::vector<std::vector<UbseMtiFeInfo>>& feInfos);
    UbseResult ConstructNewUrmaInfo(const std::string& nodeId, std::vector<std::vector<UbseMtiFeInfo>>&& feInfos);
    // 根据本节点的urma bonding信息，推算出其它所有节点的urma bonding信息
    UbseResult InferOtherNodesUrmaDevInfo(const std::string& basedNodeId, uint32_t startIdx, uint32_t batchNodeNum);
    // 下发拓扑后，删除其它节点的urmaInfo，只保留本节点的urmaInfo，避免内存占用过高
    void DeleteOtherNodesUrmaInfo(const std::string& curNodeId);

    void SetUrmaDevStateByDevEid(const std::string& urmaDevEid, UrmaDevState state);

    UbseResult GetLocalUrmaDevInfoByName(const std::string& urmaName, UbseUrmaInfo& urmaInfo);
    UbseResult AllocUrmaDev(const std::string& urmaName, std::vector<std::string>& feNames, std::string& eid);
    UbseResult GetAllUvsTopoInfo(uint32_t startServerIdx, uint32_t batchNodeNum,
                                 std::vector<UbseUrmaUvsNodeInfo>& uvsInfos);
    void SetUrmaSubPath(const std::string& urmaEid, const std::string& urmaSubPath);
    void SetFeName(const std::string& feEid, const std::string& urmaEidName);
    UbseUrmaNodeInfo GetUrmaNodeInfo(const std::string& nodeId);
    void SetAllUrmaDevStateForNode(UrmaDevState state);
    uint64_t GetUrmaUpdateTimeStamp(const std::string& nodeId);
    void SetFeTopoType(FeTopoType topoType);
    FeTopoType GetFeTopoType() const;

private:
    UbseResult CreateAndInsertUrmaInfo(const uint16_t superPodId, const uint32_t serverIdx, const std::string& nodeId,
                                       UbseMtiFeInfo& lcneFe0, UbseMtiFeInfo& lcneFe1);
    UbseResult InferOneNodeUrmaDevInfo(uint16_t superPodId, uint32_t serverIdx, const std::string& baseNodeId);
    uint16_t GenerateUniqueFeId();
    uint64_t GenerateUrmaDevId();
    void PrintNodeInfo(const UbseUrmaNodeInfo& nodeInfo);
    UbseResult GetLocalUrmaDevInfoByNameInner(const std::string& urmaName, UbseUrmaInfo& urmaInfo);
    bool IsLcneFeUsed(const UbseMtiFeInfo& fe0, const UbseMtiFeInfo& fe1);
    // clos组网下，获取主机bonding，插入到本节点的urmaList中，拓展到96bonding
    UbseResult InsertHostUrmaDevInner();
    UbseResult BuildHostUrmaDev(const ubse::urma::UbseUrmaUvsAggrDev& comDev,
                                const std::map<std::string, UbseUrmaInfo, ubse::urma::UrmaNameCompare>& urmaList,
                                UbseUrmaInfo& urmaDev);
    std::string GenerateBondingDevName(ubse::adapter_plugins::mti::UbseMtiFeType feType);

private:
    utils::ReadWriteLock rwLock;
    std::map<std::string, UbseUrmaNodeInfo> nodeInfos{};
    std::atomic<uint16_t> globalFeId{0};       // 节点内唯一的feId生成器
    std::atomic<uint64_t> globalUrmaId{0};     // 节点内唯一的urmaId生成器
    std::map<std::string, uint16_t> feIdMap{}; // <feKey, feId>
    FeTopoType feTopoType{FeTopoType::INVALID};
};
} // namespace ubse::urmaController

#endif // UBSE_URMA_CONTROLLER_MANAGER_H
