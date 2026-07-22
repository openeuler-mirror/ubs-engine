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

// Package ipc implements the common IPC communication with the UBSE daemon.
// It provides connect, send, receive, and invoke call functionalities
// over Unix domain sockets, and supports custom socket path configuration
// via SetSocketPath.
package ipc

import (
	"encoding/binary"
	"fmt"
	"net"
	"os"
	"sync"
	"time"
)

// IPC related constants
const (
	// UbseIpcSocketPath is the default Unix domain socket path for the UBSE daemon.
	UbseIpcSocketPath = "/var/run/ubse/ubse.sock"
	// MaxMessageSize is the maximum allowed message size for IPC communication.
	MaxMessageSize = 10 * 1024 * 1024 // 10M
	// DefaultTimeout is the default read/write timeout for IPC connections.
	DefaultTimeout = 30 * time.Second
)

// Status codes
const (
	// UbsSuccess indicates a successful operation.
	UbsSuccess = 0
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

// socketPathMu protects socketPath from concurrent read/write access.
var socketPathMu sync.RWMutex

// socketPath is the current Unix domain socket path used for IPC communication.
// It can be customized via SetSocketPath and defaults to UbseIpcSocketPath.
var socketPath = UbseIpcSocketPath

// SetSocketPath sets the Unix domain socket path for IPC communication.
// If path is empty, the default path (UbseIpcSocketPath) is restored.
// This function is safe for concurrent use.
// This mirrors the design of the C SDK's ubse_socket_path_set function.
func SetSocketPath(path string) {
	socketPathMu.Lock()
	defer socketPathMu.Unlock()
	if path == "" {
		socketPath = UbseIpcSocketPath
		return
	}
	socketPath = path
}

// GetSocketPath returns the current Unix domain socket path used for IPC communication.
// This function is safe for concurrent use.
func GetSocketPath() string {
	socketPathMu.RLock()
	defer socketPathMu.RUnlock()
	return socketPath
}

// ConnectToUnixSocket connects to the UBSE Unix domain socket.
// Returns a connection to the socket and an error if the connection fails.
func ConnectToUnixSocket() (net.Conn, error) {
	path := GetSocketPath()

	// Check if socket exists
	if _, err := os.Stat(path); os.IsNotExist(err) {
		return nil, fmt.Errorf("ubse daemon not running: %v", err)
	}

	// Connect to the socket
	conn, err := net.Dial("unix", path)
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

// SendRequest sends the request message to the UBSE daemon.
// conn: The connection to the UBSE daemon.
// moduleCode: The module code for the request.
// opCode: The operation code for the request.
// request: The request body.
// Returns an error if the request fails.
func SendRequest(conn net.Conn, moduleCode, opCode uint16, request []byte) error {
	if len(request) > MaxMessageSize {
		return fmt.Errorf("request body length %d exceeds maximum size %d", len(request), MaxMessageSize)
	}
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
	// Zero out reserved bytes (bytes 9-16) to prevent residual data leakage
	// from the buffer pool, which may contain clientRequestId from a previous
	// ReceiveResponse call.
	for i := 9; i < 17; i++ {
		message[i] = 0
	}
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

// ReceiveResponse receives the response message from the UBSE daemon.
// conn: The connection to the UBSE daemon.
// Returns the response body and an error if the reception fails.
func ReceiveResponse(conn net.Conn) ([]byte, error) {
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

// InvokeCall invokes a call to the UBSE daemon via IPC.
// moduleCode: The module code for the request.
// opCode: The operation code for the request.
// request: The request body.
// Returns the response body and an error if the call fails.
func InvokeCall(moduleCode, opCode uint16, request []byte) ([]byte, error) {
	// Connect to the socket
	conn, err := ConnectToUnixSocket()
	if err != nil {
		return nil, err
	}
	defer conn.Close()

	// Send request
	if err := SendRequest(conn, moduleCode, opCode, request); err != nil {
		return nil, err
	}

	// Receive response
	response, err := ReceiveResponse(conn)
	if err != nil {
		return nil, err
	}

	return response, nil
}
