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

#ifndef UBSE_URMA_CONTROLLER_MANAGER_H
#define UBSE_URMA_CONTROLLER_MANAGER_H
#include <functional>
#include <map>

#include "lock/ubse_lock.h"
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_topology_interface.h"
#include "ubse_urma_def.h"
#include "ubse_urma_uvs.h"

namespace ubse::urmaController {
using namespace ubse::common::def;
using namespace ubse::urma;
using namespace ubse::mti;

typedef struct {
    std::string bondingName;
    std::string fe1Name;
    std::string fe2Name;
    UrmaDevType bondingType;
    UrmaDevState state;
} UbseUrmaInfoForQuery;

class UbseUrmaControllerManager {
public:
    static UbseUrmaControllerManager &GetInstance()
    {
        static UbseUrmaControllerManager instance;
        return instance;
    }
    // 通过RPC通信获取其余节点的nodeInfo后，通过此方法存储
    void InsertNewNodeInfo(const std::string &nodeId, UbseUrmaNodeInfo &insertNodeInfo);
    UbseResult ConstructNewUrmaInfo(
        const std::string &nodeId,
        std::vector<UbseLcneFeInfo> &feInfos); // 更新对应节点的fe信息，计算出urmaInfo设备信息
    UbseResult ConstructNewUrmaInfo(const std::string &nodeId, std::vector<UbseLcneFeInfo> &&feInfos);
    UbseResult GetFeInfoByNodeId(const std::string &nodeId, std::vector<UbseFeInfo> &feInfos);
    bool IsUrmaInfoExists(const std::string &nodeId);
    bool IsUrmaIdExists(const std::string &nodeId, const std::string &urmaId);
    std::vector<std::string> GetEmptyNodeInfo();
    void SetActiveState(const std::string &name, const std::string &nodeId);
    UbseResult GetUrmaNameByType(const UrmaDevType type, std::vector<std::string> &urmaInfoName,
                                 std::vector<uint32_t> &status);
    void GetUrmaNameForQueryByType(const UrmaDevType type, std::vector<UbseUrmaInfoForQuery> &devInfos);
    UbseResult GetVfeByUrmaName(const std::string &urmaName, std::vector<UbseFeInfo> &feInfos);

    UbseResult GetLocalUrmaDevInfo(const std::string &urmaName, UbseUrmaInfo &urmaInfo);

    UbseResult AllocByUrmaName(const std::string &urmaInfoName, std::vector<std::string> &feNames, std::string &eid);
    UbseResult GetAllUvsInfo(std::vector<UbseUrmaUvsNodeInfo> &uvsInfos);
    void SetFeName(const std::string feEid, const std::string &urmaEidName);
    UbseUrmaNodeInfo GetUrmaNodeInfo(const std::string &nodeId);
    void SetAllUrmaInfoToInactiveForNode(const std::string &nodeId);
    void UbseUrmaBandwidthInit(const std::string &nodeId, const std::function<void(const std::string)> &initFunc);

    static std::string GetVfeInfoKey(const UbseFeInfo &info);
    static std::shared_ptr<UbseFeInfo> GetUrmaVfeFromEidGroup(EidGroup &eidGroup);

private:
    void CreateAndInsertUrmaInfo(const std::string &nodeId, const std::string &urmaId, const std::string &devEid,
                                 UbseLcneFeInfo &lcneFe0, UbseLcneFeInfo &lcneFe1);

private:
    utils::ReadWriteLock rwLock;
    std::map<std::string, UbseUrmaNodeInfo> nodeInfos{};
};
} // namespace ubse::urmaController

#endif // UBSE_URMA_CONTROLLER_MANAGER_H
