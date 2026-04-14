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
	UbseModuleCode    = 0x0005
	UbseUrmaDevGet    = 0x0005
	UbseUrmaDevAlloc  = 0x0001
	UbseUrmaDevFree   = 0x0007
	UbseUrmaQosSet    = 0x0004
	UbseUrmaQosGet    =  0x0002
	UbseUrmaQosReset  = 0x0003
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
	// According to ubs_urma_dev_get in ubs_engine_urma.cpp
	// request_buffer.length = sizeof(uint32_t)
	// request_buffer.buffer is allocated but not initialized
	request := make([]byte, 4) // sizeof(uint32_t)
	response, err := ubseInvokeCall(UbseModuleCode, UbseUrmaDevGet, request)
	if err != nil {
		return nil, err
	}

	return ubseUrmaDevUnpack(response)
}

// UbsGetSharedDevice gets the shared urma device list.
func UbsGetSharedDevice() ([]Device, error) {
	// According to ubs_urma_dev_get in ubs_engine_urma.cpp
	// request_buffer.length = sizeof(uint32_t)
	// request_buffer.buffer is allocated but not initialized
	request := make([]byte, 4) // sizeof(uint32_t)
	response, err := ubseInvokeCall(UbseModuleCode, UbseUrmaDevGet, request)
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

	response, err := ubseInvokeCall(UbseModuleCode, UbseUrmaDevAlloc, []byte(name+"\x00"))
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

	_, err := ubseInvokeCall(0, UbseUrmaDevFree, []byte(name+"\x00"))
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

	_, err := ubseInvokeCall(0, UbseUrmaQosSet, request)
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

	response, err := ubseInvokeCall(0, UbseUrmaQosGet, []byte(name+"\x00"))
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

	_, err := ubseInvokeCall(0, UbseUrmaQosReset, []byte(name+"\x00"))
	return err
}

// ubseInvokeCall invokes a call to the UBSE daemon via IPC.
func ubseInvokeCall(moduleCode, opCode uint16, request []byte) ([]byte, error) {
	const MaxMessageSize = 10 * 1024 * 1024 // 10M
	
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

	// Prepare request message according to SerializeRequestMessage function
	// Message format: isResp (1 byte) + request header (16 bytes) + request body
	message := make([]byte, 1+16+len(request))
	
	// Set isResp flag to false (0)
	message[0] = 0
	
	// Set request header
	binary.LittleEndian.PutUint16(message[1:], moduleCode)
	binary.LittleEndian.PutUint16(message[3:], opCode)
	binary.LittleEndian.PutUint32(message[5:], uint32(len(request)))
	
	// Copy request body
	if len(request) > 0 {
		copy(message[17:], request)
	}

	// Send entire message at once
	if _, err := conn.Write(message); err != nil {
		return nil, fmt.Errorf("failed to send request message: %v", err)
	}

	// Read response message header according to SerializeResponseMessage function
	// Message format: isResp (1 byte) + response header (16 bytes)
	responseMessageHeader := make([]byte, 1+16)
	bytesRead := 0
	for bytesRead < len(responseMessageHeader) {
		n, err := conn.Read(responseMessageHeader[bytesRead:])
		if err != nil {
			return nil, fmt.Errorf("failed to read response message header: %v", err)
		}
		bytesRead += n
	}

	// Check isResp flag
	isResp := responseMessageHeader[0]
	if isResp != 1 {
		return nil, fmt.Errorf("invalid response message: isResp flag is not set")
	}

	// Parse response header
	statusCode := binary.LittleEndian.Uint32(responseMessageHeader[1:])
	responseLen := binary.LittleEndian.Uint32(responseMessageHeader[5:])
	// clientRequestId is at responseMessageHeader[9:], but we don't need it for now

	// Check status code
	if statusCode != UbsSuccess {
		return nil, fmt.Errorf("ubse daemon returned error: %d", statusCode)
	}

	// Check if response body length exceeds maximum message size
	if responseLen > MaxMessageSize {
		return nil, fmt.Errorf("response body length %d exceeds maximum size %d", responseLen, MaxMessageSize)
	}

	// Read response body
	response := make([]byte, responseLen)
	
	bytesRead = 0
	for bytesRead < int(responseLen) {
		n, err := conn.Read(response[bytesRead:])
		if err != nil {
			return nil, fmt.Errorf("failed to read response: %v", err)
		}
		bytesRead += n
	}
	fmt.Println("response length ", responseLen, bytesRead)
	return response, nil
}

// WriteResponseToFile writes the response to a file.
func WriteResponseToFile(response []byte, filePath string) error {
	// Open or create the file
	file, err := os.Create(filePath)
	if err != nil {
		return fmt.Errorf("failed to create file: %v", err)
	}
	defer file.Close()

	// Write the response to the file
	n, err := file.Write(response)
	if err != nil {
		return fmt.Errorf("failed to write to file: %v", err)
	}

	if n != len(response) {
		return fmt.Errorf("failed to write all data: wrote %d bytes, expected %d bytes", n, len(response))
	}

	return nil
}

// ubseUrmaDevUnpack unpacks the device information from the response.
func ubseUrmaDevUnpack(response []byte) ([]Device, error) {
	WriteResponseToFile(response, "test.data")
	if len(response) < 4 {
		return nil, fmt.Errorf("invalid response length")
	}

	count := binary.LittleEndian.Uint32(response[0:])
	fmt.Println("Count is ", count)
	response = response[4:]

	devices := make([]Device, 0, count)
	fmt.Println("response length ", len(response))
	for i := uint32(0); i < count; i++ {
		fmt.Println("response length ", len(response))

		// Parse device name (string) - according to unpack_string function
		if len(response) < 4 {
			return nil, fmt.Errorf("invalid device name length")
		}
		strLen := binary.LittleEndian.Uint32(response[0:])
		response = response[4:]

		if strLen > UbsUrmaNameMax || int(strLen) > len(response) {
			return nil, fmt.Errorf("invalid device name length")
		}

		name := string(response[0:strLen])
		// Move past the string
		response = response[strLen:]

		// Parse healthy status (uint32)
		healthy := binary.LittleEndian.Uint32(response[0:]) == 0

		// Parse hardware resource ID (uint64)
		hwResId := binary.LittleEndian.Uint64(response[4:])
		fmt.Println("device is  ", name, healthy, hwResId)
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
