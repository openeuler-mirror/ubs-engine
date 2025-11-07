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

#ifndef UBSE_MEMORY_INTERFACE_H
#define UBSE_MEMORY_INTERFACE_H

#include <stddef.h>
#include <stdint.h>
#include "ubse_mem_resource.h"
#include "ubse_const_def.h"
#include "ubse_mem_inner_def.h"
#include "ubse_mem_obj.h"
#ifdef __cplusplus
extern "C" {
#endif
using namespace ubse::mem::obj;
using namespace ubse::resource::mem;

typedef enum tag_ubse_node_state {
    UBSE_NODE_NORMAL = 0,
    UBSE_NODE_REBOOT,
    UBSE_NODE_OOM,
    UBSE_NODE_PANIC,
    UBSE_NODE_KERNEL_REBOOT
} ubse_node_state;

typedef enum tag_ubse_node_type {
    UBSE_CPU_NODE = 0,
} ubse_node_type;

typedef struct tag_ubse_ipv4_addr {
    uint8_t addr[4]; // 4个字符存储ipv4地址
} ubse_ipv4_addr;

typedef struct tag_ubse_ipv6_addr {
    uint8_t addr[16]; // 16个字符存储ipv6地址
} ubse_ipv6_addr;

typedef enum tag_ubse_ip_type {
    UBSE_IP_V4 = 0,
    UBSE_IP_V6
} ubse_ip_type;

typedef struct tag_ubse_ip_addr {
    ubse_ip_type type;
    union {
        ubse_ipv4_addr ipv4;
        ubse_ipv6_addr ipv6;
    };
} ubse_ip_addr;

typedef struct tag_ubse_node {
    char node_id[UBSE_NODE_ID_MAX_LEN + 1]; // 节点唯一标识
    uint32_t slot_id;                      // 槽位号
    ubse_node_type type;                    // 节点类型
    ubse_node_state state;                  // 节点状态
} ubse_node;

typedef enum tag_ubse_mem_numa_status {
    UBSE_MEM_NUMA_NORMAL = 0,
    UBSE_MEM_NUMA_FAULT,
} ubse_mem_numa_status;

typedef struct tag_ubse_mem_numa_node {
    uint32_t numa_id;                                        // numa id
    uint16_t bind_core_list[UBSE_MEM_NUMA_BIND_CORE_MAX_NUM]; // 绑定的cpu核id
    uint64_t size;                                           // 总内存大小
    uint64_t free;                                           // 空闲内存
    uint32_t nr_hugepages_2M;                                // 2M大页总数
    uint32_t free_hugepages_2M;                              // 2M大页空闲总数
    ubse_mem_numa_status status;                              // numa状态
} ubse_mem_numa_node;

typedef struct tag_ubse_mem_bus_controller {
    char eid[UBSE_EID_MAX_LEN + 1];
    char guid[UBSE_GUID_MAX_LEN + 1];
    char upi[UBSE_UPI_MAX_LEN + 1];
} ubse_mem_bus_controller;

typedef struct tag_ubse_mem_obmm_info {
    uint64_t mem_id;
    ubse_mem_obmm_mem_desc desc;
} ubse_mem_obmm_info;

typedef enum tag_ubse_mem_state {
    UBSE_MEM_STATE_INIT,
    UBSE_MEM_STATE_RUNNING,
    UBSE_MEM_STATE_SUCCEEDED,
    UBSE_MEM_STATE_FAILED,
    UBSE_MEM_STATE_DESTROYING,
    UBSE_MEM_STATE_DESTROY_FAILED,
    UBSE_MEM_STATE_DESTROYED,
    UBSE_MEM_STATE_DESTROYED_FORCE,
    UBSE_MEM_STATE_DESTROYED_WITH_SMAP_BACK
} ubse_mem_state;

typedef struct tag_ubse_mem_export_status {
    bool real_exec;
    // 对应: UbseRpcManagerObmmExportResponse.memId/.memDesc/.memIdsAddr/.memDescsAddr
    uint32_t info_list_len;            // obmm导出内存的描述符列表长度
    ubse_mem_obmm_info *obmm_info_list; // obmm导出内存的描述符列表
} ubse_mem_export_status;

typedef struct tag_ubse_mem_shm_info {
    // 对应: UbseUbseCreateResourceShareAttr.baseNodeId
    char base_node[UBSE_NODE_ID_MAX_LEN + 1];
    // 对应: UbseUbseCreateResourceShareAttr.shmRegion
    // 共享域信息
    uint32_t num;
    char nodeId[UBSE_MEM_MAX_TOPOLOGY_HOSTS][UBSE_NODE_ID_MAX_LEN + 1];
    char hostName[UBSE_MEM_MAX_TOPOLOGY_HOSTS][UBSE_HOST_NAME_MAX_LEN + 1]; // nodeId对应的hostname，最长MEM_MAX_ID_LENGTH
} ubse_mem_shm_info;

typedef struct tag_ubse_mem_virtual_addr_info {
    uint64_t addr; // 需要借用的进程虚拟地址
    uint64_t size; // 该段地址借入大小
} ubse_mem_virtual_addr_info;

typedef struct tag_ubse_mem_proc_info {
    // 对应: Create接口时：CreateResourceAppBorrowAttr/CreateResourceShareAttr.uid_t/.gid
    // 对应: Attach接口时：AttachResourceShareAttr.srcPid/dstPid
    uint32_t uid; // 用户id
    uint32_t gid; // 用户组id
    // 对应: AppPriBorrowAttr.srcPid/dstPid
    uint64_t pid; // 进程id
    uint64_t dst_pid;
} __attribute__((packed)) ubse_mem_proc_info;

typedef struct tag_ubse_mem_custom_data {
    uint8_t version; // 版本，当前为1，只有版本号有效，才继续check其他字段
    // 对应: UbseUbseMemCreateRequestAttr.createType
    uint8_t type; // 借用类型，应用设备借用0，水线1，共享2，应用numa借用3
    // 对应：UbseUbseCreateResourceShareAttr.SHMRegionDesc.type
    uint8_t shm_region_type; // 共享域类型
    // 参考ubse_mem_proc_info注释
    ubse_mem_proc_info proc; // 导出进程的进程信息
    // 对应: xxAttr.size 除了AppPriBorrowAttr(vaList替代)
    uint64_t borrow_size; // 借入大小
    // 对应: xxAttr.perfLevel
    uint8_t perf_level; // perf level
    // 对应: WaterBorrowAttr.highWatermark
    uint8_t high_water_mark; // 高水线
    // 对应: WaterBorrowAttr.lowWatermark
    uint8_t low_water_mark; // 低水线
    // 对应: UbseUbseMemStrategyProviderResult.numaPresentId
    int8_t numa_present_id;
    int32_t borrow_socket_id; // 借入方的socket id，默认值-1
    int8_t borrow_numa_id;    // 借入方的远端numa id，默认值-1
    uint64_t lend_total_size; // 借出内存的总大小
    // 对应: xxAttr.name
    char resource_id[UBSE_MEM_RESOURCE_ID_MAX_LEN]; // 全局资源请求标识
    // 对端节点id
    // 非共享场景下，import对象填导出节点ID，export对象填导入节点ID；
    // 共享场景下，export对象填空，import对象填导出节点ID
    char peer_node[UBSE_NODE_ID_MAX_LEN];
    // 对应: UbseUbseCreateResourceShareAttr.requestNodeId, UbseUbseCreateResourceAppBorrowAttr.requestNodeId
    // AppPriBorrowAttr和 WaterBorrowAttr 这两个不涉及
    // AppPriBorrowAttr.srcNodeId 和 WaterBorrowAttr.waterMallocAttr.srcNid 与 attachNode 等价
    char request_node[UBSE_NODE_ID_MAX_LEN]; // 发起请求节点id
    // 对应: UbseUbseMemStrategyProviderResult.borrowLocs[i].socket_id
    int32_t borrow_socket_id_list[UBSE_MEM_EXPORT_NUMA_MAX_NUM]; // 借入方的socket id list，默认值-1
    // 对应: UbseUbseMemStrategyProviderResult.borrowLocs[i].numaid
    int8_t borrow_numa_id_list[UBSE_MEM_EXPORT_NUMA_MAX_NUM]; // 借入numa id列表，默认值-1
    // 对应: UbseUbseMemStrategyProviderResult.lenderLocs[i].socketId
    // 特例: WaterBorrowAttr.waterMallocAttr.lenderLocs.socketId，水线自行给出，和算法输出一致
    int32_t lend_socket_id_list[UBSE_MEM_EXPORT_NUMA_MAX_NUM]; // 借出方的socket id list，默认值-1
    // 对应: UbseUbseMemStrategyProviderResult.lenderLocs[i].numaid
    // 特例: WaterBorrowAttr.waterMallocAttr.lenderLocs.numaId，水线自行给出，和算法输出一致
    int8_t lend_numa_id_list[UBSE_MEM_EXPORT_NUMA_MAX_NUM]; // 借出numa id列表，默认值-1
    // 对应: UbseUbseMemStrategyProviderResult.lenderSizes[i]
    // 特例: WaterBorrowAttr.waterMallocAttr.lenderSizes，水线自行给出，和算法输出一致
    uint64_t lend_numa_size_list[UBSE_MEM_EXPORT_NUMA_MAX_NUM]; // numa 借出内存大小，与lend_numa_id_list对应
} __attribute__((packed)) ubse_mem_custom_data;

typedef enum tag_ubse_mem_fault_state {
    FAULT,
    PREDICTING_FAULT,
    UNKNOWN
} ubse_mem_fault_state;

typedef struct tag_ubse_mem_fault_info {
    uint64_t mem_id;
    uint64_t offset;
    uint64_t pageFaultNum;
    ubse_mem_fault_state state;
} ubse_mem_fault_info;

typedef struct tag_ubse_mem_export_info {
    char node_id[UBSE_NODE_ID_MAX_LEN + 1]; // 节点Id
    ubse_mem_custom_data custom_data;       // obmm存储的黑盒私有数据
    // 对应: UbseUbseCreateResourceAppBorrowAttr.vaLists
    uint32_t va_list_len;               // 借用地址段长度
    ubse_mem_virtual_addr_info *va_list; // 借用地址段
    ubse_mem_shm_info shm_info;          // 共享域，共享借用下生效
    ubse_mem_export_status status;       // 导出执行状态详情
    ubse_mem_state expect_state;         // 期望状态
    ubse_mem_state state;                // 执行状态
    bool isForceDelete;                 // 是否是强制删除
    uint32_t responseCode;              // 错误码
} ubse_mem_export_info;

typedef struct tag_ubse_mem_import_result {
    uint64_t mem_id; // obmm导入返回的memid
    int64_t numa_id; // 导入类型为NUMA时，该值为导入的numaId，以设备导入时，该值无意义
} ubse_mem_import_result;

typedef struct tag_ubse_mem_import_status {
    bool real_exec;
    uint64_t shmSize;
    // 对应: UbseRpcManagerObmmImportResponse.memId./numaId/.memIdsAddr/.numaIdsAddr
    uint32_t result_len;
    ubse_mem_import_result *result_list;
} ubse_mem_import_status;

typedef struct tag_ubse_mem_import_info {
    char node_id[UBSE_NODE_ID_MAX_LEN + 1]; // 节点Id
    ubse_mem_custom_data custom_data;       // obmm存储的黑盒私有数据
    // 对应: UbseUbseCreateResourceAppBorrowAttr.vaLists
    uint32_t va_list_len;               // 借用地址段长度
    ubse_mem_virtual_addr_info *va_list; // 借用地址段
    ubse_mem_shm_info shm_info;          // 共享域，共享借用下生效
    // 参考ubse_mem_proc_info注释
    ubse_mem_proc_info proc; // 导入进程的进程信息
    // 对应: UbseRpcManagerObmmExportResponse.memId/.memDesc/.memIdsAddr/.memDescsAddr
    uint32_t info_list_len;            // obmm导出内存的描述符列表长度
    ubse_mem_obmm_info *obmm_info_list; // obmm导出内存的描述符列表
    ubse_mem_import_status status;      // 导入执行状态详情
    ubse_mem_state expect_state;        // 期望状态
    ubse_mem_state state;               // 执行状态
    bool isForceDelete;                // 是否是强制删除
    uint32_t responseCode;             // 错误码
    uint32_t scna;
    uint32_t unimportResult; // unimport的结果
} ubse_mem_import_info;

typedef struct tag_ubse_mem_cpu_loc {
    char node_id[UBSE_NODE_ID_MAX_LEN + 1]; // 节点Id
    uint32_t socket_id;                    // cpu的socketId
    ubse_mem_bus_controller bus_controller;
} ubse_mem_cpu_loc;

typedef struct tag_ubse_mem_cpu {
    ubse_mem_cpu_loc loc;        // cpu location信息
    uint32_t peer_list_len;     // 互联cpu列表长度
    ubse_mem_cpu_loc *peer_list; // 互联cpu列表
} ubse_mem_cpu;

typedef enum tag_ubse_cpu_link_type {
    UBSE_CPU_LINK_HCCS,
    UBSE_CPU_LINK_UB
} ubse_cpu_link_type;

typedef struct tag_ubse_mem_cpu_node {
    ubse_node node; // 节点唯一标识
    char bus_instance_id[UBSE_BUS_INSTANCE_ID_MAX_LEN + 1];
    char host_name[UBSE_HOST_NAME_MAX_LEN + 1];
    uint32_t ip_list_len;
    ubse_ip_addr *ip_list;
    ubse_mem_cpu cpu_list[UBSE_CPU_MAX_NUM];                  // 节点cpu列表
    ubse_mem_numa_node numa_node_list[UBSE_MEM_NUMA_MAX_NUM]; // 本地numa列表
    uint32_t export_list_len;                               // 导出内存列表长度
    ubse_mem_export_info *export_list;                       // 导出内存列表长
    uint32_t import_list_len;                               // 导入内存列表长度
    ubse_mem_import_info *import_list;                       // 导入内存列表
    uint32_t fault_info_list_len;
    ubse_mem_fault_info *fault_info_list;
    ubse_cpu_link_type link_type; // 节点内cpu连接类型
} ubse_mem_cpu_node;

struct FaultInfo {
    uint64_t memId;
    unsigned long offset;
    const char *nodeId;
};
/* *
 * @brief 定义故障处理函数指针
 * @param fault_type[in]: 故障类型
 * @param fault_info[in]: 故障处理所需数据
 */
typedef void (*mem_fault_handler)(ubse_mem_fault_state fault_type, FaultInfo *fault_info);

/*
 * 接口结构体：ubse_memory_interface
 */
typedef struct ubse_memory_interface {
    /* *
     * @brief 获取本节点内存信息
     * @param out_count [out]: 节点 numa 内存数组大小
     * @param ubse_mem_cpu_node [in,out]: 节点内存信息
     * @return 返回值: 0 (成功) 或其他错误码
     */
    uint32_t (*collect_node_mem)(size_t *out_count, ubse_mem_cpu_node *out_node_mem);

    /* *
     * @brief 内存借出信息
     * @param export_info[out]: 返回申请到的内存的对象。
     * @return 返回值: 0 (成功) 或其他错误码
     */
    uint32_t (*export_mem)(ubse_mem_export_info *export_info);

    /* *
     * @brief 依据内存编号回收导出的内存
     * @param export_info [in]: 要回收的内存的对象
     * @return 返回值: 0 (成功) 或其他错误码
     */
    uint32_t (*unexport_mem)(ubse_mem_export_info *export_info);

    /* *
     * @brief 导入内存数据
     * @param import_info [in,out]: 引入内存的内存对象
     * @return 返回值: 0 (成功) 或其他错误码
     */
    uint32_t (*import_mem)(ubse_mem_import_info *import_info);

    /* *
     * @brief 依据内存编号回收导入的内存
     * @param ubse_mem_import_info [in]: 要回收的内存的对象
     * @return 返回值: 0 (成功) 或其他错误码
     */
    uint32_t (*unimport_mem)(ubse_mem_import_info *import_info);

    /* *
     * @brief 根据内存 ID，以及该段内存上的偏移量，查出该字节对应的物理地址。如果 ID 不存在或 offset
     * 越界，函数将返回错误。
     * @param mem_id [in]: 要查询的内存的编号
     * @param offset [in]: 要查询的 offset
     * @param pa [out]: 查询结果物理地址
     * @return 返回值: 0 (成功) 或其他错误码
     */
    uint32_t (*query_pa_by_id)(uint64_t mem_id, unsigned long offset, unsigned long *pa);

    /* *
     * @brief
     * 根据一个本机的物理地址，查询出该地址对应的内存ID，以及所指字节在该段内存中的偏移量。如果物理地址和借用内存无关，函数将返回错误。
     * @param pa [in]: 要查询的物理地址
     * @param mem_id [out]: 查询结果的内存编号
     * @param offset [out]: 查询结果的 offset
     * @return 返回值: 0 (成功) 或其他错误码
     */
    uint32_t (*query_id_by_pa)(unsigned long pa, uint64_t *mem_id, unsigned long *offset);

    /* *
     * @brief 根据内存 ID，以及该段内存上的偏移量，查出该字节对应的物理地址。如果 ID 不存在或 offset
     * 越界，函数将返回错误。
     * @param mem_id [in]: 要查询的内存的编号
     * @param offset [in]: 要查询的 offset
     * @param pa [out]: 查询结果物理地址
     * @return 返回值: 0 (成功) 或其他错误码
     */
    uint32_t (*query_pa_by_id_with_retry)(uint64_t mem_id, unsigned long offset, unsigned long *pa);

    /* *
     * @brief
     * 根据一个本机的物理地址，查询出该地址对应的内存ID，以及所指字节在该段内存中的偏移量。如果物理地址和借用内存无关，函数将返回错误。
     * @param pa [in]: 要查询的物理地址
     * @param mem_id [out]: 查询结果的内存编号
     * @param offset [out]: 查询结果的 offset
     * @return 返回值: 0 (成功) 或其他错误码
     */
    uint32_t (*query_id_by_pa_with_retry)(unsigned long pa, uint64_t *mem_id, unsigned long *offset);

    /* *
     * @brief 注册故障回调函数
     * @param fault_type [in] 故障类型
     * @param handler [in] 回调函数
     * @return 返回值: 0 (成功) 或其他错误码
     */
    uint32_t (*register_fault_handler)(ubse_mem_fault_state *fault_type, mem_fault_handler handler);

    /* *
     * @brief 上报故障
     * @param fault_type [in] 故障类型
     * @param fault_info [in] 故障信息
     * @return 返回值: 0 (成功) 或其他错误码
     */
    uint32_t (*sumbit_fault_handler)(const char *fault_type, const char *fault_info);

    uint32_t (*fd_export)(UbseMemFdBorrowExportObj &memBorrowObj);

    uint32_t (*fd_import)(UbseMemFdBorrowImportObj &memBorrowObj);

    uint32_t (*numa_export)(UbseMemNumaBorrowExportObj &memBorrowObj);

    uint32_t (*numa_import)(UbseMemNumaBorrowImportObj &memBorrowObj);

    uint32_t (*fd_unimport)(const UbseMemFdBorrowImportObj &memBorrowObj);

    uint32_t (*fd_unexport)(const UbseMemFdBorrowExportObj &memBorrowObj);

    uint32_t (*numa_unimport)(const UbseMemNumaBorrowImportObj &memBorrowObj);

    uint32_t (*numa_unexport)(const UbseMemNumaBorrowExportObj &memBorrowObj);

    uint32_t (*get_all_local_data)(NodeMemDebtInfo &nodeMemDebtInfo);
} matrix_memory_interface;

/* *
 * @brief 获取实例
 */
ubse_memory_interface *get_ubse_memory_instance();

#ifdef __cplusplus
}
#endif

#endif // UBSE_MEMORY_INTERFACE_H
