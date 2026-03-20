/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
*/

#ifndef UBSE_MANAGER_UBSE_MEM_CONTROLLER_MSG_H
#define UBSE_MANAGER_UBSE_MEM_CONTROLLER_MSG_H

#include "ubse_com_module.h"
#include "ubse_mem_controller_pre_online.h"
#include "ubse_mmi_interface.h"
#include "ubse_mem_controller_def.h"

namespace ubse::mem::controller {
using namespace ubse::com;
using namespace ubse::adapter_plugins::mmi;

void RegUbseMemControllerHandler();

/**
* 同步消息，主节点采集节点拓扑
* @param nodeId [in] 采集节点id
* @param info [out] agent节点拓扑
* @return
*/
UbseResult CollectLedge(const std::string &nodeId, NodeMemDebtInfo &info);

UbseResult CollectLedgeHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

UbseResult QueryFdImportObj(const std::string &nodeId, const std::string &name, UbseMemFdBorrowImportObj &fdInfo,
                            const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap);

UbseResult QueryFdImportObjHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

UbseResult QueryNumaImportObj(const std::string &nodeId, const std::string &name, UbseMemNumaBorrowImportObj &numaInfo,
                              const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap);

UbseResult QueryNumaImportObjHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

UbseResult QueryAddrImportObj(const std::string &nodeId, const std::string &name, UbseMemAddrBorrowImportObj &addrInfo,
                              const std::unordered_map<std::string, NodeMemDebtInfo> &allDebtInfoMap);

UbseResult QueryAddrImportObjHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

/**
* 主节点-向执行节点发送预上线处理请求
* @param nodeId
* @param req
* @return
*/
UbseResult PreOnLineRequest(const std::string &nodeId, PreOnLineReq req);

/**
* 执行节点-响应主节点预上线请求，执行预上线操作
* @param req
* @param resp
* @return
*/
UbseResult PreOnLineHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

/**
* 执行节点-向主节点发送预上线处理结果
* @param resp
* @return
*/
UbseResult PreOnLineReply(PreOnLineResp resp);

/**
* 主节点-响应执行节点预上线处理结果
* @param req
* @param resp
* @return
*/
UbseResult PreOnLineReplyHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

UbseResult GetNumaInfoByPidHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

UbseResult GetNumaInfoFromAgent(const std::string &nodeId, const uint64_t &pid, uint32_t &numaId, uint32_t &socketId);

UbseResult QueryFdExport(def::UbseMemDebtQueryRequest request, UbseMemFdBorrowExportObj &obj);

UbseResult QueryFdExportHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

UbseResult QueryNumaExport(def::UbseMemDebtQueryRequest request, UbseMemNumaBorrowExportObj &obj);

UbseResult QueryNumaExportHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

UbseResult QueryAddrExport(def::UbseMemDebtQueryRequest request, UbseMemAddrBorrowExportObj &obj);

UbseResult QueryAddrExportHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

UbseResult QueryShareExport(def::UbseMemDebtQueryRequest request, UbseMemShareBorrowExportObj &obj);

UbseResult QueryShareExportHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

UbseResult QueryFdImport(def::UbseMemDebtQueryRequest request, UbseMemFdBorrowImportObj &obj);

UbseResult QueryFdImportHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

UbseResult QueryNumaImport(def::UbseMemDebtQueryRequest request, UbseMemNumaBorrowImportObj &obj);

UbseResult QueryNumaImportHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

UbseResult QueryAddrImport(def::UbseMemDebtQueryRequest request, UbseMemAddrBorrowImportObj &obj);

UbseResult QueryAddrImportHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

UbseResult QueryShareImport(def::UbseMemDebtQueryRequest request, UbseMemShareBorrowImportObj &obj);

UbseResult QueryShareImportHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);
} // namespace ubse::mem::controller

#endif // UBSE_MANAGER_UBSE_MEM_CONTROLLER_MSG_H
