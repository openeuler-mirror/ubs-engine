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

#ifndef UBS_ENGINE_UBSE_MEM_CONTROLLER_API_COMMON_H
#define UBS_ENGINE_UBSE_MEM_CONTROLLER_API_COMMON_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <string>

#include "ubse_error.h"
#include "ubse_mem_constants.h"
#include "ubse_mem_controller.h"
#include "ubse_mem_debt_ledger.h"
#include "ubse_mem_decoder_utils.h"
#include "ubse_mmi_interface.h"
#include "lock/ubse_lock.h"

namespace ubse::mem::controller {
using ubse::adapter_plugins::mmi::MemOperationType;
using ubse::adapter_plugins::mmi::UBSE_MEM_EXPORT_DESTROYED;
using ubse::adapter_plugins::mmi::UBSE_MEM_EXPORT_DESTROYING;
using ubse::adapter_plugins::mmi::UBSE_MEM_EXPORT_RUNNING;
using ubse::adapter_plugins::mmi::UBSE_MEM_EXPORT_SUCCESS;
using ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED;
using ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYING;
using ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_RUNNING;
using ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS;
using ubse::adapter_plugins::mmi::UBSE_MEM_SCHEDULING;
using ubse::adapter_plugins::mmi::UbseMemImportStatus;
using ubse::adapter_plugins::mmi::UbseMemObmmInfo;
using ubse::adapter_plugins::mmi::UbseMemOperationResp;
using ubse::adapter_plugins::mmi::UbseMemPrivData;
using ubse::adapter_plugins::mmi::UbseMemReturnReq;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowImportObj;
using ubse::adapter_plugins::mmi::UbseShmRegionDesc;
using ubse::adapter_plugins::mmi::UbseUdsInfo;

const uint32_t SEND_RETRY_TIMES = 5;
const uint32_t SEND_RETRY_DURATION = 1;
const uint32_t SLEEP_TIME = 200;
const uint32_t ALLOCATE_RETRY_TIME = 25;
const uint32_t RETURN_RETRY_TIME = 150;
extern std::atomic<uint64_t> g_fdUnimportFailedCount;
extern std::atomic<uint64_t> g_numaUnimportFailedCount;
extern std::atomic<uint64_t> g_shareUnimportFailedCount;
extern std::atomic<uint64_t> g_addrUnimportFailedCount;

struct UbseMemBorrowStatus {
    bool hasImport = false;
    bool hasExport = false;
};

uint32_t BuildOperationRespWhenFail(UbseMemOperationResp& resp, const std::string& name,
                                    const std::string& requestNodeId, std::string errMsg, uint32_t errorCode,
                                    MemOperationType type = MemOperationType::FD_BORROW);

uint32_t BuildOperationRespWhenSuccess(UbseMemOperationResp& resp, UbseResult errorCode,
                                       MemOperationType type = MemOperationType::FD_BORROW);

std::shared_mutex& GetDecoderImportMutex();

inline std::string GenerateExportObjKey(const std::string& name, const std::string& importNodeId)
{
    return name + "_" + importNodeId;
}

bool IsSdkRequest(uint64_t requestId);

bool IsMemBorrowFeatureSupported();

bool IsMemShareFeatureSupported();

bool IsMemShareModeFeatureSupported(uint16_t cacheableFlag);

UbseResult SetDefaultMemBorrowPrivData(UbseMemPrivData& privData, uint16_t wrDelayComp = 0);

uint32_t BuildMemFeatureNotSupportedResp(UbseMemOperationResp& resp, const std::string& name,
                                         const std::string& requestNodeId,
                                         MemOperationType type = MemOperationType::FD_BORROW);

template <class importType>
UbseResult GetErrorCodeByObjState(const importType& importObj, const bool& exportObjExist)
{
    if (importObj.status.state == UBSE_MEM_IMPORT_RUNNING || importObj.status.state == UBSE_MEM_EXPORT_RUNNING ||
        importObj.status.state == UBSE_MEM_EXPORT_SUCCESS) {
        return UBSE_ERR_CREATING;
    }

    if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYING || importObj.status.state == UBSE_MEM_EXPORT_DESTROYING) {
        return UBSE_ERR_DELETING;
    }

    if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        if (exportObjExist) {
            return UBSE_ERR_UNIMPORT_SUCCESS;
        }
        return UBSE_ERR_NOT_EXIST;
    }

    if (importObj.status.state == UBSE_MEM_IMPORT_SUCCESS) {
        return UBSE_ERR_EXISTED;
    }

    if (importObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
        return UBSE_ERR_NOT_EXIST;
    }

    return UBSE_ERR_EXISTED;
}

template <class importType>
UbseMemStage GetOptStageByObjState(const importType& importObj, const bool& exportObjExist)
{
    if (importObj.status.state == UBSE_MEM_IMPORT_RUNNING || importObj.status.state == UBSE_MEM_EXPORT_RUNNING ||
        importObj.status.state == UBSE_MEM_EXPORT_SUCCESS) {
        return UbseMemStage::UBSE_CREATING;
    }

    if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYING || importObj.status.state == UBSE_MEM_EXPORT_DESTROYING) {
        return UbseMemStage::UBSE_DELETING;
    }

    if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        if (exportObjExist) {
            return UbseMemStage::UBSE_ERR_WAIT_UNEXPORT;
        }
        return UbseMemStage::UBSE_NOT_EXIST;
    }

    if (importObj.status.state == UBSE_MEM_IMPORT_SUCCESS) {
        if (!exportObjExist) {
            return UbseMemStage::UBSE_ERR_ONLY_IMPORT;
        }
        return UbseMemStage::UBSE_EXIST;
    }

    if (importObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
        return UbseMemStage::UBSE_NOT_EXIST;
    }

    return UbseMemStage::UBSE_NOT_EXIST;
}

template <class importType>
UbseMemStage GetMemStageByImportObjState(const importType& importObj, const bool& importObjExist)
{
    if (!importObjExist) {
        return UbseMemStage::UBSE_NOT_EXIST;
    }
    if (importObj.status.state == UBSE_MEM_IMPORT_RUNNING || importObj.status.state == UBSE_MEM_EXPORT_RUNNING) {
        return UbseMemStage::UBSE_CREATING;
    }

    if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYING || importObj.status.state == UBSE_MEM_EXPORT_DESTROYING) {
        return UbseMemStage::UBSE_DELETING;
    }

    return UbseMemStage::UBSE_EXIST;
}

template <class ImportType>
UbseMemStage GetMemStageByImportObjState(const std::shared_ptr<const ImportType>& importObjPtr)
{
    if (!importObjPtr) {
        return UbseMemStage::UBSE_NOT_EXIST;
    }
    if (importObjPtr->status.state == UBSE_MEM_IMPORT_RUNNING ||
        importObjPtr->status.state == UBSE_MEM_EXPORT_RUNNING) {
        return UbseMemStage::UBSE_CREATING;
    }

    if (importObjPtr->status.state == UBSE_MEM_IMPORT_DESTROYING ||
        importObjPtr->status.state == UBSE_MEM_EXPORT_DESTROYING) {
        return UbseMemStage::UBSE_DELETING;
    }

    return UbseMemStage::UBSE_EXIST;
}

template <class ExportType>
UbseMemStage GetMemStageByExportObjState(const std::shared_ptr<const ExportType>& exportObjPtr)
{
    if (!exportObjPtr) {
        return UbseMemStage::UBSE_NOT_EXIST;
    }
    if (exportObjPtr->status.state == UBSE_MEM_EXPORT_RUNNING) {
        return UbseMemStage::UBSE_CREATING;
    }

    if (exportObjPtr->status.state == UBSE_MEM_EXPORT_DESTROYING) {
        return UbseMemStage::UBSE_DELETING;
    }

    return UbseMemStage::UBSE_EXIST;
}

UbseMemStage GetMemStageByShareImportObjState(const UbseMemShareBorrowImportObj& importObj, const bool& importObjExist);

template <class exportType>
UbseMemStage GetMemStageByExportObjState(const exportType& exportObj, const bool& exportObjExist)
{
    if (!exportObjExist) {
        return UbseMemStage::UBSE_NOT_EXIST;
    }
    if (exportObj.status.state == UBSE_MEM_EXPORT_RUNNING) {
        return UbseMemStage::UBSE_CREATING;
    }

    if (exportObj.status.state == UBSE_MEM_EXPORT_DESTROYING) {
        return UbseMemStage::UBSE_DELETING;
    }

    return UbseMemStage::UBSE_EXIST;
}

void InitializeResponse(const UbseMemReturnReq& req, UbseMemOperationResp& resp);

uint32_t ImportToAddDecoderEntry(const std::pair<uint32_t, uint32_t>& chipDiePair,
                                 const std::vector<UbseMemObmmInfo>& exportObmmInfo,
                                 const decoder::utils::ImportDecoderParam& importDecoderParam,
                                 UbseMemImportStatus& status);

void UnimportToDelDecoderEntry(const std::pair<uint32_t, uint32_t>& chipDiePair, UbseMemImportStatus& status,
                               uint8_t decoderId);

uint32_t AgentInvalidateDecoderEntry(uint32_t attachSocketId, UbseMemImportStatus& status, uint8_t decoderId);

void InitAgentMaxWaitTime(uint32_t timeout);

uint32_t GetWaitTimeOut();

bool CheckCommonReturnPermission(const UbseUdsInfo& memUds, const UbseUdsInfo& reqUds,
                                 const std::string& realRequestNodeId, const std::string& importNodeId,
                                 const std::string& exportNodeId = "");
bool CheckShareReturnPermission(const UbseUdsInfo& memUds, const UbseUdsInfo& reqUds,
                                const std::string& realRequestNodeId, const UbseShmRegionDesc& shareRegion);
bool CheckShareDetachPermission(const UbseUdsInfo& memUds, const UbseUdsInfo& reqUds,
                                const std::string& realRequestNodeId, const std::string& importNodeId);

uint32_t WaitNodeStateWork(const std::string& importNode);

template <class ObjType>
bool HasAgentAlreadyReported(const std::string& name, const std::string& exeNodeId, bool ObjType::*reportedFlagField)
{
    auto objPtr = debt::UbseMemDebtLedger::GetInstance().GetDebtMap<ObjType>().GetResource(exeNodeId, name);
    if (!objPtr) {
        return true;
    } else {
        if ((*objPtr).*reportedFlagField) {
            return false;
        }
        auto copyObj = *objPtr;
        copyObj.*reportedFlagField = true;
        debt::UbseMemDebtLedger::GetInstance().GetDebtMap<ObjType>().PutResource(exeNodeId, name, copyObj);
        return true;
    }
    return true;
}
} // namespace ubse::mem::controller

#endif // UBS_ENGINE_UBSE_MEM_CONTROLLER_API_COMMON_H
