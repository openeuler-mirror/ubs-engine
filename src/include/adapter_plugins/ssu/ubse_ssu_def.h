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

#ifndef UBSE_SSU_DEF_H
#define UBSE_SSU_DEF_H
#include <cstdint>
#include <string>
#include <vector>
namespace ubse::adapter_plugins::ssu::def {
const uint8_t UBSE_SSU_MAX_NAME_LENGTH = 48;
const uint8_t UBSE_SSU_MAX_RACK_NUM = 2;
const uint8_t UBSE_SSU_MAX_HOST_NUM = 128;

enum class UbseSsuState : uint32_t {
    ONLINE = 0,
    OFFLINE = 1,
    UNKNOWN = 2,
};

// ┌─────────────────────────────────────────┐
// │          物理设备 (NVMe SSD)             │
// │                                         │
// │  ┌───────────────────────────────────┐  │
// │  │      子系统          │  │
// │  │                                   │  │
// │  │  ┌──────────┐  ┌──────────┐      │  │
// │  │  │ 控制器 0  │  │ 控制器 1  │      │  │
// │  │  │ /dev/nvme0│  │ /dev/nvme1│      │  │
// │  │  └─────┬────┘  └─────┬────┘      │  │
// │  │        │             │            │  │
// │  │        └──────┬──────┘            │  │
// │  │               ▼                   │  │
// │  │  ┌─────────────────────────┐     │  │
// │  │  │    命名空间 │     │  │
// │  │  │  n1    n2    n3   ...  │     │  │
// │  │  └─────────────────────────┘     │  │
// │  │               ▼                   │  │
// │  │         NAND Flash               │  │
// │  └───────────────────────────────────┘  │
// └─────────────────────────────────────────┘

struct UbseSsuDevNameSpaceCustomData {
    uint8_t version;                     // 版本标识
    char name[UBSE_SSU_MAX_NAME_LENGTH]; // 请求唯一标识
    uint8_t usingType;                   // ns用途类型：0-独占，1-共享
    uint8_t addressingType;              // ns地址类型：0-线性地址，1-非线性地址
    uint8_t raidLevel;                   // raid级别
    uint8_t nsNum;                       // ns数量
    uint64_t totalBytes;                 // ns总容量（字节）
    uint8_t
        allowHosts[UBSE_SSU_MAX_RACK_NUM]
                  [UBSE_SSU_MAX_HOST_NUM]; // ns允许挂载的host列表,数组下标为节点编号，1表示允许挂载，0表示不允许挂载
    uint32_t crc; // crc校验
} __attribute__((packed));

// 参数	    缩写	说明	               默认值
// --flbas	-f	LBA 格式索引	0
// --dps	-d	数据保护设置	0
// --nmic	-n	多控制器共享标志	0
// --anagrpid	-a	ANA 组 ID	0
// --nvmsetid	-i	NVM 集合 ID	0
struct UbseSsuNamespaceOptions {
    uint32_t flbas = 0;    // LBA 格式索引, -f 0 (512B) / -f 1 (4K)
    uint32_t dps = 0;      // 数据保护设置
    uint32_t nmic = 0;     // 多控制器共享标志
    uint32_t anagrpid = 0; // ANA 组 ID
    uint32_t nvmsetid = 0; // NVM 集合 ID
};

struct UbseSsuDevSubSystem {
    std::string eid;    // ssu物理设备eid
    std::string subNqn; // 子系统NQN
    uint32_t jettyId{0}; // jetty id
};

struct UbseSsuDevNameSpace {
    uint32_t namespaceId;                     // 命名空间 ID（1, 2, 3...）
    UbseSsuDevSubSystem subSystem;            // ns所在的子系统信息
    std::string guid;                         // 命名空间 GUID
    std::string uuid;                         // 命名空间 UUID
    std::string nsDevPath;                    // by-id 或 by-path 路径，ns持久化标识（重启不变）
    uint64_t nsze;                            // 命名空间大小（LBA 数量）
    uint64_t ncap;                            // 命名空间容量（LBA 数量）
    uint64_t nuse;                            // 已用容量（LBA 数量）
    UbseSsuNamespaceOptions nsOptions;        // 命名空间选项
    UbseSsuDevNameSpaceCustomData customData; // 自定义数据
};

struct UbseSsuDevCtrl {
    std::string eid;     // ssu物理设备eid
    std::string devPath; // controller设备路径
    uint16_t cntlid = 0; // 控制器ID（硬件固定，软件动态），NVMe 规范标识（子系统内唯一）
};

struct UbseSsuDevNameSpaceAttachInfo {
    std::string attachNode;
    uint32_t namespaceId;
};

struct UbseSsuDevInfo {
    UbseSsuDevSubSystem subSystem;                             // 子系统信息
    std::string serialNumber;                                  // 序列号（物理设备唯一标识）
    std::string firmware;                                      // ssu物理设备固件版本
    std::vector<UbseSsuDevCtrl> ctrlList;                      // ssu物理设备控制器信息
    uint64_t totalBytes = 0;                                   // ssu物理设备总容量（字节）
    uint64_t usedBytes = 0;                                    // ssu物理设备已用容量（字节）
    std::vector<UbseSsuDevNameSpace> nameSpaces;               // ssu物理设备命名空间信息
    std::vector<UbseSsuDevNameSpaceAttachInfo> attachInfoList; // namespace挂载节点信息
    UbseSsuState state;                                        // ssu物理设备状态
};

enum class UbseSsuRaidLevel : uint8_t {
    RAID0 = 0,
    RAID5 = 5,
};

enum class UbseSsuAddressingType : uint8_t {
    STRIPED = 0, // 条带化编址
    LINEAR = 1,  // 线性编址
};

struct UbseCreateBlockDeviceOptions {
    UbseSsuAddressingType addressingType = UbseSsuAddressingType::LINEAR;
    UbseSsuRaidLevel raidLevel = UbseSsuRaidLevel::RAID0; // UbseSsuAddressingType为STRIPED有效
    // 单位KB，一次写入单个设备的连续数据量，mdadm 默认 512KB，常见值 64KB/128KB/256KB；必须对齐扇区	chunk_size % sector_size == 0（sector_size 通常 512B）
    uint32_t chunkSize = 512; // UbseSsuAddressingType为STRIPED有效
};
} // namespace ubse::adapter_plugins::ssu::def
#endif // UBSE_SSU_DEF_H