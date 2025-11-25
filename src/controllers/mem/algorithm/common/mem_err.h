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

#ifndef OCK_COMMON_UTIL_EEROR_H
#define OCK_COMMON_UTIL_EEROR_H

#include <cstdint>
#include <string>

namespace ock::mem {
using BResult = int32_t;
using HRESULT = int32_t;
enum Error : int32_t {
    RETURN_OK = 0,
    HOK = 0,
    RETURN_ERR = 1,
    HFAIL = 1,
    RETURN_PARAM_ERROR = 2,
    RETURN_MAX,

    E_CODE_NULLPTR,
    E_CODE_POSIX_IO_ERR,
    E_CODE_THREADPOOL_INIT_FAIL,

    E_CODE_AGENT_NAME_EXIST,
    E_CODE_ADD_META_DATA,
    E_CODE_MANAGER_NAME_EXIST,
    E_CODE_MANAGER_DELETE_FAIL,
    E_CODE_WRONG_USER_FAIL,
    E_CODE_DELETE_NAME_DONOT_EXIST,
    E_CODE_CREATE_SHM_FAIL,

    E_CODE_IPC_UNKNOWN_ERROR = 1010000,
    E_CODE_IPC_OPCODE_INVALID,
    E_CODE_IPC_ENGINE_INITIALIZE_ERR,
    E_CODE_IPC_ENGINE_NOT_INIT,
    E_CODE_IPC_SERIALIZED_DATA_EMPTY,
    E_CODE_IPC_VERSION_UN_SUPPORT,
    E_CODE_IPC_DESERIALIZE_DATA_EMPTY,
    E_CODE_IPC_RESPONSE_ERROR,
    E_CODE_SERIALIZE_DESERIALIZE_ERROR,
    E_CODE_IPC_CREATE_FAILURE,
    E_CODE_IPC_TIME_OUT,
    E_CODE_IPC_ALLOC_MSG,
    E_CODE_IPC_SYNC_CALL,
    E_CODE_IPC_RECEIVE_DATA_NULL,
    E_CODE_IPC_CALL_FAIL,
    E_CODE_IPC_ROOT_NOT_SUPPORT,

    E_CODE_RPC_SYNC_CALL,

    E_CODE_UBSE_SERVICE_PARAMETER_ERROR = 1020000,
    E_CODE_UBSE_SERVICE_NO_MEMORY,
    E_CODE_UBSE_SERVICE_INITIALIZE_ERROR,
    E_CODE_UBSE_SERVICE_UNINITIALIZE_ERROR,
    E_CODE_UBSE_SERVICE_NOT_READY_ERR,

    E_CODE_UBSE_PROXY_PARAMETER_ERROR = 1040000,
    E_CODE_UBSE_PROXY_NO_MEMORY,
    E_CODE_UBSE_PROXY_INITIALIZE_ERROR,
    E_CODE_UBSE_PROXY_NOT_INITIALIZE,
    E_CODE_UBSE_PROXY_CONF_ERR,
    E_CODE_UBSE_PROXY_DIVIDED_BY_ZERO,
};

#define RETURN_ERROR_CODE_AS_NULLPTR(ptr, errCode)                      \
    do {                                                                \
        if (UNLIKELY((ptr) == nullptr)) {                               \
            LOG_ERROR("Unexpected nullptr value " + std::string(#ptr)); \
            return errCode;                                             \
        }                                                               \
    } while (0)

#define RETURN_AS_NULLPTR(ptr)                                              \
    do {                                                                    \
        if (UNLIKELY((ptr) == nullptr)) {                                   \
            LOG_ERROR("Unexpected nullptr value for " + std::string(#ptr)); \
            return;                                                         \
        }                                                                   \
    } while (0)

#define RETURN_ERROR_CODE_AS_NULLPTR(ptr, errCode)                      \
    do {                                                                \
        if (UNLIKELY((ptr) == nullptr)) {                               \
            LOG_ERROR("Unexpected nullptr value " + std::string(#ptr)); \
            return errCode;                                             \
        }                                                               \
    } while (0)

#define RETURN_FAIL_AS_NULLPTR(ptr)                                     \
    do {                                                                \
        if (UNLIKELY((ptr) == nullptr)) {                               \
            LOG_ERROR("Unexpected nullptr value " + std::string(#ptr)); \
            return E_CODE_NULLPTR;                                      \
        }                                                               \
    } while (0)

#define RETURN_NULLPTR_AS_NULLPTR(ptr)                                      \
    do {                                                                    \
        if (UNLIKELY((ptr) == nullptr)) {                                   \
            LOG_ERROR("Unexpected nullptr value for " + std::string(#ptr)); \
            return nullptr;                                                 \
        }                                                                   \
    } while (0)

#define RETURN_ERROR_CODE_WHEN_LT(left, right, errCode)                                          \
    do {                                                                                         \
        if ((left) < (right)) {                                                                  \
            LOG_ERROR("Unexpected " + std::string(#left) + " less than " + std::string(#right)); \
            return errCode;                                                                      \
        }                                                                                        \
    } while (0)

#define RETURN_ERROR_CODE_WHEN_NE(left, right, errCode)                                          \
    do {                                                                                         \
        if ((left) != (right)) {                                                                 \
            LOG_ERROR("Unexpected " + std::string(#left) + " not equal " + std::string(#right)); \
            return errCode;                                                                      \
        }                                                                                        \
    } while (0)

#define RETURN_ERROR_CODE_WHEN_GT(left, right, errCode)                                             \
    do {                                                                                            \
        if ((left) > (right)) {                                                                     \
            LOG_ERROR("Unexpected " + std::string(#left) + " greater than " + std::string(#right)); \
            return errCode;                                                                         \
        }                                                                                           \
    } while (0)

#define RETURN_FALSE_AS_NULLPTR(ptr)                                    \
    do {                                                                \
        if (UNLIKELY((ptr) == nullptr)) {                               \
            LOG_ERROR("Unexpected nullptr value " + std::string(#ptr)); \
            return false;                                               \
        }                                                               \
    } while (0)

#define HRESULT_FAIL(hr) (BResult(hr) != RETURN_OK)
#define HRESULT_OK(hr) (BResult(hr) == RETURN_OK)

inline void HandleException(HRESULT &hr, std::exception &ex) {}

inline void HandleUnknownError(HRESULT &hr) {}

const auto UNKNOWN_EXCEPTION_MSG = std::string("unknown exception msg");

inline const std::string &GetExceptionMsg()
{
    return UNKNOWN_EXCEPTION_MSG;
}
} // namespace ock::mem

#endif
