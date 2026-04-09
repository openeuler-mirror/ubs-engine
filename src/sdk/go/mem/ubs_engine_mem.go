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

// package mem 实现了mem的go sdk.
package mem

/*
#cgo CFLAGS: -I/usr/include/ubse -I${SRCDIR}/../../c/include
#include <stdlib.h>
#include <netinet/in.h>
#include "ubs_engine_mem.h"

// 定义函数指针类型
typedef int32_t (*ubs_mem_numa_list_func)(ubs_mem_numa_desc_t**, uint32_t*);
typedef int32_t (*ubs_mem_shm_create_func)(const char*, uint64_t, uint8_t*, uint64_t, const ubs_mem_nodes_t*, const ubs_mem_nodes_t*);
typedef int32_t (*ubs_mem_shm_create_with_affinity_func)(const char*, uint64_t, uint32_t, uint8_t*, uint64_t, const ubs_mem_nodes_t*, const ubs_mem_nodes_t*);
typedef int32_t (*ubs_mem_shm_attach_func)(const char*, const ubs_mem_fd_owner_t*, mode_t, ubs_mem_shm_desc_t**);
typedef int32_t (*ubs_mem_shm_get_func)(const char*, ubs_mem_shm_desc_t**);
typedef int32_t (*ubs_mem_shm_list_func)(ubs_mem_shm_desc_t**, uint32_t*);
typedef int32_t (*ubs_mem_shm_list_with_prefix_func)(const char*, ubs_mem_shm_desc_t**, uint32_t*);
typedef int32_t (*ubs_mem_shm_detach_func)(const char*);
typedef int32_t (*ubs_mem_shm_delete_func)(const char*);
typedef int32_t (*ubs_mem_shm_fault_handler)(const char *name, uint64_t memid, ubs_mem_fault_type_t type);
typedef int32_t (*ubs_mem_shm_fault_get_func)(const char*, ubs_mem_memids_fault_t*);
typedef int32_t (*ubs_mem_shm_fault_register_func)(ubs_mem_shm_fault_handler);

// C桥接函数
static int call_ubs_mem_numa_list(ubs_mem_numa_list_func fn, ubs_mem_numa_desc_t** descs, uint32_t* count) {
    if (fn == NULL) return -1;
    return fn(descs, count);
}

static int call_ubs_mem_shm_create(ubs_mem_shm_create_func fn, const char* name, uint64_t size, uint8_t* usr_info, uint64_t flag, const ubs_mem_nodes_t* region, const ubs_mem_nodes_t* provider) {
    if (fn == NULL) return -1;
    return fn(name, size, usr_info, flag, region, provider);
}

static int call_ubs_mem_shm_create_with_affinity(ubs_mem_shm_create_with_affinity_func fn, const char* name, uint64_t size, uint32_t affinity_socket_id, uint8_t* usr_info, uint64_t flag, const ubs_mem_nodes_t* region, const ubs_mem_nodes_t* provider) {
    if (fn == NULL) return -1;
    return fn(name, size, affinity_socket_id, usr_info, flag, region, provider);
}

static int call_ubs_mem_shm_attach(ubs_mem_shm_attach_func fn, const char* name, const ubs_mem_fd_owner_t* owner, mode_t mode, ubs_mem_shm_desc_t** desc) {
    if (fn == NULL) return -1;
    return fn(name, owner, mode, desc);
}

static int call_ubs_mem_shm_get(ubs_mem_shm_get_func fn, const char* name, ubs_mem_shm_desc_t** desc) {
    if (fn == NULL) return -1;
    return fn(name, desc);
}

static int call_ubs_mem_shm_list(ubs_mem_shm_list_func fn, ubs_mem_shm_desc_t** descs, uint32_t* count) {
    if (fn == NULL) return -1;
    return fn(descs, count);
}

static int call_ubs_mem_shm_list_with_prefix(ubs_mem_shm_list_with_prefix_func fn, const char* name_prefix, ubs_mem_shm_desc_t** descs, uint32_t* count) {
    if (fn == NULL) return -1;
    return fn(name_prefix, descs, count);
}

static int call_ubs_mem_shm_detach(ubs_mem_shm_detach_func fn, const char* name) {
    if (fn == NULL) return -1;
    return fn(name);
}

static int call_ubs_mem_shm_delete(ubs_mem_shm_delete_func fn, const char* name) {
    if (fn == NULL) return -1;
    return fn(name);
}

static int call_ubs_mem_shm_fault_get(ubs_mem_shm_fault_get_func fn, const char* name, ubs_mem_memids_fault_t* fault) {
    if (fn == NULL) return -1;
    return fn(name, fault);
}

static int call_ubs_mem_shm_fault_register(ubs_mem_shm_fault_register_func fn, ubs_mem_shm_fault_handler handler) {
    if (fn == NULL) return -1;
    return fn(handler);
}

extern int32_t goShmFaultHandler(char *name, uint64_t memid, ubs_mem_fault_type_t faultType);
*/
import "C"
import (
	"fmt"
	"net"
	"unsafe"

	"atomgit.com/openeuler/ubs-engine.git/src/sdk/go/dlopen"
	"atomgit.com/openeuler/ubs-engine.git/src/sdk/go/topo"
)

// 常量定义
const (
	UbseMaxUsrInfoLength = 32
	UbsMemMaxSlotNum     = 16
	UbsMemMaxDescList    = 2000
	UbsMemMaxMemidNum    = 2048
)

type UbsMemStage int

const (
	UbseNotExist UbsMemStage = iota
	UbseCreating
	UbseDeleting
	UbseExist
	UbseErrOnlyImport
	UbseErrWaitUnexport
	UbseEnd
)

type UbsMemFaultType int

const (
	UbMemAtomicDataErr UbsMemFaultType = iota
	UbMemReadDataErr
	UbMemFlowPoison
	UbMemFlowReadAuthPoison
	UbMemFlowReadAuthRespErr
	UbMemTimeoutPoison
	UbMemTimeoutRespErr
	UbMemReadDataPoison
	UbMemReadDataRespErr
	MarNoPortVldIntErr
	MarFluxIntErr
	MarWithoutCxtErr
	RspBkpreOverTimeoutErr
	MarNearAuthFailErr
	MarFarAuthFailErr
	MarTimeoutErr
	MarIllegalAccessErr
	RemoteReadDataErrOrWriteResponseErr
	UbMemHealthy = 1000
)

// UbsMemNodes 内存节点信息定义
type UbsMemNodes struct {
	NodeCnt uint32                   // 实际有效的节点数量，取值范围[1, UbsMemMaxSlotNum]
	SlotIds [UbsMemMaxSlotNum]uint32 // 节点ID数组，实际有效长度为 node_cnt, 超出部分无效
}

// UbsMemFdOwner 内存文件属主信息定义
type UbsMemFdOwner struct {
	Uid uint32 // 属主进程的运行用户的uid
	Gid uint32 // 属主进程的的运行用户的groupid
	Pid uint32 // 属主进程的的运行用户的pid, 指定pid, pid消失时, ubse自动释放借用内存(暂不提供)
}

// UbsMemNumaDesc 定义NUMA内存描述信息结构体
type UbsMemNumaDesc struct {
	Name       string           // 借用标识
	NumaId     int64            // 形成远端numa对应的numaid
	ExportNode topo.UbsTopoNode // 借出节点
	ImportNode topo.UbsTopoNode // 借入节点
	Size       uint64           // 借用大小
	UsrInfo    []byte           // 用户信息，长度为UBSE_MAX_USR_INFO_LENGTH
}

// UbsMemShmImportDesc 共享内存导入描述符定义
type UbsMemShmImportDesc struct {
	MemidCnt   uint32           // 导出的内存块数量
	Memids     []uint64         // 内存块标识信息
	ImportNode topo.UbsTopoNode // 借入节点
	MemStage   UbsMemStage      // 内存状态
}

// UbsMemShmDesc 共享内存描述符定义
type UbsMemShmDesc struct {
	Name          string                // 借用标识
	MemSize       uint64                // 导出内存的总大小, 取值为拆分粒度的整数倍, 单位Byte
	UnitSize      uint64                // 芯片表项拆分粒度, 单位Byte
	ExportNode    topo.UbsTopoNode      // 借出节点
	UsrInfo       []byte                // 用户信息
	ImportDescCnt uint32                // 导入内存描述符信息的数量
	MemStage      UbsMemStage           // 内存状态
	ImportDesc    []UbsMemShmImportDesc // 导入内存描述符信息
}

// UbsMemMemidsFault 内存故障描述符定义
type UbsMemMemidsFault struct {
	MemidCnt    uint32            // 导出的内存块数量
	Memids      []uint64          // 导出内存块标识信息
	MemidStatus []UbsMemFaultType // 内存块的健康状态，与memids一一对应
}

// UbsMemNumaList 查询本节点所有numa形态远端内存
func UbsMemNumaList() ([]UbsMemNumaDesc, error) {
	// 加载函数符号（假设库已经在UbsEngineClientInitialize中打开）
	numaListPtr, err := dlopen.LoadFunc("ubs_mem_numa_list")
	if err != nil {
		return nil, fmt.Errorf("failed to load ubs_mem_numa_list: %v", err)
	}

	// 转换为函数指针类型
	numaListFunc := (C.ubs_mem_numa_list_func)(numaListPtr)

	var cDescs *C.ubs_mem_numa_desc_t
	var cCount C.uint32_t

	// 调用C函数
	ret := C.call_ubs_mem_numa_list(numaListFunc, &cDescs, &cCount)
	if ret != 0 {
		return nil, fmt.Errorf("ubs_mem_numa_list failed, error code=%d", ret)
	}

	// 确保释放C分配的内存
	defer C.free(unsafe.Pointer(cDescs))

	count := int(cCount)

	if count <= 0 || count > topo.MaxCount {
		return []UbsMemNumaDesc{}, nil
	}

	descs := make([]UbsMemNumaDesc, 0, count)
	size := unsafe.Sizeof(C.ubs_mem_numa_desc_t{})

	// 遍历C描述信息数组
	for i := 0; i < count; i++ {
		cDesc := (*C.ubs_mem_numa_desc_t)(unsafe.Pointer(uintptr(unsafe.Pointer(cDescs)) + uintptr(i)*size))

		goDesc := UbsMemNumaDesc{
			Name:   C.GoString(&cDesc.name[0]),
			NumaId: int64(cDesc.numaid),
			Size:   uint64(cDesc.size),
		}

		// 复制usrInfo字段
		goDesc.UsrInfo = make([]byte, UbseMaxUsrInfoLength)
		copy(goDesc.UsrInfo, C.GoBytes(unsafe.Pointer(&cDesc.usrInfo[0]), C.int(UbseMaxUsrInfoLength)))

		goDesc.ExportNode = convertTopoNode(&cDesc.export_node)
		goDesc.ImportNode = convertTopoNode(&cDesc.import_node)

		descs = append(descs, goDesc)
	}

	return descs, nil
}

// UbsMemShmCreate 创建共享形态的远端内存
func UbsMemShmCreate(name string, size uint64, usrInfo [32]byte, flag uint64, region *UbsMemNodes, provider *UbsMemNodes) error {
	// 加载函数符号
	shmCreatePtr, err := dlopen.LoadFunc("ubs_mem_shm_create")
	if err != nil {
		return fmt.Errorf("failed to load ubs_mem_shm_create: %v", err)
	}

	// 转换为函数指针类型
	shmCreateFunc := (C.ubs_mem_shm_create_func)(shmCreatePtr)

	// 准备参数
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	// 转换用户信息
	var cUsrInfo *C.uint8_t
	cUsrInfo = (*C.uint8_t)(unsafe.Pointer(&usrInfo[0]))

	// 转换region和provider
	cRegion := convertMemNodes(region)
	defer freeMemNodes(cRegion)

	cProvider := convertMemNodes(provider)
	defer freeMemNodes(cProvider)

	// 调用C函数
	ret := C.call_ubs_mem_shm_create(shmCreateFunc, cName, C.uint64_t(size), cUsrInfo, C.uint64_t(flag), cRegion, cProvider)
	if ret != 0 {
		return fmt.Errorf("ubs_mem_shm_create failed, error code=%d", ret)
	}

	return nil
}

// UbsMemShmCreateWithAffinity 创建共享形态的远端内存（带亲和性）
func UbsMemShmCreateWithAffinity(name string, size uint64, affinitySocketID uint32, usrInfo [32]byte, flag uint64, region *UbsMemNodes, provider *UbsMemNodes) error {
	// 加载函数符号
	shmCreateAffPtr, err := dlopen.LoadFunc("ubs_mem_shm_create_with_affinity")
	if err != nil {
		return fmt.Errorf("failed to load ubs_mem_shm_create_with_affinity: %v", err)
	}

	// 转换为函数指针类型
	shmCreateAffFunc := (C.ubs_mem_shm_create_with_affinity_func)(shmCreateAffPtr)

	// 准备参数
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	// 转换用户信息
	var cUsrInfo *C.uint8_t
	cUsrInfo = (*C.uint8_t)(unsafe.Pointer(&usrInfo[0]))

	// 转换region和provider
	cRegion := convertMemNodes(region)
	defer freeMemNodes(cRegion)

	cProvider := convertMemNodes(provider)
	defer freeMemNodes(cProvider)

	// 调用C函数
	ret := C.call_ubs_mem_shm_create_with_affinity(shmCreateAffFunc, cName, C.uint64_t(size), C.uint32_t(affinitySocketID), cUsrInfo, C.uint64_t(flag), cRegion, cProvider)
	if ret != 0 {
		return fmt.Errorf("ubs_mem_shm_create_with_affinity failed, error code=%d", ret)
	}

	return nil
}

// UbsMemShmAttach 导入共享形态的远端内存
func UbsMemShmAttach(name string, owner *UbsMemFdOwner, mode uint32) (*UbsMemShmDesc, error) {
	// 加载函数符号
	shmAttachPtr, err := dlopen.LoadFunc("ubs_mem_shm_attach")
	if err != nil {
		return nil, fmt.Errorf("failed to load ubs_mem_shm_attach: %v", err)
	}

	// 转换为函数指针类型
	shmAttachFunc := (C.ubs_mem_shm_attach_func)(shmAttachPtr)

	// 准备参数
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	var cOwner *C.ubs_mem_fd_owner_t
	if owner != nil {
		cOwner = convertMemOwner(owner)
		defer freeMemOwner(cOwner)
	}

	var cDesc *C.ubs_mem_shm_desc_t

	// 调用C函数
	ret := C.call_ubs_mem_shm_attach(shmAttachFunc, cName, cOwner, C.mode_t(mode), &cDesc)
	if ret != 0 {
		return nil, fmt.Errorf("ubs_mem_shm_attach failed, error code=%d", ret)
	}
	defer C.free(unsafe.Pointer(cDesc))

	// 转换结果
	goDesc := convertMemShmDesc(cDesc)

	return &goDesc, nil
}

// UbsMemShmGet 查询指定共享形态远端内存
func UbsMemShmGet(name string) (*UbsMemShmDesc, error) {
	// 加载函数符号
	shmGetPtr, err := dlopen.LoadFunc("ubs_mem_shm_get")
	if err != nil {
		return nil, fmt.Errorf("failed to load ubs_mem_shm_get: %v", err)
	}

	// 转换为函数指针类型
	shmGetFunc := (C.ubs_mem_shm_get_func)(shmGetPtr)

	// 准备参数
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	var cDesc *C.ubs_mem_shm_desc_t

	// 调用C函数
	ret := C.call_ubs_mem_shm_get(shmGetFunc, cName, &cDesc)
	if ret != 0 {
		return nil, fmt.Errorf("ubs_mem_shm_get failed, error code=%d", ret)
	}
	defer C.free(unsafe.Pointer(cDesc))

	// 转换结果
	goDesc := convertMemShmDesc(cDesc)

	return &goDesc, nil
}

// UbsMemShmList 查询本节点所有共享形态远端内存
func UbsMemShmList() ([]UbsMemShmDesc, error) {
	// 加载函数符号
	shmListPtr, err := dlopen.LoadFunc("ubs_mem_shm_list")
	if err != nil {
		return nil, fmt.Errorf("failed to load ubs_mem_shm_list: %v", err)
	}

	// 转换为函数指针类型
	shmListFunc := (C.ubs_mem_shm_list_func)(shmListPtr)

	var cDescs *C.ubs_mem_shm_desc_t
	var cCount C.uint32_t

	// 调用C函数
	ret := C.call_ubs_mem_shm_list(shmListFunc, &cDescs, &cCount)
	if ret != 0 {
		return nil, fmt.Errorf("ubs_mem_shm_list failed, error code=%d", ret)
	}
	defer C.free(unsafe.Pointer(cDescs))

	count := int(cCount)
	if count <= 0 || count > UbsMemMaxDescList {
		return []UbsMemShmDesc{}, nil
	}

	descs := make([]UbsMemShmDesc, 0, count)
	size := unsafe.Sizeof(C.ubs_mem_shm_desc_t{})

	// 遍历C描述信息数组
	for i := 0; i < count; i++ {
		cDesc := (*C.ubs_mem_shm_desc_t)(unsafe.Pointer(uintptr(unsafe.Pointer(cDescs)) + uintptr(i)*size))
		goDesc := convertMemShmDesc(cDesc)
		descs = append(descs, goDesc)
	}

	return descs, nil
}

// UbsMemShmListWithPrefix 查询指定前缀的共享形态远端内存
func UbsMemShmListWithPrefix(namePrefix string) ([]UbsMemShmDesc, error) {
	// 加载函数符号
	shmListWithPrefixPtr, err := dlopen.LoadFunc("ubs_mem_shm_list_with_prefix")
	if err != nil {
		return nil, fmt.Errorf("failed to load ubs_mem_shm_list_with_prefix: %v", err)
	}

	// 转换为函数指针类型
	shmListWithPrefixFunc := (C.ubs_mem_shm_list_with_prefix_func)(shmListWithPrefixPtr)

	// 准备参数
	cNamePrefix := C.CString(namePrefix)
	defer C.free(unsafe.Pointer(cNamePrefix))

	var cDescs *C.ubs_mem_shm_desc_t
	var cCount C.uint32_t

	// 调用C函数
	ret := C.call_ubs_mem_shm_list_with_prefix(shmListWithPrefixFunc, cNamePrefix, &cDescs, &cCount)
	if ret != 0 {
		return nil, fmt.Errorf("ubs_mem_shm_list_with_prefix failed, error code=%d", ret)
	}
	defer C.free(unsafe.Pointer(cDescs))

	count := int(cCount)
	if count <= 0 || count > UbsMemMaxDescList {
		return []UbsMemShmDesc{}, nil
	}

	descs := make([]UbsMemShmDesc, 0, count)
	size := unsafe.Sizeof(C.ubs_mem_shm_desc_t{})

	// 遍历C描述信息数组
	for i := 0; i < count; i++ {
		cDesc := (*C.ubs_mem_shm_desc_t)(unsafe.Pointer(uintptr(unsafe.Pointer(cDescs)) + uintptr(i)*size))
		goDesc := convertMemShmDesc(cDesc)
		descs = append(descs, goDesc)
	}

	return descs, nil
}

// UbsMemShmDetach 从共享内存分离
func UbsMemShmDetach(name string) error {
	// 加载函数符号
	shmDetachPtr, err := dlopen.LoadFunc("ubs_mem_shm_detach")
	if err != nil {
		return fmt.Errorf("failed to load ubs_mem_shm_detach: %v", err)
	}

	// 转换为函数指针类型
	shmDetachFunc := (C.ubs_mem_shm_detach_func)(shmDetachPtr)

	// 准备参数
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	// 调用C函数
	ret := C.call_ubs_mem_shm_detach(shmDetachFunc, cName)
	if ret != 0 {
		return fmt.Errorf("ubs_mem_shm_detach failed, error code=%d", ret)
	}

	return nil
}

// UbsMemShmDelete 删除本节点指定共享内存
func UbsMemShmDelete(name string) error {
	// 加载函数符号
	shmDeletePtr, err := dlopen.LoadFunc("ubs_mem_shm_delete")
	if err != nil {
		return fmt.Errorf("failed to load ubs_mem_shm_delete: %v", err)
	}

	// 转换为函数指针类型
	shmDeleteFunc := (C.ubs_mem_shm_delete_func)(shmDeletePtr)

	// 准备参数
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	// 调用C函数
	ret := C.call_ubs_mem_shm_delete(shmDeleteFunc, cName)
	if ret != 0 {
		return fmt.Errorf("ubs_mem_shm_delete failed, error code=%d", ret)
	}

	return nil
}

// UbsMemShmFaultGet 获取指定共享内存的故障信息
func UbsMemShmFaultGet(name string) (*UbsMemMemidsFault, error) {
	// 加载函数符号
	shmFaultGetPtr, err := dlopen.LoadFunc("ubs_mem_shm_fault_get")
	if err != nil {
		return nil, fmt.Errorf("failed to load ubs_mem_shm_fault_get: %v", err)
	}

	// 转换为函数指针类型
	shmFaultGetFunc := (C.ubs_mem_shm_fault_get_func)(shmFaultGetPtr)

	// 准备参数
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	cFault := C.malloc(C.sizeof_ubs_mem_memids_fault_t)
	defer C.free(cFault)

	cFaultPtr := (*C.ubs_mem_memids_fault_t)(cFault)

	// 调用C函数
	ret := C.call_ubs_mem_shm_fault_get(shmFaultGetFunc, cName, cFaultPtr)
	if ret != 0 {
		return nil, fmt.Errorf("ubs_mem_shm_fault_get failed, error code=%d", ret)
	}

	// 转换结果
	goFault := convertMemMemidsFault(cFaultPtr)

	return &goFault, nil
}

type Handler func(name string, memid uint64, falutType UbsMemFaultType) error

var shmFaultHandler Handler

//export goShmFaultHandler
func goShmFaultHandler(cName *C.char, cMemid C.uint64_t, cFaultType C.ubs_mem_fault_type_t) C.int32_t {
	if cName == nil {
		return C.int32_t(-1)
	}
	name := C.GoString(cName)
	memid := uint64(cMemid)
	faultType := UbsMemFaultType(cFaultType)
	if shmFaultHandler != nil {
		err := shmFaultHandler(name, memid, faultType)
		if err != nil {
			return C.int32_t(-1)
		}
		return C.int32_t(0)
	} else {
		return C.int32_t(-1)
	}
}

// UbsMemShmFaultRegister 注册共享内存故障处理函数
func UbsMemShmFaultRegister(handler Handler) error {
	// 保存全局回调函数
	shmFaultHandler = handler

	// 加载函数符号
	shmFaultRegPtr, err := dlopen.LoadFunc("ubs_mem_shm_fault_register")
	if err != nil {
		return fmt.Errorf("failed to load ubs_mem_shm_fault_register: %v", err)
	}

	// 转换为函数指针类型
	shmFaultRegFunc := (C.ubs_mem_shm_fault_register_func)(shmFaultRegPtr)

	// 调用C函数注册回调，使用默认的goShmFaultHandler
	ret := C.call_ubs_mem_shm_fault_register(shmFaultRegFunc, (C.ubs_mem_shm_fault_handler)(C.goShmFaultHandler))
	if ret != 0 {
		return fmt.Errorf("ubs_mem_shm_fault_register failed, error code=%d", ret)
	}

	return nil
}

// convertTopoNode 将C的ubs_topo_node_t转换为Go的UbsTopoNode
func convertTopoNode(cNode *C.ubs_topo_node_t) topo.UbsTopoNode {
	goNode := topo.UbsTopoNode{
		SlotId:   uint32(cNode.slot_id),
		IPs:      make([]net.IP, 0), // 初始化IP列表
		HostName: C.GoString(&cNode.host_name[0]),
	}

	// 复制SocketId数组
	for j := 0; j < topo.TopoSocketNum; j++ {
		goNode.SocketId[j] = uint32(cNode.socket_id[j])
	}

	// 复制NumaIds二维数组
	for j := 0; j < topo.TopoSocketNum; j++ {
		for k := 0; k < topo.TopoNumaNum; k++ {
			goNode.NumaIds[j][k] = uint32(cNode.numa_ids[j][k])
		}
	}

	// 复制IP地址信息，只添加有效的IP地址
	for j := 0; j < topo.TopoIpAddrNum; j++ {
		cIp := cNode.ips[j]

		// 根据地址族处理IP地址
		if cIp.af == C.AF_INET {
			// IPv4地址 长度4字节
			ipv4Bytes := C.GoBytes(unsafe.Pointer(&cIp.ipv4), topo.Ipv4Length)
			ip := net.IP(ipv4Bytes)
			// 检查是否为有效IP（非零IP）
			if !ip.IsUnspecified() {
				goNode.IPs = append(goNode.IPs, ip)
			}
		} else if cIp.af == C.AF_INET6 {
			// IPv6地址 长度16字节
			ipv6Bytes := C.GoBytes(unsafe.Pointer(&cIp.ipv6), topo.Ipv6Length)
			ip := net.IP(ipv6Bytes)
			// 检查是否为有效IP（非零IP）
			if !ip.IsUnspecified() {
				goNode.IPs = append(goNode.IPs, ip)
			}
		}
	}

	return goNode
}

// convertMemNodes 将Go的UbsMemNodes转换为C的ubs_mem_nodes_t
func convertMemNodes(goNodes *UbsMemNodes) *C.ubs_mem_nodes_t {
	if goNodes == nil || int(goNodes.NodeCnt) > UbsMemMaxSlotNum {
		return nil
	}

	cNodes := C.malloc(C.sizeof_ubs_mem_nodes_t)
	cNodesPtr := (*C.ubs_mem_nodes_t)(cNodes)

	// 设置节点数量
	cNodesPtr.node_cnt = C.uint32_t(goNodes.NodeCnt)

	// 复制节点ID数组
	for i := 0; i < int(goNodes.NodeCnt); i++ {
		cNodesPtr.slot_ids[i] = C.uint32_t(goNodes.SlotIds[i])
	}

	return cNodesPtr
}

// convertMemOwner 将Go的UbsMemFdOwner转换为C的ubs_mem_fd_owner_t
func convertMemOwner(goOwner *UbsMemFdOwner) *C.ubs_mem_fd_owner_t {
	if goOwner == nil {
		return nil
	}

	cOwner := C.malloc(C.sizeof_ubs_mem_fd_owner_t)
	cOwnerPtr := (*C.ubs_mem_fd_owner_t)(cOwner)

	cOwnerPtr.uid = C.uid_t(goOwner.Uid)
	cOwnerPtr.gid = C.gid_t(goOwner.Gid)
	cOwnerPtr.pid = C.pid_t(goOwner.Pid)

	return cOwnerPtr
}

// freeMemNodes 释放C分配的ubs_mem_nodes_t内存
func freeMemNodes(cNodes *C.ubs_mem_nodes_t) {
	if cNodes != nil {
		C.free(unsafe.Pointer(cNodes))
	}
}

// freeMemOwner 释放C分配的ubs_mem_fd_owner_t内存
func freeMemOwner(cOwner *C.ubs_mem_fd_owner_t) {
	if cOwner != nil {
		C.free(unsafe.Pointer(cOwner))
	}
}

// convertMemShmImportDesc 转换C的ubs_mem_shm_import_desc_t为Go的UbsMemShmImportDesc
func convertMemShmImportDesc(cDesc *C.ubs_mem_shm_import_desc_t) UbsMemShmImportDesc {
	if cDesc == nil || uint32(cDesc.memid_cnt) > UbsMemMaxMemidNum {
		return UbsMemShmImportDesc{}
	}
	goDesc := UbsMemShmImportDesc{
		MemidCnt:   uint32(cDesc.memid_cnt),
		Memids:     make([]uint64, cDesc.memid_cnt),
		ImportNode: convertTopoNode(&cDesc.import_node),
		MemStage:   UbsMemStage(cDesc.mem_stage),
	}

	// 复制memid数组
	for i := 0; i < int(cDesc.memid_cnt); i++ {
		goDesc.Memids[i] = uint64(cDesc.memids[i])
	}

	return goDesc
}

// convertMemShmDesc 转换C的ubs_mem_shm_desc_t为Go的UbsMemShmDesc
func convertMemShmDesc(cDesc *C.ubs_mem_shm_desc_t) UbsMemShmDesc {
	if cDesc == nil || uint32(cDesc.import_desc_cnt) > topo.TopoMaxNodeNum {
		return UbsMemShmDesc{}
	}
	goDesc := UbsMemShmDesc{
		Name:          C.GoString(&cDesc.name[0]),
		MemSize:       uint64(cDesc.mem_size),
		UnitSize:      uint64(cDesc.unit_size),
		ExportNode:    convertTopoNode(&cDesc.export_node),
		ImportDescCnt: uint32(cDesc.import_desc_cnt),
		MemStage:      UbsMemStage(cDesc.mem_stage),
	}

	// 复制usrInfo字段
	goDesc.UsrInfo = make([]byte, UbseMaxUsrInfoLength)
	copy(goDesc.UsrInfo, C.GoBytes(unsafe.Pointer(&cDesc.usr_info[0]), C.int(UbseMaxUsrInfoLength)))

	// 转换import_desc数组
	goDesc.ImportDesc = make([]UbsMemShmImportDesc, cDesc.import_desc_cnt)
	for i := 0; i < int(cDesc.import_desc_cnt); i++ {
		cImpDesc := (*C.ubs_mem_shm_import_desc_t)(unsafe.Pointer(uintptr(unsafe.Pointer(cDesc.import_desc)) + uintptr(i)*unsafe.Sizeof(C.ubs_mem_shm_import_desc_t{})))
		goDesc.ImportDesc[i] = convertMemShmImportDesc(cImpDesc)
	}

	return goDesc
}

// convertMemMemidsFault 转换C的ubs_mem_memids_fault_t为Go的UbsMemMemidsFault
func convertMemMemidsFault(cFault *C.ubs_mem_memids_fault_t) UbsMemMemidsFault {
	if cFault == nil || uint32(cFault.memid_cnt) > UbsMemMaxMemidNum {
		return UbsMemMemidsFault{}
	}
	goFault := UbsMemMemidsFault{
		MemidCnt:    uint32(cFault.memid_cnt),
		Memids:      make([]uint64, cFault.memid_cnt),
		MemidStatus: make([]UbsMemFaultType, cFault.memid_cnt),
	}

	// 复制memids和状态数组
	for i := 0; i < int(cFault.memid_cnt); i++ {
		goFault.Memids[i] = uint64(cFault.memids[i])
		goFault.MemidStatus[i] = UbsMemFaultType(cFault.memid_status[i])
	}

	return goFault
}
