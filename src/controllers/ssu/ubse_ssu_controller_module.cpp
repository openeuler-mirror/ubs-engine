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
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_service_registry.h"
#include "ubse_ssu_http_handler.h"
#include "ubse_ssu_rpc_processor.h"
#include "ubse_ssu_service_imp.h"
#include "ubse_thread_pool_module.h"

UBSE_DEFINE_THIS_MODULE("ubse");

namespace ubse::ssu::controller {
using namespace ubse::log;
using namespace ubse::module;
using namespace ubse::common::def;
using namespace ubse::context;
using namespace ubse::task_executor;
using ubse::plugin::service::ssu::UbseSsuService;
using ubse::ssu::service::UbseSsuServiceImp;

// 注册插件模块
static constexpr auto G_UBSE_SSU_DEPS = std::array<UbseOptionModule, 3>{
    UbseOptionModule::UbseElectionModule,
    UbseOptionModule::UbseComModule,
    UbseOptionModule::UbseVipModule,
};
PLUGIN_MODULE_IMPL(UbseSsuControllerModule, G_UBSE_SSU_DEPS);

UbseResult UbseSsuControllerModule::Initialize()
{
    auto executorModule = UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (executorModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get task executor module";
        return UBSE_ERROR_NULLPTR;
    }
    // 创建SSU控制器任务执行器，数值参考mem 18个线程，队列容量1000
    auto ret = executorModule->Create("ubseSsuController", 18, 1000);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to create ssu controller executor, " << FormatRetCode(ret);
        return ret;
    }

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
    ssuService_ = std::shared_ptr<UbseSsuService>(&UbseSsuServiceImp::GetInstance(), [](UbseSsuService *) {});
    ubse::service::UbseServiceRegistry::GetInstance().RegisterService<UbseSsuService>(ssuService_);

    // 启动设备状态收集器，定期更新SSU设备信息
    auto ret = UbseSsuServiceImp::GetInstance().StartCollecting();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to start ssu collecting, ret=" << ret;
        return ret;
    }

    // 从设备列表重建账本，用于初始化或重启恢复
    UbseSsuServiceImp::GetInstance().RebuildLedgerFromDevList();

    // 启动空VM BusInstance清理定时器
    ret = UbseSsuServiceImp::GetInstance().StartClearTimer();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to start clear timer, ret=" << ret;
        return ret;
    }

    // 注册SSU RPC Handler
    ret = UbseSsuRpcProcessor::RegHandler();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to register ssu rpc handlers, ret=" << ret;
        return ret;
    }

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

    UbseSsuServiceImp::GetInstance().StopClearTimer();
    UbseSsuServiceImp::GetInstance().StopCollecting();

    // 从服务注册表注销
    if (ssuService_ != nullptr) {
        ubse::service::UbseServiceRegistry::GetInstance().UnRegisterService<UbseSsuService>(ssuService_);
        ssuService_.reset();
    }
}
} // namespace ubse::ssu::controller
