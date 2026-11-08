/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef UBS_ENGINE_UBSE_MEM_CONTROLLER_MIGRATE_API_H
#define UBS_ENGINE_UBSE_MEM_CONTROLLER_MIGRATE_API_H

#include <memory>
#include <string>

#include "../ubse_mem_migrate_session.h"
#include "adapter_plugins/mmi/ubse_mmi_def.h"

namespace ubse::mem::controller {
using ubse::adapter_plugins::mmi::UbseMemMigrateBeginExportReq;
using ubse::adapter_plugins::mmi::UbseMemMigrateBeginExportResp;
using ubse::adapter_plugins::mmi::UbseMemMigrateCancelResp;
using ubse::adapter_plugins::mmi::UbseMemMigrateDataCopyReq;
using ubse::adapter_plugins::mmi::UbseMemMigrateEndExportReq;
using ubse::adapter_plugins::mmi::UbseMemMigrateSwitchBeginImportReq;
using ubse::adapter_plugins::mmi::UbseMemMigrateSwitchEndImportReq;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowExportObj;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowImportObj;
using ubse::adapter_plugins::mmi::UbseMemShmMigrateGetResp;
using ubse::adapter_plugins::mmi::UbseMemShmMigrateReq;

// 判断两个导出对象是否指向同一份资源：迁移标记一致且各块设备memId一致；用于迟到回包与当前资源的归属核验。
bool IsSameExportResource(const UbseMemShareBorrowExportObj& left, const UbseMemShareBorrowExportObj& right);

// 共享内存故障迁移服务门面；实现分布于mem_controller/migrate的API、master、agent和账本单元。
class UbseMemShmMigrateService {
public:
    static UbseMemShmMigrateService& GetInstance();
    uint32_t CheckUserPermission(const std::string& name, const ubse::adapter_plugins::mmi::UbseUdsInfo& caller,
                                 bool sessionOnly = false);

    /* 受理：预检并完成调度决策或导出账本模板准备，再建会话并异步投递；同名活跃任务返回进行中。 */
    uint32_t Accept(const UbseMemShmMigrateReq& req);
    /* 查询: 活跃会话优先, 无活跃返回终态留存; 进度字段由会话派生, errCode 进 resp */
    void Get(const std::string& name, UbseMemShmMigrateGetResp& resp);
    /* 取消: 仅 INIT/EXPORTING(新导出完成前)允许, 锁内置 cancelRequested 置取消标志;
     * 清理由Execute启动检查点或导出完成回调异步续接执行, errCode 进 resp */
    void Cancel(const std::string& name, UbseMemMigrateCancelResp& resp);
    /* 执行体：使用受理时登记的导出账本执行普通导出或原导入设备就地切换；块级回调推进并完成收尾。 */
    uint32_t Execute(const std::string& name);
    void Export(const UbseMemShareBorrowExportObj& obj, UbseMemShareBorrowExportObj& rsp);
    /* agent 执行入口：同名锁内统一校验；requestMasterNodeId 为请求发起 Master，来自可信 RPC 上下文。 */
    uint32_t SwitchBeginImport(const UbseMemMigrateSwitchBeginImportReq& req, const std::string& requestMasterNodeId,
                               ubse::adapter_plugins::mmi::UbseMemMigrateOpResp& result);
    uint32_t SwitchBeginExport(const UbseMemMigrateBeginExportReq& req, const std::string& requestMasterNodeId,
                               UbseMemMigrateBeginExportResp& result);
    uint32_t SwitchEndExport(const UbseMemMigrateEndExportReq& req, const std::string& requestMasterNodeId,
                             ubse::adapter_plugins::mmi::UbseMemMigrateOpResp& result);
    uint32_t DataCopy(const UbseMemMigrateDataCopyReq& req, const std::string& requestMasterNodeId);
    uint32_t SwitchEndImport(const UbseMemMigrateSwitchEndImportReq& req, const std::string& requestMasterNodeId,
                             ubse::adapter_plugins::mmi::UbseMemMigrateOpResp& result);
    void ProcessExportRequest(const UbseMemShareBorrowExportObj& req);
    void ProcessSwitchBeginImportRequest(const UbseMemMigrateSwitchBeginImportReq& req,
                                         const std::string& requestMasterNodeId);
    void ProcessSwitchBeginExportRequest(const UbseMemMigrateBeginExportReq& req,
                                         const std::string& requestMasterNodeId);
    void ProcessCopyRequest(const UbseMemMigrateDataCopyReq& req, const std::string& requestMasterNodeId);
    void ProcessSwitchEndImportRequest(const UbseMemMigrateSwitchEndImportReq& req,
                                       const std::string& requestMasterNodeId);
    void ProcessSwitchEndExportRequest(const UbseMemMigrateEndExportReq& req, const std::string& requestMasterNodeId);
    // master结果handler投递线程池后调用；请求入队应答不进入这里。
    void NewExportResult(const UbseMemShareBorrowExportObj& result);
    void BeginExportResult(const UbseMemMigrateBeginExportResp& result);
    void BeginImportResult(const ubse::adapter_plugins::mmi::UbseMemMigrateOpResp& result);
    void CopyResult(const ubse::adapter_plugins::mmi::UbseMemMigrateOpResp& result);
    void EndResult(const ubse::adapter_plugins::mmi::UbseMemMigrateOpResp& result);
    /* 跨域只读: share 借用域 detach/unimport 检查"同名迁移进行中" */
    bool HasActiveMigration(const std::string& name) const;
    // 调用方持有name锁；归还仅检查非终态会话，新增使用/再次迁移另检查重建限制。
    uint32_t CheckOperationAllowed(const std::string& name, bool releasing = false) const;

private:
    UbseMemShmMigrateService() = default;
    ~UbseMemShmMigrateService() = default;
    UbseMemShmMigrateService(const UbseMemShmMigrateService&) = delete;
    UbseMemShmMigrateService& operator=(const UbseMemShmMigrateService&) = delete;

    uint32_t CheckSessionExecutable(const std::string& name, UbseMemShmMigrateSession& session,
                                    std::shared_ptr<UbseMemShmMigrateSession>& token);
};
} // namespace ubse::mem::controller

#endif // UBS_ENGINE_UBSE_MEM_CONTROLLER_MIGRATE_API_H
