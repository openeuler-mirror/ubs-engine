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

#ifndef UBSE_MANAGER_UBSE_URMA_MODULE_H
#define UBSE_MANAGER_UBSE_URMA_MODULE_H

#include <set>

#include "ubse_common_def.h"
#include "ubse_context.h"
#include "ubse_node_controller.h"
#include "ubse_urma.h"

namespace ubse::urma {
using namespace ubse::common::def;
using namespace ubse::context;
using namespace ubse::nodeController;

using UvsSetTopoInfo = uint32_t (*)(void *topo, uint32_t topNum);
using UvsGetDeviceNameByUrmaEid = uint32_t (*)(char *urmaEid, char *buf, size_t len);
using UvsCreateAggrDev = uint32_t (*)(char *aggrDevEid);
using UvsDeleteAggrDev = uint32_t (*)(char *aggrDevEid);

constexpr uint32_t EID_LEN = 16;
constexpr uint32_t IODIE_NUM = 2;
constexpr uint32_t IODIE_NUM_PER_CHIP = 1;
constexpr uint32_t PORT_NUM = 9;
constexpr uint32_t DEV_NUM = 64;
constexpr size_t DEV_NAME_LEN = 32;

constexpr uint32_t IPV6_FULL_FORMAT_LENGTH = 39;
constexpr uint32_t IPV6_BYTE_COUNT = 16;
constexpr size_t IPV6_SEGMENT_COUNT = 8;
constexpr size_t IPV6_SEGMENT_LENGTH = 4;

struct UbcoreTopoFe {
    uint32_t socket_id;
    char primary_eid[EID_LEN];
    char port_eid[PORT_NUM][EID_LEN];
};

struct UbcoreTopoAggrDev {
    char aggr_eid[EID_LEN];
    UbcoreTopoFe fe[IODIE_NUM];
};

struct UbcoreTopoLink {
    uint32_t peer_node;  // node id
    uint32_t peer_iodie; // iodie idx
    uint32_t peer_port;  // poet idx, UINT32_MAX=无连接
};

struct UbcoreTopoNode {
    uint32_t id;
    uint32_t is_current;
    UbcoreTopoLink link[IODIE_NUM][PORT_NUM];
    UbcoreTopoAggrDev aggr_dev[DEV_NUM];
};

class UbseUrmaUvsModule : public UbseModule {
public:
    UbseResult Initialize() override;

    void UnInitialize() override;

    UbseResult Start() override;

    void Stop() override;

    UbseResult SetUvsInfo(uint32_t &current_slot_id, const std::vector<PhysicalLink> &allLinkInfo,
                          const std::vector<UbseUrmaInfo> &bondingInfo);

    UbseResult GetNameByUrmaEid(const std::string &urmaEid, std::string &urmaEidName);

    UbseResult GetStateByUrmaEid(const std::string &urmaEid, bool isactivate);

    UbseResult ActivateBondingDevice(const std::string &urmaEid);

    UbseResult DeactivateBondingDevice(const std::string &urmaEid);

private:
    UbseResult FillNodeComInfo(const std::vector<PhysicalLink> &allLinkInfo,
                               const std::vector<UbseUrmaInfo> &bondingInfo, std::vector<UbcoreTopoNode> &nodes);

    UbseResult ParseColonHexString(const std::string &input, char outBytes[IPV6_BYTE_COUNT]);

    void InitialTopoNodes(const std::set<uint32_t> slotIds, std::unordered_map<uint32_t, UbcoreTopoNode> &nodeMap);

    UbseResult FillTopo(const std::vector<PhysicalLink> &allLinkInfo,
                        std::unordered_map<uint32_t, UbcoreTopoNode> &nodeMap);

    UbseResult FillBondingInfo(const std::vector<UbseUrmaInfo> &bondingInfo,
                               std::unordered_map<uint32_t, UbcoreTopoNode> &nodeMap);

    UbseResult FillFeInfo(std::vector<UbseFeInfo> &fes, UbcoreTopoAggrDev &aggr_dev);

    UbseResult FillFeUrmaEid(UbseUrmaInfo &urmainfo, UbcoreTopoFe &fe);

    void Cleanup();

    void *handle = nullptr; // 共享库句柄
    UvsSetTopoInfo uvsSetTopoInfo = nullptr;
    UvsGetDeviceNameByUrmaEid uvsGetDeviceNameByUrmaEid = nullptr;
    UvsCreateAggrDev uvsCreateAggrDev = nullptr;
    UvsDeleteAggrDev uvsDeleteAggrDev = nullptr;
};

} // namespace ubse::urma
#endif // UBSE_MANAGER_UBSE_URMA_MODULE_H
