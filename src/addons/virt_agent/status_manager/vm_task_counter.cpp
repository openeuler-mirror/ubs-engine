/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "vm_task_counter.h"

#include <ubse_logger.h>

namespace vm {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using namespace ubse::log;

std::atomic<int> VmTaskCounter::count(0);

void VmTaskCounter::StartTask(const std::string &name)
{
    UBSE_LOG_INFO << "start task, task = " << name;
    count.fetch_add(1);
}

void VmTaskCounter::CompleteTask(const std::string &name)
{
    UBSE_LOG_INFO << "complete task, task = " << name;
    count.fetch_sub(1);
}

int VmTaskCounter::GetTaskCount()
{
    const auto taskCount = count.load();
    UBSE_LOG_INFO << "task_count = " << taskCount;
    return taskCount;
}
}