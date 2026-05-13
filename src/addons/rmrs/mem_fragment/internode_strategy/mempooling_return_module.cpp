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

#include "mempooling_return_module.h"
#include <ubse_mem_controller.h>
#include "fault_node_module.h"
#include "mp_smap_controller.h"

namespace mempooling {
using namespace ubse::log;
using namespace ubse::nodeController;
using namespace ubse::com;
using namespace ubse::mem::controller;
using namespace mempooling::message;

bool MemReturnScanner::start()
{
    std::lock_guard<std::mutex> lock(scanMtx);
    if (running.load()) {
        return true; // 线程已在运行
    }
    try {
        running.store(true);
        std::thread([this] {
            run();
            running.store(false); // 线程结束时置 false
        }).detach();              // 自动回收线程
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturnScanner] "
                                                          << "Start to scan timeout return records";
    } catch (...) {
        running.store(false);
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturnScanner] "
                                                          << "Failed to start scan timeout return records";
        return false; // 线程创建失败
    }
    return true;
}

void MemReturnScanner::run()
{
    auto& mgr = MemReturnManager::Instance();
    while (true) {
        std::unordered_map<std::string, BorrowItem> borrowCacheAll;
        mgr.QueryAll(borrowCacheAll);
        if (borrowCacheAll.empty()) {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemReturnScanner] BorrowCache is empty, stop scanning.";
            break;
        }
        for (auto& kv : borrowCacheAll) {
            UbseMemResult result;
            if (UbseQueryResult(kv.first, result) != 0) {
                std::this_thread::sleep_for(std::chrono::seconds(DELETE_TIMEOUT_SCAN_SECONDS));
                continue;
            }
            if (result.stage == UbseMemStage::UBSE_DELETING) {
                BorrowItem updated = kv.second;
                updated.scanCount++;
                if (mgr.Update(kv.first, updated) != MEM_POOLING_OK) {
                    UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                        << "[MemReturnScanner] Update borrowId=" << kv.first << " scanCount failed.";
                    std::this_thread::sleep_for(std::chrono::seconds(DELETE_TIMEOUT_SCAN_SECONDS));
                    continue;
                }
                UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[MemReturnScanner] BorrowId=" << kv.first
                    << " is always in deleting ,scanCount=" << updated.scanCount;
            } else {
                if (mgr.Remove(kv.first) != MEM_POOLING_OK) {
                    UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                        << "[MemReturnScanner] Remove BorrowId=" << kv.first << " failed.";
                    std::this_thread::sleep_for(std::chrono::seconds(DELETE_TIMEOUT_SCAN_SECONDS));
                    continue;
                }
                UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[MemReturnScanner] Delete BorrowId=" << kv.first << " from borrowCache";
                if (mgr.IsAllReturned(kv.second.srcNid, kv.second.srcRemoteNumaId)) {
                    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                        << "[MemReturnScanner] Timeout records in numaId=" << kv.second.srcRemoteNumaId
                        << " is all processing completed";
                    EnableNodeMsg enableMsg{SMAP_ENABLE_NUMA, kv.second.srcRemoteNumaId};
                    SmapEnableNumaProcess(enableMsg);
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(DELETE_TIMEOUT_SCAN_SECONDS));
    }
    running.store(false); // 标记线程已退出
}
} // namespace mempooling