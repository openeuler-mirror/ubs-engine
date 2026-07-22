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

#include "urma_qos_cases.h"

#include <cstdlib>

#include <gtest/gtest.h>

#include "ubse_common_def.h"
#include "it_assertion.h"
#include "it_console_log.h"
#include "ubs_engine_urma.h"

namespace ubse::it::tests::urma_qos {

void RunP0UrmaQosCreateOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& client = cluster.GetSdkClient("1");

    ubs_urma_qos_config_t configs[1] = {{.priority = 0, .bandwidth = 100}};
    int32_t ret = client.UrmaQosCreate(configs, 1);
    EXPECT_IT_OK(ret);

    // 清理
    ret = client.UrmaQosDelete();
    EXPECT_IT_OK(ret);
}

void RunP1UrmaQosCreateGetMatch01(ubse::it::infra::ItCluster& cluster)
{
    auto& client = cluster.GetSdkClient("1");

    ubs_urma_qos_config_t configs[2] = {{.priority = 0, .bandwidth = 100}, {.priority = 1, .bandwidth = 50}};
    int32_t ret = client.UrmaQosCreate(configs, 2);
    EXPECT_IT_OK(ret);

    ubs_urma_qos_config_t* queriedConfigs = nullptr;
    uint32_t queriedCount = 0;
    ret = client.UrmaQosGet(&queriedConfigs, &queriedCount);
    EXPECT_IT_OK(ret);
    EXPECT_EQ(queriedCount, 2U);
    if (queriedConfigs != nullptr) {
        EXPECT_EQ(queriedConfigs[0].priority, 0U);
        EXPECT_EQ(queriedConfigs[0].bandwidth, 100U);
        EXPECT_EQ(queriedConfigs[1].priority, 1U);
        EXPECT_EQ(queriedConfigs[1].bandwidth, 50U);
        std::free(queriedConfigs);
    }

    ret = client.UrmaQosDelete();
    EXPECT_IT_OK(ret);
}

void RunP1UrmaQosLifecycle01(ubse::it::infra::ItCluster& cluster)
{
    auto& client = cluster.GetSdkClient("1");

    // 创建QoS配置
    ubs_urma_qos_config_t configs[1] = {{.priority = 0, .bandwidth = 100}};
    int32_t ret = client.UrmaQosCreate(configs, 1);
    EXPECT_IT_OK(ret);

    // 查询验证
    ubs_urma_qos_config_t* queriedConfigs = nullptr;
    uint32_t queriedCount = 0;
    ret = client.UrmaQosGet(&queriedConfigs, &queriedCount);
    EXPECT_IT_OK(ret);
    EXPECT_EQ(queriedCount, 1U);
    if (queriedConfigs != nullptr) {
        EXPECT_EQ(queriedConfigs[0].priority, 0U);
        EXPECT_EQ(queriedConfigs[0].bandwidth, 100U);
        std::free(queriedConfigs);
    }

    // 删除
    ret = client.UrmaQosDelete();
    EXPECT_IT_OK(ret);

    // 删除后查询应返回模板不存在
    ret = client.UrmaQosGet(&queriedConfigs, &queriedCount);
    EXPECT_IT_FAIL(ret);
}

// ==================== CLI测试 ====================

void RunP0CliCreateQosOk01(ubse::it::infra::ItCluster& cluster)
{
    std::string output = cluster.GetCliInvoker("1").ExecCli("create urma-qos --pri 0 --cir 100");
    EXPECT_NE(output.find("create successfully"), std::string::npos);

    auto& client = cluster.GetSdkClient("1");
    int32_t ret = client.UrmaQosDelete();
    EXPECT_IT_OK(ret);
}

void RunP1CliCreateQosParamVariant01(ubse::it::infra::ItCluster& cluster)
{
    std::string output = cluster.GetCliInvoker("1").ExecCli("create urma-qos --pri 0,1 --cir 100,50");
    EXPECT_NE(output.find("create successfully"), std::string::npos);

    auto& client = cluster.GetSdkClient("1");
    int32_t ret = client.UrmaQosDelete();
    EXPECT_IT_OK(ret);
}

void RunP0CliCreateQosBadParam01(ubse::it::infra::ItCluster& cluster)
{
    std::string output = cluster.GetCliInvoker("1").ExecCli("create urma-qos");
    EXPECT_NE(output.find("ERROR"), std::string::npos);
    EXPECT_NE(output.find("--pri"), std::string::npos);
}

void RunP0CliCreateQosInvalidVal01(ubse::it::infra::ItCluster& cluster)
{
    std::string output = cluster.GetCliInvoker("1").ExecCli("create urma-qos --pri 2 --cir 100");
    EXPECT_NE(output.find("ERROR"), std::string::npos);
    EXPECT_NE(output.find("Priority must be 0 or 1"), std::string::npos);
}

void RunP0CliCreateQosDup01(ubse::it::infra::ItCluster& cluster)
{
    std::string output = cluster.GetCliInvoker("1").ExecCli("create urma-qos --pri 0,0 --cir 100,50");
    EXPECT_NE(output.find("ERROR"), std::string::npos);
    EXPECT_NE(output.find("Duplicate"), std::string::npos);
}

void RunP0CliCreateQosOverCnt01(ubse::it::infra::ItCluster& cluster)
{
    std::string output = cluster.GetCliInvoker("1").ExecCli("create urma-qos --pri 0,1,2 --cir 100,50,30");
    EXPECT_NE(output.find("ERROR"), std::string::npos);
    EXPECT_NE(output.find("count exceeds limit"), std::string::npos);
}

void RunP0CliCreateQosBadParam02(ubse::it::infra::ItCluster& cluster)
{
    std::string output = cluster.GetCliInvoker("1").ExecCli("create urma-qos --pri 0,1 --cir 100");
    EXPECT_NE(output.find("ERROR"), std::string::npos);
    EXPECT_NE(output.find("count mismatch"), std::string::npos);
}

void RunP2CliCreateQosInvalidVal01(ubse::it::infra::ItCluster& cluster)
{
    std::string output = cluster.GetCliInvoker("1").ExecCli("create urma-qos --pri 0 --cir 0");
    EXPECT_NE(output.find("ERROR"), std::string::npos);
}

void RunP1CliDelQosOk01(ubse::it::infra::ItCluster& cluster)
{
    auto& client = cluster.GetSdkClient("1");
    ubs_urma_qos_config_t configs[1] = {{.priority = 0, .bandwidth = 100}};
    int32_t ret = client.UrmaQosCreate(configs, 1);
    EXPECT_IT_OK(ret);
    if (ret != UBS_SUCCESS) {
        return;
    }

    std::string output = cluster.GetCliInvoker("1").ExecCli("delete urma-qos");
    EXPECT_NE(output.find("delete successfully"), std::string::npos);
}

void RunP0CliDisplayQosOk01(ubse::it::infra::ItCluster& cluster)
{
    std::string output = cluster.GetCliInvoker("1").ExecCli("display urma-qos");
    EXPECT_NE(output.find("No ETS QoS priority groups"), std::string::npos);
}

void RunP1CliDisplayQosCrossConsist01(ubse::it::infra::ItCluster& cluster)
{
    auto& client = cluster.GetSdkClient("1");

    ubs_urma_qos_config_t lifecycleConfigs[1] = {{.priority = 0, .bandwidth = 100}};
    int32_t ret = client.UrmaQosCreate(lifecycleConfigs, 1);
    EXPECT_IT_OK(ret);
    if (ret != UBS_SUCCESS) {
        return;
    }

    std::string output = cluster.GetCliInvoker("1").ExecCli("display urma-qos");
    EXPECT_NE(output.find("priority"), std::string::npos);
    EXPECT_NE(output.find("bandwidth(Gbps)"), std::string::npos);
    EXPECT_NE(output.find("0"), std::string::npos);
    EXPECT_NE(output.find("100"), std::string::npos);

    output = cluster.GetCliInvoker("1").ExecCli("delete urma-qos");
    EXPECT_NE(output.find("delete successfully"), std::string::npos);

    output = cluster.GetCliInvoker("1").ExecCli("display urma-qos");
    EXPECT_NE(output.find("No ETS QoS priority groups"), std::string::npos);
}

void RunP0CliDelQosNotReady01(ubse::it::infra::ItCluster& cluster)
{
    auto& cli = cluster.GetCliInvoker("1");
    // CLI delete urma-qos 对未创建场景是幂等的，直接返回成功
    std::string output = cli.ExecCli("delete urma-qos");
    bool isSuccess = !output.empty() &&
                     (output.find("success") != std::string::npos || output.find("Success") != std::string::npos);
    EXPECT_TRUE(isSuccess) << "delete urma-qos should succeed (idempotent), got: '" << output << "'";
    IT_LOG_INFO << "P0-CliDelQos-NotReady-01 passed";
}

} // namespace ubse::it::tests::urma_qos
