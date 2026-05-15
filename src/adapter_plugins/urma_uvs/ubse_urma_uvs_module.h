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

#ifndef UBSE_MANAGER_UBSE_URMA_MODULE_H
#define UBSE_MANAGER_UBSE_URMA_MODULE_H

#include <set>

#include "ubse_common_def.h"
#include "ubse_module.h"
#include "ubse_node_controller.h"
#include "adapter_plugins/urma/ubse_urma_uvs.h"

namespace ubse::urma {
using namespace ubse::module;
using namespace ubse::common::def;
using namespace ubse::nodeController;

using UvsSetTopoInfo = uint32_t (*)(void *topo, uint32_t topo_size, uint32_t topNum);
using UvsGetDeviceNameByUrmaEid = uint32_t (*)(char *urmaEid, char *buf, size_t len);
using UvsCreateAggrDev = uint32_t (*)(char *aggrDevEid, const char *aggrDevName);
using UvsDeleteAggrDev = uint32_t (*)(char *aggrDevEid);

constexpr uint32_t EID_LEN = 16;
constexpr uint32_t IODIE_NUM = 2;
constexpr uint32_t PORT_NUM = 9;
constexpr uint32_t UVS_PORT_NUM = IODIE_NUM * PORT_NUM;
constexpr uint32_t DEV_NUM = 256;
constexpr size_t DEV_NAME_LEN = 32;

constexpr uint32_t IPV6_FULL_FORMAT_LENGTH = 39;
constexpr uint32_t IPV6_BYTE_COUNT = 16;
constexpr size_t IPV6_SEGMENT_COUNT = 8;
constexpr size_t IPV6_SEGMENT_LENGTH = 4;

constexpr uint32_t AGGR_DEV_NAME_LEN = 64; // 聚合设备名最大长度，包含'\0'

struct UbcoreTopoFe {
    uint32_t chip_id;
    uint32_t die_id;
    uint32_t entity_id;
    char primary_eid[EID_LEN];
    char port_eid[PORT_NUM][EID_LEN];
};

struct UbcoreTopoAggrDev {
    char aggr_eid[EID_LEN];
    UbcoreTopoFe fe[IODIE_NUM];
};

struct UbcoreTopoNode {
    uint32_t type;    // 0代表1D-FULLMESH, 1代表Clos组网
    uint32_t super_node_id; // 超节点Id
    uint32_t id;    // 该entry对应的节点Id，当前节点entry的links全false
    uint32_t is_current;    // 0代表非本节点，1代表是本节点
    bool links[UVS_PORT_NUM][UVS_PORT_NUM]; // 当前节点port到该entry节点port的连接矩阵
    UbcoreTopoAggrDev aggr_dev[DEV_NUM];
};

class UbseUrmaUvsModule : public UbseModule {
public:
    UbseResult Initialize() override;

    void UnInitialize() override;

    UbseResult Start() override;

    void Stop() override;

    // 提供函数指针访问
    UvsSetTopoInfo uvsSetTopoInfo = nullptr;
    UvsGetDeviceNameByUrmaEid uvsGetDeviceNameByUrmaEid = nullptr;
    UvsCreateAggrDev uvsCreateAggrDev = nullptr;
    UvsDeleteAggrDev uvsDeleteAggrDev = nullptr;

private:
    void Cleanup();
    void* handle = nullptr;
};
} // namespace ubse::urma
#endif // UBSE_MANAGER_UBSE_URMA_MODULE_H
