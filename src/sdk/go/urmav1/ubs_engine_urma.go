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

// Package urmav1 implements UBS URMA operations with Go.
package urmav1

import (
	"encoding/binary"
	"fmt"

	"atomgit.com/openeuler/ubs-engine.git/src/sdk/go/ipc"
)

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

// UbsGetVfeDevice gets the VFE URMA device list.
// Returns a list of VFE URMA devices and an error if the operation fails.
func UbsGetVfeDevice() ([]Device, error) {
	// According to ubs_urma_dev_get in ubs_engine_urma.cpp
	// request_buffer.length = sizeof(uint32_t)
	// request_buffer.buffer is allocated but not initialized
	request := make([]byte, 4) // sizeof(uint32_t)
	response, err := ipc.InvokeCall(UbseModuleCode, UbseUrmaDevGet, request)
	if err != nil {
		return nil, err
	}

	return ubseUrmaDevUnpack(response)
}

// UbsGetSharedDevice gets the shared URMA device list.
// Returns a list of shared URMA devices and an error if the operation fails.
func UbsGetSharedDevice() ([]Device, error) {
	// According to ubs_urma_dev_get in ubs_engine_urma.cpp
	// request_buffer.length = sizeof(uint32_t)
	// request_buffer.buffer is allocated but not initialized
	request := make([]byte, 4) // sizeof(uint32_t)
	response, err := ipc.InvokeCall(UbseModuleCode, UbseUrmaDevGet, request)
	if err != nil {
		return nil, err
	}

	return ubseUrmaDevUnpack(response)
}

// UbsAllocateDevice allocates a bonding URMA device and returns its paths.
// name: The name of the device to allocate.
// Returns the device information and an error if the operation fails.
func UbsAllocateDevice(name string) (DeviceInfo, error) {
	if name == "" {
		return DeviceInfo{}, fmt.Errorf("name is empty")
	}

	if len(name) >= UbsUrmaNameMax {
		return DeviceInfo{}, fmt.Errorf("name length exceeds maximum allowed")
	}

	response, err := ipc.InvokeCall(UbseModuleCode, UbseUrmaDevAlloc, []byte(name+"\x00"))
	if err != nil {
		return DeviceInfo{}, err
	}

	return ubseUrmaDevInfoUnpack(response)
}

// ubseUrmaDevUnpack unpacks the device information from the response.
// response: The response body from the UBSE daemon.
// Returns a list of devices and an error if the unpacking fails.
func ubseUrmaDevUnpack(response []byte) ([]Device, error) {
	if len(response) < 4 {
		return nil, fmt.Errorf("invalid response length: expected at least 4 bytes, got %d", len(response))
	}

	count := binary.LittleEndian.Uint32(response[0:])
	response = response[4:]

	// Check for reasonable device count
	const MaxDevices = 1024
	if count > MaxDevices {
		return nil, fmt.Errorf("device count %d exceeds maximum allowed %d", count, MaxDevices)
	}

	devices := make([]Device, 0, count)
	var name string
	var err error
	for i := uint32(0); i < count; i++ {
		// Check if enough data remains for device info
		if len(response) < 4+12 { // string length + device info
			return nil, fmt.Errorf("insufficient data for device %d: expected at least 16 bytes, got %d", i, len(response))
		}
		name, response, err = unpackString(response, UbsUrmaNameMax)
		if err != nil {
			return nil, fmt.Errorf("invalid device name for device %d: %v", i, err)
		}

		// Check if enough data remains for healthy status and hwResId
		if len(response) < 12 {
			return nil, fmt.Errorf("insufficient data for device %d status: expected at least 12 bytes, got %d", i, len(response))
		}

		// Parse healthy status (uint32)
		healthy := binary.LittleEndian.Uint32(response[0:]) == 0

		// Parse hardware resource ID (uint64)
		hwResId := binary.LittleEndian.Uint64(response[4:])
		devices = append(devices, Device{
			Name:    name,
			Healthy: healthy,
			HwResId: hwResId,
		})

		// Move to next device
		response = response[12:]
	}

	return devices, nil
}

// unpackString unpacks a string from the response.
// response: The response body from the UBSE daemon.
// maxLen: The maximum length of the string.
// Returns the unpacked string, the remaining response, and an error if the unpacking fails.
func unpackString(response []byte, maxLen uint32) (string, []byte, error) {
	if len(response) < 4 {
		return "", response, fmt.Errorf("invalid string length: expected at least 4 bytes, got %d", len(response))
	}
	strLen := binary.LittleEndian.Uint32(response[0:])
	response = response[4:]

	if strLen > maxLen {
		return "", response, fmt.Errorf("string length %d exceeds maximum allowed %d", strLen, maxLen)
	}
	if int(strLen) > len(response) {
		return "", response, fmt.Errorf("string length %d exceeds available data %d", strLen, len(response))
	}

	str := string(response[:strLen])
	response = response[strLen:]

	return str, response, nil
}

// ubseUrmaDevInfoUnpack unpacks the device info from the response.
// response: The response body from the UBSE daemon.
// Returns the device information and an error if the unpacking fails.
func ubseUrmaDevInfoUnpack(response []byte) (DeviceInfo, error) {
	// Parse bonding path
	bondingPath, response, err := unpackString(response, UbsMaxUrmaPathLength)
	if err != nil {
		return DeviceInfo{}, fmt.Errorf("invalid bonding path: %v", err)
	}

	// Parse VFE path 1
	vfePath1, response, err := unpackString(response, UbsMaxUrmaPathLength)
	if err != nil {
		return DeviceInfo{}, fmt.Errorf("invalid vfe path 1: %v", err)
	}

	// Parse VFE path 2
	vfePath2, response, err := unpackString(response, UbsMaxUrmaPathLength)
	if err != nil {
		return DeviceInfo{}, fmt.Errorf("invalid vfe path 2: %v", err)
	}

	// Parse bonding eid
	bondingEid, response, err := unpackString(response, UbsMaxUrmaPathLength)
	if err != nil {
		return DeviceInfo{}, fmt.Errorf("invalid bonding eid: %v", err)
	}

	return DeviceInfo{
		VfePaths:    []string{vfePath1, vfePath2},
		BondingPath: bondingPath,
		BondingEid:  bondingEid,
	}, nil
}
