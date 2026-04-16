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

#include "ubse_ubturbo_module.h"

namespace ubse::ubturb {
UBSE_DEFINE_THIS_MODULE("ubse");

    UbseResult UbseUbturboModule::Init()
    {
        return DlOpenLib(ubturboSo);
    }

    UbseResult UbseUbturboModule::DlOpenLib(const std::string& ubturboPath)
    {
        if (ubturboPath.empty()) {
            UBSE_LOG_ERROR << "ubturboPath is empty";
            return UBSE_ERROR_INVAL;
        }

        if (handle != nullptr) {
            UBSE_LOG_WARN << "Library already opened, closing first";
            dlclose(handle);
            handle = nullptr;
        }

        handle = dlopen(ubturboPath.c_str(), RTLD_NOW);
        if (handle == nullptr) {
            UBSE_LOG_ERROR << "Dlopen " << ubturboPath << " failed, error is " << dlerror();
            return UBSE_ERROR_INVAL;
        }

        auto ret = DlOpenFunName(notifyNumaFunc, "ubturbo_notify_numa_list_status");
        if (ret != UBSE_OK) {
            return ret;
        }
        return UBSE_OK;
    }

    UbseResult UbseUbturboModule::UbTurboNotifyNumaListStatus(const std::vector<std::pair<int64_t, int>> &numaStatus)
    {
        if (numaStatus.empty()) {
            UBSE_LOG_WARN << "numaStatus is empty";
            return UBSE_OK;
        }

        if (numaStatus.size() > UBTURB_MAX_REMOTE_NUMA) {
            UBSE_LOG_ERROR << "numaStatus size exceeds maximum capacity, max=" << UBTURB_MAX_REMOTE_NUMA;
            return UBSE_ERROR_INVAL;
        }

        NumaStatusList statusList = {};
        statusList.cnt = static_cast<uint16_t>(numaStatus.size());
        UBSE_LOG_INFO << "numaStatus entries count=" << statusList.cnt;

        for (uint16_t i = 0; i < statusList.cnt; i++) {
            statusList.entries[i].numaId = static_cast<uint16_t>(numaStatus[i].first);
            statusList.entries[i].status = static_cast<uint16_t>(numaStatus[i].second);
            UBSE_LOG_INFO << "entry[" << i << "]: numaId=" << statusList.entries[i].numaId
                        << ", status=" << statusList.entries[i].status;
        }

        if (!IsInitialized()) {
            UBSE_LOG_INFO << "Ubturbo Module not initialized, initializing...";
            auto initRet = Init();
            if (initRet != UBSE_OK) {
                UBSE_LOG_WARN << "Ubturbo Module initialization failed, ret=" << initRet;
                return UBSE_OK;
            }
        }

        int ret = notifyNumaFunc(&statusList);
        if (ret != 0) {
            UBSE_LOG_ERROR << "Ubturbo_notify_numa_list_status failed, ret=" << ret;
            return UBSE_ERROR;
        }

        UBSE_LOG_INFO << "Ubturbo Module notified NUMA status to ubturbo successfully";
        return UBSE_OK;
    }
}
