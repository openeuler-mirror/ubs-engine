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

#include "ubse_ssu_controller_module.h"

#include <array>
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_service_registry.h"
#include "ubse_ssu_http_handler.h"
#include "ubse_ssu_service_imp.h"

UBSE_DEFINE_THIS_MODULE("ubse");

namespace ubse::ssu::controller {
using namespace ubse::log;
using namespace ubse::module;
using namespace ubse::common::def;
using ubse::plugin::service::ssu::UbseSsuService;
using ubse::ssu::service::UbseSsuServiceImp;

// 注册为插件模块，依赖UbseVipModule（北向HTTP走VIP HTTP Server，需确保VIP模块就绪后再注册路由）
static constexpr auto G_UBSE_SSU_DEPS = std::array<UbseOptionModule, 1>{
    UbseOptionModule::UbseVipModule,
};
PLUGIN_MODULE_IMPL(UbseSsuControllerModule, G_UBSE_SSU_DEPS);

UbseResult UbseSsuControllerModule::Initialize()
{
    UBSE_LOG_INFO << "UbseSsuControllerModule Initialize";
    return UBSE_OK;
}

void UbseSsuControllerModule::UnInitialize() {}

UbseResult UbseSsuControllerModule::Start()
{
    UBSE_LOG_INFO << "UbseSsuControllerModule Start";

    // 将SSU服务单例注册到服务注册表，使北向接口可通过GetSsuService()获取
    // 使用空删除器：单例由GetInstance管理，不应被shared_ptr析构释放
    // GetInstance()返回单例引用，地址永远非空，无需做空指针检查
    ssuService_ = std::shared_ptr<UbseSsuService>(&UbseSsuServiceImp::GetInstance(),
                                                   [](UbseSsuService *) {});
    ubse::service::UbseServiceRegistry::GetInstance().RegisterService<UbseSsuService>(ssuService_);

    // 启动设备状态收集器，定期更新SSU设备信息
    auto ret = UbseSsuServiceImp::GetInstance().StartCollecting();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to start ssu collecting, ret=" << ret;
        return ret;
    }

    // 从设备列表重建账本，用于初始化或重启恢复
    UbseSsuServiceImp::GetInstance().RebuildLedgerFromDevList();

    // 注册北向HTTP接口路由
    ret = ubse::ssu::http_handler::RegisterSsuHttpHandlers();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to register ssu http handlers, ret=" << ret;
        return ret;
    }

    UBSE_LOG_INFO << "UbseSsuControllerModule Start success";
    return UBSE_OK;
}

void UbseSsuControllerModule::Stop()
{
    UBSE_LOG_INFO << "UbseSsuControllerModule Stop";

    UbseSsuServiceImp::GetInstance().StopCollecting();

    // 从服务注册表注销
    if (ssuService_ != nullptr) {
        ubse::service::UbseServiceRegistry::GetInstance().UnRegisterService<UbseSsuService>(ssuService_);
        ssuService_.reset();
    }
}
} // namespace ubse::ssu::controller
