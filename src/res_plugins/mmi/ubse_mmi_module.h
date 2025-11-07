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

#ifndef UBSE_MANAGER_UBSE_MMI_MODULE_H
#define UBSE_MANAGER_UBSE_MMI_MODULE_H

#include "ubse_common_def.h"
#include "ubse_mem_resource.h"
#include "ubse_module.h"
#include "ubse_mem_obj.h"

namespace ubse::mmi {
using namespace ubse::module;
using namespace ubse::mem::obj;
using namespace ubse::resource::mem;

class UbseMmiModule : public UbseModule {
public:
    UbseResult Initialize() override;

    void UnInitialize() override;

    UbseResult Start() override;

    void Stop() override;

    /* *
 * @brief   从obmm恢复对象数据
 *
 * @param memBorrowObj      [IN]/[OUT] 该节点所有的对象数据
 * @return int    0：操作成功；非0：获取失败
 */
    uint32_t UbseMemGetObjData(NodeMemDebtInfo &memBorrowObj);

    /* *
 * @brief   fd借用import操作执行器
 *
 * @param importObj      [IN]/[OUT] fd类型借用内存导入对象
 * @return int    0：操作成功；非0：获取失败
 */
    uint32_t UbseMemFdImportExecutor(UbseMemFdBorrowImportObj &importObj);

    /* *
 * @brief   fd借用unimport操作执行器
 *
 * @param importObj      [IN]/[OUT] fd类型借用内存导入对象
 * @return int    0：操作成功；非0：获取失败
 */
    uint32_t UbseMemFdUnImportExecutor(const UbseMemFdBorrowImportObj &importObj);

    /* *
 * @brief   fd借用export对象的执行器
 *
 * @param exportObj      [IN]/[OUT] fd类型借用内存导出对象
 * @return int    0：操作成功；非0：获取失败
 */
    uint32_t UbseMemFdExportExecutor(UbseMemFdBorrowExportObj &exportObj);

    /* *
 * @brief   fd借用export对象的unexport操作执行器
 *
 * @param exportObj      [IN]/[OUT] fd类型借用内存导出对象
 * @return int    0：操作成功；非0：获取失败
 */
    uint32_t UbseMemFdUnExportExecutor(const UbseMemFdBorrowExportObj &exportObj);

    /* *
 * @brief   numa借用import对象的执行器
 *
 * @param importObj      [IN]/[OUT] numa类型借用内存导入对象
 * @return int    0：操作成功；非0：获取失败
 */
    uint32_t UbseMemNumaImportExecutor(UbseMemNumaBorrowImportObj &importObj);

    /* *
 * @brief   numa借用import对象的unimport操作执行器
 *
 * @param importObj      [IN]/[OUT] numa类型借用内存导入对象
 * @return int    0：操作成功；非0：获取失败
 */
    uint32_t UbseMemNumaUnImportExecutor(const UbseMemNumaBorrowImportObj &importObj);

    /* *
 * @brief   numa借用export对象的执行器
 *
 * @param exportObj      [IN]/[OUT] numa类型借用内存导出对象
 * @return int    0：操作成功；非0：获取失败
 */
    uint32_t UbseMemNumaExportExecutor(UbseMemNumaBorrowExportObj &exportObj);

    /* *
 * @brief   numa借用export对象的unexport操作执行器
 *
 * @param exportObj      [IN]/[OUT] numa类型借用内存导出对象
 * @return int    0：操作成功；非0：获取失败
 */
    uint32_t UbseMemNumaUnExportExecutor(const UbseMemNumaBorrowExportObj &exportObj);
}; // namespace ubse::mmi
} // namespace ubse::mmi
#endif
