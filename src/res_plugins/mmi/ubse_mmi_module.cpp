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

#include "ubse_mmi_module.h"
#include "ubse_common_def.h"
#include "ubse_mem_types.h"
#include "mem_instance_inner.h"
#include "ubse_mem_common_utils.h"
#include "ubse_obmm_executor.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger_inner.h"

namespace ubse::mmi {
using namespace ubse::log;
using namespace ubse::mem::obj;
constexpr size_t OBMM_MAX_EXECUTE_TIME = 3;

DYNAMIC_CREATE(UbseMmiModule);
UbseResult UbseMmiModule::Initialize()
{
    auto ret = RmObmmExecutor::GetInstance().Init();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "RmObmmExecutor init failed, ret = " << ret;
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

uint32_t UbseMmiModule::UbseMemGetObjData(NodeMemDebtInfo &memBorrowObj)
{
    // 芯片表项碎片，导入一半的时候，进程挂掉了，从obmm获取数据前等3s,保证单次最大1G执行完成
    sleep(OBMM_MAX_EXECUTE_TIME);
    memBorrowObj.fdExportObjMap = {};
    memBorrowObj.fdImportObjMap = {};
    memBorrowObj.numaExportObjMap = {};
    memBorrowObj.numaImportObjMap = {};
    std::vector<UbseMemLocalObmmMetaData> allObmmDatas;
    uint32_t ret = UBSE_OK;
    ret = MemInstanceInner::GetInstance().MemGetObjData(memBorrowObj, allObmmDatas);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "[MMI] failed to get obj data.";
        return ret;
    }
    return ret;
}

/* *
 * @brief   fd借用import操作执行器
 *
 * @param importObj      [IN]/[OUT] fd类型借用内存导入对象
 * @return int    0：操作成功；非0：获取失败
 */
uint32_t UbseMmiModule::UbseMemFdImportExecutor(UbseMemFdBorrowImportObj &importObj)
{
    UBSE_LOG_INFO << "UbseMemFdImportExecutor, name=" << importObj.req.name;
    // 入参检查
    auto ret = MemInstanceInner::GetInstance().MemFdImportExecutor(importObj);
    UBSE_LOG_INFO << "End successful, name=" << importObj.req.name
                << " memid=" << RmCommonUtils::GetInstance().MemToStr(importObj.status.importResults);
    return ret;
}

/* *
 * @brief   fd借用unimport操作执行器
 *
 * @param importObj      [IN]/[OUT] fd类型借用内存导入对象
 * @return int    0：操作成功；非0：获取失败
 */
uint32_t UbseMmiModule::UbseMemFdUnImportExecutor(const UbseMemFdBorrowImportObj &importObj)
{
    UBSE_LOG_INFO << "UbseMemFdUnImportExecutor, name=" << importObj.req.name;
    // 入参检查
    auto ret = MemInstanceInner::GetInstance().MemFdUnImportExecutor(importObj);
    UBSE_LOG_INFO << "End successful, name=" << importObj.req.name
                << " memid=" << RmCommonUtils::GetInstance().MemToStr(importObj.status.importResults);
    return ret;
}

/* *
 * @brief   fd借用export对象的执行器
 *
 * @param exportObj      [IN]/[OUT] fd类型借用内存导出对象
 * @return int    0：操作成功；非0：获取失败
 */
uint32_t UbseMmiModule::UbseMemFdExportExecutor(UbseMemFdBorrowExportObj &exportObj)
{
    UBSE_LOG_INFO << "UbseMemFdExportExecutor, name=" << exportObj.req.name;
    // 入参检查
    auto ret = MemInstanceInner::GetInstance().MemFdExportExecutor(exportObj);
    UBSE_LOG_INFO << "End successful, name=" << exportObj.req.name
                << " memid=" << RmCommonUtils::GetInstance().MemToStr(exportObj.status.exportObmmInfo);
    return ret;
}

/* *
 * @brief   fd借用export对象的unexport操作执行器
 *
 * @param exportObj      [IN]/[OUT] fd类型借用内存导出对象
 * @return int    0：操作成功；非0：获取失败
 */
uint32_t UbseMmiModule::UbseMemFdUnExportExecutor(const UbseMemFdBorrowExportObj &exportObj)
{
    UBSE_LOG_INFO << "UbseMemFdUnExportExecutor, name=" << exportObj.req.name;
    // 入参检查
    auto ret = MemInstanceInner::GetInstance().MemFdUnExportExecutor(exportObj);
    UBSE_LOG_INFO << "End successful, name=" << exportObj.req.name
                << " memid=" << RmCommonUtils::GetInstance().MemToStr(exportObj.status.exportObmmInfo);
    return ret;
}

/* *
 * @brief   numa借用import对象的执行器
 *
 * @param importObj      [IN]/[OUT] numa类型借用内存导入对象
 * @return int    0：操作成功；非0：获取失败
 */
uint32_t UbseMmiModule::UbseMemNumaImportExecutor(UbseMemNumaBorrowImportObj &importObj)
{
    UBSE_LOG_INFO << "UbseMemNumaImportExecutor, name=" << importObj.req.name;
    // 参数检查, 入参打印
    if (importObj.exportObmmInfo.empty()) {
        return UBSE_ERROR_INVAL;
    }
    auto ret = MemInstanceInner::GetInstance().MemNumaImportExecutor(importObj);
    UBSE_LOG_INFO << "End successful, name=" << importObj.req.name
                << " memid=" << RmCommonUtils::GetInstance().MemToStr(importObj.status.importResults);
    return ret;
}

/* *
 * @brief   numa借用import对象的unimport操作执行器
 *
 * @param importObj      [IN]/[OUT] numa类型借用内存导入对象
 * @return int    0：操作成功；非0：获取失败
 */
uint32_t UbseMmiModule::UbseMemNumaUnImportExecutor(const UbseMemNumaBorrowImportObj &importObj)
{
    UBSE_LOG_INFO << "UbseMemNumaUnImportExecutor, name=" << importObj.req.name;
    // 所有的入参有效信息打印与检查
    auto ret = MemInstanceInner::GetInstance().MemNumaUnImportExecutor(importObj);
    UBSE_LOG_INFO << "End successful, name=" << importObj.req.name
                << " memid=" << RmCommonUtils::GetInstance().MemToStr(importObj.status.importResults);
    return ret;
}

/* *
 * @brief   numa借用export对象的执行器
 *
 * @param exportObj      [IN]/[OUT] numa类型借用内存导出对象
 * @return int    0：操作成功；非0：获取失败
 */

uint32_t UbseMmiModule::UbseMemNumaExportExecutor(UbseMemNumaBorrowExportObj &exportObj)
{
    UBSE_LOG_INFO << "UbseMemNumaExportExecutor, name=" << exportObj.req.name;
    /*
     * 入参检查
     * 利用export对象构造输入，自定义数据还是用原先的
     * 存元数据
     * */
    if (exportObj.algoResult.exportNumaInfos.size() > NO_2) {
        UBSE_LOG_ERROR << "Algo result is invalid, ";
        exportObj.status.errCode = UBSE_ERROR_INVAL;
        return UBSE_ERROR_INVAL;
    }
    auto ret = MemInstanceInner::GetInstance().MemNumaExportExecutor(exportObj);
    UBSE_LOG_INFO << "End successful, name=" << exportObj.req.name
                << " memid=" << RmCommonUtils::GetInstance().MemToStr(exportObj.status.exportObmmInfo);
    return ret;
}

/* *
 * @brief   numa借用export对象的unexport操作执行器
 *
 * @param exportObj      [IN]/[OUT] numa类型借用内存导出对象
 * @return int    0：操作成功；非0：获取失败
 */
uint32_t UbseMmiModule::UbseMemNumaUnExportExecutor(const UbseMemNumaBorrowExportObj &exportObj)
{
    UBSE_LOG_INFO << "UbseMemNumaUnExportExecutor, name=" << exportObj.req.name;
    // 参数打印
    auto ret = MemInstanceInner::GetInstance().MemNumaUnExportExecutor(exportObj);
    UBSE_LOG_INFO << "End successful, name=" << exportObj.req.name
                << " memid=" << RmCommonUtils::GetInstance().MemToStr(exportObj.status.exportObmmInfo);
    return ret;
}

} // namespace ubse::mmi
