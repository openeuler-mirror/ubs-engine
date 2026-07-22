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

#include <string>
#include <vector>

namespace ubse::it::tests::mem_borrow {

// CLI查询节点内存状态测试：调用check memory命令，验证返回包含两个节点的状态信息
void RunP0CliCheckMemOk01(ubse::it::infra::ItCluster& cluster);

// CLI内存操作测试（短选项）：使用短选项创建→查询borrow_detail/node_borrow/node_lend→删除NUMA内存
void RunP0CliCreateNumaOk01(ubse::it::infra::ItCluster& cluster);

// CLI内存操作测试（长选项）：使用长选项创建→查询borrow_detail/node_borrow/node_lend→删除NUMA内存
void RunP1CliCreateNumaParamVariant01(ubse::it::infra::ItCluster& cluster);

// CLI内存类型过滤查询测试：创建NUMA/FD/SHARE三种类型内存，按类型和名称查询借用详情，验证完整生命周期
void RunP1CliBorrowDetailOk01(ubse::it::infra::ItCluster& cluster);

// CLI NUMA状态查询测试：查询NUMA状态（基本查询和显示所有大页），验证输出格式
void RunP0CliNumaStatusOk01(ubse::it::infra::ItCluster& cluster);

// CLI内存配置查询测试：查询内存配置信息，验证输出格式
void RunP0CliMemConfigOk01(ubse::it::infra::ItCluster& cluster);

// 四节点SHM attach后import_desc_cnt验证：节点1创建 → 节点2/3/4分别attach(每个返回import_desc_cnt=1) → detach → delete
void RunP1ShmAttachMultiNode01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_fd_create ====================
void RunP0FdCreateOk01(ubse::it::infra::ItCluster& cluster);
void RunP0FdCreateOverLen01(ubse::it::infra::ItCluster& cluster);
void RunP0FdCreateInvalidVal01(ubse::it::infra::ItCluster& cluster);
void RunP0FdCreateInvalidVal02(ubse::it::infra::ItCluster& cluster);
void RunP0FdCreateDup01(ubse::it::infra::ItCluster& cluster);
void RunP0FdCreateNullPtr01(ubse::it::infra::ItCluster& cluster);
void RunP0FdCreateBoundMin01(ubse::it::infra::ItCluster& cluster);
void RunP0FdCreateBoundMax01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_fd_create_with_lender ====================
void RunP0FdCreateLenderOk01(ubse::it::infra::ItCluster& cluster);
void RunP0FdCreateLenderNullPtr01(ubse::it::infra::ItCluster& cluster);
void RunP0FdCreateLenderOverLen01(ubse::it::infra::ItCluster& cluster);
void RunP0FdCreateLenderInvalidVal01(ubse::it::infra::ItCluster& cluster);
void RunP0FdCreateLenderInvalidVal02(ubse::it::infra::ItCluster& cluster);
void RunP0FdCreateLenderNullPtr02(ubse::it::infra::ItCluster& cluster);
void RunP0FdCreateLenderBadParam01(ubse::it::infra::ItCluster& cluster);
void RunP0FdCreateLenderDup01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_fd_create_with_candidate ====================
void RunP0FdCreateCandidateOk01(ubse::it::infra::ItCluster& cluster);
void RunP0FdCreateCandidateOverLen01(ubse::it::infra::ItCluster& cluster);
void RunP0FdCreateCandidateInvalidVal01(ubse::it::infra::ItCluster& cluster);
void RunP0FdCreateCandidateInvalidVal02(ubse::it::infra::ItCluster& cluster);
void RunP0FdCreateCandidateNullPtr01(ubse::it::infra::ItCluster& cluster);
void RunP0FdCreateCandidateBadParam01(ubse::it::infra::ItCluster& cluster);
void RunP0FdCreateCandidateDup01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_fd_permission ====================
void RunP0FdPermNotExist01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_fd_get ====================
void RunP0FdGetNotExist01(ubse::it::infra::ItCluster& cluster);
void RunP0FdGetNullPtr01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_fd_list ====================
void RunP0FdListOk01(ubse::it::infra::ItCluster& cluster);
void RunP0FdListNullPtr01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_fd_delete ====================
void RunP0FdDelOk01(ubse::it::infra::ItCluster& cluster);
void RunP0FdDelNotExist01(ubse::it::infra::ItCluster& cluster);
void RunP0FdDelDup01(ubse::it::infra::ItCluster& cluster);
void RunP0FdDelOverLen01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_fd_get_memid_by_import ====================
void RunP0FdMemidByImportFld01(ubse::it::infra::ItCluster& cluster);
void RunP0FdMemidByImportNotExist01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_fd_fault_register ====================
void RunP0FdFaultRegNullPtr01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_numastat_get ====================
void RunP0NumaStatGetFld01(ubse::it::infra::ItCluster& cluster);
void RunP0NumaStatGetNotExist01(ubse::it::infra::ItCluster& cluster);
void RunP0NumaStatGetNullPtr01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_numa_create ====================
void RunP0NumaCreateOk01(ubse::it::infra::ItCluster& cluster);
void RunP0NumaCreateOverLen01(ubse::it::infra::ItCluster& cluster);
void RunP0NumaCreateInvalidVal01(ubse::it::infra::ItCluster& cluster);
void RunP0NumaCreateDup01(ubse::it::infra::ItCluster& cluster);
void RunP0NumaCreateNullPtr01(ubse::it::infra::ItCluster& cluster);
void RunP0NumaCreateBoundMin01(ubse::it::infra::ItCluster& cluster);
void RunP0NumaCreateBoundMax01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_numa_create_with_lender ====================
void RunP0NumaCreateLenderOk01(ubse::it::infra::ItCluster& cluster);
void RunP0NumaCreateLenderOverLen01(ubse::it::infra::ItCluster& cluster);
void RunP0NumaCreateLenderInvalidVal01(ubse::it::infra::ItCluster& cluster);
void RunP0NumaCreateLenderNullPtr01(ubse::it::infra::ItCluster& cluster);
void RunP0NumaCreateLenderNullPtr02(ubse::it::infra::ItCluster& cluster);
void RunP0NumaCreateLenderBadParam01(ubse::it::infra::ItCluster& cluster);
void RunP0NumaCreateLenderDup01(ubse::it::infra::ItCluster& cluster);
void RunP0NumaCreateLenderBoundMax01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_numa_create_with_candidate ====================
void RunP0NumaCreateCandidateOk01(ubse::it::infra::ItCluster& cluster);
void RunP0NumaCreateCandidateOverLen01(ubse::it::infra::ItCluster& cluster);
void RunP0NumaCreateCandidateInvalidVal01(ubse::it::infra::ItCluster& cluster);
void RunP0NumaCreateCandidateNullPtr01(ubse::it::infra::ItCluster& cluster);
void RunP0NumaCreateCandidateBadParam01(ubse::it::infra::ItCluster& cluster);
void RunP0NumaCreateCandidateDup01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_numa_get ====================
void RunP0NumaGetNotExist01(ubse::it::infra::ItCluster& cluster);
void RunP0NumaGetNullPtr01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_numa_list ====================
void RunP0NumaListOk01(ubse::it::infra::ItCluster& cluster);
void RunP0NumaListNullPtr01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_numa_delete ====================
void RunP0NumaDelOk01(ubse::it::infra::ItCluster& cluster);
void RunP0NumaDelNotExist01(ubse::it::infra::ItCluster& cluster);
void RunP0NumaDelDup01(ubse::it::infra::ItCluster& cluster);
void RunP0NumaDelOverLen01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_numa_get_memid_by_import ====================
void RunP0NumaMemidByImportFld01(ubse::it::infra::ItCluster& cluster);
void RunP0NumaMemidByImportNotExist01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_numa_fault_register ====================
void RunP0NumaFaultRegNullPtr01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_shm_create ====================
void RunP0ShmCreateOk01(ubse::it::infra::ItCluster& cluster, const std::vector<std::string>& regionNodeIds);
void RunP0ShmCreateWithProviderOk01(ubse::it::infra::ItCluster& cluster);
void RunP0ShmCreateOverLen01(ubse::it::infra::ItCluster& cluster);
void RunP0ShmCreateInvalidVal01(ubse::it::infra::ItCluster& cluster);
void RunP0ShmCreateDup01(ubse::it::infra::ItCluster& cluster);
void RunP0ShmCreateInvalidVal02(ubse::it::infra::ItCluster& cluster);
void RunP0ShmCreateNullPtr01(ubse::it::infra::ItCluster& cluster);
void RunP0ShmCreateBoundMin01(ubse::it::infra::ItCluster& cluster);
void RunP0ShmCreateBoundMax01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_shm_create_with_affinity ====================
void RunP0ShmCreateAffinityBadParam01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_shm_create_with_lender ====================
void RunP0ShmCreateLenderOk01(ubse::it::infra::ItCluster& cluster, const std::vector<std::string>& regionNodeIds);
void RunP0ShmCreateLenderOverLen01(ubse::it::infra::ItCluster& cluster);
void RunP0ShmCreateLenderInvalidVal01(ubse::it::infra::ItCluster& cluster);
void RunP0ShmCreateLenderNullPtr01(ubse::it::infra::ItCluster& cluster);
void RunP0ShmCreateLenderBadParam01(ubse::it::infra::ItCluster& cluster);
void RunP0ShmCreateLenderDup01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_shm_attach ====================
void RunP0ShmAttachOk01(ubse::it::infra::ItCluster& cluster);
void RunP0ShmAttachNotReady01(ubse::it::infra::ItCluster& cluster);
void RunP0ShmAttachDup01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_shm_get ====================
void RunP0ShmGetNotExist01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_shm_list ====================
void RunP0ShmListOk01(ubse::it::infra::ItCluster& cluster);
void RunP0ShmListNullPtr01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_shm_detach ====================
void RunP0ShmDetachOk01(ubse::it::infra::ItCluster& cluster);
void RunP0ShmDetachNotReady01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_shm_delete ====================
void RunP0ShmDelOk01(ubse::it::infra::ItCluster& cluster);
void RunP0ShmDelNotExist01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_shm_fault_register ====================
void RunP0ShmFaultRegNullPtr01(ubse::it::infra::ItCluster& cluster);

// ==================== ubs_mem_shm_get_memid_by_import ====================
void RunP0ShmMemidByImportNotExist01(ubse::it::infra::ItCluster& cluster);
void RunP0ShmMemidByImportInvalidVal01(ubse::it::infra::ItCluster& cluster);

// ==================== CLI P0 用例 ====================

// P0-CliCreateNuma-InvalidChar-01: CLI create numa with illegal name
void RunP0CliCreateNumaInvalidChar01(ubse::it::infra::ItCluster& cluster);

// P0-CliCreateNuma-Dup-01: CLI create numa twice, second fails
void RunP0CliCreateNumaDup01(ubse::it::infra::ItCluster& cluster);

// P0-CliCreateFd-Ok-01: CLI create fd success
void RunP0CliCreateFdOk01(ubse::it::infra::ItCluster& cluster);

// P0-CliCreateFd-InvalidVal-01: CLI create fd with size=0
void RunP0CliCreateFdInvalidVal01(ubse::it::infra::ItCluster& cluster);

// P0-CliCreateShare-Ok-01: CLI create share success
void RunP0CliCreateShareOk01(ubse::it::infra::ItCluster& cluster);

// P0-CliCreateShare-OverLen-01: CLI create share with name too long
void RunP0CliCreateShareOverLen01(ubse::it::infra::ItCluster& cluster);

// P0-CliDelMem-NotExist-01: CLI delete non-existent memory
void RunP0CliDelMemNotExist01(ubse::it::infra::ItCluster& cluster);

// P0-CliBorrowDetail-Ok-01: CLI borrow_detail with no borrows, output empty
void RunP0CliBorrowDetailEmpty01(ubse::it::infra::ItCluster& cluster);

// P0-CliAttachMem-NotReady-01: CLI attach non-existent shared memory
void RunP0CliAttachMemNotReady01(ubse::it::infra::ItCluster& cluster);

// P0-CliDetachMem-NotReady-01: CLI detach non-attached memory
void RunP0CliDetachMemNotReady01(ubse::it::infra::ItCluster& cluster);

} // namespace ubse::it::tests::mem_borrow

#endif // IT_MEM_BORROW_CASES_H
