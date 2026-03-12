/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef UBSE_MEM_SCHEDULER_H
#define UBSE_MEM_SCHEDULER_H
#include <cstdint>

#include "ubse_mmi_interface.h"
#include "ubse_node_controller.h"

namespace ubse::mem::scheduler {
using namespace ubse::adapter_plugins::mmi;
/* *
 * @brief   scheduler模块的初始化函数
 *
 * @return uint32_t    0：操作成功；非0：获取失败
 */
uint32_t Init(); // 主要是注册Node状态变化的回调函数

/* *
 * @brief   Node状态发生变化后的回调函数
 *
 * @param nodeInfo      [IN] 节点相关信息
 * @return uint32_t    0：操作成功；非0：获取失败
 */

uint32_t UbseMemNodeObjChangeHandler(const ubse::nodeController::UbseNodeInfo &nodeInfo); // 注册给nodeController

/* *
 * @brief   fd借用import对象状态发生变化之后，算法的处理函数
 *
 * @param importObj      [IN]/[OUT] fd类型借用内存导入对象
 * @return uint32_t    0：操作成功；非0：获取失败
 */
uint32_t UbseMemFdImportObjStateChangeHandler(UbseMemFdBorrowImportObj &importObj);

/* *
 * @brief   fd借用export对象状态变化的处理函数
 *
 * @param exportObj      [IN]/[OUT] addr类型借用内存导出对象
 * @return uint32_t    0：操作成功；非0：获取失败
 */
uint32_t UbseMemFdExportObjStateChangeHandler(UbseMemFdBorrowExportObj &exportObj);

/* *
 * @brief   numa借用import对象的算法处理函数
 *
 * @param importObj      [IN]/[OUT] numa类型借用内存导入对象
 * @return uint32_t    0：操作成功；非0：获取失败
 */
uint32_t UbseMemNumaImportObjStateChangeHandler(UbseMemNumaBorrowImportObj &importObj);

/* *
 * @brief   numa借用export对象状态变化的处理函数
 *
 * @param exportObj      [IN]/[OUT] numa类型借用内存导出对象
 * @return uint32_t    0：操作成功；非0：获取失败
 */

uint32_t UbseMemNumaExportObjStateChangeHandler(UbseMemNumaBorrowExportObj &exportObj);

/* *
 * @brief   shm借用import对象状态变化的处理函数
 *
 * @param importObj      [IN]/[OUT] shm类型借用内存导入对象
 * @return uint32_t    0：操作成功；非0：获取失败
 */
uint32_t UbseMemShmImportObjStateChangeHandler(UbseMemShareBorrowImportObj &importObj);

/* *
 * @brief   shm借用export对象状态变化的处理函数
 *
 * @param exportObj      [IN]/[OUT] shm类型借用内存导出对象
 * @return uint32_t    0：操作成功；非0：获取失败
 */
uint32_t UbseMemShmExportObjStateChangeHandler(UbseMemShareBorrowExportObj &exportObj);

/* *
 * @brief   addr借用import对象状态变化的回调函数
 *
 * @param importObj      [IN]/[OUT] addr类型借用内存导入对象
 * @return uint32_t    0：操作成功；非0：获取失败
 */
uint32_t UbseMemAddrImportObjStateChangeHandler(UbseMemAddrBorrowImportObj &importObj);

/* *
 * @brief   addr借用export对象状态变化的处理函数
 *
 * @param exportObj      [IN]/[OUT] addr类型借用内存导出对象
 * @return uint32_t    0：操作成功；非0：获取失败
 */
uint32_t UbseMemAddrExportObjStateChangeHandler(UbseMemAddrBorrowExportObj &exportObj);

/* *
 * @brief   主备切换之后，清理算法缓存数据
 *
 * @return void
 */
void ClearCacheValue();

/* *
 * @brief   节点状态变更的回调函数
 *
 * @return uint32
 */
uint32_t UbseMemNodeObjChangeHandler(const ubse::nodeController::UbseNodeInfo &nodeInfo);
} // namespace ubse::mem::scheduler

#endif // UBSE_MEM_SCHEDULER_H
