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

#ifndef UBSE_MEM_SCHEDULER_H
#define UBSE_MEM_SCHEDULER_H

#include <cstdint>

#include "ubse_mem_obj.h"
#include "ubse_node_controller.h"

namespace ubse::mem::scheduler {
using namespace ubse::mem::obj;
using namespace ubse::nodeController;
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
} // namespace ubse::mem::scheduler

#endif // UBSE_MEM_SCHEDULER_H
