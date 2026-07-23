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

// Package pack 提供 ubs_binary_codec.go 二进制数据打包与解包工具类。
// 设计要点:
//   - BinaryPacker 封装 []byte 并自动追踪写入位置，支持链式调用
//   - BinaryUnpacker 封装 []byte 并自动推进读取位置，内置边界校验
//   - 所有整数使用小端序 (little-endian)
//   - 字符串/字节数组采用长度前缀格式 [uint32 len][len bytes]，
//   - 定义了 ErrInsufficientData/ErrLengthExceeded错误，
//     便于上层精准捕获与转换
//
// 典型用法:
//
//	// 打包
//	packed := NewBinaryPacker().
//	    PackString(req.Name, UbsSsuMaxNameLength).
//	    PackUint64(req.NsSize).
//	    PackUint32(req.NsNum).
//	    Bytes()
//
//	// 解包
//	unpacker := NewBinaryUnpacker(response)
//	name, err := unpacker.UnpackString(UbsSsuMaxNameLength)
//	strategy, err := unpacker.UnpackUint8()
//	count, err := unpacker.UnpackUint32()
package pack

import (
	"encoding/binary"
	"errors"
	"fmt"
	"math"
)

// ==================== 错误定义 ====================

// 编解码过程中的错误，便于上层通过 errors.Is 精准判断错误类型。
var (
	// ErrInsufficientData 剩余数据不足以完成读取。
	ErrInsufficientData = errors.New("insufficient data")

	// ErrLengthExceeded 长度前缀（字符串/字节数组/列表计数）超过允许的最大值。
	ErrLengthExceeded = errors.New("length exceeded")
)

// ==================== BinaryPacker 打包器 ====================

// BinaryPacker 二进制数据打包器。
//
// 封装 []byte 并提供链式 API，自动追加写入数据，
// 消除调用方手动管理偏移量的负担。
type BinaryPacker struct {
	buf []byte
}

// NewBinaryPacker 创建新的打包器实例。
func NewBinaryPacker() *BinaryPacker {
	return &BinaryPacker{buf: make([]byte, 0, 64)}
}

// PackUint8 写入 1 字节无符号整数。
func (p *BinaryPacker) PackUint8(v uint8) *BinaryPacker {
	p.buf = append(p.buf, v)
	return p
}

// PackUint16 写入 2 字节小端序无符号整数。
func (p *BinaryPacker) PackUint16(v uint16) *BinaryPacker {
	var b [2]byte
	binary.LittleEndian.PutUint16(b[:], v)
	p.buf = append(p.buf, b[:]...)
	return p
}

// PackUint32 写入 4 字节小端序无符号整数。
func (p *BinaryPacker) PackUint32(v uint32) *BinaryPacker {
	var b [4]byte
	binary.LittleEndian.PutUint32(b[:], v)
	p.buf = append(p.buf, b[:]...)
	return p
}

// PackInt32 写入 4 字节小端序有符号整数。
func (p *BinaryPacker) PackInt32(v int32) *BinaryPacker {
	return p.PackUint32(uint32(v))
}

// PackUint64 写入 8 字节小端序无符号整数。
func (p *BinaryPacker) PackUint64(v uint64) *BinaryPacker {
	var b [8]byte
	binary.LittleEndian.PutUint64(b[:], v)
	p.buf = append(p.buf, b[:]...)
	return p
}

// PackInt64 写入 8 字节小端序有符号整数。
func (p *BinaryPacker) PackInt64(v int64) *BinaryPacker {
	return p.PackUint64(uint64(v))
}

// PackString 写入长度前缀字符串。
//
// 格式: [uint32 len][len bytes]，超过 maxLen 的部分被截断。
//
// 参数:
//   - s: 待写入的字符串
//   - maxLen: 字符串最大长度（字节数），超过则截断
func (p *BinaryPacker) PackString(s string, maxLen uint32) *BinaryPacker {
	encoded := []byte(s)
	writeLen := uint32(len(encoded))
	if writeLen > maxLen {
		writeLen = maxLen
	}
	p.PackUint32(writeLen)
	if writeLen > 0 {
		p.buf = append(p.buf, encoded[:writeLen]...)
	}
	return p
}

// PackBytes 写入长度前缀字节数组。
//
// 格式: [uint32 len][len bytes]，超过 maxLen 的部分被截断。
// 参数:
//   - data: 待写入的字节数据
//   - maxLen: 字节最大长度，超过则截断
func (p *BinaryPacker) PackBytes(data []byte, maxLen uint32) *BinaryPacker {
	writeLen := uint32(len(data))
	if writeLen > maxLen {
		writeLen = maxLen
	}
	p.PackUint32(writeLen)
	if writeLen > 0 {
		p.buf = append(p.buf, data[:writeLen]...)
	}
	return p
}

// PackRaw 写入原始字节数据（不带长度前缀）。
// 用于写入已在其他地方编码好长度的定长字段。
func (p *BinaryPacker) PackRaw(data []byte) *BinaryPacker {
	p.buf = append(p.buf, data...)
	return p
}

// Bytes 返回打包结果的字节切片。
// 返回的是内部缓冲区的副本，调用方可安全修改。
func (p *BinaryPacker) Bytes() []byte {
	result := make([]byte, len(p.buf))
	copy(result, p.buf)
	return result
}

// Len 返回已打包数据的字节数。
func (p *BinaryPacker) Len() int {
	return len(p.buf)
}

// ==================== BinaryUnpacker 解包器 ====================

// BinaryUnpacker 二进制数据解包器。
//
// 封装 []byte 并提供顺序读取 API，自动推进偏移量，
// 内置边界校验防止越界读取。
// 对齐 C++ UbseUnpackUtil 与 C-SDK unpack_* 函数系列。
type BinaryUnpacker struct {
	data   []byte
	offset int
}

// NewBinaryUnpacker 创建新的解包器实例。
//
// 参数:
//   - data: 待解包的字节数据
func NewBinaryUnpacker(data []byte) *BinaryUnpacker {
	return &BinaryUnpacker{data: data}
}

// Offset 返回当前读取偏移量。
func (u *BinaryUnpacker) Offset() int {
	return u.offset
}

// Remaining 返回剩余可读字节数。
func (u *BinaryUnpacker) Remaining() int {
	return len(u.data) - u.offset
}

// checkRemaining 校验剩余数据是否足够。
func (u *BinaryUnpacker) checkRemaining(needed int) error {
	if u.Remaining() < needed {
		return fmt.Errorf("%w: expected %d bytes, got %d",
			ErrInsufficientData, needed, u.Remaining())
	}
	return nil
}

// safeLen 将 uint32 长度安全转换为 int。
//
// 在 32 位平台上 int 为 32 位有符号整数, uint32 值超过 math.MaxInt 时
// int(length) 会溢出为负数, 导致 checkRemaining 的边界校验被绕过,
// 进而引发越界读取。此处显式校验, 超出平台 int 表示范围时返回错误。
//
// math.MaxInt 为平台自适应常量:
//   - 64 位平台: int64 最大值 (9223372036854775807), 任何 uint32 均可容纳
//   - 32 位平台: int32 最大值 (2147483647), 据此拦截溢出
func safeLen(length uint32) (int, error) {
	if int64(length) > int64(math.MaxInt) {
		return 0, fmt.Errorf("%w: length %d exceeds platform int range",
			ErrLengthExceeded, length)
	}
	return int(length), nil
}

// UnpackUint8 读取 1 字节无符号整数。
func (u *BinaryUnpacker) UnpackUint8() (uint8, error) {
	if err := u.checkRemaining(1); err != nil {
		return 0, err
	}
	v := u.data[u.offset]
	u.offset++
	return v, nil
}

// UnpackUint16 读取 2 字节小端序无符号整数。
func (u *BinaryUnpacker) UnpackUint16() (uint16, error) {
	if err := u.checkRemaining(2); err != nil {
		return 0, err
	}
	v := binary.LittleEndian.Uint16(u.data[u.offset:])
	u.offset += 2
	return v, nil
}

// UnpackUint32 读取 4 字节小端序无符号整数。
func (u *BinaryUnpacker) UnpackUint32() (uint32, error) {
	if err := u.checkRemaining(4); err != nil {
		return 0, err
	}
	v := binary.LittleEndian.Uint32(u.data[u.offset:])
	u.offset += 4
	return v, nil
}

// UnpackInt32 读取 4 字节小端序有符号整数。
func (u *BinaryUnpacker) UnpackInt32() (int32, error) {
	v, err := u.UnpackUint32()
	if err != nil {
		return 0, err
	}
	return int32(v), nil
}

// UnpackUint64 读取 8 字节小端序无符号整数。
func (u *BinaryUnpacker) UnpackUint64() (uint64, error) {
	if err := u.checkRemaining(8); err != nil {
		return 0, err
	}
	v := binary.LittleEndian.Uint64(u.data[u.offset:])
	u.offset += 8
	return v, nil
}

// UnpackInt64 读取 8 字节小端序有符号整数。
func (u *BinaryUnpacker) UnpackInt64() (int64, error) {
	v, err := u.UnpackUint64()
	if err != nil {
		return 0, err
	}
	return int64(v), nil
}

// UnpackString 读取长度前缀字符串。
//
// 先读取 uint32 长度前缀，再读取对应字节数的字符串内容。
// 参数:
//   - maxLen: 字符串最大允许长度（字节数），超过则返回 ErrLengthExceeded
//
// 返回值:
//   - 解码后的字符串
//   - 错误信息（ErrLengthExceeded(字符串超长|平台int范围溢出)/ErrInsufficientData 之一）
func (u *BinaryUnpacker) UnpackString(maxLen uint32) (string, error) {
	length, err := u.UnpackUint32()
	if err != nil {
		return "", err
	}
	if length > maxLen {
		return "", fmt.Errorf("%w: string length %d exceeds maximum allowed %d",
			ErrLengthExceeded, length, maxLen)
	}
	if length == 0 {
		return "", nil
	}
	n, err := safeLen(length)
	if err != nil {
		return "", err
	}
	if err := u.checkRemaining(n); err != nil {
		return "", err
	}
	raw := u.data[u.offset : u.offset+n]
	u.offset += n
	return string(raw), nil
}

// UnpackBytes 读取长度前缀字节数组。
//
// 先读取 uint32 长度前缀，再读取对应字节数的原始内容。
// 与 UnpackString 线格式一致，但不进行 UTF-8 解码。
//
// 参数:
//   - maxLen: 字节最大允许长度，超过则返回 ErrLengthExceeded
//
// 返回值:
//   - 原始字节数据
//   - 错误信息（ErrLengthExceeded/ErrInsufficientData 之一）
func (u *BinaryUnpacker) UnpackBytes(maxLen uint32) ([]byte, error) {
	length, err := u.UnpackUint32()
	if err != nil {
		return nil, err
	}
	if length > maxLen {
		return nil, fmt.Errorf("%w: bytes length %d exceeds maximum allowed %d",
			ErrLengthExceeded, length, maxLen)
	}
	if length == 0 {
		return []byte{}, nil
	}
	n, err := safeLen(length)
	if err != nil {
		return nil, err
	}
	if err := u.checkRemaining(n); err != nil {
		return nil, err
	}
	raw := make([]byte, n)
	copy(raw, u.data[u.offset:u.offset+n])
	u.offset += n
	return raw, nil
}

// RemainingData 返回剩余未读数据（不推进偏移量）。
func (u *BinaryUnpacker) RemainingData() []byte {
	result := make([]byte, len(u.data)-u.offset)
	copy(result, u.data[u.offset:])
	return result
}

// ==================== 通用列表解包函数 ====================

// UnpackList 通用列表解包函数。
//
// 从解包器中读取 uint32 计数，校验上限后循环调用 itemFn 解包每个元素。
//
// 类型参数:
//   - T: 列表元素类型
//
// 参数:
//   - u: 解包器实例
//   - maxCount: 列表元素数量上限
//   - itemFn: 单个元素的解包回调函数，接收 *BinaryUnpacker 并返回元素
//
// 返回值:
//   - 解包后的元素切片
//   - 错误信息（ErrLengthExceeded 或 itemFn 返回的错误）
func UnpackList[T any](u *BinaryUnpacker, maxCount uint32,
	itemFn func(*BinaryUnpacker) (T, error)) ([]T, error) {
	count, err := u.UnpackUint32()
	if err != nil {
		return nil, err
	}
	if count > maxCount {
		return nil, fmt.Errorf("%w: count %d exceeds maximum allowed %d",
			ErrLengthExceeded, count, maxCount)
	}
	result := make([]T, 0, count)
	for i := uint32(0); i < count; i++ {
		item, err := itemFn(u)
		if err != nil {
			return nil, fmt.Errorf("item %d: %w", i, err)
		}
		result = append(result, item)
	}
	return result, nil
}
