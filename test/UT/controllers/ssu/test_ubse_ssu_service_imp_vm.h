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

// VM 模式下 UbseSsuServiceImp 分支测试夹具定义

#ifndef TEST_UBSE_SSU_SERVICE_IMP_VM_H
#define TEST_UBSE_SSU_SERVICE_IMP_VM_H

#include <string>
#include "test_ubse_ssu_service_imp_fixture.h"
#include "ubse_ssu_vm_cache.h"

namespace ubse::ssu::service::ut {

// VM 模式 ServiceImp 分支测试夹具
// 继承 UbseSsuServiceImpTestBase 复用 host 模式测试基础设施(adapter mock、role mock 等)
// 不修改/不验证 UbseSsuVmCache 实现,仅通过公开接口构造测试数据
class TestUbseSsuServiceImpVm : public UbseSsuServiceImpTestBase {
public:
    TestUbseSsuServiceImpVm() = default;

    void SetUp() override;
    void TearDown() override;

protected:
    // VM 模式本地缓存(由本测试夹具持有,SetUp 时注入 ServiceImp)
    ubse::ssu::controller::UbseSsuVmCache vmCache_;

    // 工厂方法:构造一条 UbseSsuAllocResult 用于推送缓存
    static UbseSsuAllocResult MakeVmAllocResult(const std::string &name, uint32_t nsCount = 1);
    // 工厂方法:构造一条 UbseSsuConnectInfo 用于推送缓存
    static UbseSsuConnectInfo MakeVmConnectInfo(const std::string &name, const std::string &tgtEid);
};

} // namespace ubse::ssu::service::ut

#endif // TEST_UBSE_SSU_SERVICE_IMP_VM_H
