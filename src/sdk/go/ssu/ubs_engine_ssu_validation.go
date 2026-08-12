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

// validateOptionalString 校验可选字符串: 允许为空, 非空时长度不超过 bufferSize-1。
func validateOptionalString(s string, bufferSize int, fieldName string) error {
	if s == "" {
		return nil
	}
	if len(s) >= bufferSize {
		return fmt.Errorf("%s length %d exceeds maximum %d (buffer size %d)",
			fieldName, len(s), bufferSize-1, bufferSize)
	}
	return nil
}

// validateGuid 校验 GUID 非空且长度精确等于 UbsSsuMaxGuidLength。
// GUID 为定长32字节数组，长度必须精确匹配。
func validateGuid(guid string, fieldName string) error {
	if guid == "" {
		return fmt.Errorf("%s is empty", fieldName)
	}
	if len(guid) != UbsSsuMaxGuidLength {
		return fmt.Errorf("The length %s is %d, not %d", fieldName, len(guid), UbsSsuMaxGuidLength)
	}
	return nil
}

// isASCIILetterOrDigit 检查字符是否为 ASCII 字母或数字，避免 Unicode locale 差异。
func isASCIILetterOrDigit(c rune) bool {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
}

// validateName 校验名称参数是否合法 (buffer size: UbsSsuMaxNameLength=48, 含'\0'), 仅允许[a-zA-Z0-9_-.:]
func validateName(name string) error {
	if err := validateBufferString(name, UbsSsuMaxNameLength, "name"); err != nil {
		return err
	}
	for _, c := range name {
		if !isASCIILetterOrDigit(c) && c != '_' && c != '-' && c != ':' && c != '.' {
			return fmt.Errorf("name contains invalid character '%c'", c)
		}
	}
	return nil
}

// validateNqn 校验NQN参数是否合法: 允许为空, 非空时长度不超过 UbsSsuMaxNqnLength-1
func validateNqn(nqn string) error {
	return validateOptionalString(nqn, UbsSsuMaxNqnLength, "nqn")
}

// validateNqnNotEmpty 校验NQN参数是否合法: 不允许为空, 非空时长度不超过 UbsSsuMaxNqnLength-1
func validateNqnNotEmpty(nqn string) error {
	if nqn == "" {
		return fmt.Errorf("nqn is empty")
	}
	if len(nqn) >= UbsSsuMaxNqnLength {
		return fmt.Errorf("nqn length %d exceeds maximum %d (buffer size %d)",
			len(nqn), UbsSsuMaxNqnLength-1, UbsSsuMaxNqnLength)
	}
	return nil
}

// validateDevName 校验设备名称参数是否合法 (buffer size: UbsSsuMaxDevNameLength=33, 含'\0'), 仅允许[a-zA-Z0-9_/.-]
func validateDevName(devName string) error {
	if err := validateBufferString(devName, UbsSsuMaxDevNameLength, "dev_name"); err != nil {
		return err
	}
	for _, c := range devName {
		if !isASCIILetterOrDigit(c) && c != '_' && c != '/' && c != '.' && c != '-' {
			return fmt.Errorf("dev_name contains invalid character '%c'", c)
		}
	}
	return nil
}

// validateEid 校验EID参数是否合法: 允许为空, 非空时长度不超过 UbsSsuMaxEidLength-1
func validateEid(eid string) error {
	return validateOptionalString(eid, UbsSsuMaxEidLength, "eid")
}

// validateTenant 校验租户隔离标识参数是否合法: 允许为空, 非空时长度不超过 UbsSsuMaxTenantLength-1, 仅允许[a-zA-Z0-9_-.:]
func validateTenant(tenant string) error {
	if err := validateOptionalString(tenant, UbsSsuMaxTenantLength, "tenant"); err != nil {
		return err
	}
	if tenant == "" {
		return nil
	}
	for _, c := range tenant {
		if !isASCIILetterOrDigit(c) && c != '_' && c != '-' && c != ':' && c != '.' {
			return fmt.Errorf("tenant contains invalid character '%c'", c)
		}
	}
	return nil
}

// validateAllocSpaceReq 校验分配存储空间请求参数是否合法
func validateAllocSpaceReq(req UbsSsuAllocSpaceReq) error {
	if err := validateName(req.Name); err != nil {
		return err
	}
	if req.NsNum == 0 {
		return fmt.Errorf("ns_num must be greater than 0")
	}
	// ns_size 须为 1G 的整数倍
	const oneG = uint64(1024 * 1024 * 1024)
	if req.NsSize == 0 || req.NsSize%oneG != 0 {
		return fmt.Errorf("ns_size must be a multiple of 1G")
	}
	// 仅条带化策略时, ns_size 须能整除 ns_num
	if req.Strategy == Striped && req.NsSize%uint64(req.NsNum) != 0 {
		return fmt.Errorf("When using striping strategy only, ns_size must be divisible by ns_num")
	}
	if req.LbaFormat != Fmt512 && req.LbaFormat != Fmt4K {
		return fmt.Errorf("invalid lba_format: %d", req.LbaFormat)
	}
	if req.Strategy != Striped && req.Strategy != Linear {
		return fmt.Errorf("invalid strategy: %d", req.Strategy)
	}
	return validateTenant(req.Tenant)
}

// validateSpaceReq 校验空间挂载/卸载请求参数是否合法
func validateSpaceReq(req UbsSsuSpaceReq) error {
	if err := validateName(req.Name); err != nil {
		return err
	}
	if err := validateNqn(req.Nqn); err != nil {
		return err
	}
	if err := validateEid(req.SrcEid); err != nil {
		return err
	}
	return nil
}

// validateLinearSpaceReq 校验线性编址空间操作请求参数是否合法
func validateLinearSpaceReq(req UbsSsuLinearSpaceReq) error {
	if err := validateName(req.Name); err != nil {
		return err
	}
	if err := validateNqn(req.Nqn); err != nil {
		return err
	}
	if err := validateEid(req.SrcEid); err != nil {
		return err
	}
	return validateDevName(req.DevName)
}

// validateStripedSpaceReq 校验条带化空间请求参数是否合法
func validateStripedSpaceReq(req UbsSsuStripedSpaceReq) error {
	if err := validateLinearSpaceReq(UbsSsuLinearSpaceReq{
		Name:    req.Name,
		Nqn:     req.Nqn,
		SrcEid:  req.SrcEid,
		DevName: req.DevName,
	}); err != nil {
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

// validateDetachStripedSpaceReq 校验条带化空间卸载请求参数是否合法。
// 卸载操作无需校验 level/chunk_size，仅校验基本字段（name/nqn/eid/dev_name），
// 因此委托给 validateLinearSpaceReq 统一处理。
func validateDetachStripedSpaceReq(req UbsSsuStripedSpaceReq) error {
	return validateLinearSpaceReq(UbsSsuLinearSpaceReq{
		Name:    req.Name,
		Nqn:     req.Nqn,
		SrcEid:  req.SrcEid,
		DevName: req.DevName,
	})
}

// validateAccessPermission 校验访问权限操作参数是否合法
func validateAccessPermission(name string, nqn string) error {
	if err := validateName(name); err != nil {
		return err
	}
	return validateNqnNotEmpty(nqn)
}

// validateFeDeviceAllocParams 校验FE设备分配请求参数是否合法
func validateFeDeviceAllocParams(vfe *UbsUbVfe, busInstanceGuid string) error {
	if vfe == nil {
		return fmt.Errorf("vfe is nil")
	}
	// guid允许为空字符串(表示不传), 非空时长度必须为UbsSsuMaxGuidLength
	if busInstanceGuid != "" && len(busInstanceGuid) != UbsSsuMaxGuidLength {
		return fmt.Errorf("The length of guid is %d, not %d", len(busInstanceGuid), UbsSsuMaxGuidLength)
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
