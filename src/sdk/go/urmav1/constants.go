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

// Package urmav1 implements UBS URMA operations with go.
package urmav1

import (
	"time"
)

// Error codes
const (
	UbsSuccess             = 0
	UbsErrNullPointer      = 1
	UbsEngineErrInternal   = 2
	UbsEngineErrOutOfRange = 3
)

// URMA device types
const (
	UrmaUnique = 0
	UrmaShared = 1
)

// Constants from ubs_engine_urma.h
const (
	UbsUrmaNameMax       = 32
	UbsMaxUrmaPathLength = 128
	UbsUrmaVfeNum        = 2
)

// URMA command codes
const (
	UbseModuleCode   = 0x0005
	UbseUrmaDevGet   = 0x0005
	UbseUrmaDevAlloc = 0x0006
	UbseUrmaDevFree  = 0x0007
	UbseUrmaQosSet   = 0x0001
	UbseUrmaQosGet   = 0x0002
	UbseUrmaQosReset = 0x0003
)

// IPC related constants
const (
	UbseIpcSocketPath = "/var/run/ubse/ubse.sock"
	MaxMessageSize    = 10 * 1024 * 1024 // 10M
	DefaultTimeout    = 30 * time.Second
)
