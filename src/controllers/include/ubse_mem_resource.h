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

#ifndef UBSE_MEM_RESOURCE_H
#define UBSE_MEM_RESOURCE_H

#include <cstdint>
#include <functional>
#include <ostream>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

#include <linux/types.h>

#include "ubse_mem_def.h"
#include "ubse_mem_obj.h"
#include "ubse_mem_obmm_def.h"

namespace ubse::resource::mem {
constexpr uint64_t UBSE_MEM_MAX_ID_LENGTH = 48; // 节点id，共享内存name，最大限制48
constexpr uint64_t UBSE_MEM_TOPOLOGY_MAX_HOSTS = 16;
constexpr uint64_t UBSE_MAX_REGIONS_NUM = 6; // 最大共享域个数
using namespace ubse::mem::obj;

struct UbseMemNumaLoc {
    std::string nodeId; // 节点id
    int socketId{-1};   // socket id
    int64_t numaId{-1}; // numa id
    bool operator==(const UbseMemNumaLoc &other) const
    {
        return nodeId == other.nodeId && socketId == other.socketId && numaId == other.numaId;
    }
    // 重载 < 运算符
    bool operator<(const UbseMemNumaLoc &other) const
    {
        if (nodeId != other.nodeId) {
            return nodeId < other.nodeId;
        }
        if (socketId != other.socketId) {
            return socketId < other.socketId;
        }
        return numaId < other.numaId;
        ;
    }
    friend std::ostream &operator<<(std::ostream &os, const UbseMemNumaLoc &obj)
    {
        return os << "nodeId=" << obj.nodeId << ", socketId=" << obj.socketId << ", numaId=" << obj.numaId;
    }
};

/* 进程借用，借出进程的地址以及大小 */
struct UbseBorrowLocAndSizeByPid {
    uint64_t addr{0}; // 需要借用的进程虚拟地址
    uint64_t size{0}; // 该段地址借入大小
};

enum class UbseMemErrorCode : uint32_t {
    RESOURCE_EXIST = 1,           // 资源已经创建
    RESOURCE_ATTACHED = 2,        // 已经attach过了
    ATTACH_NO_NODE = 3,           // attach请求中无nodeId
    RESOURCE_NOT_CREATE = 4,      // attach时，还未create资源
    RESOURCE_ALLOCATE_FAILED = 5, // 分配失败，对应EXPORT_NO_START
    DETACH_NO_NODE = 6,           // detach时，无节点Id
    EXPORT_NO_START = 7,          // 对应EXPORT_NO_START
    CREATE_SUCCESS = 8,
    EXPORT_FAIL = 9,  // 履行侧导出失败，内部回滚 对应 EXPORT_NO_START
    IMPORT_FAIL = 10, // 履行侧导入失败，内部回滚 对应IMPORT_NO_START
    DELETE_SUCCESS = 11,
    UNEXPORT_FAIL = 12,           // 履行侧unexport失败，内部不断重试
    UNIMPORT_FAIL = 13,           // 履行侧unimport失败，内部不断重试
    EXPORT_SEND_FAIL = 14,        // 导出对象发送失败，删除memcontroller对象
    IMPORT_SEND_FAIL = 15,        // 导入对象发送失败，回滚export
    UNEXPORT_SEND_FAIL = 16,      // 下发unexport失败，主节点import对象为export_destroying
    UNIMPORT_SEND_FAIL = 17,      // 下发unimport失败，主节点import对象为import_destroying
    ERROR_BEFORE_SMAP = 18,       // smap之前的失败
    ERROR_SMAP_OP = 19,           // smap执行失败
    ERROR_END_SMAP = 20,          // smap执行成功，obmm执行失败
    ERROR_RPC_FAIL = 21,          // agent侧发送消息到中心侧失败
    ERROR_WAIT_FAIL = 22,         // agent侧等待超时失败
    RESOURCE_OP_SUCCESSFUL = 100, // 成功
};

enum class UbseResourceRequestState {
    RUNNING,
    SUCCESS,
    FAIL,
};

/**********************************
* 账本相关接口
**********************************/

// old end
using UbseMemFdImportObjMap = std::unordered_map<std::string, UbseMemFdBorrowImportObj>;     // resourceId,obj
using UbseMemFdExportObjMap = std::unordered_map<std::string, UbseMemFdBorrowExportObj>;     // resourceId,obj
using UbseMemNumaImportObjMap = std::unordered_map<std::string, UbseMemNumaBorrowImportObj>; // resourceId,obj
using UbseMemNumaExportObjMap = std::unordered_map<std::string, UbseMemNumaBorrowExportObj>; // resourceId,obj

struct NodeMemDebtInfo {
    UbseMemFdImportObjMap fdImportObjMap;
    UbseMemFdExportObjMap fdExportObjMap;
    UbseMemNumaImportObjMap numaImportObjMap;
    UbseMemNumaExportObjMap numaExportObjMap;
};

using NodeMemDebtInfoMap = std::unordered_map<std::string, NodeMemDebtInfo>; // nodeId,NodeMemDebtInfo

/**
* @brief 查询中心节点账本信息
* @param nodeId [in] 节点Id，为空则查询所有节点
* @param memDebtInfoMap [out] 借用账本对象集合
* @return 成功返回0, 失败返回非0
*/
uint32_t UbseGetMemDebtInfo(const std::string &nodeId, NodeMemDebtInfoMap &memDebtInfoMap);

/**
* @brief 查询当前节点账本信息
* @param nodeMemDebtInfo [out] 借用账本对象集合
* @return 成功返回0, 失败返回非0
*/
uint32_t UbseGetLocalNodeMemDebtInfo(NodeMemDebtInfo &nodeMemDebtInfo);
} // namespace ubse::resource::mem

#endif // UBSE_MEM_RESOURCE_H