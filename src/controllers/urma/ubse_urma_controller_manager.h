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
#include <functional>
#include <map>

#include "lock/ubse_lock.h"
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_node_controller.h"
#include "ubse_topology_interface.h"
#include "ubse_urma_def.h"
#include "ubse_urma_uvs.h"

namespace ubse::urmaController {
using namespace ubse::common::def;
using namespace ubse::urma;
using namespace ubse::mti;
using namespace ubse::nodeController;

struct UbseUrmaInfoForQuery {
    std::string urmaName;
    std::vector<std::string> feNames;
    std::vector<std::string> feEids;
    std::string devEid;
    UrmaDevType bondingType;
    UrmaDevState state;
    UrmaQosProfile qosProfile;
    friend ubse::serial::UbseSerialization &operator<<(ubse::serial::UbseSerialization &serializer,
                                                       const UbseUrmaInfoForQuery &info)
    {
        serializer << info.urmaName << info.feNames << info.feEids << info.devEid
                   << ubse::serial::enum_v(info.bondingType) << ubse::serial::enum_v(info.state) << info.qosProfile;
        return serializer;
    }

    friend ubse::serial::UbseDeSerialization &operator>>(ubse::serial::UbseDeSerialization &deserializer,
                                                         UbseUrmaInfoForQuery &info)
    {
        deserializer >> info.urmaName >> info.feNames >> info.feEids >> info.devEid
                     >> ubse::serial::enum_v(info.bondingType) >> ubse::serial::enum_v(info.state) >> info.qosProfile;
        return deserializer;
    }
};

class UbseUrmaControllerManager {
public:
    static UbseUrmaControllerManager &GetInstance()
    {
        static UbseUrmaControllerManager instance;
        return instance;
    }
    // 通过RPC通信获取其余节点的nodeInfo后，通过此方法存储
    void InsertNewNodeInfo(const std::string &nodeId, UbseUrmaNodeInfo &insertNodeInfo);
    // 更新对应节点的fe信息，计算出urmaInfo设备信息
    UbseResult ConstructNewUrmaInfo(const std::string &nodeId, std::vector<std::vector<UbseLcneFeInfo>> &feInfos);
    UbseResult ConstructNewUrmaInfo(const std::string &nodeId, std::vector<std::vector<UbseLcneFeInfo>> &&feInfos);

    bool IsUrmaInfoExists(const std::string &nodeId);
    bool IsUrmaInfoExists(const std::string &nodeId, const std::string &devEid); // 本节点是否包含指定eid的urmaInfo

    void SetActiveState(const std::string &urmaDevEid, const std::string &nodeId);
    void SetInactiveState(const std::string &urmaDevEid, const std::string &nodeId);

    UbseResult GetUrmaNameByType(const UrmaDevType type, std::vector<std::string> &urmaInfoName,
                                 std::vector<uint32_t> &status);
    void GetUrmaNameForQueryByType(const UrmaDevType type, std::vector<UbseUrmaInfoForQuery> &devInfos);

    UbseResult GetLocalUrmaDevInfo(const std::string &urmaName, UbseUrmaInfo &urmaInfo);

    UbseResult AllocByUrmaName(const std::string &urmaName, std::vector<std::string> &feNames, std::string &eid);
    UbseResult SetUrmaQos(const std::string &urmaInfoName, const UrmaQosProfile &urmaQosProfile);
    UbseResult GetUrmaQos(const std::string &urmaInfoName, UrmaQosProfile &urmaQosProfile);
    UbseResult GetAllUvsInfo(std::vector<UbseUrmaUvsNodeInfo> &uvsInfos);
    void SetUrmaSubPath(const std::string &urmaEid, const std::string &urmaSubPath);
    void SetFeName(const std::string feEid, const std::string &urmaEidName);
    UbseUrmaNodeInfo GetUrmaNodeInfo(const std::string &nodeId);
    void SetAllUrmaInfoToInactiveForNode(const std::string &nodeId);
    void SetAllUrmaInfoToActiveForNode(const std::string &nodeId);

    uint64_t GetUrmaUpdateTimeStamp(const std::string &nodeId);

private:
    UbseResult CreateAndInsertUrmaInfo(const std::string &nodeId, UbseLcneFeInfo &lcneFe0, UbseLcneFeInfo &lcneFe1);
    UbseResult GenerateUrmaDevEid(const uint32_t superNodeId, const uint32_t slotId, const uint32_t fe0Id,
                                  const uint32_t fe1Id, std::string &devEid);
    uint32_t GenerateUniqueFeId();
    uint64_t GenerateUrmaId();
    void PrintNodeInfo(const UbseUrmaNodeInfo &nodeInfo);
    UbseResult GetLocalUrmaDevInfoInner(const std::string &urmaName, UbseUrmaInfo &urmaInfo);
    bool IsLcneFeUsed(const UbseLcneFeInfo &fe0, const UbseLcneFeInfo &fe1);

private:
    utils::ReadWriteLock rwLock;
    std::map<std::string, UbseUrmaNodeInfo> nodeInfos{};
    std::atomic<uint32_t> globalFeId{0};       // 节点内唯一的feId生成器
    std::atomic<uint64_t> globalUrmaId{0};     // 节点内唯一的urmaId生成器
    std::map<std::string, uint32_t> feIdMap{}; // <feKey, feId>
};
} // namespace ubse::urmaController

#endif // UBSE_URMA_CONTROLLER_MANAGER_H
