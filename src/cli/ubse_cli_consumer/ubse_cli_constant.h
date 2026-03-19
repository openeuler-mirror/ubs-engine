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

#ifndef UBSE_CLI_CONSTANT_H
#define UBSE_CLI_CONSTANT_H
#include <string>

// Store constants unrelated to the structure.
namespace constant {
constexpr uint32_t REQUEST_BUFFER_CAPACITY = 8; // Default non-empty request size.
}

namespace systemd::error {
const std::string ALLOCATION_ERROR = "ERROR: System resource allocation failed. Please try again later.";
const std::string CONNECTION_FAILED = "ERROR: Failed to connect ubse service.";
const std::string RECV_FAILED = "ERROR: Failed to receive data.";
} // namespace systemd::error

namespace data::error {
const std::string REQUEST_INFO_SER_FAILED = "ERROR: Client serialization of the request information failed.";
const std::string RES_INFO_DESER_FAILED = "ERROR: Client deserialization of the response information failed.";
} // namespace data::error

namespace memory::error {
const std::string MEM_QUERY_NOT_EXIST_IN_CREATING =
    "ERROR: The creation process failed, and the instance has been deleted.";
const std::string MEM_STATE_UNKNOWN_IN_CREATING =
    "ERROR: Internal error, memory query result is in an undefined state.";
} // namespace memory::error

namespace memory::info {
const std::string MEM_BORROW_DETAIL_EMPTY = "INFO: The borrow detail information is empty.";
}

namespace node::error {
const std::string NODE_ATTRIBUTE_EMPTY = "ERROR: Node attribute information is empty.";
}

#endif // UBSE_CLI_CONSTANT_H
