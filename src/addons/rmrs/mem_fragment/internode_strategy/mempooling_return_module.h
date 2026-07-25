/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MEMPOOL_RETURN_MODULE_H
#define MEMPOOL_RETURN_MODULE_H

#include "mempool_borrow_module.h"

#include <securec.h>
#include <ubse_com.h>
#include <ubse_def.h>
#include <ubse_logger.h>
#include <ubse_mem_controller.h>
#include <ubse_node.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "mem_borrow_executor.h"
#include "mem_manager.h"
#include "mempooling_message.h"
#include "mp_borrow_conf_util.h"
#include "mp_configuration.h"
#include "mp_error.h"
#include "mp_module.h"
#include "mp_vector_util.h"

namespace mempooling {

using namespace ubse::log;
using namespace ubse::nodeController;
using namespace ubse::com;
using namespace mempooling::message;
using namespace ubse::mem::controller;

constexpr uint64_t DELETE_TIMEOUT_SCAN_SECONDS = 30;

class MemReturnScanner {
public:
    // 获取唯一实例
    static MemReturnScanner& Instance()
    {
        static MemReturnScanner instance;
        return instance;
    }
    // 启动线程
    // 返回 true：线程已启动或成功启动
    // 返回 false：线程启动失败
    bool start();

private:
    MemReturnScanner() : running(false) {}
    ~MemReturnScanner() = default;
    // 禁止拷贝和赋值
    MemReturnScanner(const MemReturnScanner&) = delete;
    MemReturnScanner& operator=(const MemReturnScanner&) = delete;

    std::mutex scanMtx;
    std::atomic<bool> running;
    // 线程扫描逻辑
    void run();
};

} // namespace mempooling

#endif // MEMPOOL_RETURN_MODULE_H
