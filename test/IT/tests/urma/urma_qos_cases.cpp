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

void RunQosCreateSinglePriorityTest(ubse::it::infra::ItCluster& cluster)
{
    auto& client = cluster.GetSdkClient("1");

    ubs_urma_qos_config_t configs[1] = {{.priority = 0, .bandwidth = 100}};
    int32_t ret = client.UrmaQosCreate(configs, 1);
    EXPECT_IT_OK(ret);

    // 清理
    ret = client.UrmaQosDelete();
    EXPECT_IT_OK(ret);
}

void RunQosCreateAndQueryTest(ubse::it::infra::ItCluster& cluster)
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

void RunQosCreateQueryDeleteTest(ubse::it::infra::ItCluster& cluster)
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

void RunCliCreateSuccessTest(ubse::it::infra::ItCluster& cluster)
{
    auto& client = cluster.GetSdkClient("1");
    std::string output = client.ExecCli("create urma-qos --pri 0 --cir 100");
    EXPECT_NE(output.find("create successfully"), std::string::npos);

    int32_t ret = client.UrmaQosDelete();
    EXPECT_IT_OK(ret);
}

void RunCliCreateDualPriTest(ubse::it::infra::ItCluster& cluster)
{
    auto& client = cluster.GetSdkClient("1");
    std::string output = client.ExecCli("create urma-qos --pri 0,1 --cir 100,50");
    EXPECT_NE(output.find("create successfully"), std::string::npos);

    int32_t ret = client.UrmaQosDelete();
    EXPECT_IT_OK(ret);
}

void RunCliCreateMissingParamsTest(ubse::it::infra::ItCluster& cluster)
{
    auto& client = cluster.GetSdkClient("1");
    std::string output = client.ExecCli("create urma-qos");
    EXPECT_NE(output.find("ERROR"), std::string::npos);
    EXPECT_NE(output.find("--pri"), std::string::npos);
}

void RunCliCreateInvalidPriTest(ubse::it::infra::ItCluster& cluster)
{
    auto& client = cluster.GetSdkClient("1");
    std::string output = client.ExecCli("create urma-qos --pri 2 --cir 100");
    EXPECT_NE(output.find("ERROR"), std::string::npos);
    EXPECT_NE(output.find("Priority must be 0 or 1"), std::string::npos);
}

void RunCliCreateDuplicatePriTest(ubse::it::infra::ItCluster& cluster)
{
    auto& client = cluster.GetSdkClient("1");
    std::string output = client.ExecCli("create urma-qos --pri 0,0 --cir 100,50");
    EXPECT_NE(output.find("ERROR"), std::string::npos);
    EXPECT_NE(output.find("Duplicate"), std::string::npos);
}

void RunCliCreateCountExceedTest(ubse::it::infra::ItCluster& cluster)
{
    auto& client = cluster.GetSdkClient("1");
    std::string output = client.ExecCli("create urma-qos --pri 0,1,2 --cir 100,50,30");
    EXPECT_NE(output.find("ERROR"), std::string::npos);
    EXPECT_NE(output.find("count exceeds limit"), std::string::npos);
}

void RunCliCreateMismatchTest(ubse::it::infra::ItCluster& cluster)
{
    auto& client = cluster.GetSdkClient("1");
    std::string output = client.ExecCli("create urma-qos --pri 0,1 --cir 100");
    EXPECT_NE(output.find("ERROR"), std::string::npos);
    EXPECT_NE(output.find("count mismatch"), std::string::npos);
}

void RunCliCreateZeroBandwidthTest(ubse::it::infra::ItCluster& cluster)
{
    auto& client = cluster.GetSdkClient("1");
    std::string output = client.ExecCli("create urma-qos --pri 0 --cir 0");
    EXPECT_NE(output.find("ERROR"), std::string::npos);
}

void RunCliDeleteSuccessTest(ubse::it::infra::ItCluster& cluster)
{
    auto& client = cluster.GetSdkClient("1");
    ubs_urma_qos_config_t configs[1] = {{.priority = 0, .bandwidth = 100}};
    int32_t ret = client.UrmaQosCreate(configs, 1);
    EXPECT_IT_OK(ret);
    if (ret != UBS_SUCCESS) {
        return;
    }

    std::string output = client.ExecCli("delete urma-qos");
    EXPECT_NE(output.find("delete successfully"), std::string::npos);
}

void RunCliDisplayEmptyTest(ubse::it::infra::ItCluster& cluster)
{
    auto& client = cluster.GetSdkClient("1");
    std::string output = client.ExecCli("display urma-qos");
    EXPECT_NE(output.find("No ETS QoS priority groups"), std::string::npos);
}

void RunCliFullLifecycleTest(ubse::it::infra::ItCluster& cluster)
{
    auto& client = cluster.GetSdkClient("1");

    ubs_urma_qos_config_t lifecycleConfigs[1] = {{.priority = 0, .bandwidth = 100}};
    int32_t ret = client.UrmaQosCreate(lifecycleConfigs, 1);
    EXPECT_IT_OK(ret);
    if (ret != UBS_SUCCESS) {
        return;
    }

    std::string output = client.ExecCli("display urma-qos");
    EXPECT_NE(output.find("priority"), std::string::npos);
    EXPECT_NE(output.find("bandwidth(Gbps)"), std::string::npos);
    EXPECT_NE(output.find("0"), std::string::npos);
    EXPECT_NE(output.find("100"), std::string::npos);

    output = client.ExecCli("delete urma-qos");
    EXPECT_NE(output.find("delete successfully"), std::string::npos);

    output = client.ExecCli("display urma-qos");
    EXPECT_NE(output.find("No ETS QoS priority groups"), std::string::npos);
}

} // namespace ubse::it::tests::urma_qos
