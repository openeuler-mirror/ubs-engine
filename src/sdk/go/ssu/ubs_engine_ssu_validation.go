/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

// Package ssu 提供SSU(存储服务单元)的Go客户端SDK校验功能。
package ssu

import "fmt"

// ==================== 参数校验函数 ====================
// validateBufferString 校验字符串非空且内容长度不超过 bufferSize-1。
// 参数 bufferSize 为 C++ 侧 char[] 缓冲区总容量(含结尾字符'\0'),
// 因此实际允许的字符串内容最大长度为 bufferSize-1。
// 例如: UbsSsuMaxNameLength=48 表示 char name[48], 实际内容最多 47 字节。
func validateBufferString(s string, bufferSize int, fieldName string) error {
	if s == "" {
		return fmt.Errorf("%s is empty", fieldName)
	}
	if len(s) >= bufferSize {
		return fmt.Errorf("%s length %d exceeds maximum %d (buffer size %d)",
			fieldName, len(s), bufferSize-1, bufferSize)
	}
	return nil
}

// validateGuid 校验 GUID 非空且长度不超过 maxLen。
// 与 validateBufferString 不同: GUID 常量(UbsSsuMaxGuidLength=32)不含结尾字符'\0',
// 因此直接用 len > maxLen 判断, 允许长度 1~maxLen。
func validateGuid(guid string, fieldName string) error {
	if guid == "" {
		return fmt.Errorf("%s is empty", fieldName)
	}
	if len(guid) > UbsSsuMaxGuidLength {
		return fmt.Errorf("%s length %d exceeds maximum %d", fieldName, len(guid), UbsSsuMaxGuidLength)
	}
	return nil
}

// validateName 校验名称参数是否合法 (buffer size: UbsSsuMaxNameLength=48, 含'\0')
func validateName(name string) error {
	return validateBufferString(name, UbsSsuMaxNameLength, "name")
}

// validateNqn 校验NQN参数是否合法 (buffer size: UbsSsuMaxNqnLength=69, 含'\0')
func validateNqn(nqn string) error {
	return validateBufferString(nqn, UbsSsuMaxNqnLength, "nqn")
}

// validateDevName 校验设备名称参数是否合法 (buffer size: UbsSsuMaxDevNameLength=33, 含'\0')
func validateDevName(devName string) error {
	return validateBufferString(devName, UbsSsuMaxDevNameLength, "dev_name")
}

// validateEid 校验EID参数是否合法 (buffer size: UbsSsuMaxEidLength=17, 含'\0')
func validateEid(eid string) error {
	return validateBufferString(eid, UbsSsuMaxEidLength, "eid")
}

// validateTenant 校验租户隔离标识参数是否合法 (buffer size: UbsSsuMaxTenantLength=17, 含'\0')
func validateTenant(tenant string) error {
	return validateBufferString(tenant, UbsSsuMaxTenantLength, "tenant")
}

// validateAllocSpaceReq 校验分配存储空间请求参数是否合法
func validateAllocSpaceReq(req UbsSsuAllocSpaceReq) error {
	if err := validateName(req.Name); err != nil {
		return err
	}
	if req.NsNum == 0 {
		return fmt.Errorf("ns_num must be greater than 0")
	}
	if req.NsSize == 0 {
		return fmt.Errorf("ns_size must be greater than 0")
	}
	if req.LbaFormat != Fmt512 && req.LbaFormat != Fmt4K {
		return fmt.Errorf("invalid lba_format: %d", req.LbaFormat)
	}
	if req.Strategy != Striped && req.Strategy != Linear {
		return fmt.Errorf("invalid strategy: %d", req.Strategy)
	}
	return validateTenant(req.Tenant)
}

// validateStripedSpaceReq 校验条带化空间请求参数是否合法
func validateStripedSpaceReq(req UbsSsuStripedSpaceReq) error {
	if err := validateName(req.Name); err != nil {
		return err
	}
	if err := validateNqn(req.Nqn); err != nil {
		return err
	}
	if err := validateEid(req.SrcEid); err != nil {
		return err
	}
	if err := validateDevName(req.DevName); err != nil {
		return err
	}
	if req.Level != Raid0 && req.Level != Raid5 {
		return fmt.Errorf("invalid raid level")
	}
	validChunkSizes := map[UbsSsuChunkSize]bool{
		Size4K:   true,
		Size16K:  true,
		Size32K:  true,
		Size64K:  true,
		Size128K: true,
		Size256K: true,
		Size512K: true,
	}
	if !validChunkSizes[req.ChunkSize] {
		return fmt.Errorf("invalid chunk size")
	}
	return nil
}

// validateFeDeviceAllocParams 校验FE设备分配请求参数是否合法
func validateFeDeviceAllocParams(vfe *UbsUbVfe, busInstanceGuid string) error {
	if vfe == nil {
		return fmt.Errorf("vfe is nil")
	}
	return nil
}

// validateFeDeviceFreeParams 校验FE设备释放请求参数是否合法
func validateFeDeviceFreeParams(vfe *UbsUbVfe) error {
	if vfe == nil {
		return fmt.Errorf("vfe is nil")
	}
	return validateGuid(vfe.BindBusInstanceGuid, "bind_bus_instance_guid")
}
