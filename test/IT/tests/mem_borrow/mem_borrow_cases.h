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

#ifndef IT_MEM_BORROW_CASES_H
#define IT_MEM_BORROW_CASES_H

#include "it_cluster.h"

namespace ubse::it::tests::mem_borrow {

// 两节点NUMA正常借用测试：节点1(借出方)→节点2(借用方)
// 流程：创建借用 → 等待UBSE_EXIST就绪 → 验证属性 → 删除借用
void RunNumaNormalBorrowTest(ubse::it::infra::ItCluster& cluster);

// CLI查询节点内存状态测试：调用check memory命令，验证返回包含两个节点的状态信息
void RunCliQueryNodesMemoryStatus001(ubse::it::infra::ItCluster& cluster);

// CLI内存操作测试（短选项）：使用短选项创建→查询borrow_detail/node_borrow/node_lend→删除NUMA内存
void RunCliMemoryOperationsShortOpt001(ubse::it::infra::ItCluster& cluster);

// CLI内存操作测试（长选项）：使用长选项创建→查询borrow_detail/node_borrow/node_lend→删除NUMA内存
void RunCliMemoryOperationsLongOpt001(ubse::it::infra::ItCluster& cluster);

} // namespace ubse::it::tests::mem_borrow

#endif // IT_MEM_BORROW_CASES_H
