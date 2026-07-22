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

// Package ssu 提供SSU(存储服务单元)的Go客户端SDK。
package ssu

import (
	"atomgit.com/openeuler/ubs-engine.git/src/sdk/go/ipc"
	"atomgit.com/openeuler/ubs-engine.git/src/sdk/go/pack"
)

const (
	UbsSsuMaxNameLength     = 48 // 请求标识最大48个字符, 含结尾字符'\0'
	UbsSsuMaxResultNameLen  = 32 // 结果名称最大32个字符, 含结尾字符'\0'
	UbsSsuMaxUserNameLength = 32 // 使用方进程运行用户名称最大长度, 含结尾字符'\0'
	UbsSsuMaxTenantLength   = 17 // 请求方UPI(租户隔离标识)最大长度, 含结尾字符'\0'
	UbsSsuMaxNqnLength      = 69 // NVMe NQN最大长度69个字符, 含结尾字符'\0'
	UbsSsuMaxEidLength      = 17 // EID最大长度, 含结尾字符'\0'
	UbsSsuMaxUuidLength     = 37 // UUID标准长度37个字符, 含结尾字符'\0'
	UbsSsuMaxDevPathLength  = 63 // 设备路径最大长度, 含结尾字符'\0'
	UbsSsuMaxDevNameLength  = 33 // 聚合块设备名称最大长度, 含结尾字符'\0'
	UbsSsuRaid5MinMemberNum = 3  // RAID5最少成员设备数
	UbsSsuMaxGuidLength     = 32 // GUID最大长度32个字符, 不含结尾字符'\0'

	// 列表元素数量上限, 防止恶意响应导致内存耗尽
	maxAllocResults = 1024
	maxNamespaces   = 1024
	maxNsStats      = 1024
	maxConnectInfos = 1024
	maxFeDevices    = 1024
	maxHostNqnList  = 1024
	maxDevPaths     = 1024
)

// ==================== 类型定义 ====================

// UbsSsuLbaFormat LBA格式
type UbsSsuLbaFormat uint32

const (
	Fmt512 UbsSsuLbaFormat = 512  // 512B
	Fmt4K  UbsSsuLbaFormat = 4096 // 4K
)

// UbsSsuAggregationRaidLevel 聚合RAID级别
type UbsSsuAggregationRaidLevel uint8

const (
	Raid0 UbsSsuAggregationRaidLevel = 0 // RAID0条带化
	Raid5 UbsSsuAggregationRaidLevel = 5 // RAID5条带化带校验
)

// UbsSsuChunkSize 条带化chunk大小(KB)
type UbsSsuChunkSize uint32

const (
	Size4K   UbsSsuChunkSize = 4
	Size16K  UbsSsuChunkSize = 16
	Size32K  UbsSsuChunkSize = 32
	Size64K  UbsSsuChunkSize = 64
	Size128K UbsSsuChunkSize = 128
	Size256K UbsSsuChunkSize = 256
	Size512K UbsSsuChunkSize = 512
)

// UbsSsuAllocStrategy 分配策略
type UbsSsuAllocStrategy uint8

const (
	Striped UbsSsuAllocStrategy = 0 // 分布式策略, 尽量从多个设备均等分配, 适用于条带化编址使用场景
	Linear  UbsSsuAllocStrategy = 1 // 顺序策略, 尽量从单个设备分配, 适用于线性编址使用场景
)

// ==================== 结构体类型, 与ubs_ssu_service.h保持一致 ====================

// UbsSsuNamespaceInfo 命名空间信息,
type UbsSsuNamespaceInfo struct {
	TgtEid           string          // Target EID
	TgtNqn           string          // 子系统NQN
	NsUuid           string          // 物理设备UUID
	NamespaceId      uint32          // 命名空间ID
	NsDevPath        string          // 命名空间设备路径
	NsSize           uint64          // 分配的容量, 单位字节
	LbaFormat        UbsSsuLbaFormat // LBA格式
	AllowHostNqnList []string        // 允许访问的Host NQN列表
}

// UbsSsuAllocResult 分配存储空间结果
type UbsSsuAllocResult struct {
	Name       string                // 请求标识, 最大48个字符
	Strategy   UbsSsuAllocStrategy   // 分配策略
	Namespaces []UbsSsuNamespaceInfo // 命名空间信息列表
}

// UbsSsuAllocSpaceReq 分配存储空间请求参数
type UbsSsuAllocSpaceReq struct {
	Name      string              // 请求标识, 最大48个字符
	NsSize    uint64              // 申请总容量, 单位字节, 条带化策略时需整除nsNum且整除后需为chunkSize的整数倍
	NsNum     uint32              // 命名空间数量, 等于1时strategy不生效
	LbaFormat UbsSsuLbaFormat     // LBA格式
	Strategy  UbsSsuAllocStrategy // 分配策略
	Tenant    string              // 请求方tenant(租户隔离标识)
}

// UbsSsuSpaceReq 挂载|卸载存储空间请求参数
type UbsSsuSpaceReq struct {
	Name   string // 需挂载|卸载的存储空间标识, 最大48个字符
	Nqn    string // Host 的 NVMe Qualified Name
	SrcEid string // 源EID
}

// UbsSsuLinearSpaceReq 挂载|卸载线性编址存储空间请求参数
type UbsSsuLinearSpaceReq struct {
	Name    string // 需挂载|卸载的存储空间标识, 最大48个字符
	Nqn     string // Host 的 NVMe Qualified Name
	SrcEid  string // 源EID
	DevName string // 聚合后的块设备名称, 由外部指定
}

// UbsSsuStripedSpaceReq 挂载|卸载条带化编址存储空间请求参数
type UbsSsuStripedSpaceReq struct {
	Name      string                     // 需挂载|卸载的存储空间标识, 最大48个字符
	Nqn       string                     // Host 的 NVMe Qualified Name
	SrcEid    string                     // 源EID
	DevName   string                     // 聚合后的块设备名称, 由外部指定
	Level     UbsSsuAggregationRaidLevel // RAID级别(RAID0或RAID5)
	ChunkSize UbsSsuChunkSize            // 条带化的chunk大小, 单位KB
}

// UbsUbVfe 虚拟功能单元(VFE)信息
type UbsUbVfe struct {
	SlotId              uint8  // 槽位ID
	ChipId              uint8  // 芯片ID
	DieId               uint8  // Die ID
	PfeId               uint16 // 物理功能单元ID
	VfeId               uint16 // 虚拟功能单元ID
	VfeGuid             string // vfe GUID
	BindBusInstanceGuid string // 绑定的总线实例GUID
}

// UbsSsuFe 功能单元(FE)信息，包含所属PFE及其下的VFE列表
type UbsSsuFe struct {
	SlotId  uint8      // 槽位ID
	ChipId  uint8      // 芯片ID
	DieId   uint8      // Die ID
	PfeId   uint16     // 物理功能单元ID
	PfeGuid string     // pfe GUID
	VfeList []UbsUbVfe // VFE列表
}

// UbsSsuConnectInfo 存储空间连接信息
type UbsSsuConnectInfo struct {
	SrcEid  string // Source EID
	TgtEid  string // Target EID
	TgtNqn  string // Target NQN
	HostNqn string // 默认NQN, 例: nqn.2024-01.com.huawei:uuid:12345678-...
	NsUuid  string // 物理设备UUID
	NsId    uint32 // 命名空间ID
}

// UbsSsuNsStats 存储空间状态
type UbsSsuNsStats struct {
	NsUuid    string // 物理设备UUID
	NsId      uint32 // 命名空间ID
	TotalSize uint64 // 总容量, 单位字节
	UsedSize  uint64 // 已用容量, 单位字节
}

// ==================== 主函数（核心业务逻辑） ====================

// UbsSsuListAllocInfo 列出所有已分配的存储空间信息
//
// 获取系统中所有已分配的SSU存储空间详细信息，包括命名空间列表、容量、LBA格式和使用类型等。
//
// 返回值：
//   - []UbsSsuAllocResult: 已分配空间信息列表
//   - error: 错误信息；成功返回 nil
func UbsSsuListAllocInfo() ([]UbsSsuAllocResult, error) {
	response, err := ipc.InvokeCall(UbseModuleCode, UbseSsuListAllocInfo, nil)
	if err != nil {
		return nil, err
	}
	return unpackAllocResultList(response)
}

// UbsSsuAllocSpace 分配SSU存储空间
//
// 根据请求参数分配指定数量和大小的命名空间，支持顺序分配和分布式分配两种策略。
//
// 参数：
//   - req: 分配请求参数，包含命名空间数量、容量、策略等
//
// 返回值：
//   - UbsSsuAllocResult: 分配结果，包含已分配的命名空间信息列表
//   - error: 错误信息；成功返回 nil
func UbsSsuAllocSpace(req UbsSsuAllocSpaceReq) (UbsSsuAllocResult, error) {
	if err := validateAllocSpaceReq(req); err != nil {
		return UbsSsuAllocResult{}, err
	}
	request := packAllocSpaceReq(req)
	response, err := ipc.InvokeCall(UbseModuleCode, UbseSsuAllocSpaceReq, request)
	if err != nil {
		return UbsSsuAllocResult{}, err
	}
	return unpackAllocResult(response)
}

// UbsSsuFreeSpace 释放已分配的存储空间
//
// 释放之前通过UbsSsuAllocSpace分配的存储空间及其关联的所有命名空间。
//
// 参数：
//   - name: 要释放的存储空间标识，与UbsSsuAllocSpace时的name参数一致
//
// 返回值：
//   - error: 错误信息；成功返回 nil
func UbsSsuFreeSpace(name string) error {
	if err := validateName(name); err != nil {
		return err
	}
	request := pack.NewBinaryPacker().PackString(name, UbsSsuMaxNameLength).Bytes()
	_, err := ipc.InvokeCall(UbseModuleCode, UbseSsuFreeSpaceReq, request)
	return err
}

// UbsSsuAddAccessPermission 添加存储空间访问权限
//
// 为指定的Host授予对已分配存储空间的访问权限，在Target侧将Host NQN添加到子系统的
// 允许主机列表中，使该Host可以通过NVMe-oF协议访问对应命名空间。
//
// 参数：
//   - name: 存储空间标识，与UbsSsuAllocSpace时的name参数一致
//   - nqn: Host的NVMe Qualified Name，标识被授权的主机
//
// 返回值：
//   - error: 错误信息；成功返回 nil
func UbsSsuAddAccessPermission(name string, nqn string) error {
	if err := validateName(name); err != nil {
		return err
	}
	if err := validateNqn(nqn); err != nil {
		return err
	}
	request := pack.NewBinaryPacker().
		PackString(name, UbsSsuMaxNameLength).
		PackString(nqn, UbsSsuMaxNqnLength).
		Bytes()
	_, err := ipc.InvokeCall(UbseModuleCode, UbseSsuAddAccessPermissionReq, request)
	return err
}

// UbsSsuRemoveAccessPermission 移除存储空间访问权限
//
// 撤销指定Host对已分配存储空间的访问权限，在Target侧将Host NQN从子系统的
// 允许主机列表中移除，使该Host无法再通过NVMe-oF协议访问对应命名空间。
//
// 参数：
//   - name: 存储空间标识，与UbsSsuAllocSpace时的name参数一致
//   - nqn: Host的NVMe Qualified Name，标识被撤销权限的主机
//
// 返回值：
//   - error: 错误信息；成功返回 nil
func UbsSsuRemoveAccessPermission(name string, nqn string) error {
	if err := validateName(name); err != nil {
		return err
	}
	if err := validateNqn(nqn); err != nil {
		return err
	}
	request := pack.NewBinaryPacker().
		PackString(name, UbsSsuMaxNameLength).
		PackString(nqn, UbsSsuMaxNqnLength).
		Bytes()
	_, err := ipc.InvokeCall(UbseModuleCode, UbseSsuRemoveAccessPermissionReq, request)
	return err
}

// UbsSsuAttachSpace 挂载已分配的存储空间
//
// 将指定的存储空间挂载到系统，使其可被主机访问。
//
// 参数：
//   - req: 挂载请求参数，包含存储空间标识、Host的NVMe Qualified Name和源EID
//
// 返回值：
//   - []string: 挂载后的命名空间设备路径列表
//   - error: 错误信息；成功返回 nil
func UbsSsuAttachSpace(req UbsSsuSpaceReq) ([]string, error) {
	if err := validateName(req.Name); err != nil {
		return nil, err
	}
	if err := validateNqn(req.Nqn); err != nil {
		return nil, err
	}
	if err := validateEid(req.SrcEid); err != nil {
		return nil, err
	}
	request := packSpaceReq(req)
	response, err := ipc.InvokeCall(UbseModuleCode, UbseSsuAttachSpaceReq, request)
	if err != nil {
		return nil, err
	}
	return unpackNsDevPathsResponse(response)
}

// UbsSsuDetachSpace 卸载已分配的存储空间
//
// 将指定的存储空间从系统卸载，释放设备占用。
//
// 参数：
//   - req: 卸载请求参数，包含存储空间标识、Host的NVMe Qualified Name和源EID
//
// 返回值：
//   - error: 错误信息；成功返回 nil
func UbsSsuDetachSpace(req UbsSsuSpaceReq) error {
	if err := validateName(req.Name); err != nil {
		return err
	}
	if err := validateNqn(req.Nqn); err != nil {
		return err
	}
	if err := validateEid(req.SrcEid); err != nil {
		return err
	}
	request := packSpaceReq(req)
	_, err := ipc.InvokeCall(UbseModuleCode, UbseSsuDetachSpaceReq, request)
	return err
}

// UbsSsuAttachLinearSpace 挂载线性编址的存储空间
//
// 将多个命名空间设备以线性拼接方式聚合为一个逻辑块设备并挂载。
//
// 参数：
//   - req: 挂载请求参数，包含存储空间标识、Host的NVMe Qualified Name、源EID和聚合后的块设备名称
//
// 返回值：
//   - []string: 挂载后的命名空间设备路径列表
//   - string: 挂载后的聚合设备路径
//   - error: 错误信息；成功返回 nil
func UbsSsuAttachLinearSpace(req UbsSsuLinearSpaceReq) ([]string, string, error) {
	if err := validateName(req.Name); err != nil {
		return nil, "", err
	}
	if err := validateNqn(req.Nqn); err != nil {
		return nil, "", err
	}
	if err := validateEid(req.SrcEid); err != nil {
		return nil, "", err
	}
	if err := validateDevName(req.DevName); err != nil {
		return nil, "", err
	}
	request := packLinearSpaceReq(req)
	response, err := ipc.InvokeCall(UbseModuleCode, UbseSsuAttachLinearSpaceReq, request)
	if err != nil {
		return nil, "", err
	}
	return unpackNsDevPathsAndDevPathResponse(response)
}

// UbsSsuDetachLinearSpace 卸载线性编址的存储空间
//
// 将线性聚合的块设备卸载并释放。
//
// 参数：
//   - req: 卸载请求参数，包含存储空间标识、Host的NVMe Qualified Name、源EID和聚合后的块设备名称
//
// 返回值：
//   - error: 错误信息；成功返回 nil
func UbsSsuDetachLinearSpace(req UbsSsuLinearSpaceReq) error {
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
	request := packLinearSpaceReq(req)
	_, err := ipc.InvokeCall(UbseModuleCode, UbseSsuDetachLinearSpaceReq, request)
	return err
}

// UbsSsuAttachStripedSpace 挂载条带化编址的存储空间
//
// 将多个命名空间设备以条带化方式聚合为一个逻辑块设备并挂载，支持RAID0和RAID5两种级别。
//
// 参数：
//   - req: 条带化挂载请求参数，包含存储空间标识、Host的NVMe Qualified Name、源EID、
//     聚合后的块设备名称、RAID级别和chunk大小
//
// 返回值：
//   - []string: 挂载后的命名空间设备路径列表
//   - string: 挂载后的聚合设备路径
//   - error: 错误信息；成功返回 nil
func UbsSsuAttachStripedSpace(req UbsSsuStripedSpaceReq) ([]string, string, error) {
	if err := validateStripedSpaceReq(req); err != nil {
		return nil, "", err
	}
	request := packStripedSpaceReq(req)
	response, err := ipc.InvokeCall(UbseModuleCode, UbseSsuAttachStripedSpaceReq, request)
	if err != nil {
		return nil, "", err
	}
	return unpackNsDevPathsAndDevPathResponse(response)
}

// UbsSsuDetachStripedSpace 卸载条带化编址的存储空间
//
// 将条带化聚合的块设备卸载并释放。
//
// 参数：
//   - req: 卸载请求参数，包含存储空间标识、Host的NVMe Qualified Name、源EID和聚合后的块设备名称
//
// 返回值：
//   - error: 错误信息；成功返回 nil
func UbsSsuDetachStripedSpace(req UbsSsuStripedSpaceReq) error {
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
	request := packStripedSpaceReq(req)
	_, err := ipc.InvokeCall(UbseModuleCode, UbseSsuDetachStripedSpaceReq, request)
	return err
}

// UbsSsuGetNsStats 获取存储空间的命名空间统计信息
//
// 查询指定存储空间下各命名空间的容量使用情况，包括总容量和已用容量。
//
// 参数：
//   - name: 存储空间标识
//
// 返回值：
//   - []UbsSsuNsStats: 命名空间统计信息列表
//   - error: 错误信息；成功返回 nil
func UbsSsuGetNsStats(name string) ([]UbsSsuNsStats, error) {
	if err := validateName(name); err != nil {
		return nil, err
	}
	request := pack.NewBinaryPacker().PackString(name, UbsSsuMaxNameLength).Bytes()
	response, err := ipc.InvokeCall(UbseModuleCode, UbseSsuGetNsStatsReq, request)
	if err != nil {
		return nil, err
	}
	return unpackNsStatsList(response)
}

// UbsSsuGetConnectInfo 获取存储空间的连接信息
//
// 查询指定存储空间在指定VFE上的NVMe连接信息，包括子系统NQN、Host NQN、命名空间ID等。
//
// 参数：
//   - name: 存储空间标识
//   - vfe: VFE信息，可选参数，如果指定vfe，连接信息里的src_eid为指定vfe的eid，
//     否则src_eid为host侧分配给ssu的fe的eid
//
// 返回值：
//   - []UbsSsuConnectInfo: 连接信息列表
//   - error: 错误信息；成功返回 nil
func UbsSsuGetConnectInfo(name string, vfe *UbsUbVfe) ([]UbsSsuConnectInfo, error) {
	if err := validateName(name); err != nil {
		return nil, err
	}
	request := packConnectInfoReq(name, vfe)
	response, err := ipc.InvokeCall(UbseModuleCode, UbseSsuGetConnectInfoReq, request)
	if err != nil {
		return nil, err
	}
	return unpackConnectInfoList(response)
}

// UbsSsuGetFeDeviceList 获取功能单元设备列表
//
// 查询系统中所有FE设备信息，包括每个PFE下的VFE列表。
//
// 返回值：
//   - []UbsSsuFe: FE设备信息列表
//   - error: 错误信息；成功返回 nil
func UbsSsuGetFeDeviceList() ([]UbsSsuFe, error) {
	response, err := ipc.InvokeCall(UbseModuleCode, UbseSsuGetFeDeviceListReq, nil)
	if err != nil {
		return nil, err
	}
	return unpackFeDeviceList(response)
}

// UbsSsuFeDeviceAlloc 将VFE绑定到虚拟机
//
// 将指定的虚拟功能单元绑定到目标虚拟机，使虚拟机可通过该VFE访问存储资源。
//
// 参数：
//   - upi: 租户隔离标识
//   - vfe: 要绑定的VFE信息
//   - busInstanceGuid: 总线实例GUID，标识目标虚拟机，长度为UbsSsuMaxGuidLength
//
// 返回值：
//   - string: 分配后的总线实例GUID
//   - error: 错误信息；成功返回 nil
func UbsSsuFeDeviceAlloc(upi uint32, vfe *UbsUbVfe, busInstanceGuid string) (string, error) {
	if err := validateFeDeviceAllocParams(vfe, busInstanceGuid); err != nil {
		return "", err
	}
	request := packFeDeviceAllocReq(upi, vfe, busInstanceGuid)
	response, err := ipc.InvokeCall(UbseModuleCode, UbseSsuFeDeviceAllocReq, request)
	if err != nil {
		return "", err
	}
	return unpackFeDeviceAllocResp(response)
}

// UbsSsuFeDeviceFree 释放VFE设备
//
// 将已分配的虚拟功能单元从目标虚拟机释放，回收VFE设备资源。
//
// 参数：
//   - upi: 租户隔离标识
//   - vfe: 要释放的VFE信息
//
// 返回值：
//   - error: 错误信息；成功返回 nil
func UbsSsuFeDeviceFree(upi uint32, vfe *UbsUbVfe) error {
	if err := validateFeDeviceFreeParams(vfe); err != nil {
		return err
	}
	request := packFeDeviceFreeReq(upi, vfe)
	_, err := ipc.InvokeCall(UbseModuleCode, UbseSsuFeDeviceFreeReq, request)
	return err
}
