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

package ssu

// ==================== 常量定义, 与C头文件ubs_engine_ssu.h保持一致 ====================

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

// UbsSsuNamespaceInfo 命名空间信息, 对应UbsSsuNameSpaceInfo
type UbsSsuNamespaceInfo struct {
	TgtEid      string          // Target EID
	TgtNqn      string          // 子系统NQN
	NsUuid      string          // 物理设备UUID
	NamespaceId uint32          // 命名空间ID
	NsDevPath   string          // 命名空间设备路径
	NsSize      uint64          // 分配的容量, 单位字节
	LbaFormat   UbsSsuLbaFormat // LBA格式
}

// UbsSsuAllocResult 分配存储空间结果, 对应UbsSsuAllocResult
type UbsSsuAllocResult struct {
	Name       string                // 请求标识, 最大48个字符
	Strategy   UbsSsuAllocStrategy   // 分配策略
	Namespaces []UbsSsuNamespaceInfo // 命名空间信息列表
}

// UbsSsuAllocSpaceReq 分配存储空间请求参数, 对应UbsSsuAllocSpaceReq
type UbsSsuAllocSpaceReq struct {
	Name      string              // 请求标识, 最大48个字符
	NsSize    uint64              // 申请总容量, 单位字节, 条带化策略时需整除nsNum且整除后需为chunkSize的整数倍
	NsNum     uint32              // 命名空间数量, 等于1时strategy不生效
	LbaFormat UbsSsuLbaFormat     // LBA格式
	Strategy  UbsSsuAllocStrategy // 分配策略
	Tenant    string              // 请求方tenant(租户隔离标识)
}

// UbsSsuSpaceReq 挂载|卸载存储空间请求参数, 对应ubs_ssu_space_req_t
type UbsSsuSpaceReq struct {
	Name   string // 需挂载|卸载的存储空间标识, 最大48个字符
	Nqn    string // Host 的 NVMe Qualified Name
	SrcEid string // 源EID
}

// UbsSsuLinearSpaceReq 挂载|卸载线性编址存储空间请求参数, 对应ubs_ssu_linear_space_req_t
type UbsSsuLinearSpaceReq struct {
	Name    string // 需挂载|卸载的存储空间标识, 最大48个字符
	Nqn     string // Host 的 NVMe Qualified Name
	SrcEid  string // 源EID
	DevName string // 聚合后的块设备名称, 由外部指定
}

// UbsSsuStripedSpaceReq 挂载|卸载条带化编址存储空间请求参数, 对应ubs_ssu_striped_space_req_t
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

// UbsSsuListAllocInfo 列出所有已分配的存储空间信息
//
// 获取系统中所有已分配的SSU存储空间详细信息，包括命名空间列表、容量、LBA格式和使用类型等。
//
// 返回值：
//   - []UbsSsuAllocResult: 已分配空间信息列表
//   - error: 错误信息；成功返回 nil
func UbsSsuListAllocInfo() ([]UbsSsuAllocResult, error) {
	// TODO: 实现具体逻辑
	return []UbsSsuAllocResult{}, nil
}

// UbsSsuAllocSpace 分配SSU存储空间
//
// 根据请求参数分配指定数量和大小的命名空间，支持顺序分配和分布式分配两种策略。
// 当 nsNum 为 1 时，strategy 参数不生效。
//
// 参数：
//   - req: 分配请求参数，包含命名空间数量、容量、策略等
//
// 返回值：
//   - UbsSsuAllocResult: 分配结果，包含已分配的命名空间信息列表
//   - error: 错误信息；成功返回 nil
//
// 参考：UbsSsuAllocSpaceReq, UbsSsuAllocResult
func UbsSsuAllocSpace(req UbsSsuAllocSpaceReq) (UbsSsuAllocResult, error) {
	// TODO: 实现具体逻辑
	return UbsSsuAllocResult{}, nil
}

// UbsSsuFreeSpace 释放已分配的存储空间
//
// 释放之前通过 AllocSpace 分配的存储空间及其关联的所有命名空间。
// 释放操作具有幂等性，释放不存在的空间应返回成功。
//
// 参数：
//   - name: 要释放的存储空间标识（与 AllocSpace 时的 name 参数一致）
//
// 返回值：
//   - error: 错误信息；成功返回 nil
func UbsSsuFreeSpace(name string) error {
	// TODO: 实现具体逻辑
	return nil
}

// UbsSsuAddAccessPermission 添加存储空间访问权限
//
// 为指定的 Host 授予对已分配存储空间的访问权限，在 Target 侧将 Host NQN
// 添加到子系统的允许主机列表中，使该 Host 可以通过 NVMe-oF 协议访问对应命名空间。
// 重复添加同一 Host 的访问权限应返回成功（幂等性保证）。
//
// 参数：
//   - name: 存储空间标识（与 AllocSpace 时的 name 参数一致）
//   - nqn:  Host 的 NVMe Qualified Name，标识被授权的主机
//
// 返回值：
//   - error: 错误信息；成功返回 nil
//
// 参考：RemoveAccessPermission
func UbsSsuAddAccessPermission(name string, nqn string) error {
	// TODO: 实现具体逻辑
	return nil
}

// UbsSsuRemoveAccessPermission 移除存储空间访问权限
//
// 撤销指定 Host 对已分配存储空间的访问权限，在 Target 侧将 Host NQN
// 从子系统的允许主机列表中移除，使该 Host 无法再通过 NVMe-oF 协议访问对应命名空间。
// 移除不存在的访问权限应返回成功（幂等性保证）。
// 移除权限前应确保该 Host 已断开与对应命名空间的连接。
//
// 参数：
//   - name: 存储空间标识（与 AllocSpace 时的 name 参数一致）
//   - nqn:  Host 的 NVMe Qualified Name，标识被撤销权限的主机
//
// 返回值：
//   - error: 错误信息；成功返回 nil
//
// 参考：AddAccessPermission
func UbsSsuRemoveAccessPermission(name string, nqn string) error {
	// TODO: 实现具体逻辑
	return nil
}

// UbsSsuAttachSpace 挂载已分配的存储空间
//
// 将指定的存储空间挂载到系统，使其可被主机访问。
//
// 参数：
//   - req: 挂载请求参数，包含存储空间标识、Host 的 NVMe Qualified Name 和源EID
//
// 返回值：
//   - string: 挂载后的设备路径
//   - error:  错误信息；成功返回 nil
func UbsSsuAttachSpace(req UbsSsuSpaceReq) (string, error) {
	// TODO: 实现具体逻辑
	return "", nil
}

// UbsSsuDetachSpace 卸载已分配的存储空间
//
// 将指定的存储空间从系统卸载，释放设备占用。
// 卸载前需确保没有进程正在使用该存储空间。
//
// 参数：
//   - req: 卸载请求参数，包含存储空间标识、Host 的 NVMe Qualified Name 和源EID
//
// 返回值：
//   - error: 错误信息；成功返回 nil
func UbsSsuDetachSpace(req UbsSsuSpaceReq) error {
	// TODO: 实现具体逻辑
	return nil
}

// UbsSsuAttachLinearSpace 挂载线性编址的存储空间
//
// 将多个命名空间设备以线性拼接方式聚合为一个逻辑块设备并挂载。
// 线性编址模式下，数据按顺序填充各成员设备。
//
// 参数：
//   - req: 挂载请求参数，包含存储空间标识、Host 的 NVMe Qualified Name、源EID和聚合后的块设备名称
//
// 返回值：
//   - string: 挂载后的聚合设备路径
//   - error:  错误信息；成功返回 nil
func UbsSsuAttachLinearSpace(req UbsSsuLinearSpaceReq) (string, error) {
	// TODO: 实现具体逻辑
	return "", nil
}

// UbsSsuDetachLinearSpace 卸载线性编址的存储空间
//
// 将线性聚合的块设备卸载并释放。
//
// 参数：
//   - req: 卸载请求参数，包含存储空间标识、Host 的 NVMe Qualified Name、源EID和聚合后的块设备名称
//
// 返回值：
//   - error: 错误信息；成功返回 nil
func UbsSsuDetachLinearSpace(req UbsSsuLinearSpaceReq) error {
	// TODO: 实现具体逻辑
	return nil
}

// UbsSsuAttachStripedSpace 挂载条带化编址的存储空间
//
// 将多个命名空间设备以条带化方式聚合为一个逻辑块设备并挂载，
// 支持 RAID0 和 RAID5 两种级别。
// RAID5 至少需要 3 个成员设备。
//
// 参数：
//   - req: 条带化挂载请求参数，包含存储空间标识、Host 的 NVMe Qualified Name、源EID、
//     聚合后的块设备名称、RAID级别和chunk大小
//
// 返回值：
//   - string: 挂载后的聚合设备路径
//   - error:  错误信息；成功返回 nil
//
// 参考：UbsSsuStripedSpaceReq, UbsSsuAggregationRaidLevel, UbsSsuChunkSize
func UbsSsuAttachStripedSpace(req UbsSsuStripedSpaceReq) (string, error) {
	// TODO: 实现具体逻辑
	return "", nil
}

// UbsSsuDetachStripedSpace 卸载条带化编址的存储空间
//
// 将条带化聚合的块设备卸载并释放。
//
// 参数：
//   - req: 卸载请求参数，包含存储空间标识、Host 的 NVMe Qualified Name、源EID和聚合后的块设备名称
//
// 返回值：
//   - error: 错误信息；成功返回 nil
func UbsSsuDetachStripedSpace(req UbsSsuStripedSpaceReq) error {
	// TODO: 实现具体逻辑
	return nil
}

// UbsSsuGetNsStats 获取存储空间的命名空间统计信息
//
// 查询指定存储空间下各命名空间的容量使用情况，包括总容量和已用容量。
//
// 参数：
//   - name: 存储空间标识（与 AllocSpace 时的 name 参数一致）
//
// 返回值：
//   - []UbsSsuNsStats: 命名空间统计信息列表
//   - error: 错误信息；成功返回 nil
func UbsSsuGetNsStats(name string) ([]UbsSsuNsStats, error) {
	// TODO: 实现具体逻辑
	return []UbsSsuNsStats{}, nil
}

// UbsSsuGetConnectInfo 获取存储空间的连接信息
//
// 查询指定存储空间在指定VFE上的NVMe连接信息，包括子系统NQN、Host NQN、命名空间ID等。
//
// 参数：
//   - name: 存储空间标识（与 AllocSpace 时的 name 参数一致）
//   - vfe: VFE信息指针，指定查询的虚拟功能单元，传nil时使用host侧分配给ssu的fe的eid
//
// 返回值：
//   - []UbsSsuConnectInfo: 连接信息列表
//   - error: 错误信息；成功返回 nil
func UbsSsuGetConnectInfo(name string, vfe *UbsUbVfe) ([]UbsSsuConnectInfo, error) {
	// TODO: 实现具体逻辑
	return []UbsSsuConnectInfo{}, nil
}

// UbsSsuGetFeDeviceList 获取FE设备列表
//
// 查询系统中所有FE设备信息，包括每个PFE下的VFE列表。
//
// 返回值：
//   - []UbsSsuFe: FE设备信息列表
//   - error: 错误信息；成功返回 nil
func UbsSsuGetFeDeviceList() ([]UbsSsuFe, error) {
	// TODO: 实现具体逻辑
	return []UbsSsuFe{}, nil
}

// UbsSsuFeDeviceAlloc 分配VFE设备
//
// 将指定的虚拟功能单元绑定到目标虚拟机，使虚拟机可通过该VFE访问存储资源。
//
// 参数：
//   - upi: 租户隔离标识
//   - vfe: 要绑定的VFE信息（结构体指针）
//   - busInstanceGuid: 输入输出参数，指向长度为 UbsSsuMaxGuidLength 的字节数组。
//     传入时表示目标虚拟机实例的GUID（可为空/全零），
//     返回时填充实际分配的总线实例GUID。
//
// 返回值：
//   - error: 错误信息；成功返回 nil
func UbsSsuFeDeviceAlloc(upi uint32, vfe *UbsUbVfe, busInstanceGuid *[UbsSsuMaxGuidLength]byte) error {
	// TODO: 调用 C 库实现
	return nil
}

// UbsSsuFeDeviceFree 释放VFE设备
//
// 将已分配的虚拟功能单元从目标虚拟机释放，回收VFE设备资源。
//
// 参数：
//   - upi: 租户隔离标识
//   - vfe: 要释放的VFE信息（结构体指针）
//   - busInstanceGuid: 总线实例GUID，指向长度为 UbsSsuMaxGuidLength 的字节数组。
//
// 返回值：
//   - error: 错误信息；成功返回 nil
func UbsSsuFeDeviceFree(upi uint32, vfe *UbsUbVfe, busInstanceGuid *[UbsSsuMaxGuidLength]byte) error {
	// TODO: 调用 C 库实现
	return nil
}
