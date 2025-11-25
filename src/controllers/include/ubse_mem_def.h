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

#ifndef UBSE_MEM_DEF_H
#define UBSE_MEM_DEF_H
#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif

#define MEM_MAX_ID_LENGTH 48 // 节点id，共享内存name，最大限制1024
#define MEM_INVALID_NODE_ID ("")
#define MEM_TOPOLOGY_MAX_HOSTS 16
#define MEM_MAX_NUMA_NUM_PER_ITEM 2     // 单次请求最多导出numa位置
#define MEM_TOPOLOGY_MAX_TOTAL_NUMAS 64 // 后续版本要增加，720暂不修改
#ifndef MAX_REGIONS_NUM
#define MAX_REGIONS_NUM 6
#endif

typedef enum UbseMemShmRegionType {
    ALL2ALL_SHARE = 0, /* *SHM域类型为域内任何节点提供内存都可以被域内所有节点共享访问 */
    ONE2ALL_SHARE,     /* *SHM域类型为域内单一节点作为提供方都可以被域内所有节点共享访问 */
    INCLUDE_ALL_TYPE,  /* *请求所有类型 */
} ShmRegionType;
typedef enum UbseMemPerfLevel {
    L0, /* *L0对应直连节点 */
    L1, /* *L1对应通过1跳节点，暂不支持 */
    L2  /* *L2对应过超过1跳节点 ，暂不支持 */
} PerfLevel;

typedef struct TagUbseMemSHMRegionDesc {
    PerfLevel perfLevel;
    ShmRegionType type;
    int num;
    char nodeId[MEM_TOPOLOGY_MAX_HOSTS][MEM_MAX_ID_LENGTH];
    char hostName[MEM_TOPOLOGY_MAX_HOSTS][MEM_MAX_ID_LENGTH]; // nodeId对应的hostname，最长MEM_MAX_ID_LENGTH
} SHMRegionDesc;

typedef struct TagUbseMemSHMRegions {
    int num;
    SHMRegionDesc region[MAX_REGIONS_NUM];
} SHMRegions;

typedef struct TagUbseMemSHMRegionInfo {
    int num;
    int64_t info[0];
} SHMRegionInfo;

struct NodeBorrowMemInfo {
    uint64_t availBorrowMemSize; // 当前可借入内存的大小，单位字节
    uint64_t borrowedMemSize;    // 已经借入的内存大小，单位字节
    uint64_t availLendMemSize;   // 当前可借出内存的大小，单位字节
    uint64_t lentMemSize;        // 已经借出的内存大小，单位字节
};

// 错误码同时会设置到errno变量里面，系统的范围是0-1000，mem插件用1000-11000
// 根据错误码，快速定位到需要看什么日志文件
// typedef int error_t;
typedef enum UbseMemErrorCode : int {
    HOK = 0,
    E_CODE_INVALID_PAR = 1000,         // 参数错误
    E_CODE_MEMLIB_IPC = 1001,          // ipc通信
    E_CODE_AGENT_RPC = 1002,           // rpc通信，agent发起
    E_CODE_MASTER_RPC = 1003,          // rpc通信,主节点发起的请求
    E_CODE_MEM_NOT_READY = 1004,       // 插件没准备好，切主，或者插件被卸载等错误.需要重试
    E_CODE_MEMLIB = 1005,              // 应用使能库内部错误,需要看memlib日志
    E_CODE_AGENT = 1006,               // agent mem插件内部错误，需要看mem_agent日志
    E_CODE_MANAGER = 1007,             // master 插件内部错误，需要看mem_manager日志
    E_CODE_RESOURCE_EXIST = 1008,      // 资源已存在，create失败
    E_CODE_RESOURCE_NOT_CREATE = 1009, // 删除的时候，资源不存在
    E_CODE_RESOURCE_ATTACHED = 1010,   // 删除共享内存的时候，返回还有节点在attach
    E_CODE_STRATEGY_ERROR = 1011,      // 算法失败，可能是obmm资源使用完，.有限重试
    E_CODE_OBMM_OP_FAILED =
        1012, // obmm接口调用失败，大概率是设备还在使用，不能归还，或者dmesg返回-12资源不够用，有限重试
    E_CODE_SMAP_OP_FAILED = 1013,              // SMAP接口调用失败
    E_CODE_SCBUS_DAEMON = 1014,                // 控制面接口调用失败，有限重试
    E_CODE_OPEN_FILE = 1015,                   // open文件失败（一般是obmm设备）
    E_CODE_MMAP_FILE = 1016,                   // mmap文件失败（一般是obmm设备）
    E_CODE_NULLPTR = 1017,                     // 空指针，有限重试
    E_CODE_SERIALIZE_DESERIALIZE_ERROR = 1018, // 消息反序列化，有限重试
    E_CODE_CRC_CHECK_ERROR = 1019              // 消息错误，有限重试
} RmErrorCode;

typedef enum UbseMemQueryType {
    QUERY_OBMM_CLEAR_CONF = 0,
    QUERY_MEM_NUMA_STATUS,   /* 查询numa状态信息，对应结构体：QueryNumaStatus */
    QUERY_WATER_MARK_STATUS, /* 查询水线信息，对应结构体：QueryMemWaterInfo */
    QUERY_MEM_ACCOUNT,       /* 查询账本信息，对应结构体：QueryMemAccount */
    QUERY_SHM_ACCOUNT,       /* 查询内存共享账本，对应结构体；QueryShmAccount */
    QUERY_MEM_NODE_INFO,     /* app查询本地节点信息，对应结构体：QueryLocalBorrowMemInfo;
                                    RM.mem端查询所有节点借用信息接口，对应结构体：QueryAllNodeBorrowMemInfo */
} QueryType;

void DestroyUbseMemLib(void);

// 是否开启应用借用共享性能统计记录
void EnableUbseMemHtrace(void);

/*
 * 非mem错误码，会调用strerror(code)
 */
const char *ErrCodeToStr(int errNum);
#ifdef __cplusplus
}
#endif

#endif