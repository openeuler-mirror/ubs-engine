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

// Package ssu 提供SSU(存储服务单元)的Go客户端SDK-ipc通信码。
package ssu

const (
	UbseModuleCode = 0x000D // SSU模块代码

	// SSU操作码, 对齐 C++ ubse_ssu_op_code.h
	UbseSsuListAllocInfo             = 0x0004 // 查询分配信息列表请求
	UbseSsuGetAllocInfoByNameReq     = 0x0005 // 根据名称查询分配信息请求
	UbseSsuGetNsStatsReq             = 0x0006 // 查询命名空间统计信息请求
	UbseSsuGetConnectInfoReq         = 0x0007 // 查询连接信息请求
	UbseSsuAddAccessPermissionReq    = 0x0008 // 添加访问权限请求
	UbseSsuRemoveAccessPermissionReq = 0x0009 // 移除访问权限请求
	UbseSsuAllocSpaceReq             = 0x000A // 分配存储空间请求
	UbseSsuAttachSpaceReq            = 0x000B // 挂载存储空间请求
	UbseSsuDetachSpaceReq            = 0x000C // 卸载存储空间请求
	UbseSsuFreeSpaceReq              = 0x000D // 释放存储空间请求
	UbseSsuAttachLinearSpaceReq      = 0x000E // 挂载线性编址空间请求
	UbseSsuDetachLinearSpaceReq      = 0x000F // 卸载线性编址空间请求
	UbseSsuAttachStripedSpaceReq     = 0x0010 // 挂载条带化空间请求
	UbseSsuDetachStripedSpaceReq     = 0x0011 // 卸载条带化空间请求
	UbseSsuGetFeDeviceListReq        = 0x0012 // 查询FE设备列表请求
	UbseSsuFeDeviceAllocReq          = 0x0013 // 分配VFE设备请求
	UbseSsuFeDeviceFreeReq           = 0x0014 // 释放VFE设备请求
)
