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

#ifndef IT_MEM_BORROW_FAULT_LOG_CASES_H
#define IT_MEM_BORROW_FAULT_LOG_CASES_H

#include "it_cluster.h"

namespace ubse::it::tests::mem_borrow {

// ==================== P1 Fault Log 测试 ====================

// P1-FaultLog-BorrowCheckFailed-01: NUMA借用传入的链路不存在 触发 BORROW_CHECK_FAILED
void RunP1FaultLogBorrowCheckFailed(ubse::it::infra::ItCluster& cluster);

// P1-FaultLog-BorrowNameExist-01: FD/NUMA/Share 同名重复创建触发 BORROW_NAME_EXIST
void RunP1FaultLogBorrowNameExist(ubse::it::infra::ItCluster& cluster);

// P1-FaultLog-BorrowChipNotSupport-01: 底层芯片不支持FD/NUMA借用 触发 BORROW_CHIP_NOT_SUPPORT
void RunP1FaultLogBorrowChipNotSupport(ubse::it::infra::ItCluster& cluster);

// P1-FaultLog-BorrowScheduleFailed-01: 借用调度失败 触发 BORROW_SCHEDULE_FAILED
void RunP1FaultLogBorrowScheduleFailed(ubse::it::infra::ItCluster& cluster);

// P1-FaultLog-BorrowObmmExportFailed-01: OBMM导出失败 触发 BORROW_OBMM_EXPORT_FAILED
void RunP1FaultLogBorrowObmmExportFailed(ubse::it::infra::ItCluster& cluster);

// P1-FaultLog-BorrowObmmImportFailed-01: OBMM导入失败 触发 BORROW_OBMM_IMPORT_FAILED
void RunP1FaultLogBorrowObmmImportFailed(ubse::it::infra::ItCluster& cluster);

// P1-FaultLog-ShareBorrowCheckFailed-01: Share借用传入的亲和的socket_id不存在 触发 SHARED_BORROW_CHECK_FAILED
void RunP1FaultLogShareBorrowCheckFailed(ubse::it::infra::ItCluster& cluster);

// P1-FaultLog-ShareAttachCheckFailed-01: Share attach不存在的共享内存 或 attach请求节点不在共享域 触发 SHARED_ATTACH_CHECK_FAILED
void RunP1FaultLogShareAttachCheckFailed(ubse::it::infra::ItCluster& cluster);

// P1-FaultLog-ShareAttachAuthFailed-01: Share attach与create时用户身份不一致 触发 SHARED_ATTACH_AUTH_FAILED
void RunP1FaultLogShareAttachAuthFailed(ubse::it::infra::ItCluster& cluster);

// P1-FaultLog-ShareAttachExist-01: Share attach请求节点重复attach 触发 SHARED_ATTACH_EXIST
void RunP1FaultLogShareAttachExist(ubse::it::infra::ItCluster& cluster);

// P1-FaultLog-ShareChipNotSupported-01: 底层芯片不支持Shared attach 触发 SHARED_CHIP_NOT_SUPPORTED
void RunP1FaultLogShareChipNotSupported(ubse::it::infra::ItCluster& cluster);

// P1-FaultLog-ShareChipModeNotSupported-01: 底层芯片模式不支持Share借用模式 触发 SHARED_CHIP_MODE_NOT_SUPPORTED
void RunP1FaultLogShareChipModeNotSupported(ubse::it::infra::ItCluster& cluster);

// P1-FaultLog-ReturnNameNotExist-01:  FD/NUMA/Share 归还不存在的共享内存 触发 RETURN_NAME_NOT_EXIST
void RunP1FaultLogReturnNameNotExist(ubse::it::infra::ItCluster& cluster);

// P1-FaultLog-ReturnChipNotSupported-01: 底层芯片不支持FD/NUMA归还 触发 RETURN_CHIP_NOT_SUPPORTED
void RunP1FaultLogReturnChipNotSupported(ubse::it::infra::ItCluster& cluster);

// P1-FaultLog-ReturnObmmExportFailed-01: OBMM导出失败 触发 RETURN_OBMM_EXPORT_FAILED
void RunP1FaultLogReturnObmmExportFailed(ubse::it::infra::ItCluster& cluster);

// P1-FaultLog-ReturnObmmImportFailed-01: OBMM导入失败 触发 RETURN_OBMM_IMPORT_FAILED
void RunP1FaultLogReturnObmmImportFailed(ubse::it::infra::ItCluster& cluster);

// P1-FaultLog-ShareReturnInAttached-01: Share 归还节点存在attach 触发 SHARED_RETURN_IN_ATTACHED
void RunP1FaultLogShareReturnInAttached(ubse::it::infra::ItCluster& cluster);

// P1-FaultLog-ShareReturnRegionFailed-01: Share 归还节点不在共享域 触发 SHARED_RETURN_REGION_FAILED
void RunP1FaultLogShareReturnRegionFailed(ubse::it::infra::ItCluster& cluster);

// P1-FaultLog-ShareDetachNotExist-01: Share detach不存在的共享内存 触发 SHARED_DETACH_NOT_EXIST
void RunP1FaultLogShareDetachNotExist(ubse::it::infra::ItCluster& cluster);

// P1-FaultLog-ShareReturnChipNotSupported-01: 底层芯片不支持Share归还 触发 SHARED_RETURN_CHIP_NOT_SUPPORTED
void RunP1FaultLogShareReturnChipNotSupported(ubse::it::infra::ItCluster& cluster);
} // namespace ubse::it::tests::mem_borrow

#endif // IT_MEM_BORROW_FAULT_LOG_CASES_H
