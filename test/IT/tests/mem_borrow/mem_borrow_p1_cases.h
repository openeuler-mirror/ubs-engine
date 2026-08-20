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

#ifndef IT_MEM_BORROW_P1_CASES_H
#define IT_MEM_BORROW_P1_CASES_H

#include "it_cluster.h"

namespace ubse::it::tests::mem_borrow {

// CLI内存操作测试（长选项）：使用长选项创建→查询borrow_detail/node_borrow/node_lend→删除NUMA内存
void RunP1CliCreateNumaParamVariant01(ubse::it::infra::ItCluster& cluster);

// P1-FdBorrow-MultiNode-Ok-01(四节点): 多节点 FD 借用用例
void RunP1FdBorrowMultiNodeOk01(ubse::it::infra::ItCluster& cluster);

// P1-FdBorrow-MultiTime-Ok-01: FD 多次借用用例
void RunP1FdBorrowMultiTimeOk01(ubse::it::infra::ItCluster& cluster);

// P1-FdCreate-DiffNode-Ok-01: fd创建用例，不同节点创建同名FD
void RunP1FdCreateDiffNodeOk01(ubse::it::infra::ItCluster& cluster);

// P1-FdGet-DiffNode-Ok-01: fd获取用例，不同节点获取同名FD
void RunP1FdGetDiffNodeOk01(ubse::it::infra::ItCluster& cluster);

// P1-NumaCreate-MultiNode-Ok-01(四节点): 多节点 NUMA 创建用例
void RunP1NumaCreateMultiNodeOk01(ubse::it::infra::ItCluster& cluster);

// P1-NumaCreate-MultiTime-Ok-01: numa 创建用例，多轮创建后归还
void RunP1NumaCreateMultiTimeOk01(ubse::it::infra::ItCluster& cluster);

// P1-NumaCreate-DiffNode-Ok-01: numa 创建用例，不同节点创建同名NUMA
void RunP1NumaCreateDiffNodeOk01(ubse::it::infra::ItCluster& cluster);

// P1-NumaGet-DiffNode-Ok-01: numa 获取用例，不同节点获取同名NUMA
void RunP1NumaGetDiffNodeOk01(ubse::it::infra::ItCluster& cluster);

// 四节点SHM attach后import_desc_cnt验证：节点1创建 → 节点2/3/4分别attach(每个返回import_desc_cnt=1) → detach → delete
void RunP1ShmAttachMultiNode01(ubse::it::infra::ItCluster& cluster);
} // namespace ubse::it::tests::mem_borrow

#endif // IT_MEM_BORROW_P1_CASES_H
