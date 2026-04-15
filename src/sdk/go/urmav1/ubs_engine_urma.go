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
	"net"
	"os"
	"sync"
	"time"
)

// bufferPool 用于重用网络通信缓冲区
type bufferPool struct {
	pool sync.Pool
}

// newBufferPool 创建一个新的缓冲区池
func newBufferPool() *bufferPool {
	return &bufferPool{
		pool: sync.Pool{
			New: func() interface{} {
				// 默认创建一个 4KB 的缓冲区
				return make([]byte, 4096)
			},
		},
	}
}

// Get 从池中获取一个缓冲区
func (p *bufferPool) Get(size int) []byte {
	buf := p.pool.Get().([]byte)
	if cap(buf) < size {
		// 如果缓冲区不够大，创建一个新的
		buf = make([]byte, size)
	} else {
		// 否则重置缓冲区长度
		buf = buf[:size]
	}
	return buf
}

// Put 将缓冲区放回池中
func (p *bufferPool) Put(buf []byte) {
	// 只放回小于 1MB 的缓冲区，避免池过大
	if cap(buf) <= 1024*1024 {
		p.pool.Put(buf)
	}
}

// 全局缓冲区池
var bp = newBufferPool()

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
	response, err := ubseInvokeCall(UbseModuleCode, UbseUrmaDevGet, request)
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
	response, err := ubseInvokeCall(UbseModuleCode, UbseUrmaDevGet, request)
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

	response, err := ubseInvokeCall(UbseModuleCode, UbseUrmaDevAlloc, []byte(name+"\x00"))
	if err != nil {
		return DeviceInfo{}, err
	}

	return ubseUrmaDevInfoUnpack(response)
}

// connectToUnixSocket connects to the UBSE Unix domain socket.
// Returns a connection to the socket and an error if the connection fails.
func connectToUnixSocket() (net.Conn, error) {
	// Check if socket exists
	if _, err := os.Stat(UbseIpcSocketPath); os.IsNotExist(err) {
		return nil, fmt.Errorf("ubse daemon not running: %v", err)
	}

	// Connect to the socket
	conn, err := net.Dial("unix", UbseIpcSocketPath)
	if err != nil {
		return nil, fmt.Errorf("failed to connect to ubse daemon: %v", err)
	}

	// Set read and write timeouts to avoid hanging
	switch c := conn.(type) {
	case *net.TCPConn:
		c.SetReadDeadline(time.Now().Add(DefaultTimeout))
		c.SetWriteDeadline(time.Now().Add(DefaultTimeout))
	case *net.UnixConn:
		c.SetReadDeadline(time.Now().Add(DefaultTimeout))
		c.SetWriteDeadline(time.Now().Add(DefaultTimeout))
	}

	return conn, nil
}

// sendRequest sends the request message to the UBSE daemon.
// conn: The connection to the UBSE daemon.
// moduleCode: The module code for the request.
// opCode: The operation code for the request.
// request: The request body.
// Returns an error if the request fails.
func sendRequest(conn net.Conn, moduleCode, opCode uint16, request []byte) error {
	// Prepare request message according to SerializeRequestMessage function
	// Message format: isResp (1 byte) + request header (16 bytes) + request body
	messageSize := 1 + 16 + len(request)
	message := bp.Get(messageSize)
	defer bp.Put(message)

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
		return fmt.Errorf("failed to send request message: %v", err)
	}

	return nil
}

// receiveResponse receives the response message from the UBSE daemon.
// conn: The connection to the UBSE daemon.
// Returns the response body and an error if the reception fails.
func receiveResponse(conn net.Conn) ([]byte, error) {

	// Read response message header according to SerializeResponseMessage function
	// Message format: isResp (1 byte) + response header (16 bytes)
	headerSize := 1 + 16
	responseMessageHeader := bp.Get(headerSize)
	defer bp.Put(responseMessageHeader)

	bytesRead := 0
	for bytesRead < headerSize {
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

	return response, nil
}

// ubseInvokeCall invokes a call to the UBSE daemon via IPC.
// moduleCode: The module code for the request.
// opCode: The operation code for the request.
// request: The request body.
// Returns the response body and an error if the call fails.
func ubseInvokeCall(moduleCode, opCode uint16, request []byte) ([]byte, error) {
	// Connect to the socket
	conn, err := connectToUnixSocket()
	if err != nil {
		return nil, err
	}
	defer conn.Close()

	// Send request
	if err := sendRequest(conn, moduleCode, opCode, request); err != nil {
		return nil, err
	}

	// Receive response
	response, err := receiveResponse(conn)
	if err != nil {
		return nil, err
	}

	return response, nil
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
	for i := uint32(0); i < count; i++ {
		// Check if enough data remains for device info
		if len(response) < 4+12 { // string length + device info
			return nil, fmt.Errorf("insufficient data for device %d: expected at least 16 bytes, got %d", i, len(response))
		}

		name, response, err := unpackString(response, UbsUrmaNameMax)
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
