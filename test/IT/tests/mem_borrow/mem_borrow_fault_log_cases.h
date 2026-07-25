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

// ==================== P2 Fault Log 测试 ====================

// P1-FaultLog-BorrowCheckFailed-01: NUMA借用传入的链路不存在 触发 BORROW_CHECK_FAILED
void RunP1FaultLogBorrowCheckFailed(ubse::it::infra::ItCluster& cluster);

// P1-FaultLog-BorrowNameExist-01: FD/NUMA/Share 同名重复创建触发 BORROW_NAME_EXIST
void RunP1FaultLogBorrowNameExist(ubse::it::infra::ItCluster& cluster);

// P1-FaultLog-BorrowChipNotSupport-01: 底层芯片不支持FD/NUMA借用 触发 BORROW_CHIP_NOT_SUPPORT
// void RunP2FaultLogBorrowChipNotSupport(ubse::it::infra::ItCluster& cluster);

// P1-FaultLog-BorrowScheduleFailed-01: 借用调度失败 触发 BORROW_SCHEDULE_FAILED
void RunP1FaultLogBorrowScheduleFailed(ubse::it::infra::ItCluster& cluster);

// P1-FaultLog-ShareBorrowCheckFailed-01: Share借用传入的亲和的socket_id不存在 触发 SHARE_BORROW_CHECK_FAILED
void RunP1FaultLogShareBorrowCheckFailed(ubse::it::infra::ItCluster& cluster);

// P1-FaultLog-ShareAttachCheckFailed-01: Share attach请求节点不在共享域 触发 SHARE_ATTACH_CHECK_FAILED
void RunP1FaultLogShareAttachCheckFailed(ubse::it::infra::ItCluster& cluster);

// P1-FaultLog-ShareAttachAuthFailed-01: Share attach与create时用户身份不一致 触发 SHARE_ATTACH_AUTH_FAILED
void RunP1FaultLogShareAttachAuthFailed(ubse::it::infra::ItCluster& cluster);

// P1-FaultLog-ShareAttachExist-01: Share attach请求节点重复attach 触发 SHARE_ATTACH_EXIST
void RunP1FaultLogShareAttachExist(ubse::it::infra::ItCluster& cluster);
} // namespace ubse::it::tests::mem_borrow

#endif // IT_MEM_BORROW_FAULT_LOG_CASES_H
