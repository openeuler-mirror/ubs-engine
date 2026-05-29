/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_mem_controller.h"
#include "ubse_error.h"

namespace ubse::mem::controller {
/**
* @brief 返回和传入节点相关的账本信息。
 * 传入节点故障，取除故障节点的所有节点账本信息；
 * 传入节点未故障，取所有拓扑节点的账本信息。
 * 部分平滑完成返回部分成功，全部拓扑节点平滑完成返回成功。
* @param nodeId [in] 节点Id非空且是有效节点
* @param debtInfos [out] 借用账本对象集合
* @return 成功返回0，失败返回1, 部分成功返回2
* @return #RACK_OK 0 正确
* @return #RACK_ERROR 1 失败
* @return #RACK_ERROR_INVAL 14 无效参数
* @return 2 部分成功
*/
UbseResult UbseGetNumaMemDebtInfoWithNode(const std::string& nodeId, std::vector<UbseNumaMemoryDebtInfo>& debtInfos)
{
    return UBSE_OK;
}

UbseResult UbseMemNumaDelete(const std::string& name, const UbseMemBorrower& borrower)
{
    return UBSE_OK;
}

UbseResult UbseMemNumaCreateWithLender(const std::string& name, const UbseMemBorrower& borrower,
                                       const std::vector<UbseMemNumaLender>& lenders,
                                       uint8_t usrInfo[UBSE_MAX_USR_INFO_LEN], UbseMemNumaDesc& desc)
{
    return UBSE_OK;
}

UbseResult UbseMemNumaCreate(const std::string& name, const UbseMemBorrower& borrower, const UbseMemNumaCreateOpt& opt,
                             UbseMemNumaDesc& desc)
{
    return UBSE_OK;
}

UbseResult UbseGetNumaMemDebtInfo(std::vector<UbseNumaMemoryDebtInfo>& debtInfos)
{
    return UBSE_OK;
}

UbseResult UbseGetAllNodeNumaInfo(std::vector<UbseNodeNumaInfo>& numaNodeInfoList)
{
    return UBSE_OK;
}

UbseResult UbseMemNumaCreateWithCandidate(const std::string& name, const UbseMemBorrower& borrower,
                                          const UbseMemNumaCandidateOpt& opt, UbseMemNumaDesc& desc)
{
    return UBSE_OK;
}

/**
* @brief 查询借用标识对应的操作结果
* @param name [in] 借用标识
* @param borrowType [in] 借用类型
* @param memResult [out] 借用结果
* @return 查询成功返回0，失败返回1
*/
uint32_t UbseQueryResult(const std::string& name, UbseMemResult& result, UbseMemBorrowType borrowType)
{
    return 0;
}
} // namespace ubse::mem::controller
