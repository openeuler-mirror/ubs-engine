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

#ifndef UBSE_CLI_BUFFER_GUARD_H
#define UBSE_CLI_BUFFER_GUARD_H
#include "ubse_ipc_client.h"

namespace ubse::cli::reg {
class UbseCliBufferGuard {
public:
    UbseCliBufferGuard() = delete;
    ~UbseCliBufferGuard();
    explicit UbseCliBufferGuard(ubse_api_buffer_t &buffer) : ubseApiBuffer(buffer){};

private:
    ubse_api_buffer_t &ubseApiBuffer;
};
} // namespace ubse::cli::reg

#endif // UBSE_CLI_BUFFER_GUARD_H
