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

// 共享的 vm_mode 配置 mock,供 TestUbseSsuControllerModuleVmMode /
// TestUbseSsuVmHandlers 两个夹具复用(避免 MockUbseGetBool/GetStr/GetUInt 重复定义)
// mock 行为:
// - vm_mode key:返回 g_vmModeForTest;g_vmModeConfigRetForTest 非 UBSE_OK 时模拟配置读取失败
// - enable_tls key:返回 g_enableTlsForTest
// - 其他 key:一律返回 false
// - UbseGetStr 返回空(certDir 走默认值 /var/lib/ubse/lcne_cert/),UbseGetUInt 返回 0(port 走默认值)

#ifndef TEST_UBSE_SSU_CONTROLLER_VM_MOCK_H
#define TEST_UBSE_SSU_CONTROLLER_VM_MOCK_H

#include <cstdint>
#include <string>

#include "config/ubse_conf.h"
#include "mockcpp/mockcpp.hpp"
#include "ubse_error.h"
#include "ubse_ssu_controller_module.h" // UBSE_SSU_CONF_VM_MODE_KEY / UBSE_SSU_CONF_ENABLE_TLS_KEY

namespace ubse::ssu::controller::ut {

// 测试用的配置模拟值(inline 变量,多 TU 共享单一实例)
inline bool g_vmModeForTest = false;            // vm_mode key 返回值
inline bool g_enableTlsForTest = false;         // enable_tls key 返回值
inline uint32_t g_vmModeConfigRetForTest = UBSE_OK; // 非 UBSE_OK 时模拟 vm_mode 读取失败

inline uint32_t MockUbseGetBool(const std::string &section, const std::string &key, bool &val)
{
    (void)section;
    if (key == std::string(UBSE_SSU_CONF_VM_MODE_KEY)) {
        if (g_vmModeConfigRetForTest != UBSE_OK) {
            return g_vmModeConfigRetForTest;
        }
        val = g_vmModeForTest;
    } else if (key == std::string(UBSE_SSU_CONF_ENABLE_TLS_KEY)) {
        val = g_enableTlsForTest;
    } else {
        val = false;
    }
    return UBSE_OK;
}

inline uint32_t MockUbseGetStr(const std::string &section, const std::string &key, std::string &val)
{
    (void)section;
    (void)key;
    val.clear();
    return UBSE_OK;
}

inline uint32_t MockUbseGetUInt(const std::string &section, const std::string &key, uint32_t &val)
{
    (void)section;
    (void)key;
    val = 0;
    return UBSE_OK;
}

// 安装三件 config mock(两个夹具 SetUp 共用)
// 注:mockcpp 以精确函数指针类型匹配目标,需用 static_cast 从重载集中选定 UbseGetBool/GetStr/GetUInt 的具体签名
inline void SetupVmConfigMocks()
{
    MOCKER_CPP(static_cast<uint32_t (*)(const std::string &, const std::string &, bool &)>(
        &ubse::config::UbseGetBool))
        .stubs()
        .will(invoke(MockUbseGetBool));
    MOCKER_CPP(static_cast<uint32_t (*)(const std::string &, const std::string &, std::string &)>(
        &ubse::config::UbseGetStr))
        .stubs()
        .will(invoke(MockUbseGetStr));
    MOCKER_CPP(static_cast<uint32_t (*)(const std::string &, const std::string &, uint32_t &)>(
        &ubse::config::UbseGetUInt))
        .stubs()
        .will(invoke(MockUbseGetUInt));
}

} // namespace ubse::ssu::controller::ut

#endif // TEST_UBSE_SSU_CONTROLLER_VM_MOCK_H
