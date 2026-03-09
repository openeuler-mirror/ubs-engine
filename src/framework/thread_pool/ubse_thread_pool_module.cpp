/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_thread_pool_module.h"

#include <referable/ubse_ref.h> // for Ref
#include <utility>              // for pair

#include "ubse_thread_pool.h"
#include "ubse_context.h"       // for context
#include "ubse_error.h"         // for UBSE_OK, UBSE_ERROR, UBSE_TASK_...
#include "ubse_logger.h"        // for UbseLoggerEntry, UBSE_DEFINE_TH...
#include "ubse_logger_inner.h"  // for RM_LOG_ERROR
#include "ubse_logger_module.h" // for UbseLoggerModule

namespace ubse::task_executor {
using namespace ubse::log;

BASE_DYNAMIC_CREATE(UbseTaskExecutorModule, ubse::log::UbseLoggerModule);
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_TASK_EXECUTOR_MID)

UbseResult UbseTaskExecutorModule::Initialize()
{
    return UBSE_OK;
}

void UbseTaskExecutorModule::UnInitialize() {}

UbseResult UbseTaskExecutorModule::Start()
{
    return UBSE_OK;
}

void UbseTaskExecutorModule::Stop()
{
    WriteLocker<ReadWriteLock> lock(&rwLock);
    for (const auto &executor : executors) {
        executor.second->Stop();
    }
    executors.clear();
}

UbseResult UbseTaskExecutorModule::Create(const std::string &name, uint16_t threadNum, uint32_t queueCapacity)
{
    WriteLocker<ReadWriteLock> lock(&rwLock);
    auto executor = UbseTaskExecutor::Create(name, threadNum, queueCapacity);
    if (executor == nullptr) {
        UBSE_LOG_ERROR << "Create executor " << name << " fail";
        return UBSE_ERROR;
    }
    executor->SetThreadName(name);
    if (!executor->Start()) {
        UBSE_LOG_ERROR << "Start executor " << name << " fail";
        return UBSE_ERROR;
    }
    executors.emplace(name, executor);
    return UBSE_OK;
}

UbseTaskExecutorPtr UbseTaskExecutorModule::Get(const std::string &name)
{
    ReadLocker<ReadWriteLock> lock(&rwLock);
    auto iter = executors.find(name);
    if (iter == executors.end()) {
        return nullptr;
    }
    return iter->second;
}

void UbseTaskExecutorModule::Remove(const std::string &name)
{
    WriteLocker<ReadWriteLock> lock(&rwLock);
    auto iter = executors.find(name);
    if (iter == executors.end()) {
        return;
    }
    iter->second->Stop();
    executors.erase(name);
}
} // namespace ubse::task_executor