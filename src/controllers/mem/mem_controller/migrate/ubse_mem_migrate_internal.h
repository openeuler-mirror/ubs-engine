/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef UBSE_MEM_MIGRATE_INTERNAL_H
#define UBSE_MEM_MIGRATE_INTERNAL_H
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "../ubse_mem_migrate_session.h"
#include "adapter_plugins/mmi/ubse_mmi_def.h"

namespace ubse::mem::controller {

namespace migrate_detail {
using namespace ubse::adapter_plugins::mmi;
// 业务结果等待期限（秒），发送重试使用借用链路常量
constexpr uint32_t MIGRATE_PHASE_TIMEOUT_S = 10;
constexpr uint64_t MIGRATE_COPY_TIMEOUT_BASE_S = 30;
constexpr uint64_t MIGRATE_COPY_TIMEOUT_BYTES_PER_S = 4ULL * 1024 * 1024 * 1024;
using ConstExportObjPtr = std::shared_ptr<const UbseMemShareBorrowExportObj>;
using ConstImportObjPtr = std::shared_ptr<const UbseMemShareBorrowImportObj>;

// 编排运行上下文：准备新导出时构建，逐块迁移由异步流持有；对外会话状态由 SessionManager 保存。
struct MigrationRunContext {
    std::string name{};
    std::string srcNodeId{};                    // 故障导出节点
    std::string dstNodeId{};                    // 目标节点
    ConstExportObjPtr oldObj{};                 // 老 ExportObj
    UbseMemShareBorrowExportObj newExportObj{}; // 新导出产出(响应落实后的新导出对象)
    std::vector<std::string> importNodes{};     // 仅保存参与节点，块数据来自账本
    bool endCommitStarted{false};               // end(1) 一经下发，新导出可能已被导入节点使用
    UbseMemMigrateTargetKind targetKind{UbseMemMigrateTargetKind::NOT_IMPORT_NODE};
    ConstImportObjPtr targetImportObj{}; // 目标节点已导入该共享内存时的原导入对象
    bool targetBeginStarted{
        false}; // 在下发目标begin_export前置位，不代表执行成功；首块完整回退后清除，失败时据此保留待核对账本。
    std::shared_ptr<UbseMemShmMigrateSession>
        sessionToken{}; // 本次会话实例的身份凭证，通过指针比较隔离同名新旧会话，防止旧任务推进或收尾新会话。
    bool exportConfirmed{false}; // 已收到普通导出的实际业务结果（不一定成功），作为允许进入新导出清理流程的前提。
};

// API 启动 master 编排的内部入口。
uint32_t StartNewExport(MigrationRunContext ctx);
uint32_t ExecuteMappedTargetMigration(MigrationRunContext& ctx);

} // namespace migrate_detail
} // namespace ubse::mem::controller
#endif
