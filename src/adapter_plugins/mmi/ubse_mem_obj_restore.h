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
#include "ubse_mem_types.h"
#include "ubse_mmi_interface.h"
#include "ubse_mem_common_utils.h"
#include "ubse_obmm_executor.h"
namespace ubse::mmi::restore {
#define MODULE_LOG_NAME "ubse"
using namespace ubse::adapter_plugins::mmi;
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
    friend std::ostream &operator<<(std::ostream &os, const LocalObmmMetaData &obj)
    {
        return os << "FdImportMetaData: " << obj.FdImportMetaData.size()
                  << " FdExportMetaData: " << obj.FdExportMetaData.size()
                  << " NumaImportMetaData: " << obj.NumaImportMetaData.size()
                  << " NumaExportMetaData: " << obj.NumaExportMetaData.size()
                  << " ShmImportMetaData: " << obj.ShmImportMetaData.size()
                  << " ShmExportMetaData: " << obj.ShmExportMetaData.size()
                  << " AddrImportMetaData: " << obj.AddrImportMetaData.size()
                  << " AddrExportMetaData: " << obj.AddrExportMetaData.size();
    }
};


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

UbseResult ConstructShareImportObj(const std::vector<UbseMemLocalObmmMetaData> &importLocalObmmMetaDatas,
                                   UbseMemShareImportObjMap &normalImportObjMap);

void ConstructSingleShareImportObj(const std::vector<UbseMemLocalObmmMetaData> &importLocalObmmMetaDatas,
                                   UbseMemShareBorrowImportObj &mxeMemShareBorrowImportObj, bool &isNormal);

UbseResult ConstructShareImportObjFromExportMetaData(
    const std::vector<UbseMemLocalObmmMetaData> &exportLocalObmmMetaDatas, UbseMemShareImportObjMap &importObjMap);

void ConstructSingleShareImportObjFromExportMetaData(
    const std::vector<UbseMemLocalObmmMetaData> &exportLocalObmmMetaDatas,
    UbseMemShareBorrowImportObj &mxeMemShareBorrowImportObj, bool &isNormal);

UbseResult ConstructShareExportObj(const std::vector<UbseMemLocalObmmMetaData> &exportLocalObmmMetaDatas,
                                   UbseMemShareExportObjMap &normalExportObjMap);

void AssignReqValue(UbseMemLocalObmmMetaData obmmMetaData, UbseMemShareBorrowReq &req, int &numaCount);

void ConstructSingleShareExportObj(const std::vector<UbseMemLocalObmmMetaData> &exportLocalObmmMetaDatas,
                                   UbseMemShareBorrowExportObj &mxeMemShareBorrowExportObj, bool &isNormal);

UbseResult ConstructAddrImportObj(const std::vector<UbseMemLocalObmmMetaData> &importLocalObmmMetaDatas,
                                  UbseMemAddrImportObjMap &normalImportObjMap);

void ConstructSingleAddrImportObj(const std::vector<UbseMemLocalObmmMetaData> &importLocalObmmMetaDatas,
                                  UbseMemAddrBorrowImportObj &mxeMemAddrBorrowImportObj, bool &isNormal);

UbseResult ConstructAddrExportObj(const std::vector<UbseMemLocalObmmMetaData> &exportLocalObmmMetaDatas,
                                  UbseMemAddrExportObjMap &normalExportObjMap);

void ConstructSingleAddrExportObj(const std::vector<UbseMemLocalObmmMetaData> &exportLocalObmmMetaDatas,
                                  UbseMemAddrBorrowExportObj &mxeMemAddrBorrowExportObj, bool &isNormal);

void GetBorrowObjMap(const std::vector<UbseMemLocalObmmMetaData> &localObmmMetaDatas,
                     std::unordered_map<std::string, std::vector<UbseMemLocalObmmMetaData>> &borrowObjMap);

UbseResult GetLocalObmmMeta(std::vector<UbseMemLocalObmmMetaData> &allObmmDatas,
                            LocalObmmMetaData &localObmmMetaData);

UbseResult ProcessAbnormalAddrImportObjMap(const UbseMemAddrImportObjMap &importObjMap);

template <typename TUbseMemBorrowImportObjMap>
UbseResult ProcessAbnormalImportObjMap(const TUbseMemBorrowImportObjMap &importObjMap)
{
    if (importObjMap.empty()) {
        UBSE_LOG_WARN << MMI_LOG_INFO << "ImportObjMap is empty, not need process";
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
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Process abnormal importObj map failed, memIds is "
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
        UBSE_LOG_WARN << MMI_LOG_INFO << "ExportObjMap is empty, not need process";
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
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Process abnormal exportObj map failed, memIds is "
                         << RmCommonUtils::GetInstance().MemToStr(memIds);
            return ret;
        }
    }
    return ret;
}

inline std::string ConstructNameInfoFromNamesAndMemIds(const std::vector<std::string> &names,
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

template <typename TUbseMemBorrowImportObjMap>
std::string GetNameAndMemIdFromImportObjMap(const TUbseMemBorrowImportObjMap &importObjMap)
{
    if (importObjMap.empty()) {
        UBSE_LOG_WARN << MMI_LOG_INFO << "ExportObjMap is empty, not need process";
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
        UBSE_LOG_WARN << MMI_LOG_INFO << "ExportObjMap is empty, not need process";
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
void BuildSingleNumaImportReq(const UbseMemLocalObmmCustomMeta &meta, UbseMemNumaBorrowReq &req,
                              std::string &lendNode);
void BuildSingleNumaExportReq(UbseMemLocalObmmMetaData &obmmMetaData, std::string &lendNode,
                              UbseMemNumaBorrowReq &req);

#undef MODULE_LOG_NAME
} // namespace ubse::mmi

#endif // UBSE_MEM_OBJ_RESTORE_H
