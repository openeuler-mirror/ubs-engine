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

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UBSE_MEM_RESOURCE_ID_MAX_LEN 48
#define UBSE_MEM_EXPORT_NUMA_MAX_NUM 2
#define UBSE_MEM_NUMA_MAX_NUM 8
#define UBSE_MEM_CUSTOM_DATA_MAX_LEN 128
#define UBSE_MEM_NUMA_BIND_CORE_MAX_NUM 128
#define UBSE_MEM_ID_MAX_LENGTH 48 // 节点id，共享内存name，最大限制48
#define UBSE_MEM_MAX_TOPOLOGY_HOSTS 16
#define UBSE_MEM_RESOURCE_KIND_LEN 16
#define UBSE_MEM_CONFIF_EXECUTE_STATUS_LEN 16
#define UBSE_MEM_CONFIF_EXECUTE_STATUS_TYPE_LEN 8
#define UBSE_MEM_BORROW_TYPE_LEN 16
#define UBSE_MEM_HIGH_WATERMARK_DEFAULT 88
#define UBSE_MEM_LOW_WATERMARK_DEFAULT 11
#define UBSE_MEM_APP_BORROW_REQUEST "APP_BORROW"
#define UBSE_MEM_SHARE_REQUEST "SHARE"
#define UBSE_MEM_WATER_BORROW_REQUEST "WATER_BORROW"
#define UBSE_MEM_APP_PRI_REQUEST "APP_PRI_BORROW"
#define UBSE_MEM_CREATE_TYPE "CREATE"
#define UBSE_MEM_DELETE_TYPE "DELETE"
#define UBSE_MEM_ATTACH_TYPE "ATTACH"
#define UBSE_MEM_DETACH_TYPE "DETACH"

#define MEM_MAX_ID_LENGTH 48 // 节点id，共享内存name，最大限制1024
#define MEM_INVALID_NODE_ID ("")
#define MEM_TOPOLOGY_MAX_HOSTS 16
#define MEM_MAX_NUMA_NUM_PER_ITEM 2     // 单次请求最多导出numa位置
#define MEM_TOPOLOGY_MAX_TOTAL_NUMAS 64 // 后续版本要增加，720暂不修改
#ifndef MAX_REGIONS_NUM
#define MAX_REGIONS_NUM 6
#endif

typedef enum UbseMemShmRegionType
{
    ALL2ALL_SHARE = 0, /* *SHM域类型为域内任何节点提供内存都可以被域内所有节点共享访问 */
    ONE2ALL_SHARE,     /* *SHM域类型为域内单一节点作为提供方都可以被域内所有节点共享访问 */
    INCLUDE_ALL_TYPE,  /* *请求所有类型 */
} ShmRegionType;
typedef enum UbseMemPerfLevel
{
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

typedef enum UbseMemQueryType
{
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
const char* ErrCodeToStr(int errNum);
#ifdef __cplusplus
}
#endif
#endif // UBSE_MEM_DEF_H
