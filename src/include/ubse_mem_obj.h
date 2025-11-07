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

#ifndef UBSE_MANAGER_UBSE_MEM_OBJ_H
#define UBSE_MANAGER_UBSE_MEM_OBJ_H
#include <cstdint>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

#include "ubse_mem_obmm_def.h"

namespace ubse::mem::obj {
struct UbseObjByteBuffer {
    uint8_t *data = nullptr; // 数据指针
    size_t len = 0;          // 数据长度
};
// 使用方进程信息
struct UbseUdsInfo {
    uid_t uid; // 使用方进程的运行用户的uid
    gid_t gid; // 使用方进程的的运行用户的groupid
    pid_t pid; // 使用方进程的的运行用户的pid
    friend std::ostream &operator<<(std::ostream &os, const UbseUdsInfo &obj)
    {
        return os << "(uid: " << obj.uid << " gid: " << obj.gid << " pid: " << obj.pid << ")";
    }
};
struct FdOwner {
    uid_t uid{0};  // 使用方进程的运行用户的uid
    gid_t gid{0};  // 使用方进程的的运行用户的groupid
    pid_t pid{0};  // 使用方进程的的运行用户的pid
    mode_t mode;
    friend std::ostream& operator<<(std::ostream& os, const FdOwner& obj)
    {
        return os << "(uid: " << obj.uid << " gid: " << obj.gid << " pid: " << obj.pid << " mode: " << obj.mode << ")";
    }
};
enum UbseMemDistance {
    MEM_DISTANCE_L0, // *L0对应直连节点
    MEM_DISTANCE_L1, // *L1对应通过1跳节点，暂不支持
    MEM_DISTANCE_L2  // *L2对应过超过1跳节点 ，暂不支持
};

// 内存状态枚举
enum UbseMemState {
    UBSE_MEM_STATE_INIT,
    UBSE_MEM_STATE_RUNNING,
    UBSE_MEM_STATE_SUCCEEDED,
    UBSE_MEM_STATE_FAILED,
    UBSE_MEM_STATE_DESTROYING,
    UBSE_MEM_STATE_DESTROY_FAILED,
    UBSE_MEM_STATE_DESTROYED,
    UBSE_MEM_STATE_DESTROYED_FORCE,
    UBSE_MEM_STATE_DESTROYED_WITH_SMAP_BACK,
    UBSE_MEM_SCHEDULING,
    UBSE_MEM_EXPORT_RUNNING,
    UBSE_MEM_EXPORT_SUCCESS,
    UBSE_MEM_EXPORT_DESTROYING,
    UBSE_MEM_EXPORT_DESTROYED,
    UBSE_MEM_EXPORT_DESTROYING_WAIT,
    UBSE_MEM_IMPORT_RUNNING,
    UBSE_MEM_IMPORT_SUCCESS,
    UBSE_MEM_IMPORT_DESTROYING,
    UBSE_MEM_IMPORT_DESTROYING_WAIT,
    UBSE_MEM_IMPORT_DESTROYED
};

struct UbseMemFdDesc {
    std::vector<uint64_t> memids; // 返回的内存标识
    uint32_t memidNum;            // 借用内存数量
    size_t unitSize;              // 芯片表项拆分粒度
    UbseMemState state;
    uint64_t memSize;             // 映射的共享内存大小
    std::string name;
};

enum UbseNodeStatus {
    UBSE_NODE_STATUS_NORMAL,      // 正常
    UBSE_NODE_STATUS_ABNORMAL     // 异常
};

// 节点信息
struct UbseNodeInfo {
    uint32_t index;               // 节点编号
    std::string nodeId;           // index 和 node_id 看能不能归一
    std::string hostName;         // 主机名，最大长度MAX_HOSTNAME
    UbseNodeStatus status;        // 节点状态，外部在选择共享域的时候，可以做参考
};

struct UbseShmRegionDesc {
    uint32_t nodeNum;                   // 节点数
    std::vector<UbseNodeInfo> nodelist; // 节点列表
};

struct UbseMemAddrInfo {
    uint64_t addr{0}; // 内存起始地址
    uint64_t size{0}; // 该段地址长度
    friend std::ostream &operator<<(std::ostream &os, const UbseMemAddrInfo &obj)
    {
        return os << "(addr: " << obj.addr << " size: " << obj.size << ")";
    }
};

struct UbseNumaLocation {
    std::string nodeId; // 节点ID
    uint32_t numaId;    // numa id
    friend std::ostream &operator<<(std::ostream &os, const UbseNumaLocation &obj)
    {
        return os << "(nodeId: " << obj.nodeId << " numaId: " << obj.numaId << ")";
    }
};

// 所有借用类型的基本类型
struct UbseMemBaseBorrowReq {
    std::string name;
    uint64_t timestamp;
    friend std::ostream &operator<<(std::ostream &os, const UbseMemBaseBorrowReq &obj)
    {
        return os << "(name: " << obj.name << " timestamp: " << obj.timestamp << ")";
    }
};

struct UbseMemFdBorrowReq : public UbseMemBaseBorrowReq {
    std::string requestNodeId;                  // 发起借用的节点ID
    std::string importNodeId;                   // 导入节点ID
    size_t size{0};                             // 必填，单位字节
    UbseMemDistance distance{MEM_DISTANCE_L0};  // 内存访问经过的跳数
    UbseUdsInfo udsInfo;                        // 使用方进程信息
    std::vector<UbseNumaLocation> lenderLocs{}; // 借出方地址信息，与lenderSizes一一对应，为空则走基础算法进行决策
    std::vector<uint64_t> lenderSizes{};        // 借出大小，与lenderLocs一一对应
    FdOwner owner;
    friend std::ostream &operator<<(std::ostream &os, const UbseMemFdBorrowReq &obj)
    {
        os << "(" << static_cast<const UbseMemBaseBorrowReq &>(obj) << " requestNodeId: " << obj.requestNodeId
           << " importNodeId: " << obj.importNodeId << " size: " << obj.size << " distance: " << obj.distance
           << " udsInfo: " << obj.udsInfo << " lenderLocs: ";
        for (const auto &loc : obj.lenderLocs) {
            os << loc;
        }
        os << obj.owner;
        return os << ")";
    }
};

struct UbseMemNumaBorrowReq : public UbseMemFdBorrowReq {
    int srcSocket{-1};        // 内存申请需求方节点socket信息 -1 无效
    int srcNuma{-1};          // 内存申请需求方节点NUMA信息  -1 无效
    size_t highWatermark{88}; // 必填，算法百分比，vm自己决策场景，单位%
    size_t lowWatermark{11};  // 必填，算法比分在，单位%
    friend std::ostream &operator<<(std::ostream &os, const UbseMemNumaBorrowReq &obj)
    {
        return os << "(" << static_cast<const UbseMemFdBorrowReq &>(obj) << " srcSocket: " << obj.srcSocket
                  << " srcNuma: " << obj.srcNuma << " highWatermark: " << obj.highWatermark
                  << " lowWatermark: " << obj.lowWatermark << ")";
    }
};

struct UbseMemOperationResp {
    std::string name;
    std::string requestNodeId; // 发起去借用的节点ID
    uint32_t errorCode;        // 操作错误码
    std::string errMsg;        // 错误描述信息
    std::string realSize;      // request size向上取整
    std::vector<uint64_t> memIdList{};
    int64_t remoteNumaId;      // 远端Numa在本地呈现的NumaId
};

struct UbseMemReturnReq {
    uid_t uid{};               // 删除的uid
    gid_t gid{};
    std::string name;          // 资源名
    std::string requestNodeId;
    bool isForceDelete{false}; // 强制删除，控制面用， 删除资源的入参，指定是否强制删除账本（节点故障场景）
    bool smapBack{true};       // vm指定mem是否迁移数据 // 默认值为false?
};

struct UbseMemImportResult {
    uint64_t memId;            // obmm导入返回的memid
    int64_t numaId;            // 导入类型为NUMA时，该值为导入的numaId，以设备导入时，该值无意义
    friend std::ostream &operator<<(std::ostream &os, const UbseMemImportResult &obj)
    {
        return os << "(memId: " << obj.memId << " numaId: " << obj.numaId << ")";
    }
};

struct UbseMemImportStatus {
    uint32_t errCode; // 导入执行过程中的所有可能错误信息
    uint32_t scna{};
    std::vector<UbseMemImportResult> importResults{};
    UbseMemState expectState = UBSE_MEM_STATE_SUCCEEDED;
    UbseMemState state = UBSE_MEM_STATE_INIT;
    friend std::ostream &operator<<(std::ostream &os, const UbseMemImportStatus &obj)
    {
        os << "(errCode: " << obj.errCode << " scna: " << obj.scna << " importResults: ";
        for (const auto &result : obj.importResults) {
            os << result << ",";
        }
        return os << " expectState: " << obj.expectState << " state: " << obj.state << ")";
    }
};

struct UbseMemObmmInfo {
    uint64_t memId;
    ubse_mem_obmm_mem_desc desc;
    friend std::ostream &operator<<(std::ostream &os, const UbseMemObmmInfo &obj)
    {
        return os << "(memId: " << obj.memId << " desc: " << obj.desc.addr << ")";
    }
};

struct UbseMemExportStatus {
    uint32_t errCode;                                    // 导出执行过程中的所有可能错误信息
    std::vector<UbseMemObmmInfo> exportObmmInfo{};       // 执行导出后的obmm描述信息
    UbseMemState expectState = UBSE_MEM_STATE_SUCCEEDED; // 对象期望状态
    UbseMemState state = UBSE_MEM_STATE_INIT;            // 对象当前状态
    friend std::ostream &operator<<(std::ostream &os, const UbseMemExportStatus &obj)
    {
        os << "(errCode: " << obj.errCode << " exportObmmInfo: ";
        for (const auto &obmmInfo : obj.exportObmmInfo) {
            os << obmmInfo << ",";
        }
        return os << " expectState: " << obj.expectState << " state: " << obj.state << ")";
    }
};

struct UbseMemDebtNumaInfo {
    std::string nodeId; // 节点id
    int socketId{-1};   // socket id
    int64_t numaId{-1}; // numa id
    uint64_t size{0};
    friend std::ostream &operator<<(std::ostream &os, const UbseMemDebtNumaInfo &obj)
    {
        return os << "(nodeId: " << obj.nodeId << " socketId: " << obj.socketId << " numaId: " << obj.numaId
                  << " size: " << obj.size << ")";
    }
};

// 算法决策结果
struct UbseMemAlgoResult {
    std::vector<UbseMemDebtNumaInfo> exportNumaInfos{}; // 导出numa借用关系
    std::vector<UbseMemDebtNumaInfo> importNumaInfos{}; // 导入numa借用关系
    uint32_t attachSocketId;                            // 当前借用实际使用的硬件通路，来源cna查询
    friend std::ostream &operator<<(std::ostream &os, const UbseMemAlgoResult &obj)
    {
        os << "(exportNumaInfos: ";
        for (const auto &info : obj.exportNumaInfos) {
            os << info << ",";
        }
        os << " importNumaInfos: ";
        for (const auto &info : obj.importNumaInfos) {
            os << info << ",";
        }
        return os << " attachSocketId: " << obj.attachSocketId << ")";
    }
};

class UbseMemBorrowExportBaseObj {
public:
    // 导入导出的对应关系，addr的特殊处理，在agent端的export的时候，nodeId, numaId 还有对应的size, 回到master之后更新对socketId
    // 从导出端恢复的时候，这个结果不恢复
    UbseMemAlgoResult algoResult{}; // 算法决策结果
    UbseMemExportStatus status{};   // 执行结果信息
    uint32_t errorCode{};
    friend std::ostream &operator<<(std::ostream &os, const UbseMemBorrowExportBaseObj &obj)
    {
        return os << "(algoResult: " << obj.algoResult << " status: " << obj.status << " errorCode: " << obj.errorCode
                  << ")";
    }
};

class UbseMemBorrowImportBaseObj {
public:
    // addr借用，导出完成之后，在master发给agent之前，这个结果保证给更新完, 导入端恢复数据的时候，这块需要恢复
    UbseMemAlgoResult algoResult{};                // 算法决策结果
    std::vector<UbseMemObmmInfo> exportObmmInfo{}; // 执行导出的obmm信息，用于导入
    UbseMemImportStatus status{};                  // 执行导入结果
    uint32_t errorCode;
    friend std::ostream &operator<<(std::ostream &os, const UbseMemBorrowImportBaseObj &obj)
    {
        os << "(algoResult: " << obj.algoResult << " exportObmmInfo: ";
        for (const auto &obmmInfo : obj.exportObmmInfo) {
            os << obmmInfo << ",";
        }
        return os << " status: " << obj.status << " errorCode: " << obj.errorCode << ")";
    }
};

class UbseMemFdBorrowExportObj final : public UbseMemBorrowExportBaseObj {
public:
    UbseMemFdBorrowReq req; // 请求参数
    [[nodiscard]] UbseObjByteBuffer ToBuffer() const;
    void FromBuff(const UbseObjByteBuffer &buffer);
    friend std::ostream &operator<<(std::ostream &os, const UbseMemFdBorrowExportObj &obj)
    {
        return os << "(" << static_cast<const UbseMemBorrowExportBaseObj &>(obj) << " req: " << obj.req << ")";
    }
};

class UbseMemFdBorrowImportObj final : public UbseMemBorrowImportBaseObj {
public:
    UbseMemFdBorrowReq req; // 请求参数
    [[nodiscard]] UbseObjByteBuffer ToBuffer() const;
    void FromBuff(const UbseObjByteBuffer &buffer);
    friend std::ostream &operator<<(std::ostream &os, const UbseMemFdBorrowImportObj &obj)
    {
        return os << "(" << static_cast<const UbseMemBorrowImportBaseObj &>(obj) << " req: " << obj.req << ")";
    }
};

class UbseMemNumaBorrowExportObj final : public UbseMemBorrowExportBaseObj {
public:
    UbseMemNumaBorrowReq req; // 请求参数
    [[nodiscard]] UbseObjByteBuffer ToBuffer() const;
    void FromBuff(const UbseObjByteBuffer &buffer);
    friend std::ostream &operator<<(std::ostream &os, const UbseMemNumaBorrowExportObj &obj)
    {
        return os << "(" << static_cast<const UbseMemBorrowExportBaseObj &>(obj) << " req: " << obj.req << ")";
    }
};

class UbseMemNumaBorrowImportObj final : public UbseMemBorrowImportBaseObj {
public:
    UbseMemNumaBorrowReq req; // 请求参数
    [[nodiscard]] UbseObjByteBuffer ToBuffer() const;
    void FromBuff(const UbseObjByteBuffer &buffer);
    friend std::ostream &operator<<(std::ostream &os, const UbseMemNumaBorrowImportObj &obj)
    {
        return os << "(" << static_cast<const UbseMemBorrowImportBaseObj &>(obj) << " req: " << obj.req << ")";
    }
};
} // namespace ubse::mem::obj
#endif // UBSE_MANAGER_UBSE_MEM_OBJ_H
