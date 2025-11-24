/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_MEM_MAMI_DEF_H
#define UBSE_MEM_MAMI_DEF_H

#include <cstdint>
#include <string>
#include <vector>

namespace ubse::mem::mami {
#define UB_MEMORY_IMPORT_MEMORY 0            // 普通内存借入/共享
#define UB_MEMORY_PREIMPORT_MEMORY_STATIC 1  // 静态预引入
#define UB_MEMORY_PREIMPORT_MEMORY_DYNAMIC 2 // 动态预引入

/* 路由表查询，是查单路径表还是多路径表。配置为1表示走单路径。读命令固定走多路径 */
#define UB_MEMORY_IMPORT_SINGLE_PATH (uint32_t)0x1

/* 写命令是否comp接力。配置为1表示comp不接力，否则comp接力 */
#define UB_MEMORY_IMPORT_WR_DELAY_COMP (uint32_t)0x2

/* reduce操作是否comp接力。配置为1表示comp不接力，否则comp接力。说明：SMMU不支持IO Cache，因此reduce操作全部按照不接力走 */
#define UB_MEMORY_IMPORT_REDUCE_DELAY_COMP (uint32_t)0x4

/* cache&nbsp;maintain操作是否comp接力。配置为1表示comp不接力，否则comp接力。注意：cmo暂不支持，放这里作为预留 */
#define UB_MEMORY_IMPORT_CMO_DELAY_COMP (uint32_t)0x8

/* 命令是否强保序，读固定是0 */
#define UB_MEMORY_IMPORT_SO (uint32_t)0x10

/* 决定到UMMU查表，查的是片上的页表还是存储在DDR的页表。1表示查询的是片上的页表，0表示查询DDR上的页表 */
#define UB_MEMORY_IMPORT_ADDR_TR_ONCHIP (uint32_t)0x20

/* 区分内存借用和内存共享标记，0表示借用——分配的HPA地址满足128MB粒度对齐，1表示共享——分配的HPA地址满足2MB粒度对齐 */
#define UB_MEMORY_IMPORT_SHARE_TYPE (uint32_t)0x40

/* decoder表项内容、内存的size等 */
struct UbseMamiMemImportInfo {
    uint32_t ubpuId{};
    uint32_t iouId{};
    uint32_t marId{}; // 指定端口组逻辑ID, 已包含chip id和die id; 1650和David V100硬件限制仅0, 3, 4号端口组支持配置UB
    uint32_t dstCNA{};    // 目的CNA
    uint8_t importType{}; // 内存借用类型：普通内存借入/共享、静态预引入或动态预引入
    uint8_t decoderId{}; // 使用哪张Decoder表。规则：Cacheable访问使用0号Decoder表，NC访问使用1号Decoder表。
    uint8_t lb{};       // lb取值以芯片手册为准
    uint32_t tokenId{}; // 访问远端内存使用的权限id
    uint32_t flag{};    // 指定decoder表的相关属性
    uint64_t size{};    // 借入内存的大小，单位：字节
    uint64_t uba{};     // 访问远端内存（提供方）的UBA地址
    uint64_t
        handle{}; // 标记的一次内存借入句柄；普通内存借入/共享填0即可；从静态预引入/动态预引入内存段的内存借入时，需要指定为最初静态预引入/动态预引入的handle
};

/* 删除表项信息 */
struct UbseMamiMemWithdraw {
    uint32_t ubpuId{};
    uint32_t iouId{};
    uint32_t marId{};
    uint8_t decoderIdx{};
    uint64_t handle{};
};

/* decoder 表项操作结果 */
struct UbseMamiMemImportResult {
    uint32_t marId{};
    uint64_t hpa{};    // HPA起始地址
    uint64_t handle{}; // 内存handle, handle值用于标识分配的PA范围和decoder表项，删除decoder表项时需要传入相应的handle。
};

/* 查询所有handle */
struct UbseMamiMemHandleQueryInfo {
    uint32_t ubpuId{};
    uint32_t iouId{};
    uint32_t marId{}; // 指定端口组逻辑ID，已包含chip id和die id；1650和David V100硬件限制仅0，3，4号端口组支持配置UB
    // Memory的注入路由表和decoder表，详见约束说明
    uint8_t decoderId{}; // 指定decoder表索引
    uint8_t type{};      // 指定handle类型
};

/* 单个handle信息 */
struct UbseMamiMemHandleValue {
    uint64_t handle{};        // handle值
    uint64_t hpa{};           // handle对应的HPA
    uint64_t size{};          // handle对应的HPA的大小
    uint16_t entryStartIdx{}; // handle对应分配decoder表项的起始entry索引
    uint16_t entryEndIdx{};   // handle对应分配decoder表项的终止entry索引
    uint16_t blockStartIdx{}; // handle对应分配decoder表项的起始block索引
    uint16_t blockEndIdx{};   // handle对应分配decoder表项的终止block索引
    uint8_t type{};           // handle对应的handle类型
};

/* 查询所有handle结果 */
struct UbseMamiMemHandleQueryResult {
    std::string result;                                // 操作结果, "success" 表示成功
    std::vector<UbseMamiMemHandleValue> handlerValues; // 查询得到的handler列表
};

/* mamiMemDecoder: 用于定义一张decoder表的表规格信息。 */
struct UbseMamiMemDecoderInfo {
    uint8_t decoderId{};    // 当前UbseMamiMemDecoder序号, 从0开始
    uint16_t entryOffset{}; // 指定decoder表1开始的entry索引
    uint16_t entrySize{};   // decode表一个entry表示的粒度
    uint16_t cfgType{};     // 指定管理HPA分配的类型
};

/* 配置Decoder表规格信息 */
struct UbseMamiMemDecoderSetInfo {
    uint32_t ubpuId{};
    uint32_t iouId{};
    uint32_t marId{}; // 指定端口组逻辑ID，已包含chip id和die id；1650和David V100硬件限制仅0，3，4号端口组支持配置UB
                      // Memory的注入路由表和decoder表，详见约束说明
    uint8_t decoderNum{}; // 指定decoder表数量
    std::vector<UbseMamiMemDecoderInfo> decoder{};
};
} // namespace ubse::mem::mami

#endif // UBSE_MEM_MAMI_DEF_H