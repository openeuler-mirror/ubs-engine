/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
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
 * 传入节点对账完成则返回成功
 * 传入节点未对账完成，如果剩余节点全部对账完成返回成功，否则返回部分成功。
* @param nodeId [in] 节点Id非空且是有效节点
* @param debtInfos [out] 借用账本对象集合
* @return #UBSE_OK 成功
* @return #UBSE_ERR_INVALID_ARG 无效参数
* @return #UBSE_PAR_SUCCESS 部分成功
* @return #UBSE_ERR_INTERNAL 内部错误
*/
UbseResult UbseGetNumaMemDebtInfoWithNode(const std::string &nodeId, std::vector<UbseNumaMemoryDebtInfo> &debtInfos)
{
    return UBSE_OK;
}

/**
* @brief 返回当前集群topo中的所有对账完成节点的账本信息。
* 当前集群topo中所有节点均对账完成则返回成功
* 否则返回部分成功。
* @param debtInfos [out] 借用账本对象集合
* @return #UBSE_OK 成功
* @return #UBSE_ERR_INVALID_ARG 无效参数
* @return #UBSE_PAR_SUCCESS 部分成功
* @return #UBSE_ERR_INTERNAL 内部错误
*/
UbseResult UbseGetNumaMemDebtInfo(std::vector<UbseNumaMemoryDebtInfo> &debtInfos)
{
    return UBSE_OK;
}

/**
* @brief 返回所有采集到的节点相关的numa信息。
* @param numaNodeInfoList [out] 节点numa对象集合
* @return UBSE_OK 成功
* @return UBSE_ERR_INTERNAL 获取节点信息失败
*/
UbseResult UbseGetAllNodeNumaInfo(std::vector<UbseNodeNumaInfo> &numaNodeInfoList)
{
    return UBSE_OK;
}

/**
* @brief 返回所有采集到的节点指定节点相关的numa信息。
* @param nodeId [in] 节点Id非空且是有效节点
* @param numaNodeInfoList [out] 节点numa对象集合
* @return UBSE_ERR_INVALID_ARG 传入值非法或指定节点不存在
* @return UBSE_ERR_INTERNAL 获取节点信息失败
*/
UbseResult UbseGetNodeNumaInfoByNodeId(const std::string &nodeId, std::vector<UbseNodeNumaInfo> &numaNodeInfoList)
{
    return UBSE_OK;
}

/**
 * @brief 指定借出信息，提供给插件使用numa借用 碎片场景使用
 *
 * @param name [IN] 必填，借用标识, name最大长度48字节, 含结尾字符\0; name仅可包括大小写字母、数字、"."、":"、"-"以及"_"; name在节点内保持唯一性
 * @param borrower [IN] 必填，借用方信息
 * @param lenders [IN] 必填，借出方信息
 * @param usrInfo [IN] 选填，调用方私有数据，UBSE只负责保存，get时原样返回
 * @param desc [OUT] 借用形成的远端numa信息
 * @return UbseResult
 */
UbseResult UbseMemNumaCreateWithLender(const std::string &name, const UbseMemBorrower &borrower,
                                       const std::vector<UbseMemNumaLender> &lenders,
                                       uint8_t usrInfo[UBSE_MAX_USR_INFO_LEN], UbseMemNumaDesc &desc)
{
    return UBSE_OK;
}

/**
 * @brief 删除指定numa远端内存;
 *
 * @param name  [IN] 必填，借用标识, name最大长度48字节, 含结尾字符\0; name仅可包括大小写字母、数字、"."、":"、"-"以及"_"; name在节点内保持唯一性
 * @param borrower  [IN] 必填，借用方信息
 * @return UbseResult
 */
UbseResult UbseMemNumaDelete(const std::string &name, const UbseMemBorrower &borrower)
{
    return UBSE_OK;
}
} // namespace ubse::mem::controller