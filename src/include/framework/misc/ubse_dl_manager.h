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

#ifndef UBSE_DL_MANAGER_H
#define UBSE_DL_MANAGER_H

#include <dlfcn.h>
#include <mutex>
#include <string>

#include "ubse_common_def.h"
#include "ubse_error.h"

namespace ubse::utils {
using namespace ubse::common::def;

class UbseDlManager {
public:
    explicit UbseDlManager(std::string libPath);

    ~UbseDlManager();

    UbseDlManager(const UbseDlManager &) = delete;
    UbseDlManager &operator=(const UbseDlManager &) = delete;
    UbseDlManager(UbseDlManager &&) = delete;
    UbseDlManager &operator=(UbseDlManager &&) = delete;

    UbseResult Open();

    void Close();

    bool IsOpen() const;

    template <typename FuncPtr>
    UbseResult GetFunction(FuncPtr &funcPtr, const std::string &funcName)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (handle_ == nullptr) {
            return UBSE_ERROR;
        }

        dlerror();
        funcPtr = reinterpret_cast<FuncPtr>(dlsym(handle_, funcName.c_str()));
        if (dlerror() != nullptr) {
            return UBSE_ERROR;
        }
        return UBSE_OK;
    }

private:
    std::string libPath_;
    void *handle_{nullptr};
    mutable std::mutex mutex_;
};

} // namespace ubse::utils

#endif // UBSE_DL_MANAGER_H
