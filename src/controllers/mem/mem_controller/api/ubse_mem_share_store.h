/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBS_ENGINE_UBSE_MEM_SHARE_STORE_H
#define UBS_ENGINE_UBSE_MEM_SHARE_STORE_H

#include <functional>
#include <string>
#include <vector>

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_mmi_def.h"

namespace ubse::mem::controller {
using ubse::common::def::UbseResult;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowExportObj;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowImportObj;
using ubse::adapter_plugins::mmi::UbseMemShareAttachReq;
using ubse::adapter_plugins::mmi::UbseMemState;

/**
 * @brief 共享内存借用账本存储策略接口（拓扑无关）
 * @note 唯一变化点"存储内容"的抽象：
 *       - CascadeMasterStore   全量对象，落 UbseMemDebtLedger（单层主）
 *       - GlobalMasterStore 摘要项，落 UbseGlobalLedgerSummaryStore（分层协调全局主）
 */
class IShareStore {
public:
    virtual ~IShareStore() = default;

    // 读 / 存在性
    virtual bool ExistBorrow(const std::string &name) = 0;
    virtual bool ExistAttach(const std::string &importNodeId, const std::string &name) = 0;
    virtual UbseResult LoadExport(const std::string &name, UbseMemShareBorrowExportObj &out) = 0;
    virtual UbseResult LoadImport(const std::string &importNodeId, const std::string &name,
                                  UbseMemShareBorrowImportObj &out) = 0;
    virtual UbseResult LoadAllImports(const std::string &name,
                                       std::vector<UbseMemShareBorrowImportObj> &out) = 0;

    // 遍历
    using ExportVisitor = std::function<void(const std::string &nodeId, const std::string &name,
                                              const UbseMemShareBorrowExportObj &obj)>;
    using ImportVisitor = std::function<void(const std::string &nodeId, const std::string &name,
                                              const UbseMemShareBorrowImportObj &obj)>;
    virtual void ForEachExport(ExportVisitor visitor) = 0;
    virtual void ForEachImport(ImportVisitor visitor) = 0;

    // 写
    virtual void PutExport(const UbseMemShareBorrowExportObj &obj) = 0;
    virtual void PutImport(const UbseMemShareBorrowImportObj &obj) = 0;
    virtual void UpdateExportState(const UbseMemShareBorrowExportObj &obj, const UbseMemState &state) = 0;
    virtual void UpdateImportState(const UbseMemShareBorrowImportObj &obj, const UbseMemState &state) = 0;
    virtual void UpdateExportMemIds(const UbseMemShareBorrowExportObj &obj) = 0;
    virtual void UpdateImportMemIds(const UbseMemShareBorrowImportObj &obj) = 0;
    virtual void RemoveExport(const UbseMemShareBorrowExportObj &obj) = 0;
    virtual void RemoveImport(const UbseMemShareBorrowImportObj &obj) = 0;

    // 能力
    virtual uint32_t GetCnaTopo(const UbseMemShareAttachReq &req,
                                     const UbseMemShareBorrowExportObj &exportObj,
                                     UbseMemShareBorrowImportObj &importObj) = 0;
};

/**
 * @brief 全量对象存储实现，背后为 UbseMemDebtLedger
 * @note 单层 master 与分层中继 group master 均使用此实现。
 */
class CascadeMasterStore : public IShareStore {
public:
    bool ExistBorrow(const std::string &name) override;
    bool ExistAttach(const std::string &importNodeId, const std::string &name) override;
    UbseResult LoadExport(const std::string &name, UbseMemShareBorrowExportObj &out) override;
    UbseResult LoadImport(const std::string &importNodeId, const std::string &name,
                          UbseMemShareBorrowImportObj &out) override;
    UbseResult LoadAllImports(const std::string &name,
                               std::vector<UbseMemShareBorrowImportObj> &out) override;

    void ForEachExport(ExportVisitor visitor) override;
    void ForEachImport(ImportVisitor visitor) override;

    void PutExport(const UbseMemShareBorrowExportObj &obj) override;
    void PutImport(const UbseMemShareBorrowImportObj &obj) override;
    void UpdateExportState(const UbseMemShareBorrowExportObj &obj, const UbseMemState &state) override;
    void UpdateImportState(const UbseMemShareBorrowImportObj &obj, const UbseMemState &state) override;
    void UpdateExportMemIds(const UbseMemShareBorrowExportObj &obj) override;
    void UpdateImportMemIds(const UbseMemShareBorrowImportObj &obj) override;
    void RemoveExport(const UbseMemShareBorrowExportObj &obj) override;
    void RemoveImport(const UbseMemShareBorrowImportObj &obj) override;

    uint32_t GetCnaTopo(const UbseMemShareAttachReq &req, const UbseMemShareBorrowExportObj &exportObj,
                             UbseMemShareBorrowImportObj &importObj) override;
};

/**
 * @brief 摘要项存储实现，背后为 UbseGlobalLedgerSummaryStore
 * @note 分层协调 global master 使用此实现。
 */
class GlobalMasterStore : public IShareStore {
public:
    bool ExistBorrow(const std::string &name) override;
    bool ExistAttach(const std::string &importNodeId, const std::string &name) override;
    UbseResult LoadExport(const std::string &name, UbseMemShareBorrowExportObj &out) override;
    UbseResult LoadImport(const std::string &importNodeId, const std::string &name,
                          UbseMemShareBorrowImportObj &out) override;
    UbseResult LoadAllImports(const std::string &name,
                               std::vector<UbseMemShareBorrowImportObj> &out) override;

    void ForEachExport(ExportVisitor visitor) override;
    void ForEachImport(ImportVisitor visitor) override;

    void PutExport(const UbseMemShareBorrowExportObj &obj) override;
    void PutImport(const UbseMemShareBorrowImportObj &obj) override;
    void UpdateExportState(const UbseMemShareBorrowExportObj &obj, const UbseMemState &state) override;
    void UpdateImportState(const UbseMemShareBorrowImportObj &obj, const UbseMemState &state) override;
    void UpdateExportMemIds(const UbseMemShareBorrowExportObj &obj) override;
    void UpdateImportMemIds(const UbseMemShareBorrowImportObj &obj) override;
    void RemoveExport(const UbseMemShareBorrowExportObj &obj) override;
    void RemoveImport(const UbseMemShareBorrowImportObj &obj) override;

    uint32_t GetCnaTopo(const UbseMemShareAttachReq &req, const UbseMemShareBorrowExportObj &exportObj,
                             UbseMemShareBorrowImportObj &importObj) override;
};

} // namespace ubse::mem::controller

#endif // UBS_ENGINE_UBSE_MEM_SHARE_STORE_H
