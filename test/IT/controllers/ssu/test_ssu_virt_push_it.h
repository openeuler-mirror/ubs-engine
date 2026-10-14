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

#ifndef TEST_SSU_VIRT_PUSH_IT_H
#define TEST_SSU_VIRT_PUSH_IT_H

// SSU 模块虚机化特性 IT:vsock 端到端推送链路(mock ub-device-manager)
// 被测链路:mock ub-device-manager 客户端(真实 AF_VSOCK socket + 线路协议)
//          → UbseVsockServer(真实监听/解包/线程池分发)
//          → UbseSsuControllerModule 推送 handler(真实反序列化)
//          → UbseSsuVmCache(真实缓存/持久化)
//          → UbseSsuServiceImp VM 模式查询/attach/detach(真实业务分支)
// 硬件依赖(VM 内本地适配器 libssu.so)以函数指针 mock 替代,与 UT 同一约定。
// 环境要求:AF_VSOCK 回环(vsock_loopback 内核模块);不可用时用例整体 GTEST_SKIP。

#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "ubse_ssu_controller_module.h"

namespace ubse::ssu::virt::it {

using ubse::plugin::service::ssu::UbseSsuSpaceReq;
using ubse::ssu::controller::UbseSsuControllerModule;

class TestSsuVirtPush : public testing::Test {
public:
    TestSsuVirtPush() = default;

    void SetUp() override;
    void TearDown() override;

protected:
    // VM 模式身份(与适配器 mock customData 默认值对齐)
    static UbseSsuSpaceReq MakeSpaceReq(const std::string &name);

    // 进程内组装的 SSU 控制器模块(私有成员经 -fno-access-control 注入,复用真实 handler)
    std::unique_ptr<UbseSsuControllerModule> module_;
    // 实际监听端口(SetUp 端口探测后确定)
    uint32_t vsockPort_{0};
    // 缓存文件临时路径(进程级隔离)
    std::string cacheFilePath_;
};

} // namespace ubse::ssu::virt::it

#endif // TEST_SSU_VIRT_PUSH_IT_H
