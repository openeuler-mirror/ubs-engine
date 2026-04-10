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

// Package urma implements UBS URMA operations.
package urma

/*
#cgo CFLAGS: -I/usr/include/ubse -I${SRCDIR}/../../include
#include <stdlib.h>
#include "ubs_engine_urma.h"

typedef uint32_t (*ubs_urma_dev_get_ptr)(ubs_urma_dev_t**, uint32_t*);
typedef uint32_t (*ubs_urma_dev_alloc_ptr)(const char*, ubs_urma_dev_info_t*);

uint32_t call_ubs_urma_dev_get(ubs_urma_dev_get_ptr fn, ubs_urma_dev_t **urma_devices, uint32_t *urma_cnt) {
	if (fn == NULL) return (uint32_t)-1;
	return fn(urma_devices, urma_cnt);
}

uint32_t call_ubs_urma_dev_alloc(ubs_urma_dev_alloc_ptr fn, const char *name, ubs_urma_dev_info_t *dev_info) {
	if (fn == NULL) return -1;
	return fn(name, dev_info);
}
*/
import "C"
import (
	"fmt"
	"unsafe"

	"atomgit.com/openeuler/ubs-engine.git/src/sdk/go/dlopen"
)

const (
	urmaNameMax          = 32
	urmaUnique  urmaType = urmaType(C.UNIQUE)
	urmaShared  urmaType = urmaType(C.SHARED)
)

type urmaType uint32

// Device represents urma device information.
type Device struct {
	// Name is the device id.
	Name string
	// Healthy indicates the device healthy status.
	Healthy bool
	// HwResId is the hardware resource id.
	HwResId uint64
}

// DeviceInfo represents urma device path information.
type DeviceInfo struct {
	// VfePaths are vfe urma device paths.
	VfePaths []string
	// BondingPath is the bonding urma device path.
	BondingPath string
	// BondingEid is the bonding urma device eid.
	BondingEid string
}

// UbsGetVfeDevice gets the urma device list.
func UbsGetVfeDevice() ([]Device, error) {
	getUrmaDevPtr, err := dlopen.LoadFunc("ubs_urma_dev_get")
	if err != nil {
		return []Device{}, fmt.Errorf("failed to load ubs_urma_dev_get: %v", err)
	}

	getUrmaDevFunc := (C.ubs_urma_dev_get_ptr)(getUrmaDevPtr)

	return callAndParseUrmaDevices(getUrmaDevFunc)
}

// UbsGetSharedDevice gets the shared urma device list.
func UbsGetSharedDevice() ([]Device, error) {
	getUrmaDevPtr, err := dlopen.LoadFunc("ubs_urma_dev_get")
	if err != nil {
		return []Device{}, fmt.Errorf("failed to load ubs_urma_dev_get: %v", err)
	}

	getUrmaDevFunc := (C.ubs_urma_dev_get_ptr)(getUrmaDevPtr)

	return callAndParseUrmaDevices(getUrmaDevFunc)
}

// UbsAllocateDevice allocates bonding urma device and returns paths.
func UbsAllocateDevice(name string) (DeviceInfo, error) {
	if name == "" {
		return DeviceInfo{}, fmt.Errorf("name is empty")
	}

	allocUrmaDevPtr, err := dlopen.LoadFunc("ubs_urma_dev_alloc")
	if err != nil {
		return DeviceInfo{}, fmt.Errorf("failed to load ubs_urma_dev_alloc: %v", err)
	}
	allocUrmaDevFunc := (C.ubs_urma_dev_alloc_ptr)(allocUrmaDevPtr)
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	var cInfo C.ubs_urma_dev_info_t
	ret := C.call_ubs_urma_dev_alloc(allocUrmaDevFunc, cName, &cInfo)
	if ret != 0 {
		return DeviceInfo{}, fmt.Errorf("ubs_urma_dev_alloc failed: %d", ret)
	}

	vfePaths := []string{
		C.GoString(&cInfo.vfe_path[0][0]),
		C.GoString(&cInfo.vfe_path[1][0]),
	}

	return DeviceInfo{
		VfePaths:    vfePaths,
		BondingPath: C.GoString(&cInfo.bonding_path[0]),
		BondingEid:  C.GoString(&cInfo.bonding_eid[0]),
	}, nil
}

// callAndParseUrmaDevices calls the urma device get function and parses the results.
func callAndParseUrmaDevices(fn C.ubs_urma_dev_get_ptr) ([]Device, error) {
	var cDevs *C.ubs_urma_dev_t
	var cCount C.uint32_t

	defer func() {
	    if cDevs !=nil {
	        defer C.free(unsafe.Pointer(cDevs))
	    }
	}()

	ret := C.call_ubs_urma_dev_get(fn, &cDevs, &cCount)
	if ret != 0 {
		return nil, fmt.Errorf("call_ubs_urma_dev_get_ptr failed with code %d", ret)
	}
	if cCount == 0 || cDevs == nil {
		return []Device{}, nil
	}
	return parseUrmaDeviceArray(cDevs, int(cCount)), nil
}

// parseUrmaDeviceArray parses the urma device array into Go Device structs.
func parseUrmaDeviceArray(cDevs *C.ubs_urma_dev_t, count int) []Device {
	const MaxDevices = 65535
	if count <= 0 || count > MaxDevices {
		return []Device{}
	}
	devs := make([]Device, 0, count)
	elemSize := unsafe.Sizeof(C.ubs_urma_dev_t{})

	for i := 0; i < count; i++ {
		cDev := (*C.ubs_urma_dev_t)(unsafe.Pointer(uintptr(unsafe.Pointer(cDevs)) + uintptr(i)*elemSize))
		devs = append(devs, Device{
			Name:    C.GoString(&cDev.name[0]),
			Healthy: cDev.healthy == 0,
			HwResId: uint64(cDev.hw_res_id),
		})
	}
	return devs
}
