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

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_mem_common_utils.h"
#include "ubse_mem_types.h"
#include "ubse_mmi_interface.h"
#include "ubse_obmm_executor.h"
#include "ubse_obmm_meta_restore.h"
#include "ubse_obmm_utils.h"
#include "src/controllers/mem/mem_decoder_utils/ubse_mem_prehandle_manager.h"

#include <shared_mutex>
#include <sstream>

namespace ubse::mmi {
#define MODULE_LOG_NAME "ubse"
using ubse::adapter_plugins::mmi::NodeMemDebtInfo;
using ubse::adapter_plugins::mmi::SocketCnaInfo;
using ubse::adapter_plugins::mmi::UbseMemAddrBorrowExportObj;
using ubse::adapter_plugins::mmi::UbseMemAddrBorrowImportObj;
using ubse::adapter_plugins::mmi::UbseMemFdBorrowExportObj;
using ubse::adapter_plugins::mmi::UbseMemFdBorrowImportObj;
using ubse::adapter_plugins::mmi::UbseMemNumaBorrowExportObj;
using ubse::adapter_plugins::mmi::UbseMemNumaBorrowImportObj;
using ubse::adapter_plugins::mmi::UbseMemPrivData;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowExportObj;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowImportObj;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowReq;
using ubse::common::def::UbseResult;

class MemInstanceInnerCommon {
public:
    static MemInstanceInnerCommon& GetInstance()
    {
        static MemInstanceInnerCommon instance;
        return instance;
    }
    MemInstanceInnerCommon(const MemInstanceInnerCommon& other) = delete;
    MemInstanceInnerCommon(MemInstanceInnerCommon&& other) = delete;
    MemInstanceInnerCommon& operator=(const MemInstanceInnerCommon& other) = delete;
    MemInstanceInnerCommon& operator=(MemInstanceInnerCommon&& other) noexcept = delete;

    UbseResult RemoteNumaIdInit();

    int GetNuma(const std::string& nodesocketPair) const noexcept;

    uint32_t MemGetObjData(NodeMemDebtInfo& memBorrowObj, std::vector<UbseMemLocalObmmMetaData>& allObmmDatas);

    UbseResult MemPreOnline(const std::vector<SocketCnaInfo>& cnaTopoInfos, uint64_t preImportSize);

    UbseResult MemUnPreOnline();

    void RollbackImport(const std::vector<mem_id>& memids);

    void RollbackExport(const std::vector<mem_id>& memids);

    template <typename Obj>
    static bool UbseMemExecutorCheckParam(const Obj& obj)
    {
        if (typeid(obj.req) == typeid(UbseMemShareBorrowReq)) {
            return !obj.algoResult.exportNumaInfos.empty() && !obj.req.name.empty() && obj.req.size > 0;
        }
        return !obj.algoResult.importNumaInfos.empty() && !obj.algoResult.exportNumaInfos.empty() &&
               !obj.req.name.empty() && obj.req.size > 0;
    }

    template <typename ExportObj>
    uint32_t UnExportExecutor(const ExportObj& exportObj, const std::string& expectedName, uint8_t expectedBorrowType)
    {
        UbseResult ret = UBSE_OK;
        for (size_t i = 0; i < exportObj.status.exportObmmInfo.size(); i++) {
            auto memId = exportObj.status.exportObmmInfo[i].memId;
            if (memId == INVALID_MEM_ID) {
                UBSE_LOG_ERROR << MMI_LOG_INFO << "Export obmm memid is invalid, name=" << exportObj.req.name;
                return UBSE_ERROR_INVAL;
            }
            uint8_t obmmType = 0;
            auto typeRet = RmObmmDevRead::GetBorrowTypeByMemId(memId, obmmType);
            if (typeRet == UBSE_OK && obmmType != expectedBorrowType) {
                UBSE_LOG_WARN << MMI_LOG_INFO << "BorrowType mismatch, skip unexport, memid=" << memId
                              << ", expected=" << static_cast<int>(expectedBorrowType)
                              << ", actual=" << static_cast<int>(obmmType);
                continue;
            }
            std::string obmmName;
            auto nameRet = RmObmmDevRead::GetNameByMemId(memId, obmmName);
            if (nameRet == UBSE_OK && obmmName != expectedName) {
                UBSE_LOG_WARN << MMI_LOG_INFO << "Name mismatch, skip unexport, memid=" << memId
                              << ", expected=" << expectedName << ", actual=" << obmmName;
                continue;
            }
            ret = RmObmmExecutor::GetInstance().ObmmUnExport(memId);
            if (UBSE_RESULT_FAIL(ret)) {
                UBSE_LOG_ERROR << MMI_LOG_INFO << "ObmmUnExport error!";
                return ret;
            }
        }
        return ret;
    }

    uint64_t SetObmmDescDefaultErrorValue(std::vector<ubse_mem_obmm_mem_desc>& obmmMemDescs, const uint64_t totalSize,
                                          const uint64_t blockSize);
    template <typename Obj>
    uint32_t GetObmmExportParamFromRequest(Obj& exportObj, std::vector<ubse_mem_obmm_mem_desc>& obmmMemDescs,
                                           size_t sizes[MAX_NUMA_NODES], int arraySize)
    {
        obmmMemDescs.clear();
        auto blockSize = RmCommonUtils::GetInstance().SizeMb2Byte(exportObj.algoResult.blockSize);
        const auto& numaIds = exportObj.algoResult.exportNumaInfos;

        for (size_t i = 0; i < numaIds.size(); i++) {
            std::vector<ubse_mem_obmm_mem_desc> descTmp{};
            auto blockCount =
                SetObmmDescDefaultErrorValue(descTmp, exportObj.algoResult.exportNumaInfos[i].size, blockSize);
            UBSE_LOG_INFO << MMI_LOG_INFO << "blocks=" << blockCount << ", blockSize=" << blockSize;
            obmmMemDescs.insert(obmmMemDescs.end(), descTmp.begin(), descTmp.end());

            if (numaIds[i].numaId >= MAX_NUMA_NODES || numaIds[i].numaId < 0 || numaIds[i].size <= 0) {
                UBSE_LOG_WARN << MMI_LOG_INFO << "Strategy param illegal, numaId=" << numaIds[i].numaId;
                continue;
            }
            sizes[numaIds[i].numaId] = numaIds[i].size;
        }
        return UBSE_OK;
    }

private:
    std::atomic_bool mInited{false};
    mutable std::shared_mutex mLock{};
    std::unordered_map<std::string, int> mRemoteNumaMap{};
    MemInstanceInnerCommon() = default;
};

class MemInstanceInnerFdBorrow {
public:
    static MemInstanceInnerFdBorrow& GetInstance()
    {
        static MemInstanceInnerFdBorrow instance;
        return instance;
    }

    uint32_t MemFdImportExecutor(UbseMemFdBorrowImportObj& importObj);
    uint32_t MemFdImportPermissionExecutor(UbseMemFdBorrowImportObj& importObj);
    uint32_t MemFdUnImportExecutor(const UbseMemFdBorrowImportObj& importObj);
    uint32_t MemFdExportExecutor(UbseMemFdBorrowExportObj& exportObj);
    uint32_t MemFdUnExportExecutor(const UbseMemFdBorrowExportObj& exportObj);

    MemInstanceInnerFdBorrow(const MemInstanceInnerFdBorrow& other) = delete;
    MemInstanceInnerFdBorrow(MemInstanceInnerFdBorrow&& other) = delete;
    MemInstanceInnerFdBorrow& operator=(const MemInstanceInnerFdBorrow& other) = delete;
    MemInstanceInnerFdBorrow& operator=(MemInstanceInnerFdBorrow&& other) noexcept = delete;

private:
    MemInstanceInnerFdBorrow() = default;
};

class MemInstanceInnerNumaBorrow {
public:
    static MemInstanceInnerNumaBorrow& GetInstance()
    {
        static MemInstanceInnerNumaBorrow instance;
        return instance;
    }

    uint32_t MemNumaImportExecutor(UbseMemNumaBorrowImportObj& importObj);
    uint32_t MemNumaExportExecutor(UbseMemNumaBorrowExportObj& exportObj);
    uint32_t MemNumaUnExportExecutor(const UbseMemNumaBorrowExportObj& exportObj);
    uint32_t MemNumaUnImportExecutor(const UbseMemNumaBorrowImportObj& importObj);

    MemInstanceInnerNumaBorrow(const MemInstanceInnerNumaBorrow& other) = delete;
    MemInstanceInnerNumaBorrow(MemInstanceInnerNumaBorrow&& other) = delete;
    MemInstanceInnerNumaBorrow& operator=(const MemInstanceInnerNumaBorrow& other) = delete;
    MemInstanceInnerNumaBorrow& operator=(MemInstanceInnerNumaBorrow&& other) noexcept = delete;

private:
    MemInstanceInnerNumaBorrow() = default;
};

class MemInstanceInnerAddrBorrow {
public:
    static MemInstanceInnerAddrBorrow& GetInstance()
    {
        static MemInstanceInnerAddrBorrow instance;
        return instance;
    }

    uint32_t MemAddrImportExecutor(UbseMemAddrBorrowImportObj& importObj);
    uint32_t MemAddrUnImportExecutor(const UbseMemAddrBorrowImportObj& importObj);
    uint32_t MemAddrExportExecutor(UbseMemAddrBorrowExportObj& exportObj);
    uint32_t MemAddrUnExportExecutor(const UbseMemAddrBorrowExportObj& exportObj);
    UbseResult AfterMemAddrExportExecutor(UbseMemAddrBorrowExportObj& exportObj,
                                          const std::unordered_map<uint64_t, uint64_t>& exportNumaInfoMap,
                                          const std::vector<mem_id>& memIds);

    MemInstanceInnerAddrBorrow(const MemInstanceInnerAddrBorrow& other) = delete;
    MemInstanceInnerAddrBorrow(MemInstanceInnerAddrBorrow&& other) = delete;
    MemInstanceInnerAddrBorrow& operator=(const MemInstanceInnerAddrBorrow& other) = delete;
    MemInstanceInnerAddrBorrow& operator=(MemInstanceInnerAddrBorrow&& other) noexcept = delete;

    void AddAddrRemoteNuma(int remoteNuma);
    void DeleteAddrRemoteNuma(int remoteNuma);
    UbseResult GenerateAddrRemoteNuma(int& remoteNumaId);

private:
    std::mutex mAddrLock{};
    std::set<int> addrRemoteNumaIdSet{};

    MemInstanceInnerAddrBorrow() = default;
};

class MemInstanceInnerShm {
public:
    static MemInstanceInnerShm& GetInstance()
    {
        static MemInstanceInnerShm instance;
        return instance;
    }

    uint32_t MemShmImportExecutor(UbseMemShareBorrowImportObj& importObj);
    uint32_t MemShmUnImportExecutor(const UbseMemShareBorrowImportObj& importObj);
    uint32_t MemShmExportExecutor(UbseMemShareBorrowExportObj& exportObj);
    uint32_t MemShmUnExportExecutor(const UbseMemShareBorrowExportObj& exportObj);

    MemInstanceInnerShm(const MemInstanceInnerShm& other) = delete;
    MemInstanceInnerShm(MemInstanceInnerShm&& other) = delete;
    MemInstanceInnerShm& operator=(const MemInstanceInnerShm& other) = delete;
    MemInstanceInnerShm& operator=(MemInstanceInnerShm&& other) noexcept = delete;

private:
    MemInstanceInnerShm() = default;
};
#undef MODULE_LOG_NAME
} // namespace ubse::mmi

#endif // UBSE_MANAGER_MEM_INSTANCE_INNER_H
