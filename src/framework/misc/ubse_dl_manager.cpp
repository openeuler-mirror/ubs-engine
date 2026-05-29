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

#include "framework/misc/ubse_dl_manager.h"
#include "ubse_logger.h"

namespace ubse::utils {
UBSE_DEFINE_THIS_MODULE("ubse");

UbseDlManager::UbseDlManager(std::string libPath) : libPath_(std::move(libPath)) {}

UbseDlManager::~UbseDlManager()
{
    Close();
}

UbseResult UbseDlManager::Open()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (handle_ != nullptr) {
        return UBSE_OK;
    }

    if (libPath_.empty()) {
        UBSE_LOG_ERROR << "Library path is empty";
        return UBSE_ERROR;
    }

    dlerror();
    handle_ = dlopen(libPath_.c_str(), RTLD_LAZY);
    if (handle_ == nullptr) {
        const char *err = dlerror();
        UBSE_LOG_ERROR << "Failed to load " << libPath_ << ": " << (err ? err : "unknown error");
        return UBSE_ERROR;
    }

    return UBSE_OK;
}

void UbseDlManager::Close()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (handle_ != nullptr) {
        dlclose(handle_);
        handle_ = nullptr;
    }
}

bool UbseDlManager::IsOpen() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return handle_ != nullptr;
}

} // namespace ubse::utils
