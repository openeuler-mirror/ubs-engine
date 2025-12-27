// Package urma
package urma

/*
#cgo CFLAGS: -I/usr/include/ubse
#include <stdlib.h>
#include "ubs_engine_urma.h"

typedef uint32_t (*ubs_urma_dev_get_ptr)(ubs_urma_type, urma_device_t**, uint32_t*);
typedef uint32_t (*ubs_urma_dev_alloc_ptr)(const char*, ubs_urma_dev_path_t);

uint32_t call_ubs_urma_dev_get(ubs_urma_dev_get_ptr fn, ubs_urma_type urma_type, urma_device_t **urma_devices, uint32_t *urma_cnt) {
	if (fn == NULL) return (uint32_t)-1;
	return fn(urma_type, urma_devices, urma_cnt);
}

uint32_t call_ubs_urma_dev_alloc(ubs_urma_dev_alloc_ptr fn, const char *name, ubs_urma_dev_path_t dev_info) {
	if (fn == NULL) return -1;
	return fn(name, dev_info);
}
*/
import "C"
import (
	"fmt"
	"unsafe"

	"ubs_engine_go_sdk/dlopen"
)

const (
	urmaNameMax          = 32
	urmaUnique  urmaType = urmaType(C.UNIQUE)
	urmaShared  urmaType = urmaType(C.SHARED)
)

type urmaType uint32

// Device urma device info
type Device struct {
	// Name device id
	Name string
	// Healthy device healthy status
	Healthy bool
	// NumaID device local numa id
	NumaID uint32
}

// DevicePath urma device path info
type DevicePath struct {
	// Vfe0Path vfe0 urma device path
	Vfe0Path string
	// Vfe1Path vfe1 urma device path
	Vfe1Path string
	// BondingPath bonding urma device path
	BondingPath string
}

// UbsGetVfeDevice get unique urma device list
func UbsGetVfeDevice() ([]Device, error) {
	getUrmaDevPtr, err := dlopen.LoadFunc("ubs_urma_dev_get")
	if err != nil {
		return []Device{}, fmt.Errorf("failed to load ubs_urma_dev_get: %v", err)
	}

	getUrmaDevFunc := (C.ubs_urma_dev_get_ptr)(getUrmaDevPtr)

	return callAndParseUrmaDevices(getUrmaDevFunc, urmaUnique)
}

// UbsGetSharedDevice get shared urma device list
func UbsGetSharedDevice() ([]Device, error) {
	getUrmaDevPtr, err := dlopen.LoadFunc("ubs_urma_dev_get")
	if err != nil {
		return []Device{}, fmt.Errorf("failed to load ubs_urma_dev_get: %v", err)
	}

	getUrmaDevFunc := (C.ubs_urma_dev_get_ptr)(getUrmaDevPtr)

	return callAndParseUrmaDevices(getUrmaDevFunc, urmaShared)
}

// UbsAllocateDevice allocate bonding urma device and return paths
func UbsAllocateDevice(name string) ([]string, error) {
	if name == "" {
		return nil, fmt.Errorf("name is empty")
	}
	allocUrmaDevPtr, err := dlopen.LoadFunc("ubs_urma_dev_alloc")
	if err != nil {
		return []string{}, fmt.Errorf("failed to load ubs_urma_dev_alloc: %v", err)
	}
	allocUrmaDevFunc := (C.ubs_urma_dev_alloc_ptr)(allocUrmaDevPtr)
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	var cPath C.ubs_urma_dev_path_t
	ret := C.call_ubs_urma_dev_alloc(allocUrmaDevFunc, cName, cPath)
	if ret != 0 {
		return []string{}, fmt.Errorf("ubs_urma_dev_alloc failed: %d", ret)
	}

	paths := []string{
		C.GoString(&cPath.vfe0_path[0]),
		C.GoString(&cPath.vfe1_path[0]),
		C.GoString(&cPath.bonding_path[0]),
	}

	return paths, nil
}

func callAndParseUrmaDevices(fn C.ubs_urma_dev_get_ptr, t urmaType) ([]Device, error) {
	var cDevs *C.urma_device_t
	defer C.free(unsafe.Pointer(cDevs))
	var cCount C.uint32_t
	ret := C.call_ubs_urma_dev_get(fn, C.ubs_urma_type(t), &cDevs, &cCount)
	if ret != 0 {
		return nil, fmt.Errorf("call_ubs_urma_dev_get_ptr failed with code %d", ret)
	}
	if cCount == 0 || cDevs == nil {
		return []Device{}, nil
	}
	return parseUrmaDeviceArray(cDevs, int(cCount)), nil
}

func parseUrmaDeviceArray(cDevs *C.urma_device_t, count int) []Device {
	devs := make([]Device, 0, count)
	elemSize := unsafe.Sizeof(C.urma_device_t{})

	for i := 0; i < count; i++ {
		cDev := (*C.urma_device_t)(unsafe.Pointer(uintptr(unsafe.Pointer(cDevs)) + uintptr(i)*elemSize))
		devs = append(devs, Device{
			Name:    C.GoString(&cDev.name[0]),
			NumaID:  uint32(cDev.numa),
			Healthy: cDev.healthy != 0,
		})
	}
	return devs
}
