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

#ifndef IT_TEST_CLIENT_INIT_H
#define IT_TEST_CLIENT_INIT_H

#include <cstdint>
#include <string>

namespace ubse::it::infra {

class ItCluster;

/**
 * @brief 测试环境下的 Client 初始化封装
 *
 * 封装 ubs_engine_client_initialize / ubs_engine_client_finalize,
 * 使测试环境中 init 的行为与生产环境一致:
 *   - 生产路径 (nullptr 或 /var/run/ubse/ubse.sock): 先调真实 init 测生产代码, 再 fix-up 到测试集群路径
 *   - 非生产路径 (超长、假路径等): 直接透传给 C API, 不做 fix-up
 *
 * 析构时自动恢复测试集群路径, 保证后续用例不受影响。
 */
class TestClientInit {
public:
    explicit TestClientInit(ItCluster& cluster);
    ~TestClientInit();

    /**
     * @brief 初始化客户端
     * @param path UDS 路径, nullptr 表示使用默认路径(生产行为)
     *   - 生产路径: 先调真实 init, 再 fix-up 到测试集群路径
     *   - 非生产路径: 直接透传给 C API
     * @return ubs_engine_client_initialize 的返回值
     */
    int32_t Initialize(const char* path = nullptr);

    /** @brief 获取生产环境默认 UDS 路径 */
    static const char* GetProductionPath();

    /**
     * @brief 销毁客户端
     */
    void Finalize();

private:
    std::string clusterUdsPath_;
};

} // namespace ubse::it::infra

#endif // IT_TEST_CLIENT_INIT_H
