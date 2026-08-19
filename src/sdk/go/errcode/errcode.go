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

// Package errcode 提供UBSE服务端错误码到Go error的映射。
// 错误码定义与 src/include/ubse_error.h 对齐，StatusCodeError 支持 errors.Is 判断。
package errcode

import (
	"fmt"
)

// StatusCodeError 封装服务端返回的状态码，支持 errors.Is 做类型判断。
type StatusCodeError struct {
	StatusCode uint32
	ErrName    string
}

func (e *StatusCodeError) Error() string {
	return fmt.Sprintf("ubse daemon returned error: %s (%d)", e.ErrName, e.StatusCode)
}

// Is 实现 errors.Is 接口，按 StatusCode 匹配。
func (e *StatusCodeError) Is(target error) bool {
	t, ok := target.(*StatusCodeError)
	if !ok {
		return false
	}
	return e.StatusCode == t.StatusCode
}

// =================== 对外错误码 Sentinel（对齐 ubse_error.h <10000） ===================

var (
	ErrOK = &StatusCodeError{StatusCode: 0, ErrName: "UBSE_OK"}

	// 参数错误 (1-9)
	ErrInvalidArg     = &StatusCodeError{StatusCode: 1, ErrName: "UBSE_ERR_INVALID_ARG"}
	ErrBufferTooSmall = &StatusCodeError{StatusCode: 4, ErrName: "UBSE_ERR_BUFFER_TOO_SMALL"}

	// 资源错误 (10-19)
	ErrOutOfMemory       = &StatusCodeError{StatusCode: 10, ErrName: "UBSE_ERR_OUT_OF_MEMORY"}
	ErrResourceBusy      = &StatusCodeError{StatusCode: 11, ErrName: "UBSE_ERR_RESOURCE_BUSY"}
	ErrResourceExhausted = &StatusCodeError{StatusCode: 12, ErrName: "UBSE_ERR_RESOURCE_EXHAUSTED"}
	ErrQuotaExceeded     = &StatusCodeError{StatusCode: 13, ErrName: "UBSE_ERR_QUOTA_EXCEEDED"}

	// IPC通信错误 (20-29)
	ErrIPCConnectionFailed        = &StatusCodeError{StatusCode: 20, ErrName: "UBSE_ERR_IPC_CONNECTION_FAILED"}
	ErrIPCTimeout                 = &StatusCodeError{StatusCode: 21, ErrName: "UBSE_ERR_IPC_TIMEOUT"}
	ErrIPCServiceUnavailable      = &StatusCodeError{StatusCode: 22, ErrName: "UBSE_ERR_IPC_SERVICE_UNAVAILABLE"}
	ErrIPCConnectionFailedPathLen = &StatusCodeError{StatusCode: 23, ErrName: "UBSE_ERR_IPC_CONNECTION_FAILED_PATH_LENGTH"}

	// 权限错误 (30-39)
	ErrPermissionDenied     = &StatusCodeError{StatusCode: 30, ErrName: "UBSE_ERR_PERMISSION_DENIED"}
	ErrAuthenticationFailed = &StatusCodeError{StatusCode: 31, ErrName: "UBSE_ERR_AUTHENTICATION_FAILED"}
	ErrAccessDenied         = &StatusCodeError{StatusCode: 32, ErrName: "UBSE_ERR_ACCESS_DENIED"}

	// 操作错误 (40-49)
	ErrNotImplemented  = &StatusCodeError{StatusCode: 40, ErrName: "UBSE_ERR_NOT_IMPLEMENTED"}
	ErrNotSupported    = &StatusCodeError{StatusCode: 41, ErrName: "UBSE_ERR_NOT_SUPPORTED"}
	ErrOperationFailed = &StatusCodeError{StatusCode: 42, ErrName: "UBSE_ERR_OPERATION_FAILED"}
	ErrTimedOut        = &StatusCodeError{StatusCode: 43, ErrName: "UBSE_ERR_TIMED_OUT"}

	// 守护进程错误 (50-59)
	ErrDaemonUnreachable = &StatusCodeError{StatusCode: 50, ErrName: "UBSE_ERR_DAEMON_UNREACHABLE"}
	ErrDaemonBusy        = &StatusCodeError{StatusCode: 51, ErrName: "UBSE_ERR_DAEMON_BUSY"}
	ErrDaemonCrashed     = &StatusCodeError{StatusCode: 52, ErrName: "UBSE_ERR_DAEMON_CRASHED"}
	ErrDaemonInternal    = &StatusCodeError{StatusCode: 53, ErrName: "UBSE_ERR_DAEMON_INTERNAL"}

	// UBSE错误码 (1000-1099)
	ErrEngineOutOfRange          = &StatusCodeError{StatusCode: 1000, ErrName: "UBSE_ERR_OUT_OF_RANGE"}
	ErrEngineResource            = &StatusCodeError{StatusCode: 1001, ErrName: "UBSE_ERR_RESOURCE"}
	ErrEngineConnectionFailed    = &StatusCodeError{StatusCode: 1002, ErrName: "UBSE_ERR_CONNECTION_FAILED"}
	ErrEngineAuthFailed          = &StatusCodeError{StatusCode: 1003, ErrName: "UBSE_ERR_AUTH_FAILED"}
	ErrEngineTimeout             = &StatusCodeError{StatusCode: 1004, ErrName: "UBSE_ERR_TIMEOUT"}
	ErrEngineInternal            = &StatusCodeError{StatusCode: 1005, ErrName: "UBSE_ERR_INTERNAL"}
	ErrEngineExisted             = &StatusCodeError{StatusCode: 1006, ErrName: "UBSE_ERR_EXISTED"}
	ErrEngineNotExist            = &StatusCodeError{StatusCode: 1007, ErrName: "UBSE_ERR_NOT_EXIST"}
	ErrEngineUdsInfoMismatch     = &StatusCodeError{StatusCode: 1008, ErrName: "UBSE_ERR_UDSINFO_MISMATCH"}
	ErrEngineImportAbsent        = &StatusCodeError{StatusCode: 1009, ErrName: "UBSE_ERR_IMPORT_ABSENT"}
	ErrEngineCreating            = &StatusCodeError{StatusCode: 1010, ErrName: "UBSE_ERR_CREATING"}
	ErrEngineDeleting            = &StatusCodeError{StatusCode: 1011, ErrName: "UBSE_ERR_DELETING"}
	ErrEngineUnimportSuccess     = &StatusCodeError{StatusCode: 1012, ErrName: "UBSE_ERR_UNIMPORT_SUCCESS"}
	ErrEngineAllocate            = &StatusCodeError{StatusCode: 1013, ErrName: "UBSE_ERR_ALLOCATE"}
	ErrEngineShmNoCreate         = &StatusCodeError{StatusCode: 1014, ErrName: "UBSE_ERR_SHM_NO_CREATE"}
	ErrEngineShmNoAttach         = &StatusCodeError{StatusCode: 1015, ErrName: "UBSE_ERR_SHM_NO_ATTACH"}
	ErrEngineShmAttaching        = &StatusCodeError{StatusCode: 1016, ErrName: "UBSE_ERR_SHM_ATTACHING"}
	ErrEngineShmDetaching        = &StatusCodeError{StatusCode: 1017, ErrName: "UBSE_ERR_SHM_DETACHING"}
	ErrEngineLinkNotAllowed      = &StatusCodeError{StatusCode: 1018, ErrName: "UBSE_ERR_LINK_NOT_ALLOWED"}
	ErrEngineLinkNotExist        = &StatusCodeError{StatusCode: 1019, ErrName: "UBSE_ERR_LINK_NOT_EXIST"}
	ErrEngineShmNodeEmpty        = &StatusCodeError{StatusCode: 1020, ErrName: "UBSE_ERR_SHM_NODE_EMPTY"}
	ErrEngineComFailed           = &StatusCodeError{StatusCode: 1021, ErrName: "UBSE_ERR_COM_FAILED"}
	ErrEngineFindSrcNuma         = &StatusCodeError{StatusCode: 1022, ErrName: "UBSE_ERR_FIND_SRC_NUMA"}
	ErrEngineShmDestroyed        = &StatusCodeError{StatusCode: 1023, ErrName: "UBSE_ERR_SHM_DESTROYED"}
	ErrEngineShmAttachUsing      = &StatusCodeError{StatusCode: 1024, ErrName: "UBSE_ERR_SHM_ATTACH_USING"}
	ErrEngineShmAffinityAbnormal = &StatusCodeError{StatusCode: 1025, ErrName: "UBSE_ERR_SHM_AFFINITY_PARAMS_ABNORMAL"}
	ErrEngineNumaIdNotInSocket   = &StatusCodeError{StatusCode: 1026, ErrName: "UBSE_ERR_NUMA_ID_IS_NOT_IN_SOCKET"}
	ErrEngineNodeNotExist        = &StatusCodeError{StatusCode: 1027, ErrName: "UBSE_ERR_NODE_NOT_EXIST"}
	ErrEngineNodeFault           = &StatusCodeError{StatusCode: 1028, ErrName: "UBSE_ERR_NODE_FAULT"}
	ErrEngineExportLedgering     = &StatusCodeError{StatusCode: 1029, ErrName: "UBSE_ENGINE_ERR_EXPORT_LEDGERING"}
	ErrEngineImportLedgering     = &StatusCodeError{StatusCode: 1041, ErrName: "UBSE_ENGINE_ERR_IMPORT_LEDGERING"}

	// Node Controller错误 (1100-1199)
	ErrNodeNotFound      = &StatusCodeError{StatusCode: 1100, ErrName: "UBSE_ERR_NODE_NOT_FOUND"}
	ErrNodeUnreachable   = &StatusCodeError{StatusCode: 1101, ErrName: "UBSE_ERR_NODE_UNREACHABLE"}
	ErrNodeNotActive     = &StatusCodeError{StatusCode: 1102, ErrName: "UBSE_ERR_NODE_NOT_ACTIVE"}
	ErrNodeNotResponding = &StatusCodeError{StatusCode: 1103, ErrName: "UBSE_ERR_NODE_NOT_RESPONDING"}

	// SSU Controller对外错误码 (1200-1299)
	ErrSsuSpaceNotFound        = &StatusCodeError{StatusCode: 1200, ErrName: "UBSE_SSU_ERROR_SPACE_NOT_FOUND"}
	ErrSsuNeedDetachBeforeFree = &StatusCodeError{StatusCode: 1201, ErrName: "UBSE_SSU_ERROR_NEED_DETACH_BEFORE_FREE"}
	ErrSsuStrategyMismatch     = &StatusCodeError{StatusCode: 1202, ErrName: "UBSE_SSU_ERROR_STRATEGY_MISMATCH"}

	// 重复操作错误 (2000-2099)
	ErrAlreadyAllocated = &StatusCodeError{StatusCode: 2000, ErrName: "UBSE_ERR_ALREADY_ALLOCATED"}
	ErrAlreadyAttached  = &StatusCodeError{StatusCode: 2001, ErrName: "UBSE_ERR_ALREADY_ATTACHED"}
	ErrNoNeedFree       = &StatusCodeError{StatusCode: 2002, ErrName: "UBSE_ERR_NO_NEED_FREE"}
	ErrNoNeedDetach     = &StatusCodeError{StatusCode: 2003, ErrName: "UBSE_ERR_NO_NEED_DETACH"}
)

// errMap 对外错误码映射表，与 ubse_error.h 对齐，扁平结构无模块区分。
var errMap = map[uint32]*StatusCodeError{
	0:    ErrOK,
	1:    ErrInvalidArg,
	4:    ErrBufferTooSmall,
	10:   ErrOutOfMemory,
	11:   ErrResourceBusy,
	12:   ErrResourceExhausted,
	13:   ErrQuotaExceeded,
	20:   ErrIPCConnectionFailed,
	21:   ErrIPCTimeout,
	22:   ErrIPCServiceUnavailable,
	23:   ErrIPCConnectionFailedPathLen,
	30:   ErrPermissionDenied,
	31:   ErrAuthenticationFailed,
	32:   ErrAccessDenied,
	40:   ErrNotImplemented,
	41:   ErrNotSupported,
	42:   ErrOperationFailed,
	43:   ErrTimedOut,
	50:   ErrDaemonUnreachable,
	51:   ErrDaemonBusy,
	52:   ErrDaemonCrashed,
	53:   ErrDaemonInternal,
	1000: ErrEngineOutOfRange,
	1001: ErrEngineResource,
	1002: ErrEngineConnectionFailed,
	1003: ErrEngineAuthFailed,
	1004: ErrEngineTimeout,
	1005: ErrEngineInternal,
	1006: ErrEngineExisted,
	1007: ErrEngineNotExist,
	1008: ErrEngineUdsInfoMismatch,
	1009: ErrEngineImportAbsent,
	1010: ErrEngineCreating,
	1011: ErrEngineDeleting,
	1012: ErrEngineUnimportSuccess,
	1013: ErrEngineAllocate,
	1014: ErrEngineShmNoCreate,
	1015: ErrEngineShmNoAttach,
	1016: ErrEngineShmAttaching,
	1017: ErrEngineShmDetaching,
	1018: ErrEngineLinkNotAllowed,
	1019: ErrEngineLinkNotExist,
	1020: ErrEngineShmNodeEmpty,
	1021: ErrEngineComFailed,
	1022: ErrEngineFindSrcNuma,
	1023: ErrEngineShmDestroyed,
	1024: ErrEngineShmAttachUsing,
	1025: ErrEngineShmAffinityAbnormal,
	1026: ErrEngineNumaIdNotInSocket,
	1027: ErrEngineNodeNotExist,
	1028: ErrEngineNodeFault,
	1029: ErrEngineExportLedgering,
	1041: ErrEngineImportLedgering,
	1100: ErrNodeNotFound,
	1101: ErrNodeUnreachable,
	1102: ErrNodeNotActive,
	1103: ErrNodeNotResponding,
	1200: ErrSsuSpaceNotFound,
	1201: ErrSsuNeedDetachBeforeFree,
	1202: ErrSsuStrategyMismatch,
	2000: ErrAlreadyAllocated,
	2001: ErrAlreadyAttached,
	2002: ErrNoNeedFree,
	2003: ErrNoNeedDetach,
}

// internalErrBase 内部错误码起始值，>= 此值的错误码统一映射为 ErrEngineInternal。
const internalErrBase = 10000

// StatusCodeToError 将服务端返回的状态码转换为对应的 error。
// 0 → nil，已知码 → 对应 sentinel 的拷贝，>=10000 → 兜底，未知码 → 兜底。
func StatusCodeToError(statusCode uint32) error {
	if statusCode == 0 {
		return nil
	}
	if statusCode >= internalErrBase {
		return &StatusCodeError{
			StatusCode: statusCode,
			ErrName:    fmt.Sprintf("UBSE_INTERNAL_%d", statusCode),
		}
	}
	if e, ok := errMap[statusCode]; ok {
		return &StatusCodeError{
			StatusCode: e.StatusCode,
			ErrName:    e.ErrName,
		}
	}
	return &StatusCodeError{
		StatusCode: statusCode,
		ErrName:    fmt.Sprintf("UBSE_UNKNOWN_%d", statusCode),
	}
}
