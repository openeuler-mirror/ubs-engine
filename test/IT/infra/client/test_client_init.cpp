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

#include "test_client_init.h"

#include "it_cluster.h"
#include "ubs_engine.h"
#include "ubs_error.h"

namespace ubse::it::infra {

static constexpr const char* PRODUCTION_UDS_PATH = "/var/run/ubse/ubse.sock";

TestClientInit::TestClientInit(ItCluster& cluster) : clusterUdsPath_(cluster.GetSdkClient("1").GetUdsPath()) {}

TestClientInit::~TestClientInit()
{
    // 恢复测试集群路径, 确保不影响后续测试
    ubs_engine_client_initialize(clusterUdsPath_.c_str());
}

const char* TestClientInit::GetProductionPath()
{
    return PRODUCTION_UDS_PATH;
}

int32_t TestClientInit::Initialize(const char* path)
{
    // 判断是否为生产路径: nullptr 或 /var/run/ubse/ubse.sock
    bool isProductionPath = (path == nullptr) || (path != nullptr && std::string(path) == PRODUCTION_UDS_PATH);

    if (!isProductionPath) {
        // 非生产路径(超长、假路径等): 直接透传给 C API
        return ubs_engine_client_initialize(path);
    }

    // 生产路径: 先调真实 init 测生产代码, 再 fix-up 到测试集群路径
    int32_t ret = ubs_engine_client_initialize(path);
    if (ret != UBS_SUCCESS) {
        return ret;
    }

    // fix-up: 切换到测试集群的实际 socket,
    // 使后续 SDK 调用正常(模拟生产环境中该路径有 daemon 运行)
    ubs_engine_client_initialize(clusterUdsPath_.c_str());

    return ret;
}

void TestClientInit::Finalize()
{
    ubs_engine_client_finalize();
}

} // namespace ubse::it::infra
