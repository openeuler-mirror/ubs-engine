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

package urmav1

import (
	"encoding/binary"
	"testing"
)

// TestUnpackString tests the unpackString function.
func TestUnpackString(t *testing.T) {
	tests := []struct {
		name      string
		strLen    uint32
		str       string
		maxLen    uint32
		expected  string
		expectErr bool
	}{
		{
			name:      "Valid string",
			strLen:    5,
			str:       "hello",
			maxLen:    10,
			expected:  "hello",
			expectErr: false,
		},
		{
			name:      "String length exceeds maxLen",
			strLen:    5,
			str:       "hello",
			maxLen:    3,
			expected:  "",
			expectErr: true,
		},
		{
			name:      "Empty string",
			strLen:    0,
			str:       "",
			maxLen:    10,
			expected:  "",
			expectErr: false,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			response := make([]byte, 4+len(tt.str))
			binary.LittleEndian.PutUint32(response[0:], tt.strLen)
			copy(response[4:], []byte(tt.str))

			result, remaining, err := unpackString(response, tt.maxLen)
			if (err != nil) != tt.expectErr {
				t.Errorf("unpackString() error = %v, expectErr %v", err, tt.expectErr)
				return
			}
			if result != tt.expected {
				t.Errorf("unpackString() = %q, expected %q", result, tt.expected)
			}
			if !tt.expectErr && len(remaining) != 0 {
				t.Errorf("unpackString() remaining = %d bytes, expected 0", len(remaining))
			}
		})
	}

	// Test case: Insufficient data
	t.Run("Insufficient data", func(t *testing.T) {
		response := make([]byte, 2)
		_, _, err := unpackString(response, 10)
		if err == nil {
			t.Error("Expected error for insufficient data, got nil")
		}
	})
}

// TestUbseUrmaDevUnpack tests the ubseUrmaDevUnpack function.
func TestUbseUrmaDevUnpack(t *testing.T) {
	// Create a test response with one device
	count := uint32(1)
	strLen := uint32(5)
	name := "test1"
	healthy := uint32(0) // healthy
	hwResId := uint64(12345)

	response := make([]byte, 4) // count
	binary.LittleEndian.PutUint32(response[0:], count)

	// Add device name
	nameBytes := make([]byte, 4+strLen)
	binary.LittleEndian.PutUint32(nameBytes[0:], strLen)
	copy(nameBytes[4:], []byte(name))
	response = append(response, nameBytes...)

	// Add healthy status
	healthyBytes := make([]byte, 4)
	binary.LittleEndian.PutUint32(healthyBytes[0:], healthy)
	response = append(response, healthyBytes...)

	// Add hardware resource ID
	hwResIdBytes := make([]byte, 8)
	binary.LittleEndian.PutUint64(hwResIdBytes[0:], hwResId)
	response = append(response, hwResIdBytes...)

	devices, err := ubseUrmaDevUnpack(response)
	if err != nil {
		t.Errorf("ubseUrmaDevUnpack failed: %v", err)
	}
	if len(devices) != int(count) {
		t.Errorf("Expected %d devices, got %d", count, len(devices))
	}
	if devices[0].Name != name {
		t.Errorf("Expected device name %q, got %q", name, devices[0].Name)
	}
	if devices[0].Healthy != (healthy == 0) {
		t.Errorf("Expected healthy status %v, got %v", healthy == 0, devices[0].Healthy)
	}
	if devices[0].HwResId != hwResId {
		t.Errorf("Expected hardware resource ID %d, got %d", hwResId, devices[0].HwResId)
	}

	// Test case: Empty device list
	t.Run("Empty device list", func(t *testing.T) {
		response := make([]byte, 4)
		binary.LittleEndian.PutUint32(response[0:], 0)
		devices, err := ubseUrmaDevUnpack(response)
		if err != nil {
			t.Errorf("ubseUrmaDevUnpack failed: %v", err)
		}
		if len(devices) != 0 {
			t.Errorf("Expected 0 devices, got %d", len(devices))
		}
	})

	// Test case: Insufficient data
	t.Run("Insufficient data", func(t *testing.T) {
		response := make([]byte, 2)
		_, err := ubseUrmaDevUnpack(response)
		if err == nil {
			t.Error("Expected error for insufficient data, got nil")
		}
	})
}

// TestUbseUrmaDevInfoUnpack tests the ubseUrmaDevInfoUnpack function.
func TestUbseUrmaDevInfoUnpack(t *testing.T) {
	// Create a test response with device info
	bondingPath := "bonding-path"
	vfePath1 := "vfe-path-1"
	vfePath2 := "vfe-path-2"
	bondingEid := "bonding-eid"

	response := make([]byte, 0)

	// Add bonding path
	bondingPathLen := uint32(len(bondingPath))
	bondingPathBytes := make([]byte, 4+bondingPathLen)
	binary.LittleEndian.PutUint32(bondingPathBytes[0:], bondingPathLen)
	copy(bondingPathBytes[4:], []byte(bondingPath))
	response = append(response, bondingPathBytes...)

	// Add VFE path 1 (currently parsed as bonding eid in the function)
	vfePath1Len := uint32(len(vfePath1))
	vfePath1Bytes := make([]byte, 4+vfePath1Len)
	binary.LittleEndian.PutUint32(vfePath1Bytes[0:], vfePath1Len)
	copy(vfePath1Bytes[4:], []byte(vfePath1))
	response = append(response, vfePath1Bytes...)

	// Add VFE path 2 (currently parsed as vfe path 1 in the function)
	vfePath2Len := uint32(len(vfePath2))
	vfePath2Bytes := make([]byte, 4+vfePath2Len)
	binary.LittleEndian.PutUint32(vfePath2Bytes[0:], vfePath2Len)
	copy(vfePath2Bytes[4:], []byte(vfePath2))
	response = append(response, vfePath2Bytes...)

	// Add bonding eid (currently parsed as vfe path 2 in the function)
	bondingEidLen := uint32(len(bondingEid))
	bondingEidBytes := make([]byte, 4+bondingEidLen)
	binary.LittleEndian.PutUint32(bondingEidBytes[0:], bondingEidLen)
	copy(bondingEidBytes[4:], []byte(bondingEid))
	response = append(response, bondingEidBytes...)

	deviceInfo, err := ubseUrmaDevInfoUnpack(response)
	if err != nil {
		t.Errorf("ubseUrmaDevInfoUnpack failed: %v", err)
	}
	if deviceInfo.BondingPath != bondingPath {
		t.Errorf("Expected bonding path %q, got %q", bondingPath, deviceInfo.BondingPath)
	}
	if deviceInfo.BondingEid != bondingEid {
		t.Errorf("Expected bonding eid %q, got %q", bondingEid, deviceInfo.BondingEid)
	}
	if len(deviceInfo.VfePaths) != 2 {
		t.Errorf("Expected 2 VFE paths, got %d", len(deviceInfo.VfePaths))
	}
	if deviceInfo.VfePaths[0] != vfePath1 {
		t.Errorf("Expected VFE path 1 %q, got %q", vfePath1, deviceInfo.VfePaths[0])
	}
	if deviceInfo.VfePaths[1] != vfePath2 {
		t.Errorf("Expected VFE path 2 %q, got %q", vfePath2, deviceInfo.VfePaths[1])
	}

	// Test case: Insufficient data
	t.Run("Insufficient data", func(t *testing.T) {
		response := make([]byte, 2)
		_, err := ubseUrmaDevInfoUnpack(response)
		if err == nil {
			t.Error("Expected error for insufficient data, got nil")
		}
	})
}

// TestUbsAllocateDevice tests the UbsAllocateDevice function
func TestUbsAllocateDevice(t *testing.T) {
	// Test with empty name
	_, err := UbsAllocateDevice("")
	if err == nil {
		t.Error("Expected error for empty name, got nil")
	}

	// Test with name that's too long
	longName := string(make([]byte, UbsUrmaNameMax))
	_, err = UbsAllocateDevice(longName)
	if err == nil {
		t.Error("Expected error for name too long, got nil")
	}

	// This test requires a running UBSE daemon
	// For simplicity, we'll skip the actual allocation test
	t.Skip("Skipping actual allocation test - requires running UBSE daemon")
}

// TestUbsGetVfeDevice tests the UbsGetVfeDevice function
func TestUbsGetVfeDevice(t *testing.T) {
	// Test error path (no daemon running)
	_, err := UbsGetVfeDevice()
	if err == nil {
		t.Error("Expected error when UBSE daemon is not running, got nil")
	}
}

// TestUbsGetSharedDevice tests the UbsGetSharedDevice function
func TestUbsGetSharedDevice(t *testing.T) {
	// Test error path (no daemon running)
	_, err := UbsGetSharedDevice()
	if err == nil {
		t.Error("Expected error when UBSE daemon is not running, got nil")
	}
}

// TestUbseUrmaDevUnpackEdgeCases tests edge cases for ubseUrmaDevUnpack
func TestUbseUrmaDevUnpackEdgeCases(t *testing.T) {
	// Test case: Empty device list
	emptyResponse := make([]byte, 4)
	binary.LittleEndian.PutUint32(emptyResponse[0:], 0)
	devices, err := ubseUrmaDevUnpack(emptyResponse)
	if err != nil {
		t.Errorf("ubseUrmaDevUnpack failed for empty list: %v", err)
	}
	if len(devices) != 0 {
		t.Errorf("Expected 0 devices, got %d", len(devices))
	}

	// Test case: Device count exceeds maximum
	largeCountResponse := make([]byte, 4)
	binary.LittleEndian.PutUint32(largeCountResponse[0:], 2048) // Exceeds MaxDevices (1024)
	_, err = ubseUrmaDevUnpack(largeCountResponse)
	if err == nil {
		t.Error("Expected error for large device count, got nil")
	}

	// Test case: Insufficient data for device info
	insufficientResponse := make([]byte, 4+4) // count + partial device info
	binary.LittleEndian.PutUint32(insufficientResponse[0:], 1)
	binary.LittleEndian.PutUint32(insufficientResponse[4:], 5) // string length
	_, err = ubseUrmaDevUnpack(insufficientResponse)
	if err == nil {
		t.Error("Expected error for insufficient data, got nil")
	}
}

// TestUbseUrmaDevInfoUnpackEdgeCases tests edge cases for ubseUrmaDevInfoUnpack
func TestUbseUrmaDevInfoUnpackEdgeCases(t *testing.T) {
	// Test case: Insufficient data
	insufficientResponse := make([]byte, 2)
	_, err := ubseUrmaDevInfoUnpack(insufficientResponse)
	if err == nil {
		t.Error("Expected error for insufficient data, got nil")
	}
}

// TestUbseUrmaDevInfoUnpackWithEmptyStrings tests ubseUrmaDevInfoUnpack with empty strings
func TestUbseUrmaDevInfoUnpackWithEmptyStrings(t *testing.T) {
	// Create a test response with empty strings
	response := make([]byte, 0)

	// Add empty bonding path
	bondingPathLen := uint32(0)
	bondingPathBytes := make([]byte, 4+bondingPathLen)
	binary.LittleEndian.PutUint32(bondingPathBytes[0:], bondingPathLen)
	response = append(response, bondingPathBytes...)

	// Add empty VFE path 1
	vfePath1Len := uint32(0)
	vfePath1Bytes := make([]byte, 4+vfePath1Len)
	binary.LittleEndian.PutUint32(vfePath1Bytes[0:], vfePath1Len)
	response = append(response, vfePath1Bytes...)

	// Add empty VFE path 2
	vfePath2Len := uint32(0)
	vfePath2Bytes := make([]byte, 4+vfePath2Len)
	binary.LittleEndian.PutUint32(vfePath2Bytes[0:], vfePath2Len)
	response = append(response, vfePath2Bytes...)

	// Add empty bonding eid
	bondingEidLen := uint32(0)
	bondingEidBytes := make([]byte, 4+bondingEidLen)
	binary.LittleEndian.PutUint32(bondingEidBytes[0:], bondingEidLen)
	response = append(response, bondingEidBytes...)

	deviceInfo, err := ubseUrmaDevInfoUnpack(response)
	if err != nil {
		t.Errorf("ubseUrmaDevInfoUnpack failed: %v", err)
	}
	if deviceInfo.BondingPath != "" {
		t.Errorf("Expected empty bonding path, got %q", deviceInfo.BondingPath)
	}
	if deviceInfo.BondingEid != "" {
		t.Errorf("Expected empty bonding eid, got %q", deviceInfo.BondingEid)
	}
	if len(deviceInfo.VfePaths) != 2 {
		t.Errorf("Expected 2 VFE paths, got %d", len(deviceInfo.VfePaths))
	}
	if deviceInfo.VfePaths[0] != "" {
		t.Errorf("Expected empty VFE path 1, got %q", deviceInfo.VfePaths[0])
	}
	if deviceInfo.VfePaths[1] != "" {
		t.Errorf("Expected empty VFE path 2, got %q", deviceInfo.VfePaths[1])
	}
}

// TestUbseUrmaDevInfoUnpackComprehensive tests ubseUrmaDevInfoUnpack with comprehensive data
func TestUbseUrmaDevInfoUnpackComprehensive(t *testing.T) {
	// Create a test response with device info
	bondingPath := "bonding-path-123"
	vfePath1 := "vfe-path-1"
	vfePath2 := "vfe-path-2"
	bondingEid := "bonding-eid-456"

	response := make([]byte, 0)

	// Add bonding path
	bondingPathLen := uint32(len(bondingPath))
	bondingPathBytes := make([]byte, 4+bondingPathLen)
	binary.LittleEndian.PutUint32(bondingPathBytes[0:], bondingPathLen)
	copy(bondingPathBytes[4:], []byte(bondingPath))
	response = append(response, bondingPathBytes...)

	// Add VFE path 1
	vfePath1Len := uint32(len(vfePath1))
	vfePath1Bytes := make([]byte, 4+vfePath1Len)
	binary.LittleEndian.PutUint32(vfePath1Bytes[0:], vfePath1Len)
	copy(vfePath1Bytes[4:], []byte(vfePath1))
	response = append(response, vfePath1Bytes...)

	// Add VFE path 2
	vfePath2Len := uint32(len(vfePath2))
	vfePath2Bytes := make([]byte, 4+vfePath2Len)
	binary.LittleEndian.PutUint32(vfePath2Bytes[0:], vfePath2Len)
	copy(vfePath2Bytes[4:], []byte(vfePath2))
	response = append(response, vfePath2Bytes...)

	// Add bonding eid
	bondingEidLen := uint32(len(bondingEid))
	bondingEidBytes := make([]byte, 4+bondingEidLen)
	binary.LittleEndian.PutUint32(bondingEidBytes[0:], bondingEidLen)
	copy(bondingEidBytes[4:], []byte(bondingEid))
	response = append(response, bondingEidBytes...)

	deviceInfo, err := ubseUrmaDevInfoUnpack(response)
	if err != nil {
		t.Errorf("ubseUrmaDevInfoUnpack failed: %v", err)
	}
	if deviceInfo.BondingPath != bondingPath {
		t.Errorf("Expected bonding path %q, got %q", bondingPath, deviceInfo.BondingPath)
	}
	if deviceInfo.BondingEid != bondingEid {
		t.Errorf("Expected bonding eid %q, got %q", bondingEid, deviceInfo.BondingEid)
	}
	if len(deviceInfo.VfePaths) != 2 {
		t.Errorf("Expected 2 VFE paths, got %d", len(deviceInfo.VfePaths))
	}
	if deviceInfo.VfePaths[0] != vfePath1 {
		t.Errorf("Expected VFE path 1 %q, got %q", vfePath1, deviceInfo.VfePaths[0])
	}
	if deviceInfo.VfePaths[1] != vfePath2 {
		t.Errorf("Expected VFE path 2 %q, got %q", vfePath2, deviceInfo.VfePaths[1])
	}
}

// TestUbseUrmaDevUnpackComprehensive tests ubseUrmaDevUnpack with comprehensive data
func TestUbseUrmaDevUnpackComprehensive(t *testing.T) {
	// Create a test response with a single device
	count := uint32(1)

	response := make([]byte, 4)
	binary.LittleEndian.PutUint32(response[0:], count)

	// Add device
	name := "device1"
	nameLen := uint32(len(name))
	nameBytes := make([]byte, 4+nameLen)
	binary.LittleEndian.PutUint32(nameBytes[0:], nameLen)
	copy(nameBytes[4:], []byte(name))
	response = append(response, nameBytes...)

	healthy := uint32(0) // healthy
	healthyBytes := make([]byte, 4)
	binary.LittleEndian.PutUint32(healthyBytes[0:], healthy)
	response = append(response, healthyBytes...)

	hwResId := uint64(12345)
	hwResIdBytes := make([]byte, 8)
	binary.LittleEndian.PutUint64(hwResIdBytes[0:], hwResId)
	response = append(response, hwResIdBytes...)

	devices, err := ubseUrmaDevUnpack(response)
	if err != nil {
		t.Errorf("ubseUrmaDevUnpack failed: %v", err)
	}
	if len(devices) != int(count) {
		t.Errorf("Expected %d devices, got %d", count, len(devices))
	}

	// Check device
	if devices[0].Name != name {
		t.Errorf("Expected device name %q, got %q", name, devices[0].Name)
	}
	if devices[0].Healthy != (healthy == 0) {
		t.Errorf("Expected device healthy status %v, got %v", healthy == 0, devices[0].Healthy)
	}
	if devices[0].HwResId != hwResId {
		t.Errorf("Expected device hardware resource ID %d, got %d", hwResId, devices[0].HwResId)
	}
}

// TestUnpackStringEdgeCases tests edge cases for unpackString
func TestUnpackStringEdgeCases(t *testing.T) {
	// Test case: Empty string
	emptyStringData := make([]byte, 4)
	binary.LittleEndian.PutUint32(emptyStringData[0:], 0)
	result, remaining, err := unpackString(emptyStringData, 10)
	if err != nil {
		t.Errorf("unpackString failed for empty string: %v", err)
	}
	if result != "" {
		t.Errorf("Expected empty string, got %q", result)
	}
	if len(remaining) != 0 {
		t.Errorf("Expected no remaining data, got %d bytes", len(remaining))
	}

	// Test case: String length exceeds maxLen
	longStringData := make([]byte, 4+5)
	binary.LittleEndian.PutUint32(longStringData[0:], 5)
	copy(longStringData[4:], []byte("hello"))
	_, _, err = unpackString(longStringData, 3)
	if err == nil {
		t.Error("Expected error for string length exceeding maxLen, got nil")
	}

	// Test case: String length exceeds available data
	insufficientData := make([]byte, 4+3)
	binary.LittleEndian.PutUint32(insufficientData[0:], 5)
	copy(insufficientData[4:], []byte("hel"))
	_, _, err = unpackString(insufficientData, 10)
	if err == nil {
		t.Error("Expected error for insufficient data, got nil")
	}

	// Test case: Insufficient data for string length
	noLengthData := make([]byte, 2)
	_, _, err = unpackString(noLengthData, 10)
	if err == nil {
		t.Error("Expected error for insufficient data for string length, got nil")
	}
}
