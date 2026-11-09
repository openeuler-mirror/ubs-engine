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
#include "ubse_mmi_interface.h"
#include "ubse_module.h"

namespace ubse::mmi {
using ubse::adapter_plugins::mmi::NodeMemDebtInfo;
using ubse::adapter_plugins::mmi::SocketCnaInfo;
using ubse::adapter_plugins::mmi::UbseMemAddrBorrowExportObj;
using ubse::adapter_plugins::mmi::UbseMemAddrBorrowImportObj;
using ubse::adapter_plugins::mmi::UbseMemFdBorrowExportObj;
using ubse::adapter_plugins::mmi::UbseMemFdBorrowImportObj;
using ubse::adapter_plugins::mmi::UbseMemNumaBorrowExportObj;
using ubse::adapter_plugins::mmi::UbseMemNumaBorrowImportObj;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowExportObj;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowImportObj;
using ubse::adapter_plugins::mmi::UbseMemShmSwitchExportObj;
using ubse::adapter_plugins::mmi::UbseMemShmSwitchObj;
using ubse::common::def::UbseResult;
using ubse::module::UbseModule;

class UbseMmiModule : public UbseModule {
public:
    UbseResult Initialize() override;

    void UnInitialize() override;

    UbseResult Start() override;

    void Stop() override;

    /**
    * @brief   从obmm恢复对象数据
    *
    * @param memBorrowObj      [IN]/[OUT] 该节点所有的对象数据
    * @return int    0：操作成功；非0：获取失败
    */
    uint32_t UbseMemGetObjData(NodeMemDebtInfo& memBorrowObj);

    /**
    * @brief   fd借用import permission操作执行器
    *
    * @param importObj      [IN]/[OUT] fd类型借用内存导入对象
    * @return int    0：操作成功；非0：获取失败
    */
    uint32_t UbseMemFdImportPermissionExecutor(UbseMemFdBorrowImportObj& importObj);

    /**
    * @brief   fd借用import操作执行器
    *
    * @param importObj      [IN]/[OUT] fd类型借用内存导入对象
    * @return int    0：操作成功；非0：获取失败
    */
    uint32_t UbseMemFdImportExecutor(UbseMemFdBorrowImportObj& importObj);

    /**
    * @brief   fd借用unimport操作执行器
    *
    * @param importObj      [IN]/[OUT] fd类型借用内存导入对象
    * @return int    0：操作成功；非0：获取失败
    */
    uint32_t UbseMemFdUnImportExecutor(const UbseMemFdBorrowImportObj& importObj);

    /**
    * @brief   fd借用export对象的执行器
    *
    * @param exportObj      [IN]/[OUT] fd类型借用内存导出对象
    * @return int    0：操作成功；非0：获取失败
    */
    uint32_t UbseMemFdExportExecutor(UbseMemFdBorrowExportObj& exportObj);

    /**
    * @brief   fd借用export对象的unexport操作执行器
    *
    * @param exportObj      [IN]/[OUT] fd类型借用内存导出对象
    * @return int    0：操作成功；非0：获取失败
    */
    uint32_t UbseMemFdUnExportExecutor(const UbseMemFdBorrowExportObj& exportObj);

    /**
    * @brief   numa借用import对象的执行器
    *
    * @param importObj      [IN]/[OUT] numa类型借用内存导入对象
    * @return int    0：操作成功；非0：获取失败
    */
    uint32_t UbseMemNumaImportExecutor(UbseMemNumaBorrowImportObj& importObj);

    /**
    * @brief   numa借用import对象的unimport操作执行器
    *
    * @param importObj      [IN]/[OUT] numa类型借用内存导入对象
    * @return int    0：操作成功；非0：获取失败
    */
    uint32_t UbseMemNumaUnImportExecutor(const UbseMemNumaBorrowImportObj& importObj);

    /**
    * @brief   numa借用export对象的执行器
    *
    * @param exportObj      [IN]/[OUT] numa类型借用内存导出对象
    * @return int    0：操作成功；非0：获取失败
    */
    uint32_t UbseMemNumaExportExecutor(UbseMemNumaBorrowExportObj& exportObj);

    /**
    * @brief   numa借用export对象的unexport操作执行器
    *
    * @param exportObj      [IN]/[OUT] numa类型借用内存导出对象
    * @return int    0：操作成功；非0：获取失败
    */
    uint32_t UbseMemNumaUnExportExecutor(const UbseMemNumaBorrowExportObj& exportObj);

    /**
    * @brief   shm借用import对象的执行器
    *
    * @param importObj      [IN]/[OUT] shm类型借用内存导入对象
    * @return int    0：操作成功；非0：获取失败
    */
    uint32_t UbseMemShmImportExecutor(UbseMemShareBorrowImportObj& importObj);

    /**
    * @brief   shm借用import对象的unimport执行器
    *
    * @param importObj      [IN] shm类型借用内存导入对象
    * @return int    0：操作成功；非0：获取失败
    */
    uint32_t UbseMemShmUnImportExecutor(const UbseMemShareBorrowImportObj& importObj);

    /**
    * @brief   shm借用export对象的执行器
    *
    * @param exportObj      [IN]/[OUT] shm类型借用内存导出对象
    * @return int    0：操作成功；非0：获取失败
    */
    uint32_t UbseMemShmExportExecutor(UbseMemShareBorrowExportObj& exportObj);

    /**
    * @brief   shm借用export对象的执行器
    *
    * @param exportObj      [IN] shm类型借用内存导出对象的unexport操作执行器
    * @return int    0：操作成功；非0：获取失败
    */
    uint32_t UbseMemShmUnExportExecutor(const UbseMemShareBorrowExportObj& exportObj);

    /**
    * @brief   shm类型故障迁移块级切换begin执行器
    *
    * @param switchObj      [IN] 迁移切换入参（编排层按 (memId, importNodeId) 组装）
    * @return int    0：操作成功；非0：失败
    */
    uint32_t UbseMemShmSwitchBeginImportExecutor(const UbseMemShmSwitchObj& switchObj);
    uint32_t UbseMemShmSwitchBeginExportExecutor(const UbseMemShmSwitchExportObj& switchObj,
                                                 ubse_mem_obmm_mem_desc& desc);

    /**
    * @brief   shm类型故障迁移块级切换end执行器
    *
    * @param switchObj      [IN] 迁移切换入参（复用 name/oldImportMemId 供日志与定位）
    * @param success        [IN] 迁移是否成功，映射为 obmm 的 int（1/0）
    * @return int    0：成功；UBSE_ERROR_AGAIN：内核收尾中可重试；UBSE_MMI_OBMM_SWITCH_NOT_EXIST：切换已不存在
    */
    uint32_t UbseMemShmSwitchEndExecutor(const UbseMemShmSwitchObj& switchObj, bool success);

    /**
    * @brief   shm类型故障迁移块级数据拷贝执行器
    * @param memId  [IN] 切换间隙设备ID，双后端映射后使用memcpy_mt拷贝
    * @param size   [IN] 块大小(字节)
    * @return int    0：操作成功；非0：失败
    */
    uint32_t UbseMemShmMigrateCopyExecutor(uint64_t memId, uint64_t size, bool cacheable);

    /**
    * @brief   addr借用import对象的执行器
    *
    * @param importObj      [IN]/[OUT] addr类型借用内存导入对象
    * @return int    0：操作成功；非0：获取失败
    */
    uint32_t UbseMemAddrImportExecutor(UbseMemAddrBorrowImportObj& importObj);

    /**
    * @brief   addr借用import对象的unimport操作执行器
    *
    * @param importObj      [IN]/[OUT] addr类型借用内存导入对象
    * @return int    0：操作成功；非0：获取失败
    */
    uint32_t UbseMemAddrUnImportExecutor(const UbseMemAddrBorrowImportObj& importObj);

    /**
    * @brief   addr借用export对象的执行器
    *
    * @param exportObj      [IN]/[OUT] addr类型借用内存导出对象
    * @return int    0：操作成功；非0：获取失败
    */
    uint32_t UbseMemAddrExportExecutor(UbseMemAddrBorrowExportObj& exportObj);

    /**
    * @brief   addr借用export对象unexport操作的执行器
    *
    * @param exportObj      [IN]/[OUT] addr类型借用内存导出对象
    * @return int    0：操作成功；非0：获取失败
    */
    uint32_t UbseMemAddrUnExportExecutor(const UbseMemAddrBorrowExportObj& exportObj);

    /**
    * @brief   预上线远端内存，以远端numa的形式呈现
    *
    * @param preImportSize      [IN] 预上线内存的大小,单位是字节
    * @param preImportSize      [IN] cna的连线关系
    * @return uint32_t    0：操作成功；非0：操作失败
    */
    uint32_t UbseMmiPreOnline(const std::vector<SocketCnaInfo>& cnaTopoInfos, uint64_t preImportSize);

    /**
    * @brief   下线预上线的内存
    *
    * @return uint32_t    0：操作成功；非0：操作失败
    */
    uint32_t UbseMmiUnPreOnline();
}; // namespace ubse::mmi
} // namespace ubse::mmi
#endif
