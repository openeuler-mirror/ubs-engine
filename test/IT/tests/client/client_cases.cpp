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

#include "client_cases.h"

#include <gtest/gtest.h>

#include "it_console_log.h"
#include "test_client_init.h"
#include "ubs_engine.h"
#include "ubs_engine_topo.h"
#include "ubs_error.h"

namespace ubse::it::tests::client {

// P0-Init-Ok-01: 默认路径初始化, path=NULL, 返回 UBS_SUCCESS
// 测试环境中 daemon 不在默认路径, TestClientInit 封装:
//   ① 调真实 init(NULL) 测生产代码路径 → 返回 UBS_SUCCESS
//   ② fix-up 到集群真实路径 → 后续 SDK 调用正常
//   ③ 析构自动恢复路径
void RunP0InitOk01(ubse::it::infra::ItCluster& cluster)
{
    ubse::it::infra::TestClientInit client(cluster);

    int32_t ret = client.Initialize(nullptr);
    EXPECT_EQ(ret, UBS_SUCCESS) << "init with NULL path should return UBS_SUCCESS, got " << ret;

    // fix-up 后 SDK 可正常调用（与生产环境行为一致）
    ubs_topo_node_t localNode{};
    int32_t topoRet = ubs_topo_node_local_get(&localNode);
    EXPECT_EQ(topoRet, UBS_SUCCESS) << "SDK call after init(NULL) should succeed";

    IT_LOG_INFO << "P0-Init-Ok-01 passed";
}

// P0-Init-Ok-02: 指定路径初始化, path=生产环境路径, 返回 UBS_SUCCESS
// 用生产路径 /var/run/ubse/ubse.sock 调 init, fix-up 到测试路径后 SDK 可用
void RunP0InitOk02(ubse::it::infra::ItCluster& cluster)
{
    ubse::it::infra::TestClientInit client(cluster);

    int32_t ret = client.Initialize(client.GetProductionPath());
    EXPECT_EQ(ret, UBS_SUCCESS) << "init with production path should return UBS_SUCCESS, got " << ret;

    // fix-up 后 SDK 可正常调用（与生产环境行为一致）
    ubs_topo_node_t localNode{};
    int32_t topoRet = ubs_topo_node_local_get(&localNode);
    EXPECT_EQ(topoRet, UBS_SUCCESS) << "SDK call after init(production path) should succeed";

    IT_LOG_INFO << "P0-Init-Ok-02 passed";
}

// P0-Init-OverLen-01: 路径超长, path 长度 > 107, 返回 OUT_OF_RANGE
// 异常路径 init 失败不修改内部路径, 无需 fix-up
void RunP0InitOverLen01(ubse::it::infra::ItCluster& cluster)
{
    ubse::it::infra::TestClientInit client(cluster);

    // sun_path 最大 108 字节, 含 \0 所以路径最多 107 字符
    std::string longPath(108, 'x');
    int32_t ret = client.Initialize(longPath.c_str());
    EXPECT_EQ(ret, UBS_ENGINE_ERR_OUT_OF_RANGE) << "init with path > 107 chars should return OUT_OF_RANGE, got " << ret;
    IT_LOG_INFO << "P0-Init-OverLen-01 passed";
}

// P0-Init-BadParam-01: 路径不存在, SDK 延迟连接模式, init 只设路径不连,
// 需要调 SDK 接口时才会连接失败
void RunP0InitBadParam01(ubse::it::infra::ItCluster& cluster)
{
    ubse::it::infra::TestClientInit client(cluster);

    const char* fakePath = "/tmp/ubs_engine_it_nonexistent_9999/ubse.sock";
    int32_t ret = client.Initialize(fakePath);
    // SDK 延迟连接, init 只设路径, 不验证存在性, 返回 SUCCESS
    EXPECT_EQ(ret, UBS_SUCCESS) << "init only sets path, should return SUCCESS even for non-existent path";

    // 实际调用 SDK 接口时才会连接失败
    ubs_topo_node_t localNode{};
    int32_t topoRet = ubs_topo_node_local_get(&localNode);
    EXPECT_NE(topoRet, UBS_SUCCESS) << "SDK call with non-existent path should fail, got " << topoRet;
    IT_LOG_INFO << "P0-Init-BadParam-01 passed";
}

// P0-Fin-Ok-01: 正常销毁
// SDK finalize 为空操作(延迟连接模式无持久连接需要销毁),
// 验证调用不崩溃, 且 finalize 后 SDK 仍可正常使用
void RunP0FinOk01(ubse::it::infra::ItCluster& cluster)
{
    ubse::it::infra::TestClientInit client(cluster);

    // finalize 不崩溃
    client.Finalize();
    IT_LOG_INFO << "ubs_engine_client_finalize() called without crash";

    // 延迟连接模式下 finalize 为空操作, 重新 init 后 SDK 仍可正常调用
    client.Initialize(client.GetProductionPath());
    ubs_topo_node_t localNode{};
    int32_t topoRet = ubs_topo_node_local_get(&localNode);
    EXPECT_EQ(topoRet, UBS_SUCCESS) << "SDK should still work after finalize (lazy connection mode)";
    IT_LOG_INFO << "P0-Fin-Ok-01 passed";
}

} // namespace ubse::it::tests::client
