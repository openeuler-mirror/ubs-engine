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

#ifndef UBSE_MEM_OBJ_RESTORE_H
#define UBSE_MEM_OBJ_RESTORE_H

#include <iosfwd>
#include "ubse_common_def.h"
#include "ubse_mem_resource.h"
#include "ubse_mem_types.h"
#include "ubse_mem_obj.h"
#include "ubse_mem_common_utils.h"
#include "ubse_obmm_executor.h"
namespace ubse::mmi {
using namespace ubse::mem::obj;
using namespace ubse::resource::mem;
using namespace ubse::common::def;

struct LocalObmmMetaData {
    std::vector<UbseMemLocalObmmMetaData> FdImportMetaData{};
    std::vector<UbseMemLocalObmmMetaData> FdExportMetaData{};
    std::vector<UbseMemLocalObmmMetaData> NumaImportMetaData{};
    std::vector<UbseMemLocalObmmMetaData> NumaExportMetaData{};
    std::vector<UbseMemLocalObmmMetaData> ShmImportMetaData{};
    std::vector<UbseMemLocalObmmMetaData> ShmExportMetaData{};
    std::vector<UbseMemLocalObmmMetaData> AddrImportMetaData{};
    std::vector<UbseMemLocalObmmMetaData> AddrExportMetaData{};
};

class RmMemObjRestore {
public:
    UbseResult ConstructFdExportObj(const std::vector<UbseMemLocalObmmMetaData> &exportLocalObmmMetaDatas,
                                    UbseMemFdExportObjMap &normalFdExportObjMap);

    void ConstructSingleFdExportObj(const std::vector<UbseMemLocalObmmMetaData> &exportLocalObmmMetaDatas,
                                    UbseMemFdBorrowExportObj &ubseMemFdBorrowExportObj, bool &isNormal);
    UbseResult ConstructFdImportObj(const std::vector<UbseMemLocalObmmMetaData> &fdImportLocalObmmMetaDatas,
                                    UbseMemFdImportObjMap &normalFdImportObjMap);
    void ConstructSingleFdImportObj(const std::vector<UbseMemLocalObmmMetaData> &fdImportLocalObmmMetaDatas,
                                    UbseMemFdBorrowImportObj &ubseMemFdBorrowImportObj, bool &isNormal);

    UbseResult ConstructNumaImportObj(const std::vector<UbseMemLocalObmmMetaData> &importLocalObmmMetaDatas,
                                      UbseMemNumaImportObjMap &normalImportObjMap);

    void ConstructSingleNumaImportObj(const std::vector<UbseMemLocalObmmMetaData> &importLocalObmmMetaDatas,
                                      UbseMemNumaBorrowImportObj &ubseMemNumaBorrowImportObj, bool &isNormal);

    UbseResult ConstructNumaExportObj(const std::vector<UbseMemLocalObmmMetaData> &exportLocalObmmMetaDatas,
                                      UbseMemNumaExportObjMap &normalExportObjMap);

    void ConstructSingleNumaExportObj(const std::vector<UbseMemLocalObmmMetaData> &exportLocalObmmMetaDatas,
                                      UbseMemNumaBorrowExportObj &ubseMemNumaBorrowExportObj, bool &isNormal);

    void GetBorrowObjMap(const std::vector<UbseMemLocalObmmMetaData> &localObmmMetaDatas,
                         std::unordered_map<std::string, std::vector<UbseMemLocalObmmMetaData>> &borrowObjMap);

    UbseResult GetLocalObmmMeta(std::vector<UbseMemLocalObmmMetaData> &allObmmDatas,
                                LocalObmmMetaData &localObmmMetaData);

    static RmMemObjRestore &GetInstance()
    {
        static RmMemObjRestore instance;
        return instance;
    }
    RmMemObjRestore(const RmMemObjRestore &other) = delete;
    RmMemObjRestore(RmMemObjRestore &&other) = delete;
    RmMemObjRestore &operator=(const RmMemObjRestore &other) = delete;
    RmMemObjRestore &operator=(RmMemObjRestore &&other) noexcept = delete;

private:
    template <typename TUbseMemBorrowImportObjMap>
    UbseResult ProcessAbnormalImportObjMap(const TUbseMemBorrowImportObjMap &importObjMap)
    {
        if (importObjMap.empty()) {
            UBSE_LOG_WARN << "ImportObjMap is empty, not need process";
            return UBSE_OK;
        }
        UbseResult ret = UBSE_OK;
        for (auto &item : importObjMap) {
            std::vector<mem_id> memIds{};
            auto importResults = item.second.status.importResults;
            for (int i = 0; i < importResults.size(); i++) {
                memIds.push_back(importResults[i].memId);
            }
            ret = RmObmmExecutor::GetInstance().ObmmUnImport(memIds);
            if (UBSE_RESULT_FAIL(ret)) {
                UBSE_LOG_ERROR << "Process abnormal importObj map failed, memIds is "
                             << RmCommonUtils::GetInstance().MemToStr(memIds);
                return ret;
            }
        }
        return ret;
    }

    template <typename TUbseMemBorrowExportObjMap>
    UbseResult ProcessAbnormalExportObjMap(const TUbseMemBorrowExportObjMap &exportObjMap)
    {
        if (exportObjMap.empty()) {
            UBSE_LOG_WARN << "ExportObjMap is empty, not need process";
            return UBSE_OK;
        }
        UbseResult ret = UBSE_OK;
        for (auto &item : exportObjMap) {
            std::vector<mem_id> memIds{};
            const auto &exportObmmInfo = item.second.status.exportObmmInfo;
            for (int i = 0; i < exportObmmInfo.size(); i++) {
                memIds.push_back(exportObmmInfo[i].memId);
            }
            ret = RmObmmExecutor::GetInstance().ObmmUnExport(memIds);
            if (UBSE_RESULT_FAIL(ret)) {
                UBSE_LOG_ERROR << "Process abnormal exportObj map failed, memIds is "
                             << RmCommonUtils::GetInstance().MemToStr(memIds);
                return ret;
            }
        }
        return ret;
    }

    template <typename TUbseMemBorrowImportObjMap>
    std::string GetNameAndMemIdFromImportObjMap(const TUbseMemBorrowImportObjMap &importObjMap)
    {
        if (importObjMap.empty()) {
            UBSE_LOG_WARN << "ExportObjMap is empty, not need process";
            return "";
        }
        std::vector<std::string> names{};
        std::vector<std::vector<mem_id>> allMemIds{};
        for (auto &item : importObjMap) {
            std::vector<mem_id> memIds{};
            auto tmpName = item.first;
            auto importResults = item.second.status.importResults;
            for (int i = 0; i < importResults.size(); i++) {
                memIds.push_back(importResults[i].memId);
            }
            names.push_back(tmpName);
            allMemIds.push_back(memIds);
        }
        return ConstructNameInfoFromNamesAndMemIds(names, allMemIds);
    }

    template <typename TUbseMemBorrowExportObjMap>
    std::string GetNameAndMemIdFromExportObjMap(const TUbseMemBorrowExportObjMap &exportObjMap)
    {
        if (exportObjMap.empty()) {
            UBSE_LOG_WARN << "ExportObjMap is empty, not need process";
            return "";
        }
        std::vector<std::string> names{};
        std::vector<std::vector<mem_id>> allMemIds{};
        for (auto &item : exportObjMap) {
            std::vector<mem_id> memIds{};
            auto tmpName = item.first;
            const auto &exportObmmInfo = item.second.status.exportObmmInfo;
            for (int i = 0; i < exportObmmInfo.size(); i++) {
                memIds.push_back(exportObmmInfo[i].memId);
            }
            names.push_back(tmpName);
            allMemIds.push_back(memIds);
        }
        return ConstructNameInfoFromNamesAndMemIds(names, allMemIds);
    }

    static inline std::string ConstructNameInfoFromNamesAndMemIds(const std::vector<std::string> &names,
                                                                  const std::vector<std::vector<mem_id>> &allMemIds)
    {
        std::ostringstream oss;
        for (size_t i = 0; i < names.size(); ++i) {
            const auto &name = names[i];
            const auto &memIds = allMemIds[i];
            oss << "name = " << name << ", memIds = " << RmCommonUtils::GetInstance().MemToStr(memIds) << "; ";
        }
        return oss.str();
    }

private:
    RmMemObjRestore() = default;
};

} // namespace ubse::mmi

#endif // UBSE_MEM_OBJ_RESTORE_H
