/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_URMA_CONTROLLER_QOS_H
#define UBSE_URMA_CONTROLLER_QOS_H

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>
#include "ubse_common_def.h"
#include "ubse_mti_def.h"
#include "ubse_serial_util.h"

namespace ubse::urmaController {
const std::string ETS_QOS_PROFILE_NAME = "ETS_QOS_PROFILE";

enum class EtsQosProfileState
{
    ETS_PROFILE_NOT_CREATED = 0,
    ETS_PROFILE_NOT_APPLIED = 1,
    ETS_PROFILE_APPLIED = 2,
};

enum class EtsPriority
{
    PRI_0 = 0,
    PRI_1 = 1,
    BUTT
};

struct EtsQosConfig {
    EtsPriority priority;
    uint32_t bandwidth; // 单位为Mbps

    friend ubse::serial::UbseSerialization& operator<<(ubse::serial::UbseSerialization& serializer,
                                                       const EtsQosConfig& config)
    {
        serializer << ubse::serial::enum_v(config.priority) << config.bandwidth;
        return serializer;
    }

    friend ubse::serial::UbseDeSerialization& operator>>(ubse::serial::UbseDeSerialization& deserializer,
                                                         EtsQosConfig& config)
    {
        deserializer >> ubse::serial::enum_v(config.priority) >> config.bandwidth;
        return deserializer;
    }
};
/*
 * @brief QosTemplateBase<Config>：QoS模板基类（模板式）
 *
 *  以Config为模板参数，不同QoS方案传入不同的配置结构体。
 *  新增方案时只需：
 *    1. 定义配置结构体
 *    2. 继承 QosTemplateBase<新Config> 实现 Create/Delete/Init/GetInitResult
 *    3. 特化 CreateTemplate<新Config>() 工厂函数
 */
template <typename Config>
class QosTemplateBase {
public:
    QosTemplateBase() = default;
    virtual ~QosTemplateBase() = default;

    QosTemplateBase(const QosTemplateBase&) = delete;
    QosTemplateBase& operator=(const QosTemplateBase&) = delete;
    QosTemplateBase(QosTemplateBase&&) = delete;
    QosTemplateBase& operator=(QosTemplateBase&&) = delete;

    virtual common::def::UbseResult Create(const std::vector<Config>& configs) = 0;
    virtual common::def::UbseResult Delete() = 0;
    /*
     * @brief Init：Urma controller 模块启动时，调用初始化QoS配置
     *
     *  模板初始化，确保在Create前调用。
     */
    virtual common::def::UbseResult Init() = 0;
    virtual common::def::UbseResult Query(std::vector<Config>& configs) = 0;
};

/*
 * @brief EtsTemplate：ETS模板实现
 */
class EtsTemplate : public QosTemplateBase<EtsQosConfig> {
public:
    common::def::UbseResult Create(const std::vector<EtsQosConfig>& configs) override;
    common::def::UbseResult Delete() override;
    /*
     * @brief Init：Urma controller 模块启动时，调用初始化QoS配置
     *
     *  @detail 1. 如果ETS模板已应用，更新state_为 ETS_PROFILE_NOT_APPLIED
     *          2. 如果模板已创建，未应用，更新state_为 ETS_PROFILE_NOT_CREATED，应用模板
     *          3. 如果模板未创建，调用接口创建、应用默认ETS模板
     */
    common::def::UbseResult Init() override;
    common::def::UbseResult Query(std::vector<EtsQosConfig>& configs) override;

private:
    common::def::UbseResult TryCreate(const std::vector<EtsQosConfig>& configs);
    common::def::UbseResult TryDelete();
    /**
     * @brief 将 ETS QoS 模板初始化到所有网口，使其具备带宽调度能力。
     *
     * 最终效果：
     * 1. ETS 模板（ETS_QOS_PROFILE_NAME）已创建（不存在则新建）；
     * 2. 该模板已应用到本节点所有 UB 网口；
     * 3. 模板状态置为 ETS_PROFILE_APPLIED。
     *
     * 如果所有网口已经应用了该模板，本函数为幂等操作，直接返回 UBSE_OK。
     *
     * @return UBSE_OK                        所有网口均已应用 ETS 模板
     * @return UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED  MTI 接口访问失败
     */
    common::def::UbseResult InitInner();
    common::def::UbseResult ValidateAndPrepare(const std::vector<EtsQosConfig>& configs);
    common::def::UbseResult ValidateConfig(const std::vector<EtsQosConfig>& configs);
    void SetEtsProfileState(EtsQosProfileState state);
    common::def::UbseResult CreateEtsProfileIfNotExist();
    void ClassifyAppliedEtsInterfaces(
        const std::vector<adapter_plugins::mti::UbseMtiInterfaceEtsApplication>& appliedEtsInterfaces,
        std::set<std::string>& allAppliedInterfacesName, std::set<std::string>& targetEtsAppliedInterfacesName);
    common::def::UbseResult ApplyToRemainingPorts(const std::set<std::string>& allUbPorts,
                                                  const std::set<std::string>& allAppliedInterfaceNames,
                                                  const std::set<std::string>& targetEtsAppliedInterfaces);
    common::def::UbseResult GetAllUbInterfaceNameFromMti(std::set<std::string>& allUbInterfaceNames);

    std::mutex mutex_;
    EtsQosProfileState state_{EtsQosProfileState::ETS_PROFILE_NOT_CREATED};
};

/*
 * @brief CreateTemplate<Config>：模板工厂函数
 *
 *  模板特化实现各方案的具体实例创建。
 */
template <typename Config>
std::unique_ptr<QosTemplateBase<Config>> CreateTemplate()
{
    return nullptr;
}

template <>
std::unique_ptr<QosTemplateBase<EtsQosConfig>> CreateTemplate<EtsQosConfig>();

/*----------------------------------------------------------------------------*
 *  UbseUrmaControllerQos<Config>：QoS控制器（单例 + 模板式）
 *
 *  单例 + 模板，以Config为模板参数，不同QoS方案传入不同的配置结构体。
 *  使用方式：
 *    auto &etsController = UbseUrmaControllerQos<EtsQosConfig>::GetInstance();
 *    etsController.UbseUrmaQosCreateGroups({ {0, 100}, {1, 100} });
 *    etsController.UbseUrmaQosDeleteGroups();
 *----------------------------------------------------------------------------*/
template <typename Config>
class UbseUrmaControllerQos {
public:
    static UbseUrmaControllerQos& GetInstance()
    {
        static UbseUrmaControllerQos instance;
        return instance;
    }

    ~UbseUrmaControllerQos() = default;
    UbseUrmaControllerQos(const UbseUrmaControllerQos&) = delete;
    UbseUrmaControllerQos& operator=(const UbseUrmaControllerQos&) = delete;

    common::def::UbseResult UbseUrmaQosInit();
    common::def::UbseResult UbseUrmaQosCreate(const std::vector<Config>& configs);
    common::def::UbseResult UbseUrmaQosDelete();
    common::def::UbseResult UbseUrmaQosQuery(std::vector<Config>& configs);

private:
    UbseUrmaControllerQos();
    std::unique_ptr<QosTemplateBase<Config>> qosTemplate_;
};

} // namespace ubse::urmaController
#endif // UBSE_URMA_CONTROLLER_QOS_H
