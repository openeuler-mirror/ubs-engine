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

#include <map>
#include "ubse_logger.h"
#include "ubse_mem_advice.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse_fault");

static std::map<MemAdvice, std::string> advices = {
    {MemAdvice::CHECK_FAILED, "please check and correct the input parameters."},
    {MemAdvice::COMM_FAILED, "please check the communication."},
    {MemAdvice::RESOURCE_EXIST, "The resource is already in use."},
    {MemAdvice::NODE_IN_MAINTENANCE, "please wait for the node to join the cluster or complete the smoothing stats."},
    {MemAdvice::RESOURCE_NOT_EXIST, "please check whether the resource exists."},
    {MemAdvice::RESOURCE_OPERATION_CONFLICT, "please wait for the resource creation or deletion operation to finish."},
    {MemAdvice::UBSE_NO_OPERATION_PERMISSION, "please check whether the user has permission to operate this resource."}
};

void BorrowFailedAdvice(std::string prefix, std::string name, std::string borrowType, size_t size,
                        std::string exportNode, std::string importNode, uint32_t errorCode, MemAdvice advice)
{
    std::string adviceCode = (advice == MemAdvice::INTERNAL_FAILED) ? "" : std::to_string(static_cast<int32_t>(advice));
    std::string adviceStr = (advices.find(advice) == advices.end()) ? "" : advices[advice];
    std::ostringstream oss;
    oss << "[UBSE_MEM] " << prefix << ". RequestName=" << name << ", BorrowType=" << borrowType
        << ", RequestSize=" << size << "byte, ExportNode=" << exportNode << ", ImportNode=" << importNode
        << ", ErrorCode=" << errorCode << ", AdviceCode=" << adviceCode << ", Advice="  << adviceStr;
    UBSE_LOG_INFO << oss.str();
}
} // namespace ubse::mem::controller