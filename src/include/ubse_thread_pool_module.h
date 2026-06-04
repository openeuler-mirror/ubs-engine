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

#ifndef UBSE_TASK_EXECUTOR_MODULE_H
#define UBSE_TASK_EXECUTOR_MODULE_H
#include <lock/ubse_lock.h>       // for ReadWriteLock
#include <cstdint>                // for uint16_t, uint32_t
#include <map>                    // for map
#include <string>                 // for string

#include "ubse_common_def.h"      // for UbseResult
#include "ubse_module.h"          // for UbseModule
#include "ubse_thread_pool.h"

namespace ubse::task_executor {

using namespace ubse::module;
using namespace ubse::common::def;
class UbseTaskExecutorModule final : public UbseModule {
public:
    static constexpr const char* kModuleName = "UbseTaskExecutorModule";

    UbseResult Initialize() override;

    void UnInitialize() override;

    UbseResult Start() override;

    void Stop() override;

    UbseResult Create(const std::string &name, uint16_t threadNum, uint32_t queueCapacity);

    UbseTaskExecutorPtr Get(const std::string &name);

    void Remove(const std::string &name);
    std::string Name() const override { return kModuleName; }

private:
    std::map<std::string, UbseTaskExecutorPtr> executors_;
    ReadWriteLock rwLock_;
};
} // namespace ubse::task_executor
#endif // UBSE_TASK_EXECUTOR_MODULE_H
