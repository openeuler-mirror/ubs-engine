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

#ifndef SSU_VIRT_IT_MOCK_ADAPTER_H
#define SSU_VIRT_IT_MOCK_ADAPTER_H

// SSU 虚机化 IT 适配器 mock(替代真实 libssu.so 硬件依赖)
// 与 test/UT/controllers/ssu/test_ubse_ssu_service_imp_fixture 的 mock 约定保持一致:
// - 通过注入 UbseSsuAdapterImpl 函数指针绕过 dlopen
// - acquireDevInfo mock 按 eid 返回 g_itAcquireNsList 中的 namespace,
//   使 VM 模式查询 identity 校验(本地适配器 customData 为权威源)可通过

#include <atomic>
#include <string>
#include <vector>

#include "plugin_services/ssu/ubse_ssu_service.h"
#include "ubse_ssu_adapter_interface.h"

namespace ubse::ssu::virt::it {

using ubse::plugin::service::ssu::UbseSsuAllocResult;
using ubse::plugin::service::ssu::UbseSsuConnectInfo;
using ubse::plugin::service::ssu::UbseSsuNameSpaceInfo;
using ubse::adapter_plugins::ssu::def::UbseSsuDevNameSpace;

// attach/detach 调用计数(验证 VM 模式 attach/detach 链路真实触达适配器)
extern std::atomic<int> g_itAttachCallCount;
extern std::atomic<int> g_itDetachCallCount;

// ControllableAcquireDevInfo mock 返回的 namespace 列表(按 eid 匹配)
extern std::vector<UbseSsuDevNameSpace> g_itAcquireNsList;

// 安装全部适配器 mock 函数指针(绕过 dlopen,等价 UT SetupAdapterFuncs)
void SetupItAdapterFuncs();

// 重置可观测 mock 状态(计数、namespace 列表)
void ResetItAdapterMockState();

// 释放适配器单例 dlManager 占位句柄,避免析构 dlclose 非法句柄导致段错误
void ReleaseItAdapterDlHandle();

// 构造一条"适配器可见"的 namespace 并注册进 g_itAcquireNsList,
// 返回可直接写入推送消息的 UbseSsuNameSpaceInfo(nsUuid/eid 与适配器侧对齐,
// 使 FindNsInDevice 按 (eid, nsUuid) 命中,identity 校验走本地适配器 customData)
UbseSsuNameSpaceInfo RegisterAdapterNs(const std::string &eid, const std::string &subNqn, uint32_t nsId,
                                       const std::string &uuidByte, uid_t uid = 2000,
                                       const std::string &userName = "it_user");

// 工厂:构造一条 UbseSsuConnectInfo
UbseSsuConnectInfo MakeItConnectInfo(const std::string &name, const std::string &tgtEid);

} // namespace ubse::ssu::virt::it

#endif // SSU_VIRT_IT_MOCK_ADAPTER_H
