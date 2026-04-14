// Package urmav1 implements UBS URMA operations without cgo dependency.
package urmav1

import (
	"encoding/binary"
	"fmt"
	"net"
	"os"
	"time"
)

// Error codes
const (
	UbsSuccess             = 0
	UbsErrNullPointer      = 1
	UbsEngineErrInternal   = 2
	UbsEngineErrOutOfRange = 3
)

// URMA device types
const (
	UrmaUnique = 0
	UrmaShared = 1
)

// Constants from ubs_engine_urma.h
const (
	UbsUrmaNameMax        = 32
	UbsMaxUrmaPathLength  = 128
	UbsUrmaVfeNum         = 2
)

// URMA command codes
const (
	UbseUrmaDevGet    = 0
	UbseUrmaDevAlloc  = 1
	UbseUrmaDevFree   = 2
	UbseUrmaQosSet    = 3
	UbseUrmaQosGet    = 4
	UbseUrmaQosReset  = 5
)

// IPC related constants
const (
	UbseIpcSocketPath = "/var/run/ubse/ubse.sock"
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

// UbsGetVfeDevice gets the urma device list.
func UbsGetVfeDevice() ([]Device, error) {
	response, err := ubseInvokeCall(UbseUrmaDevGet, []byte{0, 0, 0, 0})
	if err != nil {
		return nil, err
	}

	return ubseUrmaDevUnpack(response)
}

// UbsGetSharedDevice gets the shared urma device list.
func UbsGetSharedDevice() ([]Device, error) {
	response, err := ubseInvokeCall(UbseUrmaDevGet, []byte{0, 0, 0, 0})
	if err != nil {
		return nil, err
	}

	return ubseUrmaDevUnpack(response)
}

// UbsAllocateDevice allocates bonding urma device and returns paths.
func UbsAllocateDevice(name string) (DeviceInfo, error) {
	if name == "" {
		return DeviceInfo{}, fmt.Errorf("name is empty")
	}

	if len(name) >= UbsUrmaNameMax {
		return DeviceInfo{}, fmt.Errorf("name length exceeds maximum allowed")
	}

	response, err := ubseInvokeCall(UbseUrmaDevAlloc, []byte(name+"\x00"))
	if err != nil {
		return DeviceInfo{}, err
	}

	return ubseUrmaDevInfoUnpack(response)
}

// UbsFreeDevice frees bonding urma device.
func UbsFreeDevice(name string) error {
	if name == "" {
		return fmt.Errorf("name is empty")
	}

	if len(name) >= UbsUrmaNameMax {
		return fmt.Errorf("name length exceeds maximum allowed")
	}

	_, err := ubseInvokeCall(UbseUrmaDevFree, []byte(name+"\x00"))
	return err
}

// UbsSetBandwidth sets the bandwidth for a urma device.
func UbsSetBandwidth(name string, minBandwidth, maxBandwidth uint32) error {
	if name == "" {
		return fmt.Errorf("name is empty")
	}

	if len(name) >= UbsUrmaNameMax {
		return fmt.Errorf("name length exceeds maximum allowed")
	}

	if minBandwidth > maxBandwidth {
		return fmt.Errorf("minBandwidth should be less than or equal to maxBandwidth")
	}

	// Prepare request buffer
	request := make([]byte, len(name)+1+8) // name + null terminator + 2 uint32
	copy(request, []byte(name+"\x00"))
	binary.LittleEndian.PutUint32(request[len(name)+1:], minBandwidth)
	binary.LittleEndian.PutUint32(request[len(name)+5:], maxBandwidth)

	_, err := ubseInvokeCall(UbseUrmaQosSet, request)
	return err
}

// UbsGetBandwidth gets the bandwidth for a urma device.
func UbsGetBandwidth(name string) (uint32, uint32, error) {
	if name == "" {
		return 0, 0, fmt.Errorf("name is empty")
	}

	if len(name) >= UbsUrmaNameMax {
		return 0, 0, fmt.Errorf("name length exceeds maximum allowed")
	}

	response, err := ubseInvokeCall(UbseUrmaQosGet, []byte(name+"\x00"))
	if err != nil {
		return 0, 0, err
	}

	return ubseUrmaQosUnpack(response)
}

// UbsResetBandwidth resets the bandwidth for a urma device.
func UbsResetBandwidth(name string) error {
	if name == "" {
		return fmt.Errorf("name is empty")
	}

	if len(name) >= UbsUrmaNameMax {
		return fmt.Errorf("name length exceeds maximum allowed")
	}

	_, err := ubseInvokeCall(UbseUrmaQosReset, []byte(name+"\x00"))
	return err
}

// ubseInvokeCall invokes a call to the UBSE daemon via IPC.
func ubseInvokeCall(cmd uint32, request []byte) ([]byte, error) {
	// Check if socket exists
	if _, err := os.Stat(UbseIpcSocketPath); os.IsNotExist(err) {
		return nil, fmt.Errorf("ubse daemon not running: %v", err)
	}

	// Connect to the socket
	conn, err := net.Dial("unix", UbseIpcSocketPath)
	if err != nil {
		return nil, fmt.Errorf("failed to connect to ubse daemon: %v", err)
	}
	defer conn.Close()

	// Set read timeout to avoid hanging
	switch c := conn.(type) {
	case *net.TCPConn:
		c.SetReadDeadline(time.Now().Add(5 * time.Second))
	case *net.UnixConn:
		c.SetReadDeadline(time.Now().Add(5 * time.Second))
	}

	// Prepare header: 4 bytes cmd + 4 bytes length
	header := make([]byte, 8)
	binary.LittleEndian.PutUint32(header[0:], cmd)
	binary.LittleEndian.PutUint32(header[4:], uint32(len(request)))

	// Send header
	if _, err := conn.Write(header); err != nil {
		return nil, fmt.Errorf("failed to send header: %v", err)
	}

	// Send request
	if _, err := conn.Write(request); err != nil {
		return nil, fmt.Errorf("failed to send request: %v", err)
	}

	// Read response header with proper error handling
	responseHeader := make([]byte, 12) // 3 * 4 bytes (control code, align code, length)
	bytesRead := 0
	for bytesRead < 12 {
		n, err := conn.Read(responseHeader[bytesRead:])
		if err != nil {
			return nil, fmt.Errorf("failed to read response header: %v", err)
		}
		bytesRead += n
	}

	// Parse response header according to ubse_serial_util.cpp
	controlCode := binary.LittleEndian.Uint32(responseHeader[0:])
	alignCode := binary.LittleEndian.Uint32(responseHeader[4:])
	responseLen := binary.LittleEndian.Uint32(responseHeader[8:])

	// Check control code
	if controlCode != 0x01 { // HEAD_CTRL_CODE from ubse_serial_util.cpp
		return nil, fmt.Errorf("invalid control code: %d", controlCode)
	}

	// Check align code
	alignBase := alignCode + 1
	if alignBase != 1 && alignBase != 2 && alignBase != 4 && alignBase != 8 {
		return nil, fmt.Errorf("invalid align code: %d", alignCode)
	}

	// Check length
	if responseLen == 0 || responseLen%alignBase != 0 {
		return nil, fmt.Errorf("invalid response length: %d", responseLen)
	}

	// Read response body with proper error handling
	response := make([]byte, responseLen)
	bytesRead = 0
	for bytesRead < int(responseLen) {
		n, err := conn.Read(response[bytesRead:])
		if err != nil {
			return nil, fmt.Errorf("failed to read response: %v", err)
		}
		bytesRead += n
	}

	// Check for error
	if len(response) >= 4 {
		errorCode := binary.LittleEndian.Uint32(response[0:])
		if errorCode != UbsSuccess {
			return nil, fmt.Errorf("ubse daemon returned error: %d", errorCode)
		}
		// Skip error code
		response = response[4:]
	}

	return response, nil
}

// ubseUrmaDevUnpack unpacks the device information from the response.
func ubseUrmaDevUnpack(response []byte) ([]Device, error) {
	if len(response) < 4 {
		return nil, fmt.Errorf("invalid response length")
	}

	count := binary.LittleEndian.Uint32(response[0:])
	response = response[4:]

	devices := make([]Device, 0, count)
	for i := uint32(0); i < count; i++ {
		if len(response) < UbsUrmaNameMax+8+4 {
			return nil, fmt.Errorf("invalid device information length")
		}

		name := string(response[0:UbsUrmaNameMax])
		// Trim null terminator
		if idx := len(name); idx > 0 {
			if nullIdx := 0; nullIdx < idx && name[nullIdx] == '\x00' {
				name = name[:nullIdx]
			}
		}

		healthy := binary.LittleEndian.Uint32(response[UbsUrmaNameMax:]) == 0
		hwResId := binary.LittleEndian.Uint64(response[UbsUrmaNameMax+4:])

		devices = append(devices, Device{
			Name:    name,
			Healthy: healthy,
			HwResId: hwResId,
		})

		response = response[UbsUrmaNameMax+12:]
	}

	return devices, nil
}

// ubseUrmaDevInfoUnpack unpacks the device info from the response.
func ubseUrmaDevInfoUnpack(response []byte) (DeviceInfo, error) {
	if len(response) < UbsMaxUrmaPathLength*3 {
		return DeviceInfo{}, fmt.Errorf("invalid response length")
	}

	bondingPath := string(response[0:UbsMaxUrmaPathLength])
	// Trim null terminator
	if idx := len(bondingPath); idx > 0 {
		if nullIdx := 0; nullIdx < idx && bondingPath[nullIdx] == '\x00' {
			bondingPath = bondingPath[:nullIdx]
		}
	}

	bondingEid := string(response[UbsMaxUrmaPathLength : UbsMaxUrmaPathLength*2])
	// Trim null terminator
	if idx := len(bondingEid); idx > 0 {
		if nullIdx := 0; nullIdx < idx && bondingEid[nullIdx] == '\x00' {
			bondingEid = bondingEid[:nullIdx]
		}
	}

	vfePath1 := string(response[UbsMaxUrmaPathLength*2 : UbsMaxUrmaPathLength*3])
	// Trim null terminator
	if idx := len(vfePath1); idx > 0 {
		if nullIdx := 0; nullIdx < idx && vfePath1[nullIdx] == '\x00' {
			vfePath1 = vfePath1[:nullIdx]
		}
	}

	vfePath2 := string(response[UbsMaxUrmaPathLength*3:])
	// Trim null terminator
	if idx := len(vfePath2); idx > 0 {
		if nullIdx := 0; nullIdx < idx && vfePath2[nullIdx] == '\x00' {
			vfePath2 = vfePath2[:nullIdx]
		}
	}

	return DeviceInfo{
		VfePaths:    []string{vfePath1, vfePath2},
		BondingPath: bondingPath,
		BondingEid:  bondingEid,
	}, nil
}

// ubseUrmaQosUnpack unpacks the QoS information from the response.
func ubseUrmaQosUnpack(response []byte) (uint32, uint32, error) {
	if len(response) < 8 {
		return 0, 0, fmt.Errorf("invalid response length")
	}

	minBandwidth := binary.LittleEndian.Uint32(response[0:])
	maxBandwidth := binary.LittleEndian.Uint32(response[4:])

	return minBandwidth, maxBandwidth, nil
}
