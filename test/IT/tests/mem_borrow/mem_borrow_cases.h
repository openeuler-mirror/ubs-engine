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

// CLI内存类型过滤查询测试：创建NUMA/FD/SHARE三种类型内存，按类型和名称查询借用详情，验证完整生命周期
void RunCliMemoryTypeFilterOperations001(ubse::it::infra::ItCluster& cluster);

// CLI NUMA状态查询测试：查询NUMA状态（基本查询和显示所有大页），验证输出格式
void RunCliNumaStatusQuery001(ubse::it::infra::ItCluster& cluster);

// CLI内存配置查询测试：查询内存配置信息，验证输出格式
void RunCliMemoryConfigQuery001(ubse::it::infra::ItCluster& cluster);

// 四节点SHM attach后import_desc_cnt验证：节点1创建 → 节点2/3/4分别attach(每个返回import_desc_cnt=1) → detach → delete
void RunShmFourNodesAttachImportDescCntTest(ubse::it::infra::ItCluster& cluster);

} // namespace ubse::it::tests::mem_borrow

#endif // IT_MEM_BORROW_CASES_H
