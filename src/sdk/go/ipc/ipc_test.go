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

package ipc

import (
	"encoding/binary"
	"net"
	"testing"
	"time"
)

// TestBufferPool tests the buffer pool functionality
func TestBufferPool(t *testing.T) {
	pool := newBufferPool()

	// Test getting a buffer
	buf1 := pool.Get(1024)
	if len(buf1) != 1024 {
		t.Errorf("Expected buffer length 1024, got %d", len(buf1))
	}

	// Test getting a larger buffer
	buf2 := pool.Get(8192)
	if len(buf2) != 8192 {
		t.Errorf("Expected buffer length 8192, got %d", len(buf2))
	}

	// Test putting buffers back
	pool.Put(buf1)
	pool.Put(buf2)

	// Test reusing a buffer
	buf3 := pool.Get(1024)
	if len(buf3) != 1024 {
		t.Errorf("Expected buffer length 1024, got %d", len(buf3))
	}
	pool.Put(buf3)

	// Test that large buffers are not put back
	largeBuf := pool.Get(2 * 1024 * 1024) // 2MB
	pool.Put(largeBuf)
	// This buffer should not be reused
}

// TestConnectToUnixSocket tests the ConnectToUnixSocket function
func TestConnectToUnixSocket(t *testing.T) {
	// This test will likely fail in a test environment
	// since the UBSE daemon is not running
	_, err := ConnectToUnixSocket()
	// We expect an error since the socket probably doesn't exist
	if err == nil {
		t.Error("Expected error when connecting to non-existent socket, got nil")
	}
}

// mockConn is a mock net.Conn for testing
type mockConn struct {
	writeData []byte
	readData  []byte
	readIdx   int
}

func (m *mockConn) Read(b []byte) (n int, err error) {
	if m.readIdx >= len(m.readData) {
		return 0, io.EOF
	}
	n = copy(b, m.readData[m.readIdx:])
	m.readIdx += n
	return n, nil
}

func (m *mockConn) Write(b []byte) (n int, err error) {
	m.writeData = append(m.writeData, b...)
	return len(b), nil
}

func (m *mockConn) Close() error {
	return nil
}

func (m *mockConn) LocalAddr() net.Addr {
	return nil
}

func (m *mockConn) RemoteAddr() net.Addr {
	return nil
}

func (m *mockConn) SetDeadline(t time.Time) error {
	return nil
}

func (m *mockConn) SetReadDeadline(t time.Time) error {
	return nil
}

func (m *mockConn) SetWriteDeadline(t time.Time) error {
	return nil
}

// TestSendRequest tests the SendRequest function
func TestSendRequest(t *testing.T) {
	mock := &mockConn{}
	request := []byte("test request")

	err := SendRequest(mock, 1, 2, request)
	if err != nil {
		t.Errorf("SendRequest failed: %v", err)
	}

	// Check that data was written
	if len(mock.writeData) == 0 {
		t.Error("Expected data to be written, got empty")
	}
}

// TestReceiveResponse tests the ReceiveResponse function
func TestReceiveResponse(t *testing.T) {
	// Create a mock response
	// Format: isResp (1 byte) + status code (4 bytes) + response len (4 bytes) + clientRequestId (8 bytes) + response body
	responseData := make([]byte, 1+4+4+8+5)
	responseData[0] = 1                                         // isResp = true
	binary.LittleEndian.PutUint32(responseData[1:], UbsSuccess) // status code
	binary.LittleEndian.PutUint32(responseData[5:], 5)           // response length
	// clientRequestId is 8 bytes, set to 0
	copy(responseData[17:], []byte("hello")) // response body

	mock := &mockConn{
		readData: responseData,
	}

	response, err := ReceiveResponse(mock)
	if err != nil {
		t.Errorf("ReceiveResponse failed: %v", err)
	}

	if string(response) != "hello" {
		t.Errorf("Expected response 'hello', got '%s' (length: %d)", string(response), len(response))
	}
}

// TestBufferPoolEdgeCases tests edge cases for buffer pool
func TestBufferPoolEdgeCases(t *testing.T) {
	pool := newBufferPool()

	// Test getting a buffer with zero size
	buf := pool.Get(0)
	if len(buf) != 0 {
		t.Errorf("Expected buffer length 0, got %d", len(buf))
	}
	pool.Put(buf)

	// Test getting a very small buffer
	smallBuf := pool.Get(1)
	if len(smallBuf) != 1 {
		t.Errorf("Expected buffer length 1, got %d", len(smallBuf))
	}
	pool.Put(smallBuf)

	// Test putting back a nil buffer
	var nilBuf []byte
	pool.Put(nilBuf) // Should not panic
}

// TestReceiveResponseEdgeCases tests edge cases for ReceiveResponse
func TestReceiveResponseEdgeCases(t *testing.T) {
	// Test case: Invalid isResp flag
	invalidRespData := make([]byte, 1+4+4+8)
	invalidRespData[0] = 0 // isResp = false
	binary.LittleEndian.PutUint32(invalidRespData[1:], UbsSuccess)
	binary.LittleEndian.PutUint32(invalidRespData[5:], 0)

	mock := &mockConn{
		readData: invalidRespData,
	}

	_, err := ReceiveResponse(mock)
	if err == nil {
		t.Error("Expected error for invalid isResp flag, got nil")
	}

	// Test case: Non-success status code
	errorRespData := make([]byte, 1+4+4+8)
	errorRespData[0] = 1                                // isResp = true
	binary.LittleEndian.PutUint32(errorRespData[1:], 1) // Error code
	binary.LittleEndian.PutUint32(errorRespData[5:], 0)

	mock = &mockConn{
		readData: errorRespData,
	}

	_, err = ReceiveResponse(mock)
	if err == nil {
		t.Error("Expected error for non-success status code, got nil")
	}
}

// TestSendRequestEdgeCases tests edge cases for SendRequest
func TestSendRequestEdgeCases(t *testing.T) {
	mock := &mockConn{}

	// Test with nil request
	err := SendRequest(mock, 1, 2, nil)
	if err != nil {
		t.Errorf("SendRequest failed with nil request: %v", err)
	}

	// Test with empty request
	err = SendRequest(mock, 1, 2, []byte{})
	if err != nil {
		t.Errorf("SendRequest failed with empty request: %v", err)
	}
}

// TestInvokeCall tests the InvokeCall function
func TestInvokeCall(t *testing.T) {
	// Test error path (no daemon running)
	_, err := InvokeCall(1, 1, nil)
	if err == nil {
		t.Error("Expected error when UBSE daemon is not running, got nil")
	}

	// Test with empty request
	_, err = InvokeCall(1, 1, []byte{})
	if err == nil {
		t.Error("Expected error when UBSE daemon is not running, got nil")
	}
}

// TestConnectToUnixSocketEdgeCases tests edge cases for ConnectToUnixSocket
func TestConnectToUnixSocketEdgeCases(t *testing.T) {
	// This test will likely fail in a test environment
	// since the UBSE daemon is not running
	_, err := ConnectToUnixSocket()
	// We expect an error since the socket probably doesn't exist
	if err == nil {
		t.Error("Expected error when connecting to non-existent socket, got nil")
	}
}

// TestReceiveResponseWithLargeData tests ReceiveResponse with large data
func TestReceiveResponseWithLargeData(t *testing.T) {
	// Create a mock response with larger data
	responseBody := "This is a test response with some data"
	responseLen := uint32(len(responseBody))

	// Format: isResp (1 byte) + status code (4 bytes) + response len (4 bytes) + clientRequestId (8 bytes) + response body
	responseData := make([]byte, 1+4+4+8+responseLen)
	responseData[0] = 1                                          // isResp = true
	binary.LittleEndian.PutUint32(responseData[1:], UbsSuccess)  // status code
	binary.LittleEndian.PutUint32(responseData[5:], responseLen) // response length
	// clientRequestId is 8 bytes, set to 0
	copy(responseData[17:], []byte(responseBody)) // response body

	mock := &mockConn{
		readData: responseData,
	}

	response, err := ReceiveResponse(mock)
	if err != nil {
		t.Errorf("ReceiveResponse failed: %v", err)
	}

	if string(response) != responseBody {
		t.Errorf("Expected response %q, got %q", responseBody, string(response))
	}
}

// TestReceiveResponseWithError tests ReceiveResponse with error status code
func TestReceiveResponseWithError(t *testing.T) {
	// Create a mock response with error status code
	errorCode := uint32(1)

	// Format: isResp (1 byte) + status code (4 bytes) + response len (4 bytes) + clientRequestId (8 bytes)
	responseData := make([]byte, 1+4+4+8)
	responseData[0] = 1                                        // isResp = true
	binary.LittleEndian.PutUint32(responseData[1:], errorCode) // error code
	binary.LittleEndian.PutUint32(responseData[5:], 0)         // response length
	// clientRequestId is 8 bytes, set to 0

	mock := &mockConn{
		readData: responseData,
	}

	_, err := ReceiveResponse(mock)
	if err == nil {
		t.Error("Expected error for error status code, got nil")
	}
}

// TestReceiveResponseWithInvalidIsResp tests ReceiveResponse with invalid isResp flag
func TestReceiveResponseWithInvalidIsResp(t *testing.T) {
	// Create a mock response with invalid isResp flag

	// Format: isResp (1 byte) + status code (4 bytes) + response len (4 bytes) + clientRequestId (8 bytes)
	responseData := make([]byte, 1+4+4+8)
	responseData[0] = 0                                         // isResp = false (invalid)
	binary.LittleEndian.PutUint32(responseData[1:], UbsSuccess) // status code
	binary.LittleEndian.PutUint32(responseData[5:], 0)          // response length
	// clientRequestId is 8 bytes, set to 0

	mock := &mockConn{
		readData: responseData,
	}

	_, err := ReceiveResponse(mock)
	if err == nil {
		t.Error("Expected error for invalid isResp flag, got nil")
	}
}

// TestSendRequestWithLargeData tests SendRequest with large data
func TestSendRequestWithLargeData(t *testing.T) {
	mock := &mockConn{}
	largeRequest := make([]byte, 1024) // 1KB of data
	for i := range largeRequest {
		largeRequest[i] = byte(i % 256)
	}

	err := SendRequest(mock, 1, 2, largeRequest)
	if err != nil {
		t.Errorf("SendRequest failed with large data: %v", err)
	}

	// Check that data was written
	if len(mock.writeData) == 0 {
		t.Error("Expected data to be written, got empty")
	}
}

// TestBufferPoolWithVariousSizes tests buffer pool with various sizes
func TestBufferPoolWithVariousSizes(t *testing.T) {
	pool := newBufferPool()

	// Test different buffer sizes
	sizes := []int{0, 1, 100, 1024, 4096, 8192, 16384}
	for _, size := range sizes {
		buf := pool.Get(size)
		if len(buf) != size {
			t.Errorf("Expected buffer length %d, got %d", size, len(buf))
		}
		pool.Put(buf)
	}

	// Test that very large buffers are not reused
	largeSize := 2 * 1024 * 1024 // 2MB
	largeBuf := pool.Get(largeSize)
	if len(largeBuf) != largeSize {
		t.Errorf("Expected buffer length %d, got %d", largeSize, len(largeBuf))
	}
	pool.Put(largeBuf)

	// Test that we still get a valid buffer after putting back a large one
	smallBuf := pool.Get(100)
	if len(smallBuf) != 100 {
		t.Errorf("Expected buffer length 100, got %d", len(smallBuf))
	}
	pool.Put(smallBuf)
}

// TestSetSocketPath tests the SetSocketPath function
func TestSetSocketPath(t *testing.T) {
	// Save original path and restore after test
	originalPath := GetSocketPath()
	defer SetSocketPath(originalPath)

	// Test setting a custom path
	customPath := "/tmp/test_ubse.sock"
	SetSocketPath(customPath)
	if got := GetSocketPath(); got != customPath {
		t.Errorf("Expected socket path %q, got %q", customPath, got)
	}

	// Test that empty path restores the default
	SetSocketPath("")
	if got := GetSocketPath(); got != UbseIpcSocketPath {
		t.Errorf("Expected default socket path %q, got %q", UbseIpcSocketPath, got)
	}
}

// TestSetSocketPathConcurrent tests that SetSocketPath is safe for concurrent use
func TestSetSocketPathConcurrent(t *testing.T) {
	// Save original path and restore after test
	originalPath := GetSocketPath()
	defer SetSocketPath(originalPath)

	paths := []string{
		"/tmp/ubse_test_1.sock",
		"/tmp/ubse_test_2.sock",
		"/tmp/ubse_test_3.sock",
		"",
	}

	done := make(chan bool)
	for i := 0; i < 100; i++ {
		go func(idx int) {
			SetSocketPath(paths[idx%len(paths)])
			_ = GetSocketPath()
			done <- true
		}(i)
	}

	for i := 0; i < 100; i++ {
		<-done
	}
	// If we get here without panicking or deadlocking, the test passes
}
