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

// Package ssu 提供SSU(存储服务单元)的Go客户端SDK打包解包功能。
package ssu

import (
	"fmt"

	"atomgit.com/openeuler/ubs-engine.git/src/sdk/go/pack"
)

// ==================== 对象级别打包函数 ====================
//
// 对象级别函数仅处理结构体本身的字段序列化, 不包含消息层面的标志位或计数前缀。
// 消息层面的逻辑(如hasVfe标志位、列表计数)由消息级别函数处理。

// packVfe 将VFE信息打包为字节数组
//
// 字段顺序: slotId(u8), chipId(u8), dieId(u8), pfeId(u16), vfeId(u16),
//
//	vfeGuid(string,UbsSsuMaxGuidLength), bindBusInstanceGuid(string,UbsSsuMaxGuidLength)
//
// 注意: 本函数不包含hasVfe标志位, 标志位由消息级别函数 packConnectInfoReq 处理。
func packVfe(vfe *UbsUbVfe) []byte {
	return pack.NewBinaryPacker().
		PackUint8(vfe.SlotId).
		PackUint8(vfe.ChipId).
		PackUint8(vfe.DieId).
		PackUint16(vfe.PfeId).
		PackUint16(vfe.VfeId).
		PackString(vfe.VfeGuid, UbsSsuMaxGuidLength).
		PackString(vfe.BindBusInstanceGuid, UbsSsuMaxGuidLength).
		Bytes()
}

// unpackVfe 从解包器中读取VFE信息
//
// 本函数不读取hasVfe标志位, 标志位由调用方处理。
func unpackVfe(u *pack.BinaryUnpacker) (UbsUbVfe, error) {
	slotId, err := u.UnpackUint8()
	if err != nil {
		return UbsUbVfe{}, err
	}
	chipId, err := u.UnpackUint8()
	if err != nil {
		return UbsUbVfe{}, err
	}
	dieId, err := u.UnpackUint8()
	if err != nil {
		return UbsUbVfe{}, err
	}
	pfeId, err := u.UnpackUint16()
	if err != nil {
		return UbsUbVfe{}, err
	}
	vfeId, err := u.UnpackUint16()
	if err != nil {
		return UbsUbVfe{}, err
	}
	vfeGuid, err := u.UnpackString(UbsSsuMaxGuidLength)
	if err != nil {
		return UbsUbVfe{}, err
	}
	bindBusInstanceGuid, err := u.UnpackString(UbsSsuMaxGuidLength)
	if err != nil {
		return UbsUbVfe{}, err
	}
	return UbsUbVfe{
		SlotId:              slotId,
		ChipId:              chipId,
		DieId:               dieId,
		PfeId:               pfeId,
		VfeId:               vfeId,
		VfeGuid:             vfeGuid,
		BindBusInstanceGuid: bindBusInstanceGuid,
	}, nil
}

// unpackNamespaceInfo 从解包器中读取命名空间信息
//
// 字段顺序: tgtEid(string,17), tgtNqn(string,69), nsUuid(string,37),
//
//	namespaceId(u32), nsDevPath(string,63), nsSize(u64),
//	lbaFormat(u32), nqnCount(u32), allowHostNqnList[](string,69)
func unpackNamespaceInfo(u *pack.BinaryUnpacker) (UbsSsuNamespaceInfo, error) {
	tgtEid, err := u.UnpackString(UbsSsuMaxEidLength)
	if err != nil {
		return UbsSsuNamespaceInfo{}, err
	}
	tgtNqn, err := u.UnpackString(UbsSsuMaxNqnLength)
	if err != nil {
		return UbsSsuNamespaceInfo{}, err
	}
	nsUuid, err := u.UnpackString(UbsSsuMaxUuidLength)
	if err != nil {
		return UbsSsuNamespaceInfo{}, err
	}
	namespaceId, err := u.UnpackUint32()
	if err != nil {
		return UbsSsuNamespaceInfo{}, err
	}
	nsDevPath, err := u.UnpackString(UbsSsuMaxDevPathLength)
	if err != nil {
		return UbsSsuNamespaceInfo{}, err
	}
	nsSize, err := u.UnpackUint64()
	if err != nil {
		return UbsSsuNamespaceInfo{}, err
	}
	lbaFormatRaw, err := u.UnpackUint32()
	if err != nil {
		return UbsSsuNamespaceInfo{}, err
	}
	if lbaFormatRaw != uint32(Fmt512) && lbaFormatRaw != uint32(Fmt4K) {
		return UbsSsuNamespaceInfo{}, fmt.Errorf("invalid lba_format: %d", lbaFormatRaw)
	}
	allowHostNqnList, err := pack.UnpackList(u, maxHostNqnList, func(u *pack.BinaryUnpacker) (string, error) {
		return u.UnpackString(UbsSsuMaxNqnLength)
	})
	if err != nil {
		return UbsSsuNamespaceInfo{}, err
	}
	return UbsSsuNamespaceInfo{
		TgtEid:           tgtEid,
		TgtNqn:           tgtNqn,
		NsUuid:           nsUuid,
		NamespaceId:      namespaceId,
		NsDevPath:        nsDevPath,
		NsSize:           nsSize,
		LbaFormat:        UbsSsuLbaFormat(lbaFormatRaw),
		AllowHostNqnList: allowHostNqnList,
	}, nil
}

// unpackAllocResultImpl 从解包器中读取分配结果(对象级别, 对齐C++ AllocResultPack)。
//
// 字段顺序: name(string,48), strategy(u8), listSize(u32), nameSpaceList[](NameSpaceInfo)
func unpackAllocResultImpl(u *pack.BinaryUnpacker) (UbsSsuAllocResult, error) {
	name, err := u.UnpackString(UbsSsuMaxNameLength)
	if err != nil {
		return UbsSsuAllocResult{}, err
	}
	strategyRaw, err := u.UnpackUint8()
	if err != nil {
		return UbsSsuAllocResult{}, err
	}
	if strategyRaw != uint8(Striped) && strategyRaw != uint8(Linear) {
		return UbsSsuAllocResult{}, fmt.Errorf("invalid strategy: %d", strategyRaw)
	}
	namespaces, err := pack.UnpackList(u, maxNamespaces, unpackNamespaceInfo)
	if err != nil {
		return UbsSsuAllocResult{}, err
	}
	return UbsSsuAllocResult{
		Name:       name,
		Strategy:   UbsSsuAllocStrategy(strategyRaw),
		Namespaces: namespaces,
	}, nil
}

// unpackFe 从解包器中读取FE信息(对象级别, 对齐C++ FePack)。
//
// 字段顺序: slotId(u8), chipId(u8), dieId(u8), pfeId(u16),
//
//	pfeGuid(string,32), vfeCount(u32), vfeList[](Vfe)
//
// 注意: C++ FePack中vfeCount使用uint32打包, 这里必须使用UnpackUint32。
func unpackFe(u *pack.BinaryUnpacker) (UbsSsuFe, error) {
	slotId, err := u.UnpackUint8()
	if err != nil {
		return UbsSsuFe{}, err
	}
	chipId, err := u.UnpackUint8()
	if err != nil {
		return UbsSsuFe{}, err
	}
	dieId, err := u.UnpackUint8()
	if err != nil {
		return UbsSsuFe{}, err
	}
	pfeId, err := u.UnpackUint16()
	if err != nil {
		return UbsSsuFe{}, err
	}
	pfeGuid, err := u.UnpackString(UbsSsuMaxGuidLength)
	if err != nil {
		return UbsSsuFe{}, err
	}
	vfeList, err := pack.UnpackList(u, maxFeDevices, unpackVfe)
	if err != nil {
		return UbsSsuFe{}, err
	}
	return UbsSsuFe{
		SlotId:  slotId,
		ChipId:  chipId,
		DieId:   dieId,
		PfeId:   pfeId,
		PfeGuid: pfeGuid,
		VfeList: vfeList,
	}, nil
}

// unpackConnectInfo 从解包器中读取连接信息(对象级别, 对齐C++ ConnectInfoPack)。
//
// 字段顺序: srcEid(string,17), tgtEid(string,17), tgtNqn(string,69),
//
//	hostNqn(string,69), nsUuid(string,37), nsId(u32)
func unpackConnectInfo(u *pack.BinaryUnpacker) (UbsSsuConnectInfo, error) {
	srcEid, err := u.UnpackString(UbsSsuMaxEidLength)
	if err != nil {
		return UbsSsuConnectInfo{}, err
	}
	tgtEid, err := u.UnpackString(UbsSsuMaxEidLength)
	if err != nil {
		return UbsSsuConnectInfo{}, err
	}
	tgtNqn, err := u.UnpackString(UbsSsuMaxNqnLength)
	if err != nil {
		return UbsSsuConnectInfo{}, err
	}
	hostNqn, err := u.UnpackString(UbsSsuMaxNqnLength)
	if err != nil {
		return UbsSsuConnectInfo{}, err
	}
	nsUuid, err := u.UnpackString(UbsSsuMaxUuidLength)
	if err != nil {
		return UbsSsuConnectInfo{}, err
	}
	nsId, err := u.UnpackUint32()
	if err != nil {
		return UbsSsuConnectInfo{}, err
	}
	return UbsSsuConnectInfo{
		SrcEid:  srcEid,
		TgtEid:  tgtEid,
		TgtNqn:  tgtNqn,
		HostNqn: hostNqn,
		NsUuid:  nsUuid,
		NsId:    nsId,
	}, nil
}

// unpackNsStats 从解包器中读取命名空间统计信息(对象级别, 对齐C++ NsStatsPack)。
//
// 字段顺序: nsUuid(string,37), nsId(u32), totalSize(u64), usedSize(u64)
func unpackNsStats(u *pack.BinaryUnpacker) (UbsSsuNsStats, error) {
	nsUuid, err := u.UnpackString(UbsSsuMaxUuidLength)
	if err != nil {
		return UbsSsuNsStats{}, err
	}
	nsId, err := u.UnpackUint32()
	if err != nil {
		return UbsSsuNsStats{}, err
	}
	totalSize, err := u.UnpackUint64()
	if err != nil {
		return UbsSsuNsStats{}, err
	}
	usedSize, err := u.UnpackUint64()
	if err != nil {
		return UbsSsuNsStats{}, err
	}
	return UbsSsuNsStats{
		NsUuid:    nsUuid,
		NsId:      nsId,
		TotalSize: totalSize,
		UsedSize:  usedSize,
	}, nil
}

// ==================== 消息级别打包函数 (对齐 ubse_ssu_handler_message.cpp) ====================
//
// 消息级别函数处理请求参数的完整序列化, 可能包含标志位、可选字段等消息层逻辑。

// packAllocSpaceReq 将分配存储空间请求参数打包为字节数组
//
// 字段顺序: name(string,48), nsSize(u64), nsNum(u32), lbaFormat(u32),
//
//	strategy(u8), tenant(string,17)
func packAllocSpaceReq(req UbsSsuAllocSpaceReq) []byte {
	return pack.NewBinaryPacker().
		PackString(req.Name, UbsSsuMaxNameLength).
		PackUint64(req.NsSize).
		PackUint32(req.NsNum).
		PackUint32(uint32(req.LbaFormat)).
		PackUint8(uint8(req.Strategy)).
		PackString(req.Tenant, UbsSsuMaxTenantLength).
		Bytes()
}

// packSpaceReq 将挂载|卸载存储空间请求参数打包为字节数组
//
// 字段顺序: name(string,48), nqn(string,69), srcEid(string,17)
func packSpaceReq(req UbsSsuSpaceReq) []byte {
	return pack.NewBinaryPacker().
		PackString(req.Name, UbsSsuMaxNameLength).
		PackString(req.Nqn, UbsSsuMaxNqnLength).
		PackString(req.SrcEid, UbsSsuMaxEidLength).
		Bytes()
}

// packLinearSpaceReq 将挂载|卸载线性编址存储空间请求参数打包为字节数组
//
// 字段顺序: SpaceReq字段 + devName(string,33)
func packLinearSpaceReq(req UbsSsuLinearSpaceReq) []byte {
	return pack.NewBinaryPacker().
		PackString(req.Name, UbsSsuMaxNameLength).
		PackString(req.Nqn, UbsSsuMaxNqnLength).
		PackString(req.SrcEid, UbsSsuMaxEidLength).
		PackString(req.DevName, UbsSsuMaxDevNameLength).
		Bytes()
}

// packStripedSpaceReq 将挂载|卸载条带化编址存储空间请求参数打包为字节数组
//
// 字段顺序: LinearSpaceReq字段 + level(u8), chunkSize(u32)
func packStripedSpaceReq(req UbsSsuStripedSpaceReq) []byte {
	return pack.NewBinaryPacker().
		PackString(req.Name, UbsSsuMaxNameLength).
		PackString(req.Nqn, UbsSsuMaxNqnLength).
		PackString(req.SrcEid, UbsSsuMaxEidLength).
		PackString(req.DevName, UbsSsuMaxDevNameLength).
		PackUint8(uint8(req.Level)).
		PackUint32(uint32(req.ChunkSize)).
		Bytes()
}

// packConnectInfoReq 打包获取连接信息请求参数
//
// 消息格式: name(string,48) + hasVfe(u8) + [可选]VFE结构体
//
//	hasVfe=0: 无VFE(vfe为nil), 后续无VFE字段
//	hasVfe=1: 有VFE, 后续紧跟VFE结构体字段(由packVfe打包)
func packConnectInfoReq(name string, vfe *UbsUbVfe) []byte {
	packer := pack.NewBinaryPacker().PackString(name, UbsSsuMaxNameLength)
	if vfe == nil {
		packer.PackUint8(0)
	} else {
		packer.PackUint8(1)
		packer.PackRaw(packVfe(vfe))
	}
	return packer.Bytes()
}

// packFeDeviceAllocReq 将FE设备操作请求参数打包为字节数组
//
// 字段顺序: upi(u32), VFE结构体, busInstanceGuid(string,UbsSsuMaxGuidLength)
func packFeDeviceAllocReq(upi uint32, vfe *UbsUbVfe, busInstanceGuid string) []byte {
	return pack.NewBinaryPacker().
		PackUint32(upi).
		PackRaw(packVfe(vfe)).
		PackString(busInstanceGuid, UbsSsuMaxGuidLength).
		Bytes()
}

// packFeDeviceFreeReq 将FE设备操作请求参数打包为字节数组
//
// 字段顺序: upi(u32), VFE结构体
func packFeDeviceFreeReq(upi uint32, vfe *UbsUbVfe) []byte {
	return pack.NewBinaryPacker().
		PackUint32(upi).
		PackRaw(packVfe(vfe)).
		Bytes()
}

// ==================== 响应解包函数 (公开接口, 接收 []byte) ====================

// unpackAllocResult 从响应中解包分配结果
func unpackAllocResult(response []byte) (UbsSsuAllocResult, error) {
	return unpackAllocResultImpl(pack.NewBinaryUnpacker(response))
}

// unpackAllocResultList 从响应中解包分配结果列表
//
// 消息格式: listSize(u32), AllocResult[]
func unpackAllocResultList(response []byte) ([]UbsSsuAllocResult, error) {
	return pack.UnpackList(pack.NewBinaryUnpacker(response), maxAllocResults, unpackAllocResultImpl)
}

// unpackNsDevPathsResponse 从AttachSpace响应中解包命名空间设备路径列表
//
// 消息格式: listSize(u32), devPath[](string,63)
func unpackNsDevPathsResponse(response []byte) ([]string, error) {
	u := pack.NewBinaryUnpacker(response)
	return pack.UnpackList(u, maxDevPaths, func(u *pack.BinaryUnpacker) (string, error) {
		return u.UnpackString(UbsSsuMaxDevPathLength)
	})
}

// unpackNsDevPathsAndDevPathResponse 从AttachLinear/StripedSpace响应中解包命名空间设备路径列表和聚合设备路径
//
// 消息格式: listSize(u32), devPath[](string,63), devPath(string,63)
func unpackNsDevPathsAndDevPathResponse(response []byte) ([]string, string, error) {
	u := pack.NewBinaryUnpacker(response)
	nsDevPaths, err := pack.UnpackList(u, maxDevPaths, func(u *pack.BinaryUnpacker) (string, error) {
		return u.UnpackString(UbsSsuMaxDevPathLength)
	})
	if err != nil {
		return nil, "", err
	}
	devPath, err := u.UnpackString(UbsSsuMaxDevPathLength)
	if err != nil {
		return nil, "", err
	}
	return nsDevPaths, devPath, nil
}

// unpackNsStatsList 从响应中解包命名空间统计信息列表
//
// 消息格式: listSize(u32), NsStats[]
func unpackNsStatsList(response []byte) ([]UbsSsuNsStats, error) {
	return pack.UnpackList(pack.NewBinaryUnpacker(response), maxNsStats, unpackNsStats)
}

// unpackConnectInfoList 从响应中解包连接信息列表
//
// 消息格式: listSize(u32), ConnectInfo[]
func unpackConnectInfoList(response []byte) ([]UbsSsuConnectInfo, error) {
	return pack.UnpackList(pack.NewBinaryUnpacker(response), maxConnectInfos, unpackConnectInfo)
}

// unpackFeDeviceList 从响应中解包FE设备列表
//
// 消息格式: listSize(u32), Fe[]
func unpackFeDeviceList(response []byte) ([]UbsSsuFe, error) {
	return pack.UnpackList(pack.NewBinaryUnpacker(response), maxFeDevices, unpackFe)
}

// unpackFeDeviceAllocResp 从响应中解包FE设备分配结果
//
// 消息格式: busInstanceGuid(string,UbsSsuMaxGuidLength)
func unpackFeDeviceAllocResp(response []byte) (string, error) {
	return pack.NewBinaryUnpacker(response).UnpackString(UbsSsuMaxGuidLength)
}
