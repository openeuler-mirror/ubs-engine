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

// package collector SDK for implementing the collection interface
package collector

/*
#cgo CFLAGS: -I/usr/include/ubse
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "ubs_virt_agent_container.h"

typedef int32_t (*ubs_container_info_query_func)(pid_param*, pid_mem_info**, uint32_t*);
typedef int32_t (*ubs_container_get_container_pids_func)(container_id_list*, container_pid_info**, uint32_t*);

int call_ubs_container_info_query(ubs_container_info_query_func fn,
                                  pid_param* param,
                                  pid_mem_info** pidMemInfo,
                                  uint32_t* count)
{
    if (fn == NULL) return -1;
    return fn(param, pidMemInfo, count);
}

int call_ubs_container_get_container_pids(ubs_container_get_container_pids_func fn,
        container_id_list* containerIdList,
        container_pid_info** param,
        uint32_t *InfoSize)
{
    if (fn == NULL) return -1;
    return fn(containerIdList, param, InfoSize);
}
*/
import "C"
import (
	"fmt"
	"unsafe"

	"ubs_virt_agent_go_sdk/dlopen"
)

// ContainerMemInfo Container scenario is used to store the remote and local memory used by the collected PIDs.
type ContainerMemInfo struct {
	Pid           uint64
	NumaIds       []uint16
	LocalMemSize  uint64
	RemoteMemSize uint64
}

// PidParam Information about the PID passed by the SDK
type PidParam struct {
	SrcNid string
	Pids   []uint64
}

// ContainerPidInfo storage the list of PIDs under the corresponding container ID
type ContainerPidInfo struct {
	Pids        []uint64
	ContainerId string
}

func buildCParam(param PidParam) (C.pid_param, func()) {
	var cParam C.pid_param
	cParam = C.pid_param{}

	b := []byte(param.SrcNid)
	if len(b) > int(C.SDK_NO_16-1) {
		b = b[:int(C.SDK_NO_16-1)]
	}
	copy((*[C.SDK_NO_16]byte)(unsafe.Pointer(&cParam.srcNid[0]))[:], b)

	maxp := len(param.Pids)
	if maxp > int(C.SDK_NO_2048) {
		maxp = int(C.SDK_NO_2048)
	}
	for i := 0; i < maxp; i++ {
		cParam.pids[i] = C.pid_t(param.Pids[i])
	}
	cParam.pids_size = C.size_t(len(param.Pids))

	return cParam, func() {} // 空 cleanup
}

func convertCInfos(cInfos *C.pid_mem_info, count int) []ContainerMemInfo {
	results := make([]ContainerMemInfo, 0, count)
	if cInfos == nil || count <= 0 {
		return results
	}

	size := unsafe.Sizeof(C.pid_mem_info{})

	for i := 0; i < count; i++ {
		ptr := unsafe.Pointer(uintptr(unsafe.Pointer(cInfos)) + uintptr(i)*size)
		c := (*C.pid_mem_info)(ptr)

		numaCount := int(c.localNumaCount)
		numaIds := make([]uint16, 0, numaCount)
		for j := 0; j < numaCount && j < int(C.SDK_NO_64); j++ {
			numaIds = append(numaIds, uint16(c.localNumaIds[j]))
		}
		results = append(results, ContainerMemInfo{
			Pid:           uint64(c.pid),
			LocalMemSize:  uint64(c.localUsedMem),
			RemoteMemSize: uint64(c.remoteUsedMem),
			NumaIds:       numaIds,
		})
	}
	return results
}

// UbsGetContainerMemInfo go sdk for remote and local memory used for calling the SDK to obtain the PID
func UbsGetContainerMemInfo(param PidParam) ([]ContainerMemInfo, error) {
	cParam, cleanup := buildCParam(param)
	defer cleanup()

	funcPtr, err := dlopen.LoadFunc("ubs_container_info_query")
	if err != nil {
		return nil, fmt.Errorf("failed to load symbol: %v", err)
	}
	queryFunc := (C.ubs_container_info_query_func)(funcPtr)

	var cInfos *C.pid_mem_info
	var cCount C.uint32_t

	ret := C.call_ubs_container_info_query(queryFunc, &cParam, &cInfos, &cCount)

	if ret != 0 {
		return nil, fmt.Errorf("ubs_container_info_query failed: %v", ret)
	}

	defer func() {
		if cInfos != nil {
			C.free(unsafe.Pointer(cInfos))
		}
	}()

	count := int(cCount)
	if count <= 0 || count > C.SDK_NO_2048 {
		return []ContainerMemInfo{}, fmt.Errorf("invalid container count returned from C: %d", count)
	}

	results := convertCInfos(cInfos, count)
	return results, nil
}

func goStringsToCContainerIdList(ids []string) (C.container_id_list, func(), error) {
	var cList C.container_id_list
	cList = C.container_id_list{}

	limit := len(ids)
	if limit > int(C.SDK_NO_128) {
		limit = int(C.SDK_NO_128)
	}

	for i := 0; i < limit; i++ {
		b := []byte(ids[i])
		if len(b) > int(C.SDK_NO_128-1) {
			b = b[:int(C.SDK_NO_128-1)]
		}
		copy((*[C.SDK_NO_128]byte)(unsafe.Pointer(&cList.containerId[i][0]))[:], b)
	}
	cList.containerIdSize = C.size_t(limit)
	return cList, func() {}, nil
}

func convertCContainerPidInfos(cInfos *C.container_pid_info, count int) []ContainerPidInfo {
	if cInfos == nil || count == 0 {
		return nil
	}

	results := make([]ContainerPidInfo, 0, count)
	size := unsafe.Sizeof(C.container_pid_info{})

	for i := 0; i < count; i++ {
		ptr := unsafe.Pointer(uintptr(unsafe.Pointer(cInfos)) + uintptr(i)*size)
		ci := (*C.container_pid_info)(ptr)

		pcount := int(ci.pidsCount)
		pids := make([]uint64, 0, pcount)
		for j := 0; j < pcount && j < int(C.SDK_NO_2048); j++ {
			pids = append(pids, uint64(ci.pids[j]))
		}

		cid := C.GoString(ci.containerId)

		results = append(results, ContainerPidInfo{
			Pids:        pids,
			ContainerId: cid,
		})
	}

	return results
}

// UbsGetContainerPids obtain the list of PIDs under the corresponding container ID
func UbsGetContainerPids(containerIds []string) ([]ContainerPidInfo, error) {
	cList, cleanup, err := goStringsToCContainerIdList(containerIds)
	if err != nil {
		return nil, err
	}
	defer cleanup()

	funcPtr, err := dlopen.LoadFunc("ubs_container_get_container_pids")
	if err != nil {
		return nil, fmt.Errorf("failed to load symbol: %v", err)
	}
	fn := (C.ubs_container_get_container_pids_func)(funcPtr)

	var cResult *C.container_pid_info
	var cCount C.uint32_t
	defer func() {
		if cResult != nil {
			C.free(unsafe.Pointer(cResult))
		}
	}()

	ret := C.call_ubs_container_get_container_pids(fn, &cList, &cResult, &cCount)
	if ret != 0 {
		return nil, fmt.Errorf("ubs_container_get_container_pids failed: %d", int(ret))
	}

	count := int(cCount)
	if count <= 0 || count > C.SDK_NO_128 {
		return []ContainerPidInfo{}, fmt.Errorf("invalid pid count returned from C: %d", count)
	}
	results := convertCContainerPidInfos(cResult, count)
	return results, nil
}
