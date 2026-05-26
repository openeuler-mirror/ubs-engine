/*
* Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_MTI_INTERFACE_H
#define UBSE_MTI_INTERFACE_H
#include <vector>

#include "ubse_common_def.h"
#include "ubse_mti_def.h"
#include "ubse_mti_mami_def.h"

namespace ubse::adapter_plugins::mti {
class UbseMtiInterface {
public:
    virtual ~UbseMtiInterface() = default;

    static UbseMtiInterface& GetInstance();

    /**
     * @brief 获取当前节点LCNE感知的节点信息
     * @param nodeInfo 当前节点LCNE感知的节点信息
     * @return UBSE_OK 标识成功
     * @return UBSE_ERROR_MODULE_LOAD_FAILED mti模块未加载
     * @return UBSE_ERROR 表示失败
     */
    virtual common::def::UbseResult GetLocalNodeInfo(UbseMtiNodeInfo& nodeInfo) = 0;

    /**
    * @brief 获取集群内所有节点对应LCNE感知的节点信息
    * @param nodeInfoList 集群内所有节点对应LCNE感知的节点信息
    * @return UBSE_OK 标识成功
    * @return UBSE_ERROR_MODULE_LOAD_FAILED mti模块未加载
    * @return UBSE_ERROR 表示失败
    */
    virtual common::def::UbseResult GetClusterNodeInfoList(std::vector<UbseMtiNodeInfo>& nodeInfoList) = 0;

    /**
    * @brief 集群基于Tcp进行通信时，当前节点ip
    * @param localIp
    * @return UBSE_OK 标识成功
    * @return UBSE_ERROR_MODULE_LOAD_FAILED mti模块未加载
    * @return UBSE_ERROR 表示失败
    */
    virtual common::def::UbseResult GetLocalIp(std::string& localIp) = 0;

    /**
    * @brief 集群基于Tcp进行通信时，返回配置文件中配置的集群所有节点Ip列表
    * @param ipList
    * @return UBSE_OK 标识成功
    * @return UBSE_ERROR_MODULE_LOAD_FAILED mti模块未加载
    * @return UBSE_ERROR 表示失败
    */
    virtual common::def::UbseResult GetClusterIpList(std::vector<std::string>& ipList) = 0;

    /**
    * @brief 获取节点CPU拓扑信息
    * @param topo
    * @return UBSE_OK 标识成功
    * @return UBSE_ERROR_MODULE_LOAD_FAILED mti模块未加载
    * @return UBSE_ERROR 表示失败
    */
    virtual common::def::UbseResult GetClusterCpuTopo(UbseMtiCpuTopoInfoMap& topo) = 0;

    /**
     * 获取全量规划的urma通信EID（物理意义）
     * @param socketInfoMap 全量规划的urma通信EID key为devName: nodeId+socketId 值为当前设备的UbseLcneSocketInfo
     * @return UBSE_OK 标识成功
     * @return UBSE_ERROR_MODULE_LOAD_FAILED mti模块未加载
     * @return UBSE_ERROR 表示失败
     */
    virtual common::def::UbseResult GetAllSocketComEid(std::map<UbseDevName, UbseMtiEidGroup>& socketInfoMap) = 0;
    /**
     * 增加Decoder表项
     * @param importInfo decoder表项内容
     * @param importResult decoder 表项操作结果
     * @param trustRingData decoder 信任环数据
     * @return UBSE_OK 标识成功
     * @return UBSE_ERROR_MODULE_LOAD_FAILED mti模块未加载
     * @return UBSE_ERROR 表示失败
     */
    virtual common::def::UbseResult AddDecoderEntry(
        const mami::UbseMamiMemImportInfo& importInfo, mami::UbseMamiMemImportResult& importResult,
        const ubse::adapter_plugins::mti::UbseDecoderTrustRingData& trustRingData = {}) = 0;

    /**
     * 删除表项信息
     * @param drawInfo 待删除表项信息
     * @return UBSE_OK 标识成功
     * @return UBSE_ERROR_MODULE_LOAD_FAILED mti模块未加载
     * @return UBSE_ERROR 表示失败
     */
    virtual common::def::UbseResult DeleteDecoderEntry(const mami::UbseMamiMemWithdraw& drawInfo) = 0;

    /**
     * 无效表项信息
     * @param drawInfo 待无效表项信息
     * @return UBSE_OK 标识成功
     * @return UBSE_ERROR_MODULE_LOAD_FAILED mti模块未加载
     * @return UBSE_ERROR 表示失败
     */
    virtual common::def::UbseResult InvalidateDecoderEntry(const mami::UbseMamiMemWithdraw& drawInfo) = 0;

    /**
     * 查询全部handle
     * @param queryInfo 查询参数
     * @param handleValues 查询结果
     * @return UBSE_OK 标识成功
     * @return UBSE_ERROR_MODULE_LOAD_FAILED mti模块未加载
     * @return UBSE_ERROR 表示失败
     */
    virtual common::def::UbseResult GetAllMemHandles(const mami::UbseMamiMemHandleQueryInfo& queryInfo,
                                                     std::vector<mami::UbseMamiMemHandleValue>& handleValues) = 0;

    /**
     * 获取LCNE感知的VfeEid信息
     * @param [in] iouInfo, 本节点的slot/ubpu/iou信息
     * @param [out] allFeInfos: 本节点的Vfe及对应的Eid信息
     * @return 成功返回0, 失败返回非0
     */
    virtual common::def::UbseResult UbseGetFeEid(UbseMtiIouInfo iouInfo, std::vector<UbseMtiFeInfo> &allFeInfos)  = 0;

    /**
     * @brief 下发xml消息到Lcne上创建QosProfile
     * @param [in] ubseLcneQosProfile：待创建profile信息
     * @return 成功返回0, 失败返回非0
     */
    virtual common::def::UbseResult UbseCreateQosProfile(UbseMtiQosProfile ubseLcneQosProfile) = 0;

    /**
     * @brief 下发xml消息到Lcne上删除QosProfile
     * @param [in] proflieName：待删除profile名称
     * @return 成功返回0, 失败返回非0
     */
    virtual common::def::UbseResult UbseDeleteQosProfile(std::string proflieName) = 0;

    /**
     * @brief 下发xml消息到Lcne上查询QosProfile的具体参数
     * @param [in] proflieName：待查询profile名称
     * @param [out] ubseLcneQosProfile：查询到的profile信息
     * @return 成功返回0, 失败返回非0
     */
    virtual common::def::UbseResult UbseQueryQosProfile(std::string proflieName,
                                                        UbseMtiQosProfile& ubseLcneQosProfile) = 0;

    /**
     * @brief 下发xml消息到Lcne上使能应用QosProfile
     * @param [in] ubseFeInfo：待生效的Vfe信息
     * @param [in] proflieName：待生效profile名称
     * @return 成功返回0, 失败返回非0
     */
    virtual common::def::UbseResult UbseApplyVfeQos(UbseMtiFeInfo ubseFeInfo, std::string proflieName) = 0;

    /**
     * @brief 下发xml消息到Lcne上删除Vfe上的QosProfile应用
     * @param [in] ubseFeInfo：待删除的Vfe
     * @return 成功返回0, 失败返回非0
     */
    virtual common::def::UbseResult UbseDeleteVfeQos(UbseMtiFeInfo ubseFeInfo) = 0;

    /**
     * @brief 下发xml消息到Lcne上查询Vfe上的QosProfile应用
     * @param [in] ubseFeInfo：待查询的Vfe
     * @param [out] proflieName：查询到的profile名称
     * @return 成功返回0, 失败返回非0
     */
    virtual common::def::UbseResult UbseQueryVfeQos(UbseMtiFeInfo ubseFeInfo, std::string& proflieName) = 0;
    virtual common::def::UbseResult UbseQueryVfeQos(UbseMtiFeInfo ubseFeInfo, std::string &proflieName) = 0;

    /** @brief 创建ETS模板，支持携带VL和优先级组配置 */
    virtual common::def::UbseResult UbseCreateEtsProfile(const UbseMtiEtsProfile &etsProfile) = 0;

    /** @brief 增量新增ETS模板VL配置 */
    virtual common::def::UbseResult UbseAddEtsProfileVls(const std::string &profileName,
                                                         const std::vector<UbseEtsVl> &vls) = 0;

    /** @brief 增量新增ETS模板优先级组配置 */
    virtual common::def::UbseResult UbseAddEtsProfilePriorityGroups(
        const std::string &profileName, const std::vector<UbseEtsPriorityGroup> &priorityGroups) = 0;

    /** @brief 删除ETS模板 */
    virtual common::def::UbseResult UbseDeleteEtsProfile(const std::string &profileName) = 0;

    /** @brief 删除ETS模板VL配置 */
    virtual common::def::UbseResult UbseRemoveEtsProfileVls(const std::string &profileName) = 0;

    /** @brief 删除ETS模板优先级组配置 */
    virtual common::def::UbseResult UbseRemoveEtsProfilePriorityGroups(const std::string &profileName) = 0;

    /**
     * @brief 查询ETS模板
     * @param [in] profileName：待查询ETS模板名称
     * @param [out] etsProfile：查询到的ETS模板配置
     * @return 成功返回0, 模板不存在返回UBSE_MTI_ERROR_NOT_EXIST, 其他失败返回非0
     */
    virtual common::def::UbseResult UbseQueryEtsProfile(const std::string &profileName,
                                                        UbseMtiEtsProfile &etsProfile) = 0;
};
} // namespace ubse::adapter_plugins::mti
#endif // UBSE_MTI_INTERFACE_H
