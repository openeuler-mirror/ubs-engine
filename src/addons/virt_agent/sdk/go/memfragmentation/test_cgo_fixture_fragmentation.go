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

// This file provides C-side data structure fixtures for the memory fragmentation
// unit tests. The Go toolchain forbids cgo in _test.go files
// (use of cgo in test not supported), so the logic that builds/reads C structs
// must live in an ordinary .go file compiled together with the product; test
// files call it only through pure Go signatures.
// Note: cgo preamble declarations are not shared across files — each cgo file's
// C references are resolved against its own preamble. The struct declarations
// copied here on demand must therefore stay exactly identical to the
// memfragmentation.go preamble (same-named C types in one package unify into a
// single Go type; mismatched declarations break cross-file parameter passing).
// Function definitions from the preamble must not be copied either, otherwise
// the two definitions produce duplicate symbols at link time.

package memfragmentation

/*
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#define VIRT_MEM_MAX_NODE_ID_LENGTH 48
#define VIRT_MEM_MAX_POD_ID_LENGTH 48
#define UBS_VA_HOST_NAME_MAX 64
#define VIRT_MAX_UUID_LENGTH 37
#define VIRT_MAX_NAME_LENGTH 128
#define VIRT_MAX_STATE_LENGTH 128
#define MEM_TASK_ID_MAX 256
#define MAX_BORROW_ID_COUNT 100
#define MAX_BORROW_ID_LENGTH 128

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
    char borrow_ids_ptr[MAX_BORROW_ID_COUNT][MAX_BORROW_ID_LENGTH];
    uint16_t present_numa_ids_ptr[MAX_BORROW_ID_COUNT];
    uint32_t borrow_ids_size;
    uint32_t present_numa_ids_size;
    char task_id[MEM_TASK_ID_MAX];
} mem_borrow_result_c;

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
*/
import "C"
import (
	"unsafe"
)

// borrowSettingDestView C-side view of a single destination-node entry in a borrow strategy
type borrowSettingDestView struct {
	HostID   string
	SocketID uint16
	NumaNums uint16
	NumaIDs  []int32
	MemSizes []uint64
}

// borrowSettingView Go-side view of borrow_setting_c, exposing only the fields the unit tests care about
type borrowSettingView struct {
	SrcHostID   string
	SrcSocketID int16
	SrcNumaID   int16
	BorrowSize  uint64
	IsAsync     bool
	DestInfos   []borrowSettingDestView
}

// rollbackParamView Go-side view of RollbackSrcParam
type rollbackParamView struct {
	NodeID       string
	BorrowIDSize uint32
	BorrowIDs    []string
}

// memBorrowParamView Go-side view of mem_borrow_param_s; nil NumaIDs means no C-side memory was allocated
type memBorrowParamView struct {
	SrcNid     string
	BorrowSize uint64
	NumaLen    uint32
	NumaIDs    []int16
}

// numaInfoConvertForTest builds a C-side numa_info_t fixture and converts it to
// a Go struct; empty hugePages covers the nil huge-page-pointer branch
func numaInfoConvertForTest(hugePages []NumaPageData) NumaInfo {
	var ci C.numa_info_t
	ci.timestamp = 1700000000
	setCString(unsafe.Pointer(&ci.node_id[0]), maxNodeIDLen, "node-01")
	setCString(unsafe.Pointer(&ci.host_name[0]), maxHostNameLen, "host-01")
	ci.numa_id = 3
	ci.socket_id = 1
	ci.is_local = 1
	ci.mem_total = 102400
	ci.mem_free = 51200
	if len(hugePages) > 0 {
		var pageSize C.numa_page_data
		cPages := (*C.numa_page_data)(C.calloc(C.size_t(len(hugePages)), C.size_t(unsafe.Sizeof(pageSize))))
		if cPages == nil {
			return NumaInfo{}
		}
		defer C.free(unsafe.Pointer(cPages))
		pages := unsafe.Slice(cPages, len(hugePages))
		for i, p := range hugePages {
			pages[i].pageSize = C.uint64_t(p.PageSize)
			pages[i].hugePageTotal = C.uint64_t(p.HugePageTotal)
			pages[i].hugePageFree = C.uint64_t(p.HugePageFree)
		}
		ci.huge_page_data = cPages
		ci.numaPageInfoCount = C.uint64_t(len(hugePages))
	}
	return convertNumaInfoC(&ci)
}

// vmDomainInfoConvertForTest builds a C-side vm_domain_info_t fixture and
// converts it to a Go struct; empty numas covers the nil numaInfo-pointer branch
func vmDomainInfoConvertForTest(numas []VmNumaInfo) VmDomainInfo {
	var cv C.vm_domain_info_t
	cv.timestamp = 1700000000
	setCString(unsafe.Pointer(&cv.metadata.nodeId[0]), maxNodeIDLen, "node-01")
	setCString(unsafe.Pointer(&cv.metadata.hostName[0]), maxHostNameLen, "host-01")
	setCString(unsafe.Pointer(&cv.metadata.uuid[0]), maxUuidLen, "11111111-2222-3333-4444-555555555555")
	setCString(unsafe.Pointer(&cv.metadata.name[0]), maxNameLen, "vm-001")
	setCString(unsafe.Pointer(&cv.metadata.state[0]), maxStateLen, "running")
	cv.metadata.vmCreateTime = 1699999999
	cv.metadata.maxMem = 8388608
	cv.metadata.pid = 12345
	if len(numas) > 0 {
		var numaSize C.vm_numa_info_t
		cNumas := (*C.vm_numa_info_t)(C.calloc(C.size_t(len(numas)), C.size_t(unsafe.Sizeof(numaSize))))
		if cNumas == nil {
			return VmDomainInfo{}
		}
		defer C.free(unsafe.Pointer(cNumas))
		entries := unsafe.Slice(cNumas, len(numas))
		for i, n := range numas {
			entries[i].numaId = C.int16_t(n.NumaId)
			entries[i].socketId = C.int16_t(n.SocketId)
			entries[i].isLocal = C.bool(n.IsLocal)
			entries[i].pageSize = C.uint64_t(n.PageSize)
			entries[i].usedMem = C.int64_t(n.UsedMem)
		}
		cv.numaInfo = cNumas
		cv.numaInfoCount = C.uint64_t(len(numas))
	}
	return convertVmDomainInfoC(&cv)
}

// memBorrowResultConvertForTest builds a C-side mem_borrow_result_c fixture and
// converts it to a Go struct; borrowIDSize/presentNumaIDSize may differ from the
// slice lengths to verify the over-limit clamping branches
func memBorrowResultConvertForTest(taskID string, borrowIDs []string, presentNumaIDs []uint16,
	borrowIDSize, presentNumaIDSize uint32) MemBorrowResult {
	var cr C.mem_borrow_result_c
	setCString(unsafe.Pointer(&cr.task_id[0]), maxTaskIdLen, taskID)
	for i, id := range borrowIDs {
		setCString(unsafe.Pointer(&cr.borrow_ids_ptr[i][0]), maxBorrowIdLength, id)
	}
	for i, n := range presentNumaIDs {
		cr.present_numa_ids_ptr[i] = C.uint16_t(n)
	}
	cr.borrow_ids_size = C.uint32_t(borrowIDSize)
	cr.present_numa_ids_size = C.uint32_t(presentNumaIDSize)
	return convertMemBorrowResultC(&cr)
}

// borrowSettingViewForTest calls buildBorrowSettingC to build the C input, then reads its content back as a Go view
func borrowSettingViewForTest(setting BorrowSetting) borrowSettingView {
	c := buildBorrowSettingC(setting)
	st := &c.borrow_strategy
	view := borrowSettingView{
		SrcHostID:   cStringN(unsafe.Pointer(&st.src_host_id[0]), maxNodeIDLen),
		SrcSocketID: int16(st.src_socket_id),
		SrcNumaID:   int16(st.src_numa_id),
		BorrowSize:  uint64(st.borrow_size),
		IsAsync:     bool(c.isAsync),
	}
	destCount := int(st.dest_numa_infos_size)
	if destCount > 0 {
		view.DestInfos = make([]borrowSettingDestView, 0, destCount)
		for i := 0; i < destCount; i++ {
			dst := &st.dest_numa_infos[i]
			dv := borrowSettingDestView{
				HostID:   cStringN(unsafe.Pointer(&dst.host_id[0]), maxNodeIDLen),
				SocketID: uint16(dst.socket_id),
				NumaNums: uint16(dst.numa_nums),
			}
			for j := 0; j < int(dst.numa_nums); j++ {
				dv.NumaIDs = append(dv.NumaIDs, int32(dst.numa_ids[j]))
				dv.MemSizes = append(dv.MemSizes, uint64(dst.mem_sizes[j]))
			}
			view.DestInfos = append(view.DestInfos, dv)
		}
	}
	return view
}

// rollbackParamViewForTest calls buildRollbackParamC to build the C input, then reads its content back as a Go view
func rollbackParamViewForTest(param RollbackParam) rollbackParamView {
	c := buildRollbackParamC(param)
	view := rollbackParamView{
		NodeID:       cStringN(unsafe.Pointer(&c.node_id[0]), maxNodeIDLen),
		BorrowIDSize: uint32(c.borrow_id_size),
	}
	for i := 0; i < int(c.borrow_id_size); i++ {
		view.BorrowIDs = append(view.BorrowIDs,
			cStringN(unsafe.Pointer(&c.borrow_id_list[i][0]), maxBorrowIdLength))
	}
	return view
}

// memBorrowParamViewForTest calls buildMemBorrowParamC to build the C input
// (including allocation and cleanup), then reads its content back as a Go view
func memBorrowParamViewForTest(param MemBorrowParam) (memBorrowParamView, error) {
	c, cleanup, err := buildMemBorrowParamC(param)
	if err != nil {
		return memBorrowParamView{}, err
	}
	defer cleanup()
	view := memBorrowParamView{
		SrcNid:     cStringN(unsafe.Pointer(&c.src_nid[0]), maxNodeIDLen),
		BorrowSize: uint64(c.borrow_size),
		NumaLen:    uint32(c.numa_len),
	}
	if c.numa_meta_infos != nil {
		metas := unsafe.Slice(c.numa_meta_infos, int(c.numa_len))
		for i := range metas {
			view.NumaIDs = append(view.NumaIDs, int16(metas[i].numa_id))
		}
	}
	return view, nil
}

type numaSpec struct {
	NumaId    int16
	SocketId  int16
	IsLocal   bool
	MemTotal  uint64
	MemFree   uint64
	Timestamp int64
	HostName  string
	HugePages []NumaPageData
}

type nodeInfoSpec struct {
	NodeId    string
	IsCurrent bool
	IsMaster  bool
	PodId     string
	Numas     []numaSpec
}

// newPodIdBuffer mallocs a NUL-terminated pod_id buffer matching the C ABI (empty string -> nullptr + 0)
func newPodIdBuffer(podId string) (*C.char, C.uint32_t) {
	if podId == "" {
		return nil, 0
	}
	if len(podId) >= maxPodIdLen {
		podId = podId[:maxPodIdLen-1]
	}
	return C.CString(podId), C.uint32_t(len(podId))
}

func setPodIdNodeC(n *C.node_info_s, podId string) {
	n.pod_id, n.pod_id_len = newPodIdBuffer(podId)
}

func setPodIdVmNodeC(n *C.vm_node_info_s, podId string) {
	n.pod_id, n.pod_id_len = newPodIdBuffer(podId)
}

func nodeInfoListConvertForTest(specs []nodeInfoSpec) ([]NodeInfo, error) {
	var list C.node_info_list_s
	if len(specs) == 0 {
		return convertNodeInfoListC(&list)
	}

	var nodeSize C.node_info_s
	var numaSize C.numa_info_t
	var pageSize C.numa_page_data

	nodes := (*C.node_info_s)(C.calloc(C.size_t(len(specs)), C.size_t(unsafe.Sizeof(nodeSize))))
	if nodes == nil {
		return []NodeInfo{}, nil
	}
	list.node_infos = nodes
	list.node_len = C.uint32_t(len(specs))

	nodeArr := unsafe.Slice(nodes, len(specs))
	for i, s := range specs {
		setCString(unsafe.Pointer(&nodeArr[i].node_id[0]), maxNodeIDLen, s.NodeId)
		nodeArr[i].is_current = C.bool(s.IsCurrent)
		nodeArr[i].is_master = C.bool(s.IsMaster)
		setPodIdNodeC(&nodeArr[i], s.PodId)
		if len(s.Numas) == 0 {
			continue
		}
		numas := (*C.numa_info_t)(C.calloc(C.size_t(len(s.Numas)), C.size_t(unsafe.Sizeof(numaSize))))
		if numas == nil {
			break
		}
		nodeArr[i].numa_infos = numas
		nodeArr[i].numa_len = C.uint32_t(len(s.Numas))
		numaArr := unsafe.Slice(numas, len(s.Numas))
		for j, ns := range s.Numas {
			numaArr[j].timestamp = C.int64_t(ns.Timestamp)
			setCString(unsafe.Pointer(&numaArr[j].node_id[0]), maxNodeIDLen, s.NodeId)
			setCString(unsafe.Pointer(&numaArr[j].host_name[0]), maxHostNameLen, ns.HostName)
			numaArr[j].numa_id = C.int16_t(ns.NumaId)
			numaArr[j].socket_id = C.int16_t(ns.SocketId)
			if ns.IsLocal {
				numaArr[j].is_local = 1
			}
			numaArr[j].mem_total = C.uint64_t(ns.MemTotal)
			numaArr[j].mem_free = C.uint64_t(ns.MemFree)
			if len(ns.HugePages) == 0 {
				continue
			}
			pages := (*C.numa_page_data)(C.calloc(C.size_t(len(ns.HugePages)), C.size_t(unsafe.Sizeof(pageSize))))
			if pages == nil {
				break
			}
			pageArr := unsafe.Slice(pages, len(ns.HugePages))
			for k, hp := range ns.HugePages {
				pageArr[k].pageSize = C.uint64_t(hp.PageSize)
				pageArr[k].hugePageTotal = C.uint64_t(hp.HugePageTotal)
				pageArr[k].hugePageFree = C.uint64_t(hp.HugePageFree)
			}
			numaArr[j].huge_page_data = pages
			numaArr[j].numaPageInfoCount = C.uint64_t(len(ns.HugePages))
		}
	}

	result, err := convertNodeInfoListC(&list)
	freeNodeInfoListC(&list)
	return result, err
}

func nodeInfoListOverCountForTest(nodeLen uint32) ([]NodeInfo, error) {
	var list C.node_info_list_s
	var nodeSize C.node_info_s
	nodes := (*C.node_info_s)(C.calloc(1, C.size_t(unsafe.Sizeof(nodeSize))))
	if nodes == nil {
		return nil, nil
	}
	list.node_infos = nodes
	list.node_len = C.uint32_t(nodeLen)
	result, err := convertNodeInfoListC(&list)
	C.free(unsafe.Pointer(list.node_infos))
	list.node_infos = nil
	return result, err
}

type vmNumaSpec struct {
	NumaId   int16
	SocketId int16
	IsLocal  bool
	PageSize uint64
	UsedMem  int64
}

type vmSpec struct {
	Uuid         string
	Name         string
	State        string
	HostName     string
	VmCreateTime int64
	MaxMem       uint64
	Pid          int32
	Timestamp    int64
	Numas        []vmNumaSpec
}

type vmNodeInfoSpec struct {
	NodeId    string
	IsCurrent bool
	IsMaster  bool
	PodId     string
	Vms       []vmSpec
}

func vmInfoListConvertForTest(specs []vmNodeInfoSpec) ([]VmNodeInfo, error) {
	var list C.vm_info_list_s
	if len(specs) == 0 {
		return convertVmInfoListC(&list)
	}

	var nodeSize C.vm_node_info_s
	var vmSize C.vm_domain_info_t
	var vmNumaSize C.vm_numa_info_t

	nodes := (*C.vm_node_info_s)(C.calloc(C.size_t(len(specs)), C.size_t(unsafe.Sizeof(nodeSize))))
	if nodes == nil {
		return []VmNodeInfo{}, nil
	}
	list.vm_node_infos = nodes
	list.node_len = C.uint32_t(len(specs))

	nodeArr := unsafe.Slice(nodes, len(specs))
	for i, s := range specs {
		setCString(unsafe.Pointer(&nodeArr[i].node_id[0]), maxNodeIDLen, s.NodeId)
		nodeArr[i].is_current = C.bool(s.IsCurrent)
		nodeArr[i].is_master = C.bool(s.IsMaster)
		setPodIdVmNodeC(&nodeArr[i], s.PodId)
		if len(s.Vms) == 0 {
			continue
		}
		vms := (*C.vm_domain_info_t)(C.calloc(C.size_t(len(s.Vms)), C.size_t(unsafe.Sizeof(vmSize))))
		if vms == nil {
			break
		}
		nodeArr[i].vm_infos = vms
		nodeArr[i].vm_len = C.uint32_t(len(s.Vms))
		vmArr := unsafe.Slice(vms, len(s.Vms))
		for j, vs := range s.Vms {
			vmArr[j].timestamp = C.int64_t(vs.Timestamp)
			setCString(unsafe.Pointer(&vmArr[j].metadata.nodeId[0]), maxNodeIDLen, s.NodeId)
			setCString(unsafe.Pointer(&vmArr[j].metadata.hostName[0]), maxHostNameLen, vs.HostName)
			setCString(unsafe.Pointer(&vmArr[j].metadata.uuid[0]), maxUuidLen, vs.Uuid)
			setCString(unsafe.Pointer(&vmArr[j].metadata.name[0]), maxNameLen, vs.Name)
			setCString(unsafe.Pointer(&vmArr[j].metadata.state[0]), maxStateLen, vs.State)
			vmArr[j].metadata.vmCreateTime = C.int64_t(vs.VmCreateTime)
			vmArr[j].metadata.maxMem = C.uint64_t(vs.MaxMem)
			vmArr[j].metadata.pid = C.pid_t(vs.Pid)
			if len(vs.Numas) == 0 {
				continue
			}
			numas := (*C.vm_numa_info_t)(C.calloc(C.size_t(len(vs.Numas)), C.size_t(unsafe.Sizeof(vmNumaSize))))
			if numas == nil {
				break
			}
			vmArr[j].numaInfo = numas
			vmArr[j].numaInfoCount = C.uint64_t(len(vs.Numas))
			numaArr := unsafe.Slice(numas, len(vs.Numas))
			for k, ns := range vs.Numas {
				numaArr[k].numaId = C.int16_t(ns.NumaId)
				numaArr[k].socketId = C.int16_t(ns.SocketId)
				numaArr[k].isLocal = C.bool(ns.IsLocal)
				numaArr[k].pageSize = C.uint64_t(ns.PageSize)
				numaArr[k].usedMem = C.int64_t(ns.UsedMem)
			}
		}
	}

	result, err := convertVmInfoListC(&list)
	freeVmInfoListC(&list)
	return result, err
}

func vmInfoListOverCountForTest(nodeLen uint32) ([]VmNodeInfo, error) {
	var list C.vm_info_list_s
	var nodeSize C.vm_node_info_s
	nodes := (*C.vm_node_info_s)(C.calloc(1, C.size_t(unsafe.Sizeof(nodeSize))))
	if nodes == nil {
		return nil, nil
	}
	list.vm_node_infos = nodes
	list.node_len = C.uint32_t(nodeLen)
	result, err := convertVmInfoListC(&list)
	C.free(unsafe.Pointer(list.vm_node_infos))
	list.vm_node_infos = nil
	return result, err
}

// freeNodeInfoListForTest builds a three-level node_info_list_s structure and
// frees it via freeNodeInfoListC, returning whether node_infos is nil after the
// free and the node_len value; empty=true only verifies an empty list does not
// panic
func freeNodeInfoListForTest(empty bool) (nodeInfosNil bool, nodeLen uint32) {
	var list C.node_info_list_s
	if empty {
		freeNodeInfoListC(&list)
		return list.node_infos == nil, uint32(list.node_len)
	}

	var nodeSize C.node_info_s
	var numaSize C.numa_info_t
	var pageSize C.numa_page_data

	// Build a three-level structure: 1 node → 2 NUMAs (one of them with a huge-page array)
	nodes := (*C.node_info_s)(C.calloc(1, C.size_t(unsafe.Sizeof(nodeSize))))
	if nodes == nil {
		return false, uint32(list.node_len)
	}
	list.node_infos = nodes
	list.node_len = 1

	node := unsafe.Slice(nodes, 1)
	setCString(unsafe.Pointer(&node[0].node_id[0]), maxNodeIDLen, "node-01")
	node[0].is_current = true

	numas := (*C.numa_info_t)(C.calloc(2, C.size_t(unsafe.Sizeof(numaSize))))
	if numas == nil {
		return false, uint32(list.node_len)
	}
	node[0].numa_infos = numas
	node[0].numa_len = 2

	numaArr := unsafe.Slice(numas, 2)
	pages := (*C.numa_page_data)(C.calloc(1, C.size_t(unsafe.Sizeof(pageSize))))
	if pages == nil {
		return false, uint32(list.node_len)
	}
	pages.pageSize = 2048
	numaArr[0].huge_page_data = pages
	numaArr[0].numaPageInfoCount = 1
	// numaArr[1] carries no huge-page data (calloc already zeroed it), covering the free guard branch

	freeNodeInfoListC(&list)
	return list.node_infos == nil, uint32(list.node_len)
}

// freeVmInfoListForTest builds a three-level vm_info_list_s structure and frees
// it via freeVmInfoListC, returning whether vm_node_infos is nil after the free
// and the node_len value; empty=true only verifies an empty list does not panic
func freeVmInfoListForTest(empty bool) (vmNodeInfosNil bool, nodeLen uint32) {
	var list C.vm_info_list_s
	if empty {
		freeVmInfoListC(&list)
		return list.vm_node_infos == nil, uint32(list.node_len)
	}

	var nodeSize C.vm_node_info_s
	var vmSize C.vm_domain_info_t
	var numaSize C.vm_numa_info_t

	// Build a three-level structure: 1 node → 2 VMs (one of them with a NUMA array)
	nodes := (*C.vm_node_info_s)(C.calloc(1, C.size_t(unsafe.Sizeof(nodeSize))))
	if nodes == nil {
		return false, uint32(list.node_len)
	}
	list.vm_node_infos = nodes
	list.node_len = 1

	node := unsafe.Slice(nodes, 1)
	setCString(unsafe.Pointer(&node[0].node_id[0]), maxNodeIDLen, "node-01")
	node[0].is_current = true

	vms := (*C.vm_domain_info_t)(C.calloc(2, C.size_t(unsafe.Sizeof(vmSize))))
	if vms == nil {
		return false, uint32(list.node_len)
	}
	node[0].vm_infos = vms
	node[0].vm_len = 2

	vmArr := unsafe.Slice(vms, 2)
	numas := (*C.vm_numa_info_t)(C.calloc(1, C.size_t(unsafe.Sizeof(numaSize))))
	if numas == nil {
		return false, uint32(list.node_len)
	}
	numas.numaId = 3
	vmArr[0].numaInfo = numas
	vmArr[0].numaInfoCount = 1
	// vmArr[1] carries no NUMA array (calloc already zeroed it), covering the free guard branch

	freeVmInfoListC(&list)
	return list.vm_node_infos == nil, uint32(list.node_len)
}
