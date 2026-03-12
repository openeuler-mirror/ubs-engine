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

#ifndef UBSE_MMI_INTERFACE_H
#define UBSE_MMI_INTERFACE_H
#include "ubse_mmi_def.h"
#include "ubse_def.h"
#include "ubse_common_def.h"

namespace ubse::adapter_plugins::mmi {
using namespace ubse::common::def;
class UbseMmiInterface {
public:
    virtual ~UbseMmiInterface() = default;

    static UbseMmiInterface& GetInstance();

    /**
    * @brief   从obmm恢复对象数据
    *
    * @param memBorrowObj      [IN]/[OUT] 该节点所有的对象数据
    * @return UbseResult    0：操作成功；非0：获取失败
    */
    virtual UbseResult GetObjData(NodeMemDebtInfo& memBorrowObj) = 0;

    /**
    * @brief   fd借用import permission操作执行器
    *
    * @param importObj      [IN]/[OUT] fd类型借用内存导入对象
    * @return UbseResult    0：操作成功；非0：获取失败
    */
    virtual UbseResult FdImportPermissionExecutor(UbseMemFdBorrowImportObj& importObj) = 0;

    /**
    * @brief   fd借用import操作执行器
    *
    * @param importObj      [IN]/[OUT] fd类型借用内存导入对象
    * @return UbseResult    0：操作成功；非0：获取失败
    */
    virtual UbseResult FdImportExecutor(UbseMemFdBorrowImportObj& importObj) = 0;

    /**
    * @brief   fd借用unimport操作执行器
    *
    * @param importObj      [IN] fd类型借用内存导入对象
    * @return UbseResult    0：操作成功；非0：获取失败
    */
    virtual UbseResult FdUnImportExecutor(const UbseMemFdBorrowImportObj& importObj) = 0;

    /**
    * @brief   fd借用export对象的执行器
    *
    * @param exportObj      [IN]/[OUT] fd类型借用内存导出对象
    * @return UbseResult    0：操作成功；非0：获取失败
    */
    virtual UbseResult FdExportExecutor(UbseMemFdBorrowExportObj& exportObj) = 0;

    /**
    * @brief   fd借用export对象的unexport操作执行器
    *
    * @param exportObj      [IN] fd类型借用内存导出对象
    * @return UbseResult    0：操作成功；非0：获取失败
    */
    virtual UbseResult FdUnExportExecutor(const UbseMemFdBorrowExportObj& exportObj) = 0;

    /**
    * @brief   numa借用import对象的执行器
    *
    * @param importObj      [IN]/[OUT] numa类型借用内存导入对象
    * @return UbseResult    0：操作成功；非0：获取失败
    */
    virtual UbseResult NumaImportExecutor(UbseMemNumaBorrowImportObj& importObj) = 0;

    /**
    * @brief   numa借用import对象的unimport操作执行器
    *
    * @param importObj      [IN] numa类型借用内存导入对象
    * @return UbseResult    0：操作成功；非0：获取失败
    */
    virtual UbseResult NumaUnImportExecutor(const UbseMemNumaBorrowImportObj& importObj) = 0;

    /**
    * @brief   numa借用export对象的执行器
    *
    * @param exportObj      [IN]/[OUT] numa类型借用内存导出对象
    * @return UbseResult    0：操作成功；非0：获取失败
    */
    virtual UbseResult NumaExportExecutor(UbseMemNumaBorrowExportObj& exportObj) = 0;

    /**
    * @brief   numa借用export对象的unexport操作执行器
    *
    * @param exportObj      [IN] numa类型借用内存导出对象
    * @return UbseResult    0：操作成功；非0：获取失败
    */
    virtual UbseResult NumaUnExportExecutor(const UbseMemNumaBorrowExportObj& exportObj) = 0;

    /**
    * @brief   shm借用import对象的执行器
    *
    * @param importObj      [IN]/[OUT] shm类型借用内存导入对象
    * @return UbseResult    0：操作成功；非0：获取失败
    */
    virtual UbseResult ShmImportExecutor(UbseMemShareBorrowImportObj& importObj) = 0;

    /**
    * @brief   shm借用import对象的unimport执行器
    *
    * @param importObj      [IN] shm类型借用内存导入对象
    * @return UbseResult    0：操作成功；非0：获取失败
    */
    virtual UbseResult ShmUnImportExecutor(const UbseMemShareBorrowImportObj& importObj) = 0;

    /**
    * @brief   shm借用export对象的执行器
    *
    * @param exportObj      [IN]/[OUT] shm类型借用内存导出对象
    * @return UbseResult    0：操作成功；非0：获取失败
    */
    virtual UbseResult ShmExportExecutor(UbseMemShareBorrowExportObj& exportObj) = 0;

    /**
    * @brief   shm类型借用内存导出对象的unexport操作执行器
    *
    * @param exportObj      [IN] shm类型借用内存导出对象的unexport操作执行器
    * @return UbseResult    0：操作成功；非0：获取失败
    */
    virtual UbseResult ShmUnExportExecutor(const UbseMemShareBorrowExportObj& exportObj) = 0;

    /**
    * @brief   addr借用import对象的执行器
    *
    * @param importObj      [IN]/[OUT] addr类型借用内存导入对象
    * @return UbseResult    0：操作成功；非0：获取失败
    */
    virtual UbseResult AddrImportExecutor(UbseMemAddrBorrowImportObj& importObj) = 0;

    /**
    * @brief   addr借用import对象的unimport操作执行器
    *
    * @param importObj      [IN] addr类型借用内存导入对象
    * @return UbseResult    0：操作成功；非0：获取失败
    */
    virtual UbseResult AddrUnImportExecutor(const UbseMemAddrBorrowImportObj& importObj) = 0;

    /**
    * @brief   addr借用export对象的执行器
    *
    * @param exportObj      [IN]/[OUT] addr类型借用内存导出对象
    * @return UbseResult    0：操作成功；非0：获取失败
    */
    virtual UbseResult AddrExportExecutor(UbseMemAddrBorrowExportObj& exportObj) = 0;

    /**
    * @brief   addr借用export对象unexport操作的执行器
    *
    * @param exportObj      [IN] addr类型借用内存导出对象
    * @return UbseResult    0：操作成功；非0：获取失败
    */
    virtual UbseResult AddrUnExportExecutor(const UbseMemAddrBorrowExportObj& exportObj) = 0;

    /**
    * @brief   预上线远端内存，以远端numa的形式呈现
    *
    * @param cnaTopoInfos      [IN] cna的连线关系
    * @param preImportSize      [IN] 预上线内存的大小,单位是字节
    * @return UbseResult    0：操作成功；非0：操作失败
    */
    virtual UbseResult PreOnline(const std::vector<SocketCnaInfo>& cnaTopoInfos, uint64_t preImportSize) = 0;

    /**
    * @brief   下线预上线的内存
    *
    * @return UbseResult    0：操作成功；非0：操作失败
    */
    virtual UbseResult UnPreOnline() = 0;
};
}  // namespace ubse::adapter_plugins::mmi
#endif  // UBSE_MMI_INTERFACE_H
