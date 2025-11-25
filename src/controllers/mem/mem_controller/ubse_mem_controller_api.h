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

#ifndef UBSE_MEM_CONTROLLER_API_H
#define UBSE_MEM_CONTROLLER_API_H

#include <cstdint>
#include <string>
#include <vector>
#include "lock/ubse_lock.h"

#include "ubse_api_server_module.h"
#include "ubse_mem_obj.h"
#include "ubse_mem_resource.h"
#include "ubse_node_controller.h"

namespace ubse::mem::controller {
using namespace ubse::mem::obj;
using namespace ubse::resource::mem;
using namespace ubse::nodeController;
using namespace api::server;

enum class BorrowedType {
    FD,
    NUMA,
    FD_RETURN,
    NUMA_RETURN
};

/* *
 * 初始化
 * @return 0: 成功; 非0: 失败
 */
UbseResult RegisterNodeCtlNotify();
uint32_t Init();
uint32_t GetCnaInfoWhenImport(const std::string &exportNodeId, const std::string &importNodeId,
                              UbseMemBorrowImportBaseObj &importObj);

uint32_t UbseMemFdBorrowDispatch(const UbseIpcMessage &buffer, const UbseRequestContext &context);

uint32_t UbseMemFdBorrowWithLenderDispatch(const UbseIpcMessage &buffer, const UbseRequestContext &context);
/* *
 * Fd内存借用
 * @param req [IN] 请求参数
 * @param resp [IN/OUT] 操作结果
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemFdBorrow(const UbseMemFdBorrowReq &req, UbseMemOperationResp &resp);

/* *
 * Numa内存借用
 * @param req [IN] 请求参数
 * @param resp [IN/OUT] 操作结果
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemNumaBorrow(const UbseMemNumaBorrowReq &req, UbseMemOperationResp &resp);

/* *
 * 内存归还，适用于所有借用
 * @param req [IN] 请求参数
 * @param resp [IN/OUT] 操作结果
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemReturn(const UbseMemReturnReq &req, UbseMemOperationResp &resp);

/* *
 * Fd类型内存的Export对象回调
 * @param exportObj [IN] Fd类型内存的Export对象
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemFdBorrowExportObjCallback(const UbseMemFdBorrowExportObj &exportObj);

/* *
 * Fd类型内存的Import对象回调
 * @param exportObj [IN] Fd类型内存的Import对象
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemFdBorrowImportObjCallback(const UbseMemFdBorrowImportObj &importObj);

/* *
 * Numa类型内存的Export对象回调
 * @param exportObj [IN] Numa类型内存的Export对象
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemNumaBorrowExportObjCallback(const UbseMemNumaBorrowExportObj &exportObj);

/* *
 * Numa类型内存的Import对象回调
 * @param exportObj [IN] Numa类型内存的Import对象
 * @return 0: 成功; 非0: 失败
 */
uint32_t UbseMemNumaBorrowImportObjCallback(const UbseMemNumaBorrowImportObj &importObj);

/* *
 * 启动时加载所有初始对象
 * @return 0: 成功; 非0: 失败
 */
uint32_t LoadLocalAllObjs(const ubse::nodeController::UbseNodeInfo &node);

uint32_t UbseMemFdReturn(const UbseMemReturnReq &req, UbseMemOperationResp &resp);
uint32_t UbseMemNumaReturn(const UbseMemReturnReq &req, UbseMemOperationResp &resp);

/* *
 * 查找指定import类型对象和指定export类型对象
 * @tparam ImportObjType ：import对象类型
 * @tparam ExportObjType : export对象类型
 * @param name : 需要匹配的name
 * @param importObj : 返回查询对象
 * @param exportObj : 返回的借用对象
 * @param getImportMap : 用于获取对应map的函数对象
 * @param getExportMap:  用于获取对应map的函数对象
 */
template <typename ImportObjType, typename ExportObjType>
void FindBorrowObjByName(
    const std::string &name,
    ImportObjType &outImportObj, // 输出参数
    ExportObjType &outExportObj, // 输出参数
    bool &foundImport,           // 指示是否找到导入对象
    bool &foundExport,           // 指示是否找到导出对象
    const std::function<const std::unordered_map<std::string, ImportObjType> &(const NodeMemDebtInfo &)> &getImportMap,
    const std::function<const std::unordered_map<std::string, ExportObjType> &(const NodeMemDebtInfo &)> &getExportMap);

NodeMemDebtInfoMap GetNodeMemDebtInfoMap();

uint32_t GetNodeMemDebtInfoById(const std::string &nodeId, NodeMemDebtInfo &info);

uint32_t DeleteFdExport(const UbseMemFdBorrowExportObj &exportObj);

uint32_t DeleteNumaExport(const UbseMemNumaBorrowExportObj &exportObj);

uint32_t AddFdImport(const UbseMemFdBorrowImportObj &importObj);

uint32_t AddFdExport(const UbseMemFdBorrowExportObj &exportObj);

uint32_t AddNumaImport(const UbseMemNumaBorrowImportObj &importObj);

uint32_t AddNumaExport(const UbseMemNumaBorrowExportObj &exportObj);

uint64_t UbseMemRequestIdGet(const std::string &name);

uint32_t UbseMemFdReturnDispatch(const UbseIpcMessage &buffer, const UbseRequestContext &context);

UbseResult UbseMemNumaCreateHandler(const UbseIpcMessage &buffer, const UbseRequestContext &context);
UbseResult UbseMemNumaCreateWithLender(const UbseIpcMessage &buffer, const UbseRequestContext &context);
UbseResult UbseMemNumaDelete(const UbseIpcMessage &buffer, const UbseRequestContext &context);
UbseResult UbseMemNumaBorrowRespHandler(const ubse::mem::obj::UbseMemOperationResp &resp);
UbseResult UbseMemNumaReturnRespHandler(const ubse::mem::obj::UbseMemOperationResp &resp);
uint32_t GetNdeoMemDebtInfoMap(const std::string &nodeId, NodeMemDebtInfoMap &memDebtInfoMap);

uint32_t ImportToAddDecoderEntry(UbseMemImportStatus &status, uint8_t importType, uint32_t socketId,
                                const std::vector<UbseMemObmmInfo> &exportObmmInfo, bool isShare);

void UnimportToDelDecoderEntry(UbseMemImportStatus &status, uint32_t socketId);                                
} // namespace ubse::mem::controller

#endif // UBSE_MEM_CONTROLLER_API_H
