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
#ifndef UBSE_VM_TASK_COUNTER_H
#define UBSE_VM_TASK_COUNTER_H

#include <atomic>
#include <string>

namespace vm {
class VmTaskCounter {
public:
    static void StartTask(const std::string &name);
    static void CompleteTask(const std::string &name);
    static int GetTaskCount();

private:
    static std::atomic<int> count;
};
}
#endif // UBSE_VM_TASK_COUNTER_H
