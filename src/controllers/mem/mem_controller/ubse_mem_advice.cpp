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

#include "ubse_mem_advice.h"
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include "ubse_election.h"
#include "ubse_logger.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse_fault");

enum class ProcessType : uint8_t
{
    BORROW_FAILED = 0,
    IMPORT_FAILED = 1,
    EXPORT_FAILED = 2,
    RETURN_FAILED = 3,
    UNIMPORT_FAILED = 4,
    UNEXPORT_FAILED = 5,
};

enum class MemAdvice : uint8_t
{
    INTERNAL_FAILED = 0,
    CHECK_FAILED = 1,
    COMM_FAILED = 2,
    SCHEDULE_FAILED = 3,
    OBMM_FAILED = 4,
    RESOURCE_EXIST = 5,
    TIME_OUT = 6,
    NODE_IN_MAINTENANCE = 7,
    RESOURCE_NOT_EXIST = 8,
    RESOURCE_OPERATION_CONFLICT = 9,
    UBSE_NO_OPERATION_PERMISSION = 10,
    UB_FEATURE_NOT_SUPPORTED = 11,
    DECODE_FAILED = 12,
};

struct FaultInfo {
    ProcessType processType;
    MemAdvice adviceCode;
    const char* errorInfo;
};

// 局部类型别名：避免在静态映射表初始化中重复书写枚举类名前缀
using PT = ProcessType;
using MA = MemAdvice;
using MF = MemFault;

namespace {
const auto& GetProcessDescMap()
{
    static const std::unordered_map<ProcessType, const char*> map = {
        {PT::BORROW_FAILED, "Borrow Schedule failed"}, {PT::IMPORT_FAILED, "Import failed"},
        {PT::EXPORT_FAILED, "Export failed"},          {PT::RETURN_FAILED, "Return Schedule failed"},
        {PT::UNIMPORT_FAILED, "UnImport failed"},      {PT::UNEXPORT_FAILED, "UnExport failed"},
    };
    return map;
}

const auto& GetAdviceStrMap()
{
    static const std::unordered_map<MemAdvice, const char*> map = {
        {MA::INTERNAL_FAILED, ""},
        {MA::CHECK_FAILED, "please check and correct the input parameters."},
        {MA::COMM_FAILED, "please check the communication."},
        {MA::SCHEDULE_FAILED, "failed to schedule resources."},
        {MA::OBMM_FAILED, "please check the OBMM status."},
        {MA::RESOURCE_EXIST, "the resource is already in use."},
        {MA::TIME_OUT, "please try again."},
        {MA::NODE_IN_MAINTENANCE, "please wait for the node to join the cluster or complete the smoothing stats."},
        {MA::RESOURCE_NOT_EXIST, "please check whether the resource exists."},
        {MA::RESOURCE_OPERATION_CONFLICT, "please wait for the resource creation or deletion operation to finish."},
        {MA::UBSE_NO_OPERATION_PERMISSION, "please check whether the user has permission to operate this resource."},
        {MA::UB_FEATURE_NOT_SUPPORTED, "please check the ub feature of the cluster."},
        {MA::DECODE_FAILED, "please check the decoder table."},
    };
    return map;
}

const auto& GetFaultCodeMap()
{
    static const std::unordered_map<MemFault, FaultInfo> map = {
        // INTERNAL_FAULT_*
        {MF::BORROW_FAULT_INTERNAL, {PT::BORROW_FAILED, MA::INTERNAL_FAILED, "internal fault."}},
        {MF::BORROW_FAULT_IMPORT_INTERNAL, {PT::IMPORT_FAILED, MA::INTERNAL_FAILED, "internal fault."}},
        {MF::BORROW_FAULT_EXPORT_INTERNAL, {PT::EXPORT_FAILED, MA::INTERNAL_FAILED, "internal fault."}},
        {MF::SHARED_FAULT_ATTACH_INTERNAL, {PT::IMPORT_FAILED, MA::INTERNAL_FAILED, "internal fault."}},
        {MF::RETURN_FAULT_INTERNAL, {PT::RETURN_FAILED, MA::INTERNAL_FAILED, "internal fault."}},
        {MF::RETURN_FAULT_IMPORT_INTERNAL, {PT::UNIMPORT_FAILED, MA::INTERNAL_FAILED, "internal fault."}},
        {MF::RETURN_FAULT_EXPORT_INTERNAL, {PT::UNEXPORT_FAILED, MA::INTERNAL_FAILED, "internal fault."}},
        {MF::SHARED_FAULT_DETACH_INTERNAL, {PT::UNIMPORT_FAILED, MA::INTERNAL_FAILED, "internal fault."}},
        // BORROW_*
        {MF::BORROW_CHECK_FAILED, {PT::BORROW_FAILED, MA::CHECK_FAILED, "the input parameter is not valid."}},
        {MF::BORROW_PARAM_INVALID, {PT::BORROW_FAILED, MA::CHECK_FAILED, "the input parameter is not valid."}},
        {MF::BORROW_NAME_EXIST, {PT::BORROW_FAILED, MA::CHECK_FAILED, "the borrow name is already in use."}},
        {MF::BORROW_CHIP_NOT_SUPPORTED,
         {PT::BORROW_FAILED, MA::UB_FEATURE_NOT_SUPPORTED, "the ub feature of memory borrow is not supported."}},
        {MF::BORROW_SCHEDULE_FAILED,
         {PT::BORROW_FAILED, MA::SCHEDULE_FAILED, "the borrow scheduler failed to allocate memory."}},
        {MF::BORROW_MASTER_SEND_FAILED,
         {PT::BORROW_FAILED, MA::COMM_FAILED, "the master node failed to send messages."}},
        {MF::BORROW_EXPORT_SEND_FAILED,
         {PT::EXPORT_FAILED, MA::COMM_FAILED, "the export node failed to send messages."}},
        {MF::BORROW_IMPORT_SEND_FAILED,
         {PT::IMPORT_FAILED, MA::COMM_FAILED, "the import node failed to send messages."}},
        {MF::BORROW_REQ_SEND_FAILED, {PT::BORROW_FAILED, MA::COMM_FAILED, "the request node failed to send messages."}},
        {MF::BORROW_DECODE_FAILED,
         {PT::IMPORT_FAILED, MA::DECODE_FAILED, "the import node failed to add decoder table."}},
        {MF::BORROW_IMPORT_IN_MAINTENANCE,
         {PT::IMPORT_FAILED, MA::NODE_IN_MAINTENANCE, "the import node is in maintenance."}},
        {MF::BORROW_OBMM_EXPORT_FAILED, {PT::EXPORT_FAILED, MA::OBMM_FAILED, "failed to export memory from OBMM."}},
        {MF::BORROW_OBMM_IMPORT_FAILED, {PT::IMPORT_FAILED, MA::OBMM_FAILED, "failed to import memory to OBMM."}},
        {MF::BORROW_TIME_OUT, {PT::BORROW_FAILED, MA::TIME_OUT, "the borrow operation timed out."}},
        // SHARED_*
        {MF::SHARED_BORROW_CHECK_FAILED, {PT::BORROW_FAILED, MA::CHECK_FAILED, "the input parameter is not valid."}},
        {MF::SHARED_ATTACH_CHECK_FAILED,
         {PT::IMPORT_FAILED, MA::CHECK_FAILED, "the shared attach parameter is not valid."}},
        {MF::SHARED_ATTACH_AUTH_FAILED,
         {PT::IMPORT_FAILED, MA::UBSE_NO_OPERATION_PERMISSION, "the shared attach authentication failed."}},
        {MF::SHARED_ATTACH_EXIST, {PT::IMPORT_FAILED, MA::RESOURCE_EXIST, "the shared memory already exists."}},
        {MF::SHARED_CHIP_NOT_SUPPORTED,
         {PT::BORROW_FAILED, MA::UB_FEATURE_NOT_SUPPORTED, "the chip does not support shared memory."}},
        {MF::SHARED_CHIP_MODE_NOT_SUPPORTED,
         {PT::BORROW_FAILED, MA::UB_FEATURE_NOT_SUPPORTED, "the chip mode does not support shared memory."}},
        // RETURN_*
        {MF::RETURN_NAME_NOT_EXIST, {PT::RETURN_FAILED, MA::RESOURCE_NOT_EXIST, "the borrow name does not exist."}},
        {MF::RETURN_PARAM_INVALID, {PT::RETURN_FAILED, MA::CHECK_FAILED, "the input parameter is not valid."}},
        {MF::RETURN_CHIP_NOT_SUPPORTED,
         {PT::RETURN_FAILED, MA::UB_FEATURE_NOT_SUPPORTED, "the chip does not support memory return."}},
        {MF::RETURN_MASTER_SEND_FAILED,
         {PT::RETURN_FAILED, MA::COMM_FAILED, "The master node failed to send messages."}},
        {MF::RETURN_EXPORT_SEND_FAILED,
         {PT::UNEXPORT_FAILED, MA::COMM_FAILED, "the export node failed to send messages."}},
        {MF::RETURN_IMPORT_SEND_FAILED,
         {PT::UNIMPORT_FAILED, MA::COMM_FAILED, "the import node failed to send messages."}},
        {MF::RETURN_REQ_SEND_FAILED, {PT::RETURN_FAILED, MA::COMM_FAILED, "the request node failed to send messages."}},
        {MF::RETURN_REQ_CONFLICT,
         {PT::RETURN_FAILED, MA::RESOURCE_OPERATION_CONFLICT, "the return request conflicts with another operation."}},
        {MF::RETURN_AUTH_FAILED,
         {PT::RETURN_FAILED, MA::UBSE_NO_OPERATION_PERMISSION, "the return authentication failed."}},
        {MF::RETURN_IMPORT_IN_MAINTENANCE,
         {PT::UNIMPORT_FAILED, MA::NODE_IN_MAINTENANCE, "the import node is in maintenance."}},
        {MF::RETURN_EXPORT_IN_MAINTENANCE,
         {PT::UNEXPORT_FAILED, MA::NODE_IN_MAINTENANCE, "the export node is in maintenance."}},
        {MF::RETURN_OBMM_EXPORT_FAILED, {PT::UNEXPORT_FAILED, MA::OBMM_FAILED, "failed to unexport memory from OBMM."}},
        {MF::RETURN_OBMM_IMPORT_FAILED, {PT::UNIMPORT_FAILED, MA::OBMM_FAILED, "failed to unimport memory to OBMM."}},
        {MF::RETURN_TIME_OUT, {PT::RETURN_FAILED, MA::TIME_OUT, "the return operation timed out."}},
        // SHARED_RETURN_*
        {MF::SHARED_RETURN_IN_ATTACH,
         {PT::RETURN_FAILED, MA::RESOURCE_OPERATION_CONFLICT, "the shared memory is still in attach."}},
        {MF::SHARED_DETACH_CHECK_FAILED,
         {PT::UNIMPORT_FAILED, MA::CHECK_FAILED, "the shared detach parameter is not valid."}},
        {MF::SHARED_DETACH_AUTH_FAILED,
         {PT::UNIMPORT_FAILED, MA::UBSE_NO_OPERATION_PERMISSION, "the shared detach authentication failed."}},
        {MF::SHARED_DETACH_REQ_CONFLICT,
         {PT::UNIMPORT_FAILED, MA::RESOURCE_OPERATION_CONFLICT,
          "the detach request conflicts with another operation."}},
        {MF::SHARED_DETACH_NOT_EXIST,
         {PT::UNIMPORT_FAILED, MA::RESOURCE_NOT_EXIST, "the shared memory does not exist."}},
        {MF::SHARED_RETURN_CHIP_NOT_SUPPORTED,
         {PT::RETURN_FAILED, MA::UB_FEATURE_NOT_SUPPORTED, "the chip does not support shared memory return."}},
    };
    return map;
}

const auto& GetBorrowTypeMap()
{
    static const std::unordered_map<MemType, const char*> map = {
        {MemType::FD, "WATER_BORROW"},
        {MemType::NUMA, "APP_NUMA_BORROW"},
        {MemType::SHM, "SHARE_BORROW"},
        {MemType::ADDR, "APP_PRI_BORROW"},
    };
    return map;
}
} // namespace

void BorrowFailedAdvice(const BorrowFailedAdviceCtx& ctx)
{
    const auto& faultCodeMap = GetFaultCodeMap();
    auto it = faultCodeMap.find(ctx.faultCode);
    if (it == faultCodeMap.end()) {
        UBSE_LOG_INFO << "[UBSE_MEM] Unknown faultCode=" << static_cast<uint32_t>(ctx.faultCode);
        return;
    }
    const FaultInfo& fault = it->second;

    const auto& processDescMap = GetProcessDescMap();
    auto processIt = processDescMap.find(fault.processType);
    const char* processDesc = (processIt != processDescMap.end()) ? processIt->second : "";
    const auto& adviceStrMap = GetAdviceStrMap();
    auto adviceIt = adviceStrMap.find(fault.adviceCode);
    const char* adviceStr = (adviceIt != adviceStrMap.end()) ? adviceIt->second : "";
    const auto& borrowTypeMap = GetBorrowTypeMap();
    auto typeIt = borrowTypeMap.find(ctx.borrowType);
    const char* borrowTypeStr = (typeIt != borrowTypeMap.end()) ? typeIt->second : "";
    std::string masterId{};
    ubse::election::UbseGetMasterNodeId(masterId); // 获取主节点ID，若失败则返回空字符串
    std::ostringstream errorCodeStream;
    errorCodeStream << "ubse_borrow_" << std::setw(4) << std::setfill('0') << static_cast<uint32_t>(ctx.faultCode);
    std::string errorCode = errorCodeStream.str();

    std::ostringstream oss;
    oss << "[UBSE_MEM] " << processDesc << ". RequestName=" << ctx.name << ", BorrowType=" << borrowTypeStr
        << ", RequestSize=" << ctx.size << "byte, ExportNode=" << ctx.exportNode << ", ImportNode=" << ctx.importNode
        << ", RequestNode=" << ctx.requestNode << ", MasterNode=" << masterId << ", ErrorCode=" << errorCode
        << ", ErrorInfo=" << fault.errorInfo << ", AdviceCode=" << static_cast<uint32_t>(fault.adviceCode)
        << ", Advice=" << adviceStr;
    UBSE_LOG_INFO << oss.str();
}
} // namespace ubse::mem::controller