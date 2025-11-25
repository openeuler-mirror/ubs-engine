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

#ifndef UBSE_MANAGER_MEM_INSTANCE_INNER_H
#define UBSE_MANAGER_MEM_INSTANCE_INNER_H

#include <sstream>
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_mem_resource.h"
#include "ubse_mem_types.h"
#include "ubse_mem_obj.h"
#include "ubse_obmm_executor.h"
#include "ubse_obmm_meta_restore.h"
#include "ubse_obmm_utils.h"

namespace ubse::mmi {
using namespace ubse::mem::obj;
using namespace ubse::resource::mem;
using namespace ubse::common::def;

class MemInstanceInner {
public:
    static MemInstanceInner &GetInstance()
    {
        static MemInstanceInner instance;
        return instance;
    }
    MemInstanceInner(const MemInstanceInner &other) = delete;
    MemInstanceInner(MemInstanceInner &&other) = delete;
    MemInstanceInner &operator=(const MemInstanceInner &other) = delete;
    MemInstanceInner &operator=(MemInstanceInner &&other) noexcept = delete;

    uint32_t MemNumaImportExecutor(UbseMemNumaBorrowImportObj &importObj);
    uint32_t MemNumaExportExecutor(UbseMemNumaBorrowExportObj &exportObj);
    uint32_t MemNumaUnExportExecutor(const UbseMemNumaBorrowExportObj &exportObj);
    uint32_t MemNumaUnImportExecutor(const UbseMemNumaBorrowImportObj &importObj);

    uint32_t MemFdImportExecutor(UbseMemFdBorrowImportObj &importObj);
    uint32_t MemFdUnImportExecutor(const UbseMemFdBorrowImportObj &importObj);
    uint32_t MemFdExportExecutor(UbseMemFdBorrowExportObj &exportObj);
    uint32_t MemFdUnExportExecutor(const UbseMemFdBorrowExportObj &exportObj);

    uint32_t MemGetObjData(NodeMemDebtInfo &memBorrowObj, std::vector<UbseMemLocalObmmMetaData> &allObmmDatas);

    void RollbackImport(const std::vector<mem_id> &memids);

    void RollbackExport(const std::vector<mem_id> &memids);

    template <typename Obj>
    static bool UbseMemExecutorCheckParam(const Obj &obj)
    {
        return !obj.algoResult.importNumaInfos.empty() && !obj.algoResult.exportNumaInfos.empty() &&
               !obj.req.name.empty() && obj.req.size > 0;
    }

private:
    MemInstanceInner() = default;

    template <typename ExportObj>
    uint32_t UnExportExecutor(const ExportObj &exportObj)
    {
        UbseResult ret = UBSE_OK;
        std::vector<mem_id> memIds{};
        for (int i = 0; i < exportObj.status.exportObmmInfo.size(); i++) {
            if (exportObj.status.exportObmmInfo[i].memId == INVALID_MEM_ID) {
                UBSE_LOG_ERROR << "Export obmm memid is invalid, name = " << exportObj.req.name;
                return UBSE_ERROR_INVAL;
            }
            memIds.push_back(exportObj.status.exportObmmInfo[i].memId);
        }
        if (memIds.empty()) {
            UBSE_LOG_ERROR << "memIds is empty!";
            return UBSE_ERROR_INVAL;
        }
        ret = RmObmmExecutor::GetInstance().ObmmUnExport(memIds);
        if (UBSE_RESULT_FAIL(ret)) {
            UBSE_LOG_ERROR << "ObmmUnExport error!";
        }
        return ret;
    }

    uint64_t SetObmmDescDefaultErrorValue(std::vector<ubse_mem_obmm_mem_desc> &obmmMemDescs, const uint64_t totalSize,
                                          const uint64_t blockSize);
    template <typename Obj>
    uint32_t GetObmmExportParamFromRequest(Obj &exportObj, std::vector<ubse_mem_obmm_mem_desc> &obmmMemDescs,
                                           size_t sizes[MAX_NUMA_NODES])
    {
        auto blockSize = DEFAULT_BLOCK_SIZE;
        auto ret = RmObmmUtils::GetInstance().GetBlockSize(blockSize);
        if (UBSE_RESULT_FAIL(ret)) {
            UBSE_LOG_ERROR << "Get block size failed, ret = " << ret;
        }
        auto blockCount = SetObmmDescDefaultErrorValue(obmmMemDescs, exportObj.req.size, blockSize);
        if (blockCount <= 0) {
            UBSE_LOG_ERROR << "The blocks is " << blockCount;
            exportObj.status.errCode = UBSE_ERROR_INVAL;
            return UBSE_ERROR_INVAL;
        }
        const auto &numaIds = exportObj.algoResult.exportNumaInfos;
        for (size_t i = 0; i < numaIds.size(); i++) {
            if (numaIds[i].numaId >= MAX_NUMA_NODES || numaIds[i].numaId < 0 || numaIds[i].size <= 0) {
                UBSE_LOG_WARN << "Strategy param illegal, numaId is " << numaIds[i].numaId;
                continue;
            }
            sizes[numaIds[i].numaId] = numaIds[i].size;
        }
        return UBSE_OK;
    }
};
} // namespace ubse::mmi

#endif // UBSE_MEM_INSTANCE_INNER_H
