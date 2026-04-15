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

#include "ubse_common_def.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_common_utils.h"
#include "ubse_mem_def.h"
#include "ubse_mem_instance_inner.h"
#include "ubse_mem_types.h"
#include "ubse_mmi_module.h"
#include "ubse_obmm_executor.h"

namespace ubse::mmi {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::adapter_plugins::mmi;

DYNAMIC_CREATE(UbseMmiModule);
UbseResult UbseMmiModule::Initialize()
{
    auto ret = RmObmmExecutor::GetInstance().Init();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "RmObmmExecutor init failed, ret = " << ret;
        return ret;
    }
    return UBSE_OK;
}

void UbseMmiModule::UnInitialize()
{
    RmObmmExecutor::GetInstance().Exit();
}

UbseResult UbseMmiModule::Start()
{
    return UBSE_OK;
}

void UbseMmiModule::Stop() {}

static void PrintNodeMemDebtInfo(const NodeMemDebtInfo &memBorrowObj)
{
    for (const auto &map : memBorrowObj.fdExportObjMap) {
        UBSE_LOG_DEBUG << MMI_LOG_INFO << RmCommonUtils::GetInstance().TranStructToStr(map.second);
    }
    for (const auto &map : memBorrowObj.fdImportObjMap) {
        UBSE_LOG_DEBUG << MMI_LOG_INFO << RmCommonUtils::GetInstance().TranStructToStr(map.second);
    }
    for (const auto &map : memBorrowObj.numaExportObjMap) {
        UBSE_LOG_DEBUG << MMI_LOG_INFO << RmCommonUtils::GetInstance().TranStructToStr(map.second);
    }
    for (const auto &map : memBorrowObj.numaImportObjMap) {
        UBSE_LOG_DEBUG << MMI_LOG_INFO << RmCommonUtils::GetInstance().TranStructToStr(map.second);
    }
    for (const auto &map : memBorrowObj.shareExportObjMap) {
        UBSE_LOG_DEBUG << MMI_LOG_INFO << RmCommonUtils::GetInstance().TranStructToStr(map.second);
    }
    for (const auto &map : memBorrowObj.shareImportObjMap) {
        UBSE_LOG_DEBUG << MMI_LOG_INFO << RmCommonUtils::GetInstance().TranStructToStr(map.second);
    }
    for (const auto &map : memBorrowObj.addrImportObjMap) {
        UBSE_LOG_DEBUG << MMI_LOG_INFO << RmCommonUtils::GetInstance().TranStructToStr(map.second);
    }
    for (const auto &map : memBorrowObj.addrExportObjMap) {
        UBSE_LOG_DEBUG << MMI_LOG_INFO << RmCommonUtils::GetInstance().TranStructToStr(map.second);
    }
}

uint32_t UbseMmiModule::UbseMemGetObjData(NodeMemDebtInfo &memBorrowObj)
{
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemGetObjData start.";
    // 芯片表项碎片，导入一半的时候，进程挂掉了，从obmm获取数据前等3s,保证单次最大1G执行完成
    memBorrowObj.fdExportObjMap = {};
    memBorrowObj.fdImportObjMap = {};
    memBorrowObj.numaExportObjMap = {};
    memBorrowObj.numaImportObjMap = {};
    memBorrowObj.shareExportObjMap = {};
    memBorrowObj.shareImportObjMap = {};
    memBorrowObj.addrImportObjMap = {};
    memBorrowObj.addrExportObjMap = {};
    std::vector<UbseMemLocalObmmMetaData> allObmmDatas;
    uint32_t ret = UBSE_OK;
    ret = MemInstanceInnerCommon::GetInstance().MemGetObjData(memBorrowObj, allObmmDatas);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "[MMI] failed to get obj data.";
        return ret;
    }
    PrintNodeMemDebtInfo(memBorrowObj);
    return ret;
}

/**
 * @brief   fd借用import操作执行器
 *
 * @param importObj      [IN]/[OUT] fd类型借用内存导入对象
 * @return int    0：操作成功；非0：获取失败
 */
uint32_t UbseMmiModule::UbseMemFdImportExecutor(UbseMemFdBorrowImportObj &importObj)
{
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemFdImportExecutor, name=" << importObj.req.name << ", objDetails is "
                  << RmCommonUtils::GetInstance().TranStructToStr(importObj);
    if (!MemInstanceInnerCommon::UbseMemExecutorCheckParam(importObj)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Obj check param failed, name=" << importObj.req.name;
        return UBSE_ERROR_INVAL;
    }
    auto ret = MemInstanceInnerFdBorrow::GetInstance().MemFdImportExecutor(importObj);
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemFdImportExecutor end, name=" << importObj.req.name
                  << " memid=" << RmCommonUtils::GetInstance().MemToStr(importObj.status.importResults);
    return ret;
}

/**
 * @brief   fd借用import permission操作执行器
 *
 * @param importObj      [IN]/[OUT] fd类型借用内存导入对象
 * @return int    0：操作成功；非0：获取失败
 */
uint32_t UbseMmiModule::UbseMemFdImportPermissionExecutor(UbseMemFdBorrowImportObj &importObj)
{
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemFdImportPermissionExecutor begin, name=" << importObj.req.name
                  << ", objDetails=" << RmCommonUtils::GetInstance().TranStructToStr(importObj);
    if (!MemInstanceInnerCommon::UbseMemExecutorCheckParam(importObj)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Obj check param failed, name=" << importObj.req.name;
        return UBSE_ERROR_INVAL;
    }
    auto ret = MemInstanceInnerFdBorrow::GetInstance().MemFdImportPermissionExecutor(importObj);
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemFdImportPermissionExecutor end, name=" << importObj.req.name
                  << " memid=" << RmCommonUtils::GetInstance().MemToStr(importObj.status.importResults);
    return ret;
}

/**
 * @brief   fd借用unimport操作执行器
 *
 * @param importObj      [IN]/[OUT] fd类型借用内存导入对象
 * @return int    0：操作成功；非0：获取失败
 */
uint32_t UbseMmiModule::UbseMemFdUnImportExecutor(const UbseMemFdBorrowImportObj &importObj)
{
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemFdUnImportExecutor, name=" << importObj.req.name << ", objDetails is "
                  << RmCommonUtils::GetInstance().TranStructToStr(importObj);
    if (!MemInstanceInnerCommon::UbseMemExecutorCheckParam(importObj)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Obj check param failed, name=" << importObj.req.name;
        return UBSE_ERROR_INVAL;
    }
    auto ret = MemInstanceInnerFdBorrow::GetInstance().MemFdUnImportExecutor(importObj);
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemFdUnImportExecutor end, ret =" << ret << ", name=" << importObj.req.name
                  << " memid=" << RmCommonUtils::GetInstance().MemToStr(importObj.status.importResults);
    return ret;
}

/**
 * @brief   fd借用export对象的执行器
 *
 * @param exportObj      [IN]/[OUT] fd类型借用内存导出对象
 * @return int    0：操作成功；非0：获取失败
 */
uint32_t UbseMmiModule::UbseMemFdExportExecutor(UbseMemFdBorrowExportObj &exportObj)
{
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemFdExportExecutor, name=" << exportObj.req.name << ", objDetails is "
                  << RmCommonUtils::GetInstance().TranStructToStr(exportObj);
    if (!MemInstanceInnerCommon::UbseMemExecutorCheckParam(exportObj)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Obj check param failed, name=" << exportObj.req.name;
        return UBSE_ERROR_INVAL;
    }
    auto ret = MemInstanceInnerFdBorrow::GetInstance().MemFdExportExecutor(exportObj);
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemFdExportExecutor end, ret =" << ret << ", name=" << exportObj.req.name
                  << " memid=" << RmCommonUtils::GetInstance().MemToStr(exportObj.status.exportObmmInfo);
    return ret;
}

/**
 * @brief   fd借用export对象的unexport操作执行器
 *
 * @param exportObj      [IN]/[OUT] fd类型借用内存导出对象
 * @return int    0：操作成功；非0：获取失败
 */
uint32_t UbseMmiModule::UbseMemFdUnExportExecutor(const UbseMemFdBorrowExportObj &exportObj)
{
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemFdUnExportExecutor, name=" << exportObj.req.name << ", objDetails is "
                  << RmCommonUtils::GetInstance().TranStructToStr(exportObj);
    if (!MemInstanceInnerCommon::UbseMemExecutorCheckParam(exportObj)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Obj check param failed, name=" << exportObj.req.name;
        return UBSE_ERROR_INVAL;
    }
    auto ret = MemInstanceInnerFdBorrow::GetInstance().MemFdUnExportExecutor(exportObj);
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemFdUnExportExecutor end, ret =" << ret << ", name=" << exportObj.req.name
                  << " memid=" << RmCommonUtils::GetInstance().MemToStr(exportObj.status.exportObmmInfo);
    return ret;
}

/**
 * @brief   numa借用import对象的执行器
 *
 * @param importObj      [IN]/[OUT] numa类型借用内存导入对象
 * @return int    0：操作成功；非0：获取失败
 */
uint32_t UbseMmiModule::UbseMemNumaImportExecutor(UbseMemNumaBorrowImportObj &importObj)
{
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemNumaImportExecutor, name=" << importObj.req.name << ", objDetails is "
                  << RmCommonUtils::GetInstance().TranStructToStr(importObj);
    if (!MemInstanceInnerCommon::UbseMemExecutorCheckParam(importObj)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Obj check param failed, name=" << importObj.req.name;
        return UBSE_ERROR_INVAL;
    }
    if (importObj.exportObmmInfo.empty()) {
        return UBSE_ERROR_INVAL;
    }
    auto ret = MemInstanceInnerNumaBorrow::GetInstance().MemNumaImportExecutor(importObj);
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemNumaImportExecutor end, ret =" << ret << ", name=" << importObj.req.name
                  << " memid=" << RmCommonUtils::GetInstance().MemToStr(importObj.status.importResults);
    return ret;
}

/**
 * @brief   numa借用import对象的unimport操作执行器
 *
 * @param importObj      [IN]/[OUT] numa类型借用内存导入对象
 * @return int    0：操作成功；非0：获取失败
 */
uint32_t UbseMmiModule::UbseMemNumaUnImportExecutor(const UbseMemNumaBorrowImportObj &importObj)
{
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemNumaUnImportExecutor, name=" << importObj.req.name << ", objDetails is "
                  << RmCommonUtils::GetInstance().TranStructToStr(importObj);
    if (!MemInstanceInnerCommon::UbseMemExecutorCheckParam(importObj)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Obj check param failed, name=" << importObj.req.name;
        return UBSE_ERROR_INVAL;
    }
    auto ret = MemInstanceInnerNumaBorrow::GetInstance().MemNumaUnImportExecutor(importObj);
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemNumaUnImportExecutor end, ret=" << ret << ", name=" << importObj.req.name
                  << ", memid and numa=" << RmCommonUtils::GetInstance().MemToStr(importObj.status.importResults);
    return ret;
}

/**
 * @brief   numa借用export对象的执行器
 *
 * @param exportObj      [IN]/[OUT] numa类型借用内存导出对象
 * @return int    0：操作成功；非0：获取失败
 */
uint32_t UbseMmiModule::UbseMemNumaExportExecutor(UbseMemNumaBorrowExportObj &exportObj)
{
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemNumaExportExecutor, name=" << exportObj.req.name << ", objDetails is "
                  << RmCommonUtils::GetInstance().TranStructToStr(exportObj);
    if (!MemInstanceInnerCommon::UbseMemExecutorCheckParam(exportObj)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Obj check param failed, name=" << exportObj.req.name;
        return UBSE_ERROR_INVAL;
    }
    if (exportObj.algoResult.exportNumaInfos.size() > NO_2) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Algo result is invalid, ";
        exportObj.status.errCode = UBSE_ERROR_INVAL;
        return UBSE_ERROR_INVAL;
    }
    auto ret = MemInstanceInnerNumaBorrow::GetInstance().MemNumaExportExecutor(exportObj);
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemNumaExportExecutor end, ret =" << ret << ", name=" << exportObj.req.name
                  << " memid=" << RmCommonUtils::GetInstance().MemToStr(exportObj.status.exportObmmInfo);
    return ret;
}

/**
 * @brief   numa借用export对象的unexport操作执行器
 *
 * @param exportObj      [IN]/[OUT] numa类型借用内存导出对象
 * @return int    0：操作成功；非0：获取失败
 */
uint32_t UbseMmiModule::UbseMemNumaUnExportExecutor(const UbseMemNumaBorrowExportObj &exportObj)
{
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemNumaUnExportExecutor, name=" << exportObj.req.name << ", objDetails is "
                  << RmCommonUtils::GetInstance().TranStructToStr(exportObj);
    if (!MemInstanceInnerCommon::UbseMemExecutorCheckParam(exportObj)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Obj check param failed, name=" << exportObj.req.name;
        return UBSE_ERROR_INVAL;
    }
    auto ret = MemInstanceInnerNumaBorrow::GetInstance().MemNumaUnExportExecutor(exportObj);
    UBSE_LOG_INFO << MMI_LOG_INFO << "Execute end, ret =" << ret << ", name=" << exportObj.req.name
                  << " memid=" << RmCommonUtils::GetInstance().MemToStr(exportObj.status.exportObmmInfo);
    return ret;
}

/**
 * @brief   shm借用import对象的执行器
 *
 * @param importObj      [IN]/[OUT] shm类型借用内存导入对象
 * @return int    0：操作成功；非0：获取失败
 */
uint32_t UbseMmiModule::UbseMemShmImportExecutor(UbseMemShareBorrowImportObj &importObj)
{
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemShmImportExecutor, name=" << importObj.req.name << ", objDetails is "
                  << RmCommonUtils::GetInstance().TranStructToStr(importObj);
    if (!MemInstanceInnerCommon::UbseMemExecutorCheckParam(importObj)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Obj check param failed, name=" << importObj.req.name;
        return UBSE_ERROR_INVAL;
    }
    auto ret = MemInstanceInnerShm::GetInstance().MemShmImportExecutor(importObj);
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemShmImportExecutor end, ret=" << ret << ", name=" << importObj.req.name
                  << " memid=" << RmCommonUtils::GetInstance().MemToStr(importObj.status.importResults);
    UBSE_LOG_INFO << MMI_LOG_INFO << "privData=(onePth=" << importObj.req.ubseMemPrivData.onePth
                  << ", wrDelayComp=" << importObj.req.ubseMemPrivData.wrDelayComp
                  << ", reduceDelayComp=" << importObj.req.ubseMemPrivData.reduceDelayComp
                  << ", cmoDelayComp=" << importObj.req.ubseMemPrivData.cmoDelayComp
                  << ", so=" << importObj.req.ubseMemPrivData.so
                  << ", adTrOchip=" << importObj.req.ubseMemPrivData.adTrOchip
                  << ", cacheableFlag=" << importObj.req.ubseMemPrivData.cacheableFlag
                  << ", marId=" << importObj.req.ubseMemPrivData.marId
                  << ", marId=" << importObj.req.ubseMemPrivData.rsv0 << ")";
    return ret;
}

/**
 * @brief   shm借用import对象的unimport执行器
 *
 * @param importObj      [IN] shm类型借用内存导入对象
 * @return int    0：操作成功；非0：获取失败
 */
uint32_t UbseMmiModule::UbseMemShmUnImportExecutor(const UbseMemShareBorrowImportObj &importObj)
{
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemShmUnImportExecutor, name=" << importObj.req.name << ", objDetails is "
                  << RmCommonUtils::GetInstance().TranStructToStr(importObj);
    if (!MemInstanceInnerCommon::UbseMemExecutorCheckParam(importObj)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Obj check param failed, name=" << importObj.req.name;
        return UBSE_ERROR_INVAL;
    }
    auto ret = MemInstanceInnerShm::GetInstance().MemShmUnImportExecutor(importObj);
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemShmUnImportExecutor end, ret=" << ret << ", name=" << importObj.req.name
                  << ", memid=" << RmCommonUtils::GetInstance().MemToStr(importObj.status.importResults);
    return ret;
}

/**
 * @brief   shm借用export对象的执行器
 *
 * @param exportObj      [IN]/[OUT] shm类型借用内存导出对象
 * @return int    0：操作成功；非0：获取失败
 */
uint32_t UbseMmiModule::UbseMemShmExportExecutor(UbseMemShareBorrowExportObj &exportObj)
{
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemShmExportExecutor, name=" << exportObj.req.name << ", objDetails is "
                  << RmCommonUtils::GetInstance().TranStructToStr(exportObj);
    if (!MemInstanceInnerCommon::UbseMemExecutorCheckParam(exportObj)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Obj check param failed, name=" << exportObj.req.name;
        return UBSE_ERROR_INVAL;
    }
    auto ret = MemInstanceInnerShm::GetInstance().MemShmExportExecutor(exportObj);
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemShmExportExecutor end, ret=" << ret << ", name=" << exportObj.req.name
                  << ", memid=" << RmCommonUtils::GetInstance().MemToStr(exportObj.status.exportObmmInfo);
    return ret;
}

/**
 * @brief   shm借用export对象的执行器
 *
 * @param exportObj      [IN] shm类型借用内存导出对象的unexport操作执行器
 * @return int    0：操作成功；非0：获取失败
 */
uint32_t UbseMmiModule::UbseMemShmUnExportExecutor(const UbseMemShareBorrowExportObj &exportObj)
{
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemShmUnExportExecutor, name=" << exportObj.req.name << ", objDetails is "
                  << RmCommonUtils::GetInstance().TranStructToStr(exportObj);
    if (!MemInstanceInnerCommon::UbseMemExecutorCheckParam(exportObj)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Obj check param failed, name=" << exportObj.req.name;
        return UBSE_ERROR_INVAL;
    }
    auto ret = MemInstanceInnerShm::GetInstance().MemShmUnExportExecutor(exportObj);
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemShmUnExportExecutor end, ret=" << ret << ", name=" << exportObj.req.name
                  << ", memid=" << RmCommonUtils::GetInstance().MemToStr(exportObj.status.exportObmmInfo);
    return ret;
}

/**
 * @brief   addr借用import对象的执行器
 *
 * @param importObj      [IN]/[OUT] addr类型借用内存导入对象
 * @return int    0：操作成功；非0：获取失败
 */
uint32_t UbseMmiModule::UbseMemAddrImportExecutor(UbseMemAddrBorrowImportObj &importObj)
{
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemAddrImportExecutor, name=" << importObj.req.name << ", objDetails is "
                  << RmCommonUtils::GetInstance().TranStructToStr(importObj);
    for (const auto &numaInfo : importObj.algoResult.exportNumaInfos) {
        UBSE_LOG_INFO << MMI_LOG_INFO << numaInfo.nodeId << ", " << numaInfo.socketId << ", " << numaInfo.numaId << ", "
                      << numaInfo.size;
    }
    auto ret = MemInstanceInnerAddrBorrow::GetInstance().MemAddrImportExecutor(importObj);
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemAddrImportExecutor end, ret=" << ret << ", name=" << importObj.req.name
                  << ", memid=" << RmCommonUtils::GetInstance().MemToStr(importObj.status.importResults);
    return ret;
}

/**
 * @brief   addr借用import对象的unimport操作执行器
 *
 * @param importObj      [IN]/[OUT] addr类型借用内存导入对象
 * @return int    0：操作成功；非0：获取失败
 */
uint32_t UbseMmiModule::UbseMemAddrUnImportExecutor(const UbseMemAddrBorrowImportObj &importObj)
{
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemAddrUnImportExecutor, name=" << importObj.req.name << ", objDetails is "
                  << RmCommonUtils::GetInstance().TranStructToStr(importObj);
    auto ret = MemInstanceInnerAddrBorrow::GetInstance().MemAddrUnImportExecutor(importObj);
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemAddrUnImportExecutor end, ret=" << ret << ", name=" << importObj.req.name
                  << ", memid=" << RmCommonUtils::GetInstance().MemToStr(importObj.status.importResults);
    return ret;
}

/**
 * @brief   addr借用export对象的执行器
 *
 * @param exportObj      [IN]/[OUT] addr类型借用内存导出对象
 * @return int    0：操作成功；非0：获取失败
 */
uint32_t UbseMmiModule::UbseMemAddrExportExecutor(UbseMemAddrBorrowExportObj &exportObj)
{
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemAddrExportExecutor, name=" << exportObj.req.name << ", objDetails is "
                  << RmCommonUtils::GetInstance().TranStructToStr(exportObj);
    auto ret = MemInstanceInnerAddrBorrow::GetInstance().MemAddrExportExecutor(exportObj);
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemAddrExportExecutor end, ret=" << ret << ", name=" << exportObj.req.name
                  << ", memid=" << RmCommonUtils::GetInstance().MemToStr(exportObj.status.exportObmmInfo);
    for (const auto &numaInfo : exportObj.algoResult.exportNumaInfos) {
        UBSE_LOG_INFO << MMI_LOG_INFO << numaInfo.nodeId << ", " << numaInfo.socketId << ", " << numaInfo.numaId << ", "
                      << numaInfo.size;
    }
    return ret;
}

/**
 * @brief   addr借用export对象unexport操作的执行器
 *
 * @param exportObj      [IN]/[OUT] addr类型借用内存导出对象
 * @return int    0：操作成功；非0：获取失败
 */
uint32_t UbseMmiModule::UbseMemAddrUnExportExecutor(const UbseMemAddrBorrowExportObj &exportObj)
{
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemAddrUnExportExecutor, name=" << exportObj.req.name << ", objDetails is "
                  << RmCommonUtils::GetInstance().TranStructToStr(exportObj);
    auto ret = MemInstanceInnerAddrBorrow::GetInstance().MemAddrUnExportExecutor(exportObj);
    UBSE_LOG_INFO << MMI_LOG_INFO << "UbseMemAddrUnExportExecutor end, ret=" << ret << ", name=" << exportObj.req.name
                  << ", memid=" << RmCommonUtils::GetInstance().MemToStr(exportObj.status.exportObmmInfo);
    return ret;
}

uint32_t UbseMmiModule::UbseMmiPreOnline(const std::vector<SocketCnaInfo> &cnaTopoInfos, uint64_t preImportSize)
{
    UBSE_LOG_INFO << MMI_LOG_INFO << "Start to preOnline, preImportSize=" << preImportSize;
    if (cnaTopoInfos.empty()) {
        UBSE_LOG_WARN << MMI_LOG_INFO << "SocketCnaInfo is empty, not need preOnline.";
        return UBSE_OK;
    }
    auto ret = MemInstanceInnerCommon::GetInstance().MemPreOnline(cnaTopoInfos, preImportSize);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "UbseMmiPreOnline MemPreOnline failed, ret=" << ret;
        return ret;
    }
    return UBSE_OK;
}

uint32_t UbseMmiModule::UbseMmiUnPreOnline()
{
    UBSE_LOG_INFO << MMI_LOG_INFO << "Start to unPreOnline.";
    return MemInstanceInnerCommon::GetInstance().MemUnPreOnline();
}

} // namespace ubse::mmi
