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

#ifndef UBSE_UBSE_MEM_DATASTORE_H
#define UBSE_UBSE_MEM_DATASTORE_H

#include <ostream>
#include <unordered_map>
#include <vector>
#include "ubse_mem_resource.h"

namespace ubse::mmi {

constexpr uint32_t UBSE_MEM_MAX_NAME_LENGTH = 48u;
constexpr uint32_t UBSE_MEM_MAX_NODE_ID_LENGTH = 16u;
constexpr uint32_t UBSE_MEM_MAX_EXPORT_NUMA_SIZE = 2u;

// obmm自定义存储空间，最大128字节
struct UbseMemLocalObmmCustomMeta {
    uint8_t version{0};   // 版本，当前为1，只有版本号有效，才继续check其他字段,如果memcpy不成功，说明不是mmi申请的
    uint8_t type{};       // 应用设备借用0，水线触发remote numa 借用1，共享2，进程借用3
    uint32_t uid{};       // 鉴权
    uint32_t gid{};       // 鉴权
    uint32_t pid{};       // 可靠性处理
    uint64_t timeStamp{}; // 对象的时间戳
    // addr借用使用
    char importNodeId[UBSE_MEM_MAX_NODE_ID_LENGTH]{}; // 导入端的nodeId
    uint64_t srcPid{};                                // 进程借用 导出端存储 src pid, 应用借用时为应用进程的Pid
    uint64_t dstPid{};                                // 进程借用 导出端存储 dst pid
    uint64_t virAddr{};                               // 进程借用 虚拟地址

    // 用于判断一个obmm设备有没有被使用，存储pid可以快速判断，没有pid用lsof判断
    char exportNodeId[UBSE_MEM_MAX_NODE_ID_LENGTH]{};
    char requestNodeId[UBSE_MEM_MAX_NODE_ID_LENGTH]{};
    uint64_t exportMemid{};                // 在导入端填，恢复对象的时候使用, 有memid就足够
    uint32_t attachSocket{};               // 导入时实际挂载的socketId
    char name[UBSE_MEM_MAX_NAME_LENGTH]{}; // 全局uuid，控制面需要！
    uint16_t memidCount{};                 // memid的总个数，芯片表项碎片导致一次借用，会有多个memid，决策完之后就能知道，用于回滚
    uint32_t regionMask{0};                // 共享域的bit位
    uint8_t importNumaIds[UBSE_MEM_MAX_EXPORT_NUMA_SIZE]{}; // 共享没有import numaid
    uint8_t exportNumaIds[UBSE_MEM_MAX_EXPORT_NUMA_SIZE]{};
    int32_t importSocket{};
    int32_t exportSocket{};
    uint64_t numaSizes[UBSE_MEM_MAX_EXPORT_NUMA_SIZE]{};
} __attribute__((packed));

struct UbseMemLocalObmmMetaData {
    uint8_t memIdType{};       // 导出1或者导入0
    int16_t remoteNumaId{-1};  // 远程numaid,导入端，numa呈现才有
    uint64_t usedPidCount{};   // 采集得时候，obmm设备在使用得进程数，用来判断共享能不能是不是自己map自己。
    uint64_t totalSize{};      // 内存申请请求大小
    uint64_t localMemId{0};    // 导出或者导入memid
    std::string localNodeId{}; // 收集节点id，从框架获取
    ubse_mem_obmm_mem_desc obmmMemExportInfo{}; // 内存导出描述符，从obmm获取，共享会用到，用于import参数
    UbseMemLocalObmmCustomMeta customMeta{};   // obmm自定义缓存空间，用于对账，恢复
    std::vector<uint64_t> usedPidSet;          // 恢复0引用元数据用
};

} // namespace ubse::mmi

// 哈希函数
template <>
struct std::hash<ubse::resource::mem::UbseMemNumaLoc> {
    std::size_t operator()(const ubse::resource::mem::UbseMemNumaLoc &key) const noexcept
    {
        static std::hash<int> intHash;
        static std::hash<int64_t> int64Hash;
        static std::hash<std::string> strHash;
        return strHash(key.nodeId) ^ intHash(key.socketId) ^ int64Hash(key.numaId);
    }
};
#endif // UBSE_UBSE_MEM_DATASTORE_H
