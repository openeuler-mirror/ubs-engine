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

// 双节点SHM 创建→节点1attach→borrow_detail查账(存在)→detach/delete→再查账(为空)
void RunP1CliBorrowDetailLedger01(ubse::it::infra::ItCluster& cluster);

// 双节点SHM 节点1/节点2各自创建(region覆盖双节点)→borrow_detail查账(2条share记录)→节点2删除→再查账(仅剩节点1记录)
void RunP1CliBorrowDetailMultiNode01(ubse::it::infra::ItCluster& cluster);

// P1-ShmCreate-Concurrent-01: 双节点，8并发创建共享内存，全部创建成功
void RunP1ShmCreateConcurrent01(ubse::it::infra::ItCluster& cluster);
// P1-ShmCreate-Concurrent-MultiNode-01: 四节点，4节点各自并发创建共享内存，全部创建成功
void RunP1ShmCreateConcurrentMultiNode01(ubse::it::infra::ItCluster& cluster);
// P1-ShmCreate-WithProviders-MultiNode-01: 四节点，指定2个provider创建共享内存，校验export_node在provider集合内
void RunP1ShmCreateWithProvidersMultiNode01(ubse::it::infra::ItCluster& cluster);
// P1-ShmRecreate-AfterDelete-01: 双节点，节点1创建SHM→删除→节点2创建同名SHM成功（验证删除释放同名）
void RunP1ShmRecreateAfterDelete01(ubse::it::infra::ItCluster& cluster);

// P1-ShmDetach-MultiNode-01: 四节点SHM attach后detach，校验内存账本consumer归空
void RunP1ShmDetachMultiNode01(ubse::it::infra::ItCluster& cluster);

// P1-ShmDetachReattach-MultiNode-01: 双节点SHM 节点1创建→节点1attach→节点1detach→节点2attach，全部成功
void RunP1ShmDetachReattachMultiNode01(ubse::it::infra::ItCluster& cluster);
} // namespace ubse::it::tests::mem_borrow

#endif // IT_MEM_BORROW_P1_CASES_H
