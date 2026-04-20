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

#ifndef UBS_ENGINE_UBSE_UBTURB_MODULE_H
#define UBS_ENGINE_UBSE_UBTURB_MODULE_H

#include <string>
#include <dlfcn.h>
#include <vector>

#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_common_def.h"

namespace ubse::ubturb {
#define MODULE_LOG_NAME "ubse"
using namespace ubse::common::def;

constexpr uint16_t UBTURB_MAX_REMOTE_NUMA = 18;

struct NumaEntry {
    uint16_t numaId;
    uint16_t status;
};

struct NumaStatusList {
    uint16_t cnt;
    struct NumaEntry entries[UBTURB_MAX_REMOTE_NUMA];
};

using UbturbNotifyNumaPtr = int (*)(NumaStatusList* msg);

/**
 * @class UbseUbturboModule
 * @brief Ubturbo module adapter for UBS engine
 * @details This class provides an interface to interact with the ubturbo client library
 */
class UbseUbturboModule {
public:
    /**
     * @brief Default constructor
     */
    UbseUbturboModule() = default;

    /**
     * @brief Destructor
     */
    ~UbseUbturboModule()
    {
        if (handle != nullptr) {
            dlclose(handle);
            handle = nullptr;
        }
    }

    /**
     * @brief Get singleton instance
     * @return Reference to the singleton instance
     */
    static UbseUbturboModule &GetInstance()
    {
        static UbseUbturboModule instance;
        return instance;
    }

    /**
     * @brief Initialize the module
     * @return UBSE_OK on success, UBSE_ERROR on failure
     */
    [[nodiscard]] UbseResult Init();

    /**
     * @brief Open the ubturbo client library
     * @param ubturboPath Path to the ubturbo client library
     * @return UBSE_OK on success, UBSE_ERROR on failure
     */
    [[nodiscard]] UbseResult DlOpenLib(const std::string& ubturboPath);

    /**
     * @brief Get function pointer from the library
     * @tparam T Type of the function pointer
     * @param funPtr Reference to store the function pointer
     * @param funName Name of the function to get
     * @return UBSE_OK on success, UBSE_ERROR on failure
     */
    template <typename T>
    [[nodiscard]] UbseResult DlOpenFunName(T &funPtr, const std::string &funName)
    {
        if (handle == nullptr) {
            UBSE_LOG_ERROR << "Library handle is null, cannot get function=" << funName;
            return UBSE_ERROR;
        }

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        // Using reinterpret_cast is necessary here for dlsym return value conversion to function pointer
        funPtr = reinterpret_cast<T>(dlsym(handle, funName.c_str()));
        if (funPtr == nullptr) {
            UBSE_LOG_ERROR << "Get function from " << ubturboSo << " failed, fun name=" << funName
                           << ", error=" << dlerror();
            dlclose(handle);
            handle = nullptr;
            return UBSE_ERROR;
        }
        return UBSE_OK;
    }

    /**
     * @brief Notify NUMA list status to ubturbo
     * @param numaStatus Vector of NUMA node status pairs
     * @return UBSE_OK on success, UBSE_ERROR on failure
     */
    [[nodiscard]] UbseResult UbseNotifyNumaListStatus(const std::vector<std::pair<int64_t, int>> &numaStatus);

    /**
     * @brief Check if the module is initialized
     * @return true if initialized, false otherwise
     */
    [[nodiscard]] bool IsInitialized() const
    {
        return handle != nullptr && notifyNumaFunc != nullptr;
    }

private:
    static constexpr auto ubturboSo = "libubturbo_client.so";
    void* handle = nullptr;
    UbturbNotifyNumaPtr notifyNumaFunc = nullptr;
};
#undef MODULE_LOG_NAME
} // namespace ubse::ubturb

#endif // UBS_ENGINE_UBSE_UBTURB_MODULE_H
