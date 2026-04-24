/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 */

// package escape interface for achieving memory escape
package escape

/*
#cgo CFLAGS: -I/usr/include/ubse -I${SRCDIR}/../../include
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "ubs_virt_agent_container.h"

typedef int32_t (*ubs_container_inject_waterLine_func)(watermark_t*);
typedef int32_t (*ubs_virt_agent_waterline_mem_borrow_func)(mem_borrow_request_t*, char***, uint32_t*);
typedef int32_t (*ubs_virt_agent_waterline_mem_migrate_func)(mem_migrate_request_t*);
typedef int32_t (*ubs_virt_agent_waterline_mem_return_func)(return_request_t*);

int call_ubs_container_inject_waterLine(ubs_container_inject_waterLine_func fn, watermark_t* param)
{
    if (fn == NULL) return -1;
    return fn(param);
}

int call_ubs_virt_agent_waterline_mem_borrow(ubs_virt_agent_waterline_mem_borrow_func fn,
        mem_borrow_request_t *memBorrowRequest,
        char*** borrowIds,
        uint32_t *idsSize)
{
    if (fn == NULL) return -1;
    return fn(memBorrowRequest, borrowIds, idsSize);
}

int call_ubs_virt_agent_waterline_mem_migrate(ubs_virt_agent_waterline_mem_migrate_func fn,
        mem_migrate_request_t *memMigrateRequest)
{
    if (fn == NULL) return -1;
    return fn(memMigrateRequest);
}

int call_ubs_virt_agent_waterline_mem_return(ubs_virt_agent_waterline_mem_return_func fn,
        return_request_t *returnRequest)
{
    if (fn == NULL) return -1;
    return fn(returnRequest);
}
*/
import "C"
import (
	"fmt"
	"unsafe"

	"atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/dlopen"
)

// WaterMark waterMark value for escape
type WaterMark struct {
	HighWaterMark uint16
	LowWaterMark  uint16
}

// MemBorrowRequest request param to borrow mem for sdk
type MemBorrowRequest struct {
	BorrowParam BorrowParam
	BorrowSizes []uint64
	WaterMark   WaterMark
}

// BorrowParam escape node info
type BorrowParam struct {
	SrcNid       string
	SrcLocations []SrcLocation
}

// SrcLocation escape node's socketId and numaId
type SrcLocation struct {
	SocketId int
	NumaId   int
}

// MemMigrateRequest request param to migrate mem for sdk
type MemMigrateRequest struct {
	BorrowParam    BorrowParam
	BorrowIds      []string
	ContainerParam []ContainerParam
}

// ContainerParam mem ratio of pid
type ContainerParam struct {
	Pid   uint64
	Ratio int
}

// MemReturnRequest request param to return mem for sdk
type MemReturnRequest struct {
	BorrowParam BorrowParam
	BorrowIds   []string
	Pids        []uint64
}

func buildCWaterMark(w WaterMark) C.watermark_t {
	var c C.watermark_t
	c.highWaterMark = C.uint16_t(w.HighWaterMark)
	c.lowWaterMark = C.uint16_t(w.LowWaterMark)
	return c
}

// UbsInjectWaterLine inject water line to rmrs
func UbsInjectWaterLine(w WaterMark) error {
	cW := buildCWaterMark(w)

	funcPtr, err := dlopen.LoadFunc("ubs_container_inject_waterLine")
	if err != nil {
		return fmt.Errorf("failed to load symbol: %v", err)
	}
	fn := (C.ubs_container_inject_waterLine_func)(funcPtr)

	ret := C.call_ubs_container_inject_waterLine(fn, &cW)
	if ret != 0 {
		return fmt.Errorf("ubs_container_inject_waterLine failed: %d", int(ret))
	}
	return nil
}

func convertMemBorrowRequestToC(req *MemBorrowRequest) *C.mem_borrow_request_t {
	if req == nil {
		return nil
	}

	cReq := (*C.mem_borrow_request_t)(C.malloc(C.size_t(unsafe.Sizeof(C.mem_borrow_request_t{}))))
	if cReq == nil {
		return nil
	}

	*cReq = C.mem_borrow_request_t{}

	// srcNid
	b := []byte(req.BorrowParam.SrcNid)
	if len(b) > int(C.SDK_NO_16-1) {
		b = b[:int(C.SDK_NO_16-1)]
	}
	copy((*[C.SDK_NO_16]byte)(unsafe.Pointer(&cReq.borrowParam.srcNid[0]))[:], b)

	// srcLocations
	locCount := len(req.BorrowParam.SrcLocations)
	if locCount > int(C.SDK_NO_16) {
		locCount = int(C.SDK_NO_16)
	}
	for i := 0; i < locCount; i++ {
		cReq.borrowParam.srcLocations[i].socketId = C.int(req.BorrowParam.SrcLocations[i].SocketId)
		cReq.borrowParam.srcLocations[i].numaId = C.int(req.BorrowParam.SrcLocations[i].NumaId)
	}
	cReq.borrowParam.srcLocationsSize = C.size_t(locCount)

	// borrowSizes
	bCount := len(req.BorrowSizes)
	if bCount > int(C.SDK_NO_64) {
		bCount = int(C.SDK_NO_64)
	}
	for i := 0; i < bCount; i++ {
		cReq.borrowSizes[i] = C.uint64_t(req.BorrowSizes[i])
	}
	cReq.borrowSizesSize = C.size_t(bCount)

	cReq.waterMark.highWaterMark = C.uint16_t(req.WaterMark.HighWaterMark)
	cReq.waterMark.lowWaterMark = C.uint16_t(req.WaterMark.LowWaterMark)

	return cReq
}

func convertMemMigrateRequestToC(req *MemMigrateRequest) *C.mem_migrate_request_t {
	if req == nil || req.ContainerParam == nil {
		return nil
	}

	cReq := allocMemMigrateRequest()
	if cReq == nil {
		return nil
	}

	fillMigrateSrcNid(cReq, req)
	fillMigrateSrcLocations(cReq, req)
	fillMigrateBorrowIds(cReq, req)
	fillMigrateContainerParam(cReq, req)

	return cReq
}

func allocMemMigrateRequest() *C.mem_migrate_request_t {
	cReq := (*C.mem_migrate_request_t)(
		C.malloc(C.size_t(unsafe.Sizeof(C.mem_migrate_request_t{}))),
	)
	if cReq == nil {
		return nil
	}

	*cReq = C.mem_migrate_request_t{}
	return cReq
}

func fillMigrateSrcNid(cReq *C.mem_migrate_request_t, req *MemMigrateRequest) {
	b := []byte(req.BorrowParam.SrcNid)
	if len(b) > int(C.SDK_NO_16-1) {
		b = b[:int(C.SDK_NO_16-1)]
	}

	copy(
		(*[C.SDK_NO_16]byte)(unsafe.Pointer(&cReq.borrowParam.srcNid[0]))[:],
		b,
	)
}

func fillMigrateSrcLocations(cReq *C.mem_migrate_request_t, req *MemMigrateRequest) {
	locCount := len(req.BorrowParam.SrcLocations)
	if locCount > int(C.SDK_NO_16) {
		locCount = int(C.SDK_NO_16)
	}

	for i := 0; i < locCount; i++ {
		cReq.borrowParam.srcLocations[i].socketId =
			C.int(req.BorrowParam.SrcLocations[i].SocketId)
		cReq.borrowParam.srcLocations[i].numaId =
			C.int(req.BorrowParam.SrcLocations[i].NumaId)
	}

	cReq.borrowParam.srcLocationsSize = C.size_t(locCount)
}

func fillMigrateBorrowIds(cReq *C.mem_migrate_request_t, req *MemMigrateRequest) {
	idCount := len(req.BorrowIds)
	if idCount > int(C.SDK_NO_64) {
		idCount = int(C.SDK_NO_64)
	}

	for i := 0; i < idCount; i++ {
		b := []byte(req.BorrowIds[i])
		if len(b) > int(C.SDK_NO_128-1) {
			b = b[:int(C.SDK_NO_128-1)]
		}

		copy(
			(*[C.SDK_NO_128]byte)(
				unsafe.Pointer(&cReq.borrowIds[i][0]),
			)[:],
			b,
		)
	}

	cReq.borrowIdsSize = C.size_t(idCount)
}

func fillMigrateContainerParam(cReq *C.mem_migrate_request_t, req *MemMigrateRequest) {
	cCount := len(req.ContainerParam)
	if cCount > int(C.SDK_NO_16) {
		cCount = int(C.SDK_NO_16)
	}

	for i := 0; i < cCount; i++ {
		cReq.containerParam[i].pid =
			C.pid_t(req.ContainerParam[i].Pid)
		cReq.containerParam[i].ratio =
			C.int(req.ContainerParam[i].Ratio)
	}

	cReq.containerParamSize = C.size_t(cCount)
}

func convertReturnRequestToC(req *MemReturnRequest) *C.return_request_t {
	cReq := (*C.return_request_t)(C.malloc(C.size_t(unsafe.Sizeof(C.return_request_t{}))))
	if cReq == nil {
		return nil
	}

	*cReq = C.return_request_t{}

	// srcNid
	b := []byte(req.BorrowParam.SrcNid)
	if len(b) > int(C.SDK_NO_16-1) {
		b = b[:int(C.SDK_NO_16-1)]
	}
	copy((*[C.SDK_NO_16]byte)(unsafe.Pointer(&cReq.borrowParam.srcNid[0]))[:], b)

	// srcLocations
	locCount := len(req.BorrowParam.SrcLocations)
	if locCount > int(C.SDK_NO_16) {
		locCount = int(C.SDK_NO_16)
	}
	for i := 0; i < locCount; i++ {
		cReq.borrowParam.srcLocations[i].socketId = C.int(req.BorrowParam.SrcLocations[i].SocketId)
		cReq.borrowParam.srcLocations[i].numaId = C.int(req.BorrowParam.SrcLocations[i].NumaId)
	}
	cReq.borrowParam.srcLocationsSize = C.size_t(locCount)

	// borrowIds
	idCount := len(req.BorrowIds)
	if idCount > int(C.SDK_NO_16) {
		idCount = int(C.SDK_NO_16)
	}
	for i := 0; i < idCount; i++ {
		b := []byte(req.BorrowIds[i])
		if len(b) > int(C.SDK_NO_128-1) {
			b = b[:int(C.SDK_NO_128-1)]
		}
		copy((*[C.SDK_NO_128]byte)(unsafe.Pointer(&cReq.borrowIds[i][0]))[:], b)
	}
	cReq.borrowIdsSize = C.size_t(idCount)

	// pids
	pidCount := len(req.Pids)
	if pidCount > int(C.SDK_NO_64) {
		pidCount = int(C.SDK_NO_64)
	}
	for i := 0; i < pidCount; i++ {
		cReq.pids[i] = C.pid_t(req.Pids[i])
	}
	cReq.pidsSize = C.size_t(pidCount)

	return cReq
}

func convertBorrowIdsToGo(borrowIds **C.char, idsSize C.uint32_t) []string {
	size := int(idsSize)
	if borrowIds == nil {
		return nil
	}

	var res []string
	if size > 0 && size <= int(C.SDK_NO_128) {
		res = make([]string, size, size)
	} else {
		return nil
	}

	ptr := unsafe.Pointer(borrowIds)
	stride := unsafe.Sizeof(uintptr(0))

	for i := 0; i < size; i++ {
		p := *(**C.char)(unsafe.Pointer(uintptr(ptr) + uintptr(i)*stride))
		if p == nil {
			res[i] = ""
			continue
		}

		s := C.GoString(p)
		res[i] = s
		C.free(unsafe.Pointer(p))
	}

	return res
}

// UbsMemBorrow request to borrow mem
func UbsMemBorrow(memBorrowRequest MemBorrowRequest) ([]string, error) {

	cReq := convertMemBorrowRequestToC(&memBorrowRequest)

	funcPtr, err := dlopen.LoadFunc("ubs_virt_agent_waterline_mem_borrow")
	if err != nil {
		return nil, fmt.Errorf("[UbsMemBorrow] LoadFunc error: %v", err)
	}

	fn := (C.ubs_virt_agent_waterline_mem_borrow_func)(funcPtr)
	if fn == nil {
		return nil, fmt.Errorf("[UbsMemBorrow] function pointer cast failed")
	}

	var cBorrowIds **C.char
	var cCount C.uint32_t

	ret := C.call_ubs_virt_agent_waterline_mem_borrow(fn, cReq, &cBorrowIds, &cCount)
	defer C.free(unsafe.Pointer(cBorrowIds))
	defer C.free(unsafe.Pointer(cReq))

	if ret != 0 {
		return nil, fmt.Errorf("ubs_virt_agent_waterline_mem_borrow failed: %d", int(ret))
	}

	results := convertBorrowIdsToGo(cBorrowIds, cCount)

	return results, nil
}

// UbsMemMigrate request to migrate mem
func UbsMemMigrate(req MemMigrateRequest) error {

	cReq := convertMemMigrateRequestToC(&req)

	funcPtr, err := dlopen.LoadFunc("ubs_virt_agent_waterline_mem_migrate")
	if err != nil {
		return fmt.Errorf("[UbsMemMigrate] LoadFunc error: %v", err)
	}

	fn := (C.ubs_virt_agent_waterline_mem_migrate_func)(funcPtr)

	ret := C.call_ubs_virt_agent_waterline_mem_migrate(fn, cReq)
	C.free(unsafe.Pointer(cReq))

	if ret != 0 {
		return fmt.Errorf("ubs_virt_agent_waterline_mem_migrate failed: %d", int(ret))
	}
	return nil
}

// UbsMemReturn request to return mem
func UbsMemReturn(req MemReturnRequest) error {
	cReq := convertReturnRequestToC(&req)
	if cReq == nil {
		return fmt.Errorf("convertReturnRequestToC returned nil")
	}
	defer C.free(unsafe.Pointer(cReq))

	funcPtr, err := dlopen.LoadFunc("ubs_virt_agent_waterline_mem_return")
	if err != nil {
		return fmt.Errorf("failed to load symbol 'ubs_virt_agent_waterline_mem_return': %w", err)
	}
	if funcPtr == nil {
		return fmt.Errorf("loaded nil function pointer for mem_return")
	}

	fn := (C.ubs_virt_agent_waterline_mem_return_func)(funcPtr)
	if fn == nil {
		return fmt.Errorf("invalid function pointer for mem_return")
	}

	ret := C.call_ubs_virt_agent_waterline_mem_return(fn, cReq)
	if ret != 0 {
		return fmt.Errorf("ubs_virt_agent_waterline_mem_return failed: %d", int(ret))
	}

	return nil
}
