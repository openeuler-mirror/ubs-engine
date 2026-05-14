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

#ifndef UBSE_MEM_CONTROLLER_H
#define UBSE_MEM_CONTROLLER_H

#include <netinet/in.h>
#include <iostream>
#include <string>
#include <vector>
#include "ubse_common_def.h"

namespace ubse::mem::controller {
using namespace ubse::common::def;

enum class UbseMemStage : uint32_t {
    UBSE_NOT_EXIST = 0,         // 借用关系不存在
    UBSE_CREATING = 1,          // 正在创建中
    UBSE_DELETING = 2,          // 正在删除中
    UBSE_EXIST = 3,             // 创建成功
    UBSE_ERR_ONLY_IMPORT = 4,   // 只存在借入
    UBSE_ERR_WAIT_UNEXPORT = 5, // 等待unexport执行，对账会执行，可以手动删除
};

struct UbseMemResult {
    std::string name;
    std::string importNodeId;
    uint64_t realSize;
    UbseMemStage stage;
};

enum class UbseMemBorrowType {
    FD_BORROW = 0,
    NUMA_BORROW = 1,
    ADDR_BORROW = 2,
    SHM_BORROW = 3,
    SHM_ATTACH = 4
};

/**
* @brief 查询借用标识对应的操作结果
* @param name [in] 借用标识
* @param borrowType [in] 借用类型
* @param memResult [out] 借用结果
* @return 查询成功返回0，失败返回1
*/
uint32_t UbseQueryResult(const std::string& name, UbseMemResult& result,
                         UbseMemBorrowType borrowType = UbseMemBorrowType::NUMA_BORROW);

static constexpr uint32_t UBSE_MAX_USR_INFO_LEN = 32;
struct UbseNumaMemoryDebtInfo {
    std::string name; // 资源名称标识
    std::string borrowNodeId;
    std::vector<int> borrowSocketIdList;
    uint8_t usrInfo[UBSE_MAX_USR_INFO_LEN]; // 调用方私有数据，UBSE只负责保存，get时原样返回
    std::vector<uint64_t> borrowMemId;
    std::string lentNodeId;
    std::vector<int> lentSocketIdList;
    std::vector<int16_t> lentNumaIdList;
    std::vector<uint64_t> lentNumaSizeList;
    std::vector<uint64_t> lentMemId;
    uint64_t size; // 总借用内存大小（字节）
    int64_t remoteNumaId = -1;
    uid_t uid{0};           // 发起借用方运行用户的uid，后续资源管理权限都由此用户管理
    std::string username{}; // 发起借用方运行用户的名称，后续资源管理权限都由此用户管理
};

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
UbseResult UbseGetNumaMemDebtInfoWithNode(const std::string& nodeId, std::vector<UbseNumaMemoryDebtInfo>& debtInfos);

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
UbseResult UbseGetNumaMemDebtInfo(std::vector<UbseNumaMemoryDebtInfo>& debtInfos);

struct UbseNumaMemoryImportDebtInfo {
    std::string name; // 资源名称标识
    std::string borrowNodeId;
    std::vector<int> borrowSocketIdList;
    uint64_t size;                          // 总借用内存大小（字节）
    uint8_t usrInfo[UBSE_MAX_USR_INFO_LEN]; // 调用方私有数据，UBSE只负责保存，get时原样返回
    int64_t remoteNumaId = -1;
};

/**
* @brief 返回当前节点导入账本信息。
* 本地节点账本已经从OBMM恢复完成则返回成功
* 否则返回部分成功。
* @param debtInfos [out] 借用账本对象集合
* @return #UBSE_OK 成功
* @return #UBSE_ERR_INVALID_ARG 无效参数
* @return #UBSE_PAR_SUCCESS 部分成功
* @return #UBSE_ERR_INTERNAL 内部错误
*/
UbseResult UbseGetNumaMemImportDebtInfoWithLocalNode(std::vector<UbseNumaMemoryImportDebtInfo>& debtInfos);

enum UbseMemDistance {
    MEM_DISTANCE_L0, // L0对应直接CPU连线节点
    MEM_DISTANCE_L1, // L1对应通过1跳节点, 暂不支持
    MEM_DISTANCE_L2  // L2对应过超过1跳节点 , 暂不支持
};

struct UbseMemBorrower {
    std::string nodeId;       // 节点id
    int affinitySocketId{-1}; // 可选
    uid_t uid{0};             // 发起借用方运行用户的uid，后续资源管理权限都由此用户管理
    std::string username{};   // 发起借用方运行用户的名称，后续资源管理权限都由此用户管理
};

struct UbseMemNumaLender {
    uint32_t slotId;   // 节点唯一标识, 采用slotid, 与lcne保持一致
    uint32_t socketId; // socket id
    uint64_t numaId;   // 节点中的numa id
    uint64_t size;     // 借出内存大小, 单位Byte, 取值范围大于等于4*1024*1024
};

struct UbseTopoIpAddress {
    int32_t af;           // 地址族，ipv4为AF_INET，ipv6为AF_INET6
    struct in_addr ipv4;  // ipv4地址
    struct in6_addr ipv6; // IPv6地址
};

struct UbseTopoNode {
    uint32_t slotId;
    std::vector<int16_t> socketIdList; // socketIdList和numaIdList长度一样
    std::vector<int32_t> numaIdList;
    std::vector<UbseTopoIpAddress> ips;
    std::string hostName; // 主机名
};

struct UbseMemNumaDesc {
    std::string name;                       // 借用标识
    int64_t numaId;                         // 形成远端numa对应的numaid
    UbseTopoNode exportNode;                // 借出节点
    UbseTopoNode importNode;                // 借入节点
    uint64_t size;                          // 借用大小
    uint8_t usrInfo[UBSE_MAX_USR_INFO_LEN]; // 调用方私有数据，UBSE只负责保存，get时原样返回
};

struct UbseNodeNumaInfo {
    std::string nodeId;         // 该numa的nodeId
    std::string hostName;       // 主机名
    uint32_t numaId;            // numa id
    uint32_t socketId;          // socket id
    uint64_t mReservedMemRatio; // 预留内存比例
    uint64_t memTotal;          // 总内存量（字节）
    uint64_t memFree;           // 空闲内存量（字节）
    uint64_t nrHugepages;       // 2M大页数量
    uint64_t freeHugepages;     // 2M大页空闲数量
    uint64_t usedHugepages;     // nrHugepages -  freeHugepages
    uint64_t timestamp;         // 数据采集的时间戳（秒级）
    uint64_t memLent;           // 借出内存量（字节）
    uint64_t memShared;         // 共享内存量（字节
};

/**
* @brief 返回所有采集到的节点相关的numa信息。
* @param numaNodeInfoList [out] 节点numa对象集合
* @return UBSE_OK 成功
* @return UBSE_ERR_INTERNAL 获取节点信息失败
*/
UbseResult UbseGetAllNodeNumaInfo(std::vector<UbseNodeNumaInfo>& numaNodeInfoList);

/**
* @brief 返回所有采集到的节点指定节点相关的numa信息。
* @param nodeId [in] 节点Id非空且是有效节点
* @param numaNodeInfoList [out] 节点numa对象集合
* @return UBSE_ERR_INVALID_ARG 传入值非法或指定节点不存在
* @return UBSE_ERR_INTERNAL 获取节点信息失败
*/
UbseResult UbseGetNodeNumaInfoByNodeId(const std::string& nodeId, std::vector<UbseNodeNumaInfo>& numaNodeInfoList);

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
UbseResult UbseMemNumaCreateWithLender(const std::string& name, const UbseMemBorrower& borrower,
                                       const std::vector<UbseMemNumaLender>& lenders,
                                       uint8_t usrInfo[UBSE_MAX_USR_INFO_LEN], UbseMemNumaDesc& desc);

struct UbseMemNumaCreateOpt {
    uint64_t size{0};                          // 借出内存大小, 单位Byte, 取值范围大于等于4*1024*1024
    UbseMemDistance distance{MEM_DISTANCE_L0}; // 当前只支持MEM_DISTANCE_L0
    size_t highWatermark{88}; // 内存使用量百分比，vm自己决策场景，算法决策后尽量保证节点借出后内存使用量不超过此值
    uint8_t usrInfo[UBSE_MAX_USR_INFO_LEN]{0XFF}; // 调用方私有数据，UBSE只负责保存，get时原样返回
};

/**
 * @brief 提供给插件使用numa借用， virAgent透传给超分场景使用
 * @param name [IN] 必填，借用标识, name最大长度48字节, 含结尾字符\0; name仅可包括大小写字母、数字、"."、":"、"-"以及"_"; name在节点内保持唯一性
 * @param borrower [IN] 必填，借用方信息
 * @param param [IN] 必填，借出方信息
 * @param usrInfo [IN] 选填，调用方私有数据，UBSE只负责保存，get时原样返回
 * @param desc [OUT] 借用形成的远端numa信息
 * @return UbseResult
 */
UbseResult UbseMemNumaCreate(const std::string& name, const UbseMemBorrower& borrower, const UbseMemNumaCreateOpt& opt,
                             UbseMemNumaDesc& desc);

struct UbseMemNumaCandidateOpt : UbseMemNumaCreateOpt {
    std::vector<std::string> slotIds; // 候选借出节点范围
};
/**
 * @brief 指定候选借出节点,提供给插件使用numa借用， virAgent透传给超分场景使用
 * @param name [IN] 必填，借用标识, name最大长度48字节, 含结尾字符\0; name仅可包括大小写字母、数字、"."、":"、"-"以及"_"; name在节点内保持唯一性
 * @param borrower [IN] 必填，借用方信息
 * @param opt [IN] 必填，可选项
 * @param desc [OUT] 借用形成的远端numa信息
 * @return UBSE_OK 成功，其他失败;
 */
UbseResult UbseMemNumaCreateWithCandidate(const std::string& name, const UbseMemBorrower& borrower,
                                          const UbseMemNumaCandidateOpt& opt, UbseMemNumaDesc& desc);

/**
 * @brief 删除指定numa远端内存;
 *
 * @param name  [IN] 必填，借用标识, name最大长度48字节, 含结尾字符\0; name仅可包括大小写字母、数字、"."、":"、"-"以及"_"; name在节点内保持唯一性
 * @param borrower  [IN] 必填，借用方信息
 * @return UbseResult
 */
UbseResult UbseMemNumaDelete(const std::string& name, const UbseMemBorrower& borrower);

/* 进程借用，借出进程的地址以及大小 */
struct UbseMemAddrBorrowLocAndSizeByPid {
    uint64_t addr{0}; // 需要借用的进程虚拟地址
    uint64_t size{0}; // 该段地址借入大小
};

struct UbseMemProcessLender {
    uint32_t slotId;                                         // 节点唯一标识, 采用slotid, 与lcne保持一致
    int socketId{-1};                                        /* *内存申请借出方节点socket信息 -1 无效 */
    uint64_t pid{0};                                         // 借出进程PID
    std::vector<UbseMemAddrBorrowLocAndSizeByPid> vaLists{}; // 借用地址段  最大段数=CPU核数
};

struct UbseMemAddrDesc {
    std::string name;            // 借用标识
    int64_t numaId;              // 形成远端numa对应的numaid
    UbseMemProcessLender lender; // 借出节点
    UbseTopoNode importNode;     // 借入节点
    uint64_t size;               // 借用大小
};

#define UBSE_MEM_FLAG_NO_WR_DELAY 0x1 // 非写接力

/**
 * @brief 提供给插件使用addr借用
 *
 * @param name [IN] 必填，借用标识, name最大长度48字节, 含结尾字符\0; name仅可包括大小写字母、数字、"."、":"、"-"以及"_"; name在节点内保持唯一性
 * @param borrower [IN] 必填，借用方信息
 * @param lenders [IN] 必填，借出方信息
 * @param flag [IN] 额外的内存借用属性，1为非接力模式，0为接力模式
 * @param desc [OUT] 借用形成的远端numa信息
 * @return UbseResult
 */
UbseResult UbseMemAddrCreate(const std::string& name, const UbseMemBorrower& borrower,
                             const UbseMemProcessLender& lender, uint32_t flag, UbseMemAddrDesc& desc);

/**
 * @brief 删除addr借用;
 *
 * @param name  [IN] 必填，借用标识, name最大长度48字节, 含结尾字符\0; name仅可包括大小写字母、数字、"."、":"、"-"以及"_"; name在节点内保持唯一性
 * @param borrower  [IN] 必填，借用方信息
 * @return UbseResult;
 */
UbseResult UbseMemAddrDelete(const std::string& name, const UbseMemBorrower& borrower);

/**
 * @brief 检查借用是否成环，srcNodeId是否出现在借出方、dstNodeId是否出现在借入方
 * @param srcNodeId [in] 借入NodeId
 * @param dstNodeId [in] 借出NodeId
 * @param isCircle [out] 是否成环
 * @return //返回码再补充
 */
UbseResult UbseMemDebtCircleCheck(const std::string& srcNodeId, const std::string& dstNodeId, bool& isCircle);

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
UbseResult UbseGetAddrMemDebtInfoWithNode(const std::string& nodeId, std::vector<UbseMemAddrDesc>& debtInfos);
} // namespace ubse::mem::controller

#endif // UBSE_MEM_CONTROLLER_H
