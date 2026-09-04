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

// Package inject 实现 helper 内 VIP 配置注入协议:
// 定义 UBSE_VIP/UBSE_VIP_CFG_PUSH 常量、23 字节定长 LittleEndian 序列化与 PushVipCfg。
// VIP 注入协议仅在本 helper 内部自持,不在 SDK 侧暴露。
package inject

import (
	"encoding/binary"

	"atomgit.com/openeuler/ubs-engine.git/src/sdk/go/ipc"
)

const (
	// UBSE_VIP 为 VIP 框架模块的 IPC module_code。
	UBSE_VIP uint16 = 0x0008
	// UBSE_VIP_CFG_PUSH 为 helper 经 UDS 推送 VIP 配置的 op_code。
	UBSE_VIP_CFG_PUSH uint16 = 0x0001
)

// payloadSize 为定长注入 payload 字节数,与 C++ 侧 UbseVipCfgPushPayload 逐一字节对齐。
const payloadSize = 23

// VipCfgPushPayload 为注入 VIP 配置的定长二进制结构(4+2+1+16=23 字节),统一 LittleEndian。
type VipCfgPushPayload struct {
	// Addr 为大端数值型 IPv4(即 in_addr 的整型表示),再由 Marshal 按 LittleEndian 落盘。
	// 例如 192.168.100.200 对应 0xC0A864C8(=3232261320)。调用方须先经
	// net.IPv4().To4() 转成 uint32 语义后再赋值,勿直接写点分十进制字符串。
	Addr   uint32
	Port   uint16   // [1024, 65535]
	Prefix uint8    // 1~32
	Iface  [16]byte // '\0' 结尾,最多 15 有效字符
}

// Marshal 将 payload 序列化为 23 字节 LittleEndian 二进制,与 C++ 侧逐字节一致。
func (p *VipCfgPushPayload) Marshal() []byte {
	buf := make([]byte, payloadSize)
	binary.LittleEndian.PutUint32(buf[0:4], p.Addr)
	binary.LittleEndian.PutUint16(buf[4:6], p.Port)
	buf[6] = p.Prefix
	copy(buf[7:23], p.Iface[:])
	return buf
}

// PushVipCfg 经 UDS 将 VIP 配置推送给宿主机 ubse,丢弃响应体,仅返回调用错误。
func PushVipCfg(payload *VipCfgPushPayload) error {
	_, err := ipc.InvokeCall(UBSE_VIP, UBSE_VIP_CFG_PUSH, payload.Marshal())
	return err
}