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

package memfragmentation

/*
#cgo CFLAGS: -I${SRCDIR}/../../include -I/usr/include/ubse
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#define VIRT_MEM_MAX_NODE_ID_LENGTH 48
#define VIRT_MEM_MAX_POD_ID_LENGTH 48
#define VIRT_MAX_UUID_LENGTH 37
#define VIRT_MAX_NAME_LENGTH 128
#define VIRT_MAX_STATE_LENGTH 128
#define UBS_VA_HOST_NAME_MAX 64
#define MEM_TASK_ID_MAX 256
#define MAX_NUMA_NUM 512
#define MAX_VM_NUM 300
#define MAX_NODE_NUM 32
#define MAX_BORROW_ID_COUNT 100
#define MAX_BORROW_ID_LENGTH 128
#define MAX_DEST_PARAM_SIZE 100
#define MAX_DEST_NUMA_NUM 1

typedef struct {
    uint64_t pageSize;
    uint64_t hugePageTotal;
    uint64_t hugePageFree;
} numa_page_data;

typedef struct {
    int64_t timestamp;
    char node_id[VIRT_MEM_MAX_NODE_ID_LENGTH];
    char host_name[UBS_VA_HOST_NAME_MAX];
    int16_t numa_id;
    int16_t socket_id;
    int16_t is_local;
    uint64_t mem_total;
    uint64_t mem_free;
    numa_page_data* huge_page_data;
    uint64_t numaPageInfoCount;
} numa_info_t;

typedef struct {
    int16_t numaId;
    int16_t socketId;
    bool isLocal;
    uint64_t pageSize;
    int64_t usedMem;
} vm_numa_info_t;

typedef struct {
    char nodeId[VIRT_MEM_MAX_NODE_ID_LENGTH];
    char hostName[UBS_VA_HOST_NAME_MAX];
    char uuid[VIRT_MAX_UUID_LENGTH];
    char name[VIRT_MAX_NAME_LENGTH];
    char state[VIRT_MAX_STATE_LENGTH];
    int64_t vmCreateTime;
    uint64_t maxMem;
    pid_t pid;
} vm_meta_data_t;

typedef struct {
    int64_t timestamp;
    vm_meta_data_t metadata;
    vm_numa_info_t* numaInfo;
    uint64_t numaInfoCount;
} vm_domain_info_t;

typedef struct {
    char host_id[VIRT_MEM_MAX_NODE_ID_LENGTH];
    uint16_t socket_id;
    uint16_t numa_nums;
    int numa_ids[MAX_DEST_NUMA_NUM];
    uint64_t mem_sizes[MAX_DEST_NUMA_NUM];
} dst_numa_info_c;

typedef struct {
    char src_host_id[VIRT_MEM_MAX_NODE_ID_LENGTH];
    int16_t src_socket_id;
    int16_t src_numa_id;
    uint64_t borrow_size;
    dst_numa_info_c dest_numa_infos[MAX_DEST_PARAM_SIZE];
    uint32_t dest_numa_infos_size;
} borrow_strategy_c;

typedef struct {
    borrow_strategy_c borrow_strategy;
    bool isAsync;
    uint64_t page_size;
} borrow_setting_c;

typedef struct {
    char borrow_ids_ptr[MAX_BORROW_ID_COUNT][MAX_BORROW_ID_LENGTH];
    uint16_t present_numa_ids_ptr[MAX_BORROW_ID_COUNT];
    uint32_t borrow_ids_size;
    uint32_t present_numa_ids_size;
    char task_id[MEM_TASK_ID_MAX];
} mem_borrow_result_c;

typedef enum {
    ASYNC_TASK_NOT_EXIST = 0,
    ASYNC_TASK_RUNNING = 1,
    ASYNC_TASK_SUCCESS = 2,
    ASYNC_TASK_FAILED = 3
} async_task_status_c;

typedef struct {
    char task_id[MEM_TASK_ID_MAX];
    async_task_status_c status;
    uint32_t resultCode;
    mem_borrow_result_c memBorrowResult;
} async_task_info_c;

typedef struct {
    char node_id[VIRT_MEM_MAX_NODE_ID_LENGTH];
    char borrow_id_list[MAX_BORROW_ID_COUNT][MAX_BORROW_ID_LENGTH];
    uint32_t borrow_id_size;
} RollbackSrcParam;

typedef struct {
    char node_id[VIRT_MEM_MAX_NODE_ID_LENGTH];
    numa_info_t* numa_infos;
    uint32_t numa_len;
    bool is_current;
    bool is_master;
    char* pod_id;
    uint32_t pod_id_len;
} node_info_s;

typedef struct {
    node_info_s* node_infos;
    uint32_t node_len;
} node_info_list_s;

typedef struct {
    char node_id[VIRT_MEM_MAX_NODE_ID_LENGTH];
    vm_domain_info_t* vm_infos;
    uint32_t vm_len;
    bool is_current;
    bool is_master;
    char* pod_id;
    uint32_t pod_id_len;
} vm_node_info_s;

typedef struct {
    vm_node_info_s* vm_node_infos;
    uint32_t node_len;
} vm_info_list_s;

typedef struct {
    int16_t numa_id;
} numa_meta_info_s;

typedef struct {
    char src_nid[VIRT_MEM_MAX_NODE_ID_LENGTH];
    numa_meta_info_s* numa_meta_infos;
    uint32_t numa_len;
    uint64_t borrow_size;
    uint64_t page_size;
} mem_borrow_param_s;

typedef struct {
    mem_borrow_result_c* mem_borrow_result_list;
    uint16_t mem_borrow_result_list_len;
} mem_borrow_result_s;

typedef int32_t (*va_memfrag_node_info_func)(numa_info_t**, uint32_t*);
typedef int32_t (*va_memfrag_vm_info_func)(vm_domain_info_t**, uint32_t*);
typedef int32_t (*va_memfrag_node_info_list_func)(node_info_list_s*);
typedef int32_t (*va_memfrag_vm_info_list_func)(vm_info_list_s*);
typedef int32_t (*va_memfrag_borrow_execute_func)(const borrow_setting_c*, mem_borrow_result_c*);
typedef int32_t (*va_memfrag_return_func)(bool, char**, uint32_t*);
typedef int32_t (*va_memfrag_rollback_func)(const RollbackSrcParam*);
typedef int32_t (*va_memfrag_sync_task_query_func)(char*, uint32_t, async_task_info_c*);
typedef int32_t (*va_memfrag_borrow_func)(const mem_borrow_param_s*, bool, mem_borrow_result_s*);
typedef int32_t (*va_memfrag_heartbeat_func)(void);

int va_memfrag_call_node_info(va_memfrag_node_info_func fn, numa_info_t** nodeList, uint32_t* nodeCnt)
{
    if (fn == NULL) return -1;
    return fn(nodeList, nodeCnt);
}

int va_memfrag_call_vm_info(va_memfrag_vm_info_func fn, vm_domain_info_t** vmInfoList, uint32_t* vmInfoCnt)
{
    if (fn == NULL) return -1;
    return fn(vmInfoList, vmInfoCnt);
}

int va_memfrag_call_node_info_list(va_memfrag_node_info_list_func fn, node_info_list_s* nodeList)
{
    if (fn == NULL) return -1;
    return fn(nodeList);
}

int va_memfrag_call_vm_info_list(va_memfrag_vm_info_list_func fn, vm_info_list_s* vmInfoList)
{
    if (fn == NULL) return -1;
    return fn(vmInfoList);
}

int va_memfrag_call_borrow_execute(va_memfrag_borrow_execute_func fn,
                                   const borrow_setting_c* setting,
                                   mem_borrow_result_c* result)
{
    if (fn == NULL) return -1;
    return fn(setting, result);
}

int va_memfrag_call_return(va_memfrag_return_func fn, bool isAsync, char** taskId, uint32_t* taskIdLen)
{
    if (fn == NULL) return -1;
    return fn(isAsync, taskId, taskIdLen);
}

int va_memfrag_call_rollback(va_memfrag_rollback_func fn, const RollbackSrcParam* param)
{
    if (fn == NULL) return -1;
    return fn(param);
}

int va_memfrag_call_sync_task_query(va_memfrag_sync_task_query_func fn,
                                    char* taskId,
                                    uint32_t taskIdLen,
                                    async_task_info_c* result)
{
    if (fn == NULL) return -1;
    return fn(taskId, taskIdLen, result);
}

int va_memfrag_call_borrow(va_memfrag_borrow_func fn,
                           const mem_borrow_param_s* param,
                           bool isAsync,
                           mem_borrow_result_s* result)
{
    if (fn == NULL) return -1;
    return fn(param, isAsync, result);
}

int va_memfrag_call_heartbeat(va_memfrag_heartbeat_func fn)
{
    if (fn == NULL) return -1;
    return fn();
}
*/
import "C"
import (
	"fmt"
	"unsafe"

	"atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/dlopen"
)

// Go-side constants kept in sync with the C macros in the cgo preamble
const (
	maxNodeIDLen      = int(C.VIRT_MEM_MAX_NODE_ID_LENGTH)
	maxPodIdLen       = int(C.VIRT_MEM_MAX_POD_ID_LENGTH)
	maxHostNameLen    = int(C.UBS_VA_HOST_NAME_MAX)
	maxUuidLen        = int(C.VIRT_MAX_UUID_LENGTH)
	maxNameLen        = int(C.VIRT_MAX_NAME_LENGTH)
	maxStateLen       = int(C.VIRT_MAX_STATE_LENGTH)
	maxTaskIdLen      = int(C.MEM_TASK_ID_MAX)
	maxNumaNum        = int(C.MAX_NUMA_NUM)
	maxVmNum          = int(C.MAX_VM_NUM)
	maxNodeNum        = int(C.MAX_NODE_NUM)
	maxBorrowIdCount  = int(C.MAX_BORROW_ID_COUNT)
	maxBorrowIdLength = int(C.MAX_BORROW_ID_LENGTH)
	maxDestParamSize  = int(C.MAX_DEST_PARAM_SIZE)
	maxDestNumaNum    = int(C.MAX_DEST_NUMA_NUM)
)

type NumaPageData struct {
	PageSize      uint64
	HugePageTotal uint64
	HugePageFree  uint64
}

type NumaInfo struct {
	Timestamp    int64
	NodeId       string
	HostName     string
	NumaId       int16
	SocketId     int16
	IsLocal      bool
	MemTotal     uint64 // kB
	MemFree      uint64 // kB
	HugePageData []NumaPageData
}

// VmNumaInfo VM NUMA memory information
type VmNumaInfo struct {
	NumaId   int16
	SocketId int16
	IsLocal  bool
	PageSize uint64
	UsedMem  int64
}

type VmMetaData struct {
	NodeId       string
	HostName     string
	Uuid         string
	Name         string
	State        string
	VmCreateTime int64
	MaxMem       uint64
	Pid          int32
}

// VmDomainInfo VM information
type VmDomainInfo struct {
	Timestamp int64
	MetaData  VmMetaData
	NumaInfo  []VmNumaInfo
}

// DstNumaInfo NUMA information of the borrow destination node
type DstNumaInfo struct {
	HostId   string
	SocketId uint16
	NumaIds  []int32
	MemSizes []uint64 // 各NUMA借用大小，单位kB
}

// BorrowStrategy memory borrow strategy
type BorrowStrategy struct {
	SrcHostId     string
	SrcSocketId   int16
	SrcNumaId     int16
	BorrowSize    uint64 // 借用大小，单位kB
	DestNumaInfos []DstNumaInfo
}

// BorrowSetting input parameters for memory borrow execution
type BorrowSetting struct {
	BorrowStrategy BorrowStrategy
	IsAsync        bool
	PageSize       uint64 // kB
}

// MemBorrowResult memory borrow result
type MemBorrowResult struct {
	BorrowIds      []string
	PresentNumaIds []uint16
	TaskId         string
}

type AsyncTaskStatus int32

const (
	AsyncTaskNotExist AsyncTaskStatus = iota
	AsyncTaskRunning
	AsyncTaskSuccess
	AsyncTaskFailed
)

type AsyncTaskInfo struct {
	TaskId          string
	Status          AsyncTaskStatus
	ResultCode      uint32
	MemBorrowResult MemBorrowResult
}

// RollbackParam input parameters for borrowed memory rollback
type RollbackParam struct {
	NodeId       string
	BorrowIdList []string
}

// NodeInfo cluster node information (including the node's NUMA memory information)
type NodeInfo struct {
	NodeId    string
	NumaInfos []NumaInfo
	IsCurrent bool
	IsMaster  bool
	PodId     string // super pod ID (second-level super-node ID), empty when unavailable
}

// VmNodeInfo cluster node information (including the node's VM information)
type VmNodeInfo struct {
	NodeId    string
	VmInfos   []VmDomainInfo
	IsCurrent bool
	IsMaster  bool
	PodId     string // super pod ID (second-level super-node ID), empty when unavailable
}

// NumaMetaInfo borrower-side NUMA meta information
type NumaMetaInfo struct {
	NumaId int16
}

// MemBorrowParam input parameters for large-VM memory borrow
type MemBorrowParam struct {
	SrcNid        string
	NumaMetaInfos []NumaMetaInfo
	BorrowSize    uint64 // MB
	PageSize      uint64 // kB
}

// setCString copies a Go string into a fixed-length C char array, truncating if too long
func setCString(dst unsafe.Pointer, dstLen int, src string) {
	arr := unsafe.Slice((*byte)(dst), dstLen)
	for i := range arr {
		arr[i] = 0
	}
	b := []byte(src)
	if len(b) > dstLen-1 {
		b = b[:dstLen-1]
	}
	copy(arr, b)
}

// cStringN reads a fixed-length C char array as a Go string (stops at NUL)
func cStringN(src unsafe.Pointer, maxLen int) string {
	arr := unsafe.Slice((*byte)(src), maxLen)
	n := 0
	for n < maxLen && arr[n] != 0 {
		n++
	}
	return string(arr[:n])
}

// cPodIdC reads a C-side NUL-terminated pod_id buffer (pointer + length form); nil means empty
func cPodIdC(src *C.char, srcLen int) string {
	if src == nil || srcLen <= 0 {
		return ""
	}
	if srcLen > maxPodIdLen {
		srcLen = maxPodIdLen
	}
	return cStringN(unsafe.Pointer(src), srcLen)
}

// convertNumaInfoC converts a C numa_info_t to a Go struct (read-only, does not free memory)
func convertNumaInfoC(ci *C.numa_info_t) NumaInfo {
	info := NumaInfo{
		Timestamp: int64(ci.timestamp),
		NodeId:    cStringN(unsafe.Pointer(&ci.node_id[0]), maxNodeIDLen),
		HostName:  cStringN(unsafe.Pointer(&ci.host_name[0]), maxHostNameLen),
		NumaId:    int16(ci.numa_id),
		SocketId:  int16(ci.socket_id),
		IsLocal:   ci.is_local != 0,
		MemTotal:  uint64(ci.mem_total),
		MemFree:   uint64(ci.mem_free),
	}
	pageCount := int(ci.numaPageInfoCount)
	if pageCount > 0 && ci.huge_page_data != nil {
		pages := unsafe.Slice(ci.huge_page_data, pageCount)
		for i := range pages {
			info.HugePageData = append(info.HugePageData, NumaPageData{
				PageSize:      uint64(pages[i].pageSize),
				HugePageTotal: uint64(pages[i].hugePageTotal),
				HugePageFree:  uint64(pages[i].hugePageFree),
			})
		}
	}
	return info
}

// convertMemBorrowResultC converts a C mem_borrow_result_c to a Go struct
func convertMemBorrowResultC(cr *C.mem_borrow_result_c) MemBorrowResult {
	result := MemBorrowResult{
		TaskId: cStringN(unsafe.Pointer(&cr.task_id[0]), maxTaskIdLen),
	}
	idCount := int(cr.borrow_ids_size)
	if idCount > maxBorrowIdCount {
		idCount = maxBorrowIdCount
	}
	for i := 0; i < idCount; i++ {
		result.BorrowIds = append(result.BorrowIds,
			cStringN(unsafe.Pointer(&cr.borrow_ids_ptr[i][0]), maxBorrowIdLength))
	}
	numaCount := int(cr.present_numa_ids_size)
	if numaCount > maxBorrowIdCount {
		numaCount = maxBorrowIdCount
	}
	for i := 0; i < numaCount; i++ {
		result.PresentNumaIds = append(result.PresentNumaIds, uint16(cr.present_numa_ids_ptr[i]))
	}
	return result
}

// buildBorrowSettingC converts a Go BorrowSetting to a C borrow_setting_c
func buildBorrowSettingC(setting BorrowSetting) C.borrow_setting_c {
	var cSetting C.borrow_setting_c
	strategy := &cSetting.borrow_strategy
	setCString(unsafe.Pointer(&strategy.src_host_id[0]), maxNodeIDLen, setting.BorrowStrategy.SrcHostId)
	strategy.src_socket_id = C.int16_t(setting.BorrowStrategy.SrcSocketId)
	strategy.src_numa_id = C.int16_t(setting.BorrowStrategy.SrcNumaId)
	strategy.borrow_size = C.uint64_t(setting.BorrowStrategy.BorrowSize)

	destCount := len(setting.BorrowStrategy.DestNumaInfos)
	if destCount > maxDestParamSize {
		destCount = maxDestParamSize
	}
	for i := 0; i < destCount; i++ {
		dst := &strategy.dest_numa_infos[i]
		di := &setting.BorrowStrategy.DestNumaInfos[i]
		setCString(unsafe.Pointer(&dst.host_id[0]), maxNodeIDLen, di.HostId)
		dst.socket_id = C.uint16_t(di.SocketId)
		numaCount := len(di.NumaIds)
		if numaCount > len(di.MemSizes) {
			numaCount = len(di.MemSizes)
		}
		if numaCount > maxDestNumaNum {
			numaCount = maxDestNumaNum
		}
		dst.numa_nums = C.uint16_t(numaCount)
		for j := 0; j < numaCount; j++ {
			dst.numa_ids[j] = C.int(di.NumaIds[j])
			dst.mem_sizes[j] = C.uint64_t(di.MemSizes[j])
		}
	}
	strategy.dest_numa_infos_size = C.uint32_t(destCount)
	cSetting.isAsync = C.bool(setting.IsAsync)
	cSetting.page_size = C.uint64_t(setting.PageSize)
	return cSetting
}

// buildRollbackParamC converts a Go RollbackParam to a C RollbackSrcParam
func buildRollbackParamC(param RollbackParam) C.RollbackSrcParam {
	var cParam C.RollbackSrcParam
	setCString(unsafe.Pointer(&cParam.node_id[0]), maxNodeIDLen, param.NodeId)
	idCount := len(param.BorrowIdList)
	if idCount > maxBorrowIdCount {
		idCount = maxBorrowIdCount
	}
	for i := 0; i < idCount; i++ {
		setCString(unsafe.Pointer(&cParam.borrow_id_list[i][0]), maxBorrowIdLength, param.BorrowIdList[i])
	}
	cParam.borrow_id_size = C.uint32_t(idCount)
	return cParam
}

// buildMemBorrowParamC converts a Go MemBorrowParam to a C mem_borrow_param_s.
// If NUMA meta information is present, memory is allocated on the C side and a
// cleanup function is returned for the caller to defer.
func buildMemBorrowParamC(param MemBorrowParam) (C.mem_borrow_param_s, func(), error) {
	var cParam C.mem_borrow_param_s
	setCString(unsafe.Pointer(&cParam.src_nid[0]), maxNodeIDLen, param.SrcNid)
	cParam.borrow_size = C.uint64_t(param.BorrowSize)
	cParam.page_size = C.uint64_t(param.PageSize)

	numaCount := len(param.NumaMetaInfos)
	if numaCount > maxNumaNum {
		numaCount = maxNumaNum
	}
	cleanup := func() {}
	if numaCount > 0 {
		var metaSize C.numa_meta_info_s
		cMetas := (*C.numa_meta_info_s)(C.calloc(C.size_t(numaCount), C.size_t(unsafe.Sizeof(metaSize))))
		if cMetas == nil {
			return cParam, cleanup, fmt.Errorf("failed to allocate memory for numa_meta_infos")
		}
		cleanup = func() {
			C.free(unsafe.Pointer(cMetas))
		}
		metas := unsafe.Slice(cMetas, numaCount)
		for i := range metas {
			metas[i].numa_id = C.int16_t(param.NumaMetaInfos[i].NumaId)
		}
		cParam.numa_meta_infos = cMetas
		cParam.numa_len = C.uint32_t(numaCount)
	}
	return cParam, cleanup, nil
}

// UbsMemFragmentationNodeInfo queries NUMA memory fragmentation information of the local node.
// C API: ubs_virt_agent_mem_fragmentation_node_info
func UbsMemFragmentationNodeInfo() ([]NumaInfo, error) {
	funcPtr, err := dlopen.LoadFunc("ubs_virt_agent_mem_fragmentation_node_info")
	if err != nil {
		return nil, fmt.Errorf("failed to load symbol: %v", err)
	}
	fn := (C.va_memfrag_node_info_func)(funcPtr)

	var cList *C.numa_info_t
	var cCnt C.uint32_t
	ret := C.va_memfrag_call_node_info(fn, &cList, &cCnt)
	if ret != 0 {
		return nil, fmt.Errorf("ubs_virt_agent_mem_fragmentation_node_info failed: %d", int(ret))
	}
	count := int(cCnt)
	if cList == nil || count <= 0 {
		return []NumaInfo{}, nil
	}
	defer C.free(unsafe.Pointer(cList))
	if count > maxNumaNum {
		return nil, fmt.Errorf("invalid numa info count returned from C: %d", count)
	}

	cInfos := unsafe.Slice(cList, count)
	infos := make([]NumaInfo, 0, count)
	for i := range cInfos {
		infos = append(infos, convertNumaInfoC(&cInfos[i]))
		if cInfos[i].huge_page_data != nil {
			C.free(unsafe.Pointer(cInfos[i].huge_page_data))
		}
	}
	return infos, nil
}

// UbsMemFragmentationVmInfo queries VM information of the local node.
// C API: ubs_virt_agent_mem_fragmentation_vm_info
func UbsMemFragmentationVmInfo() ([]VmDomainInfo, error) {
	funcPtr, err := dlopen.LoadFunc("ubs_virt_agent_mem_fragmentation_vm_info")
	if err != nil {
		return nil, fmt.Errorf("failed to load symbol: %v", err)
	}
	fn := (C.va_memfrag_vm_info_func)(funcPtr)

	var cList *C.vm_domain_info_t
	var cCnt C.uint32_t
	ret := C.va_memfrag_call_vm_info(fn, &cList, &cCnt)
	if ret != 0 {
		return nil, fmt.Errorf("ubs_virt_agent_mem_fragmentation_vm_info failed: %d", int(ret))
	}
	count := int(cCnt)
	if cList == nil || count <= 0 {
		return []VmDomainInfo{}, nil
	}
	defer C.free(unsafe.Pointer(cList))
	if count > maxVmNum {
		return nil, fmt.Errorf("invalid vm info count returned from C: %d", count)
	}

	cVms := unsafe.Slice(cList, count)
	vms := make([]VmDomainInfo, 0, count)
	for i := range cVms {
		cv := &cVms[i]
		vms = append(vms, convertVmDomainInfoC(cv))
		if cv.numaInfoCount > 0 && cv.numaInfo != nil {
			C.free(unsafe.Pointer(cv.numaInfo))
		}
	}
	return vms, nil
}

// convertVmDomainInfoC converts a C vm_domain_info_t to a Go struct (does not free C memory; the caller is responsible for freeing)
func convertVmDomainInfoC(cv *C.vm_domain_info_t) VmDomainInfo {
	vm := VmDomainInfo{
		Timestamp: int64(cv.timestamp),
		MetaData: VmMetaData{
			NodeId:       cStringN(unsafe.Pointer(&cv.metadata.nodeId[0]), maxNodeIDLen),
			HostName:     cStringN(unsafe.Pointer(&cv.metadata.hostName[0]), maxHostNameLen),
			Uuid:         cStringN(unsafe.Pointer(&cv.metadata.uuid[0]), maxUuidLen),
			Name:         cStringN(unsafe.Pointer(&cv.metadata.name[0]), maxNameLen),
			State:        cStringN(unsafe.Pointer(&cv.metadata.state[0]), maxStateLen),
			VmCreateTime: int64(cv.metadata.vmCreateTime),
			MaxMem:       uint64(cv.metadata.maxMem),
			Pid:          int32(cv.metadata.pid),
		},
	}
	numaCount := int(cv.numaInfoCount)
	if numaCount > 0 && cv.numaInfo != nil {
		if numaCount > maxNumaNum {
			numaCount = maxNumaNum
		}
		cNumas := unsafe.Slice(cv.numaInfo, numaCount)
		for j := range cNumas {
			vm.NumaInfo = append(vm.NumaInfo, VmNumaInfo{
				NumaId:   int16(cNumas[j].numaId),
				SocketId: int16(cNumas[j].socketId),
				IsLocal:  bool(cNumas[j].isLocal),
				PageSize: uint64(cNumas[j].pageSize),
				UsedMem:  int64(cNumas[j].usedMem),
			})
		}
	}
	return vm
}

// freeNodeInfoListC frees all C-side memory in a node_info_list_s (three levels: node array, NUMA array, huge page array)
func freeNodeInfoListC(cList *C.node_info_list_s) {
	if cList.node_infos == nil || cList.node_len == 0 {
		return
	}
	nodes := unsafe.Slice(cList.node_infos, int(cList.node_len))
	for i := range nodes {
		if nodes[i].pod_id != nil {
			C.free(unsafe.Pointer(nodes[i].pod_id))
		}
		if nodes[i].numa_infos == nil || nodes[i].numa_len == 0 {
			continue
		}
		numas := unsafe.Slice(nodes[i].numa_infos, int(nodes[i].numa_len))
		for j := range numas {
			if numas[j].numaPageInfoCount > 0 && numas[j].huge_page_data != nil {
				C.free(unsafe.Pointer(numas[j].huge_page_data))
			}
		}
		C.free(unsafe.Pointer(nodes[i].numa_infos))
	}
	C.free(unsafe.Pointer(cList.node_infos))
	cList.node_infos = nil
	cList.node_len = 0
}

// UbsMemFragmentationNodeInfoList queries memory information of all nodes in the cluster.
// C API: ubs_virt_agent_mem_fragmentation_node_info_list
func UbsMemFragmentationNodeInfoList() ([]NodeInfo, error) {
	funcPtr, err := dlopen.LoadFunc("ubs_virt_agent_mem_fragmentation_node_info_list")
	if err != nil {
		return nil, fmt.Errorf("failed to load symbol: %v", err)
	}
	fn := (C.va_memfrag_node_info_list_func)(funcPtr)

	var cList C.node_info_list_s
	ret := C.va_memfrag_call_node_info_list(fn, &cList)
	if ret != 0 {
		return nil, fmt.Errorf("ubs_virt_agent_mem_fragmentation_node_info_list failed: %d", int(ret))
	}
	defer freeNodeInfoListC(&cList)
	return convertNodeInfoListC(&cList)
}

func convertNodeInfoListC(cList *C.node_info_list_s) ([]NodeInfo, error) {
	nodeCount := int(cList.node_len)
	if cList.node_infos == nil || nodeCount <= 0 {
		return []NodeInfo{}, nil
	}
	if nodeCount > maxNodeNum {
		return nil, fmt.Errorf("invalid node count returned from C: %d", nodeCount)
	}

	cNodes := unsafe.Slice(cList.node_infos, nodeCount)
	nodes := make([]NodeInfo, 0, nodeCount)
	for i := range cNodes {
		cn := &cNodes[i]
		node := NodeInfo{
			NodeId:    cStringN(unsafe.Pointer(&cn.node_id[0]), maxNodeIDLen),
			IsCurrent: bool(cn.is_current),
			IsMaster:  bool(cn.is_master),
			PodId:     cPodIdC(cn.pod_id, int(cn.pod_id_len)),
		}
		numaCount := int(cn.numa_len)
		if numaCount > 0 && cn.numa_infos != nil {
			if numaCount > maxNumaNum {
				numaCount = maxNumaNum
			}
			cNumas := unsafe.Slice(cn.numa_infos, numaCount)
			for j := range cNumas {
				node.NumaInfos = append(node.NumaInfos, convertNumaInfoC(&cNumas[j]))
			}
		}
		nodes = append(nodes, node)
	}
	return nodes, nil
}

// freeVmInfoListC frees all C-side memory in a vm_info_list_s (three levels: node array, VM array, numaInfo array)
func freeVmInfoListC(cList *C.vm_info_list_s) {
	if cList.vm_node_infos == nil || cList.node_len == 0 {
		return
	}
	nodes := unsafe.Slice(cList.vm_node_infos, int(cList.node_len))
	for i := range nodes {
		if nodes[i].pod_id != nil {
			C.free(unsafe.Pointer(nodes[i].pod_id))
		}
		if nodes[i].vm_infos == nil || nodes[i].vm_len == 0 {
			continue
		}
		vms := unsafe.Slice(nodes[i].vm_infos, int(nodes[i].vm_len))
		for j := range vms {
			if vms[j].numaInfoCount > 0 && vms[j].numaInfo != nil {
				C.free(unsafe.Pointer(vms[j].numaInfo))
			}
		}
		C.free(unsafe.Pointer(nodes[i].vm_infos))
	}
	C.free(unsafe.Pointer(cList.vm_node_infos))
	cList.vm_node_infos = nil
	cList.node_len = 0
}

// UbsMemFragmentationVmInfoList queries VM information of all nodes in the cluster.
// C API: ubs_virt_agent_mem_fragmentation_vm_info_list
func UbsMemFragmentationVmInfoList() ([]VmNodeInfo, error) {
	funcPtr, err := dlopen.LoadFunc("ubs_virt_agent_mem_fragmentation_vm_info_list")
	if err != nil {
		return nil, fmt.Errorf("failed to load symbol: %v", err)
	}
	fn := (C.va_memfrag_vm_info_list_func)(funcPtr)

	var cList C.vm_info_list_s
	ret := C.va_memfrag_call_vm_info_list(fn, &cList)
	if ret != 0 {
		return nil, fmt.Errorf("ubs_virt_agent_mem_fragmentation_vm_info_list failed: %d", int(ret))
	}
	defer freeVmInfoListC(&cList)
	return convertVmInfoListC(&cList)
}

func convertVmInfoListC(cList *C.vm_info_list_s) ([]VmNodeInfo, error) {
	nodeCount := int(cList.node_len)
	if cList.vm_node_infos == nil || nodeCount <= 0 {
		return []VmNodeInfo{}, nil
	}
	if nodeCount > maxNodeNum {
		return nil, fmt.Errorf("invalid node count returned from C: %d", nodeCount)
	}

	cNodes := unsafe.Slice(cList.vm_node_infos, nodeCount)
	nodes := make([]VmNodeInfo, 0, nodeCount)
	for i := range cNodes {
		cn := &cNodes[i]
		node := VmNodeInfo{
			NodeId:    cStringN(unsafe.Pointer(&cn.node_id[0]), maxNodeIDLen),
			IsCurrent: bool(cn.is_current),
			IsMaster:  bool(cn.is_master),
			PodId:     cPodIdC(cn.pod_id, int(cn.pod_id_len)),
		}
		vmCount := int(cn.vm_len)
		if vmCount > 0 && cn.vm_infos != nil {
			if vmCount > maxVmNum {
				vmCount = maxVmNum
			}
			cVms := unsafe.Slice(cn.vm_infos, vmCount)
			for j := range cVms {
				node.VmInfos = append(node.VmInfos, convertVmDomainInfoC(&cVms[j]))
			}
		}
		nodes = append(nodes, node)
	}
	return nodes, nil
}

// UbsMemFragmentationHeartbeat detects whether the local ubse service is alive.
// C API: ubs_virt_agent_mem_fragmentation_heartbeat
func UbsMemFragmentationHeartbeat() error {
	funcPtr, err := dlopen.LoadFunc("ubs_virt_agent_mem_fragmentation_heartbeat")
	if err != nil {
		return fmt.Errorf("failed to load symbol: %v", err)
	}
	fn := (C.va_memfrag_heartbeat_func)(funcPtr)

	ret := C.va_memfrag_call_heartbeat(fn)
	if ret != 0 {
		return fmt.Errorf("ubs_virt_agent_mem_fragmentation_heartbeat failed: %d", int(ret))
	}
	return nil
}

// UbsMemBorrowExecute executes a memory borrow according to the borrow strategy and returns the borrow result.
// C API: ubs_virt_agent_mem_borrow_execute
func UbsMemBorrowExecute(setting BorrowSetting) (MemBorrowResult, error) {
	cSetting := buildBorrowSettingC(setting)

	funcPtr, err := dlopen.LoadFunc("ubs_virt_agent_mem_borrow_execute")
	if err != nil {
		return MemBorrowResult{}, fmt.Errorf("failed to load symbol: %v", err)
	}
	fn := (C.va_memfrag_borrow_execute_func)(funcPtr)

	var cResult C.mem_borrow_result_c
	ret := C.va_memfrag_call_borrow_execute(fn, &cSetting, &cResult)
	if ret != 0 {
		return MemBorrowResult{}, fmt.Errorf("ubs_virt_agent_mem_borrow_execute failed: %d", int(ret))
	}
	return convertMemBorrowResultC(&cResult), nil
}

// UbsMemReturn returns the borrowed memory; when executed asynchronously it returns a background
// task ID, whose status can be queried via UbsSyncTaskQuery.
// C API: ubs_virt_agent_mem_return
func UbsMemReturn(isAsync bool) (string, error) {
	funcPtr, err := dlopen.LoadFunc("ubs_virt_agent_mem_return")
	if err != nil {
		return "", fmt.Errorf("failed to load symbol: %v", err)
	}
	fn := (C.va_memfrag_return_func)(funcPtr)

	var cTaskId *C.char
	var cTaskIdLen C.uint32_t
	ret := C.va_memfrag_call_return(fn, C.bool(isAsync), &cTaskId, &cTaskIdLen)
	if ret != 0 {
		return "", fmt.Errorf("ubs_virt_agent_mem_return failed: %d", int(ret))
	}
	if cTaskId == nil {
		return "", fmt.Errorf("invalid task id returned from C")
	}
	defer C.free(unsafe.Pointer(cTaskId))
	return C.GoString(cTaskId), nil
}

// UbsMemRollback rolls back borrowed memory by node and borrow ID list.
// C API: ubs_virt_agent_mem_rollback
func UbsMemRollback(param RollbackParam) error {
	cParam := buildRollbackParamC(param)

	funcPtr, err := dlopen.LoadFunc("ubs_virt_agent_mem_rollback")
	if err != nil {
		return fmt.Errorf("failed to load symbol: %v", err)
	}
	fn := (C.va_memfrag_rollback_func)(funcPtr)

	ret := C.va_memfrag_call_rollback(fn, &cParam)
	if ret != 0 {
		return fmt.Errorf("ubs_virt_agent_mem_rollback failed: %d", int(ret))
	}
	return nil
}

// UbsSyncTaskQuery synchronously queries the status and result of a borrow/return asynchronous task.
// C API: ubs_virt_agent_sync_task_query
func UbsSyncTaskQuery(taskId string) (AsyncTaskInfo, error) {
	if len(taskId) == 0 || len(taskId) >= maxTaskIdLen {
		return AsyncTaskInfo{}, fmt.Errorf("invalid task id length: %d", len(taskId))
	}
	cTaskId := C.CString(taskId)
	defer C.free(unsafe.Pointer(cTaskId))

	funcPtr, err := dlopen.LoadFunc("ubs_virt_agent_sync_task_query")
	if err != nil {
		return AsyncTaskInfo{}, fmt.Errorf("failed to load symbol: %v", err)
	}
	fn := (C.va_memfrag_sync_task_query_func)(funcPtr)

	var cResult C.async_task_info_c
	ret := C.va_memfrag_call_sync_task_query(fn, cTaskId, C.uint32_t(len(taskId)), &cResult)
	if ret != 0 {
		return AsyncTaskInfo{}, fmt.Errorf("ubs_virt_agent_sync_task_query failed: %d", int(ret))
	}
	return AsyncTaskInfo{
		TaskId:          cStringN(unsafe.Pointer(&cResult.task_id[0]), maxTaskIdLen),
		Status:          AsyncTaskStatus(cResult.status),
		ResultCode:      uint32(cResult.resultCode),
		MemBorrowResult: convertMemBorrowResultC(&cResult.memBorrowResult),
	}, nil
}

// UbsMemBorrow borrows memory by node for large-VM scenarios and returns the borrow result list.
// C API: ubs_virt_agent_mem_borrow
func UbsMemBorrow(param MemBorrowParam, isAsync bool) ([]MemBorrowResult, error) {
	cParam, cleanup, err := buildMemBorrowParamC(param)
	if err != nil {
		return nil, err
	}
	defer cleanup()

	funcPtr, err := dlopen.LoadFunc("ubs_virt_agent_mem_borrow")
	if err != nil {
		return nil, fmt.Errorf("failed to load symbol: %v", err)
	}
	fn := (C.va_memfrag_borrow_func)(funcPtr)

	var cResult C.mem_borrow_result_s
	ret := C.va_memfrag_call_borrow(fn, &cParam, C.bool(isAsync), &cResult)
	if ret != 0 {
		return nil, fmt.Errorf("ubs_virt_agent_mem_borrow failed: %d", int(ret))
	}
	defer C.free(unsafe.Pointer(cResult.mem_borrow_result_list))

	resultCount := int(cResult.mem_borrow_result_list_len)
	if resultCount <= 0 || cResult.mem_borrow_result_list == nil {
		return []MemBorrowResult{}, nil
	}
	cResults := unsafe.Slice(cResult.mem_borrow_result_list, resultCount)
	results := make([]MemBorrowResult, 0, resultCount)
	for i := range cResults {
		results = append(results, convertMemBorrowResultC(&cResults[i]))
	}
	return results, nil
}
