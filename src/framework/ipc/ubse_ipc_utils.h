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

#ifndef UBSE_MANAGER_UBSE_IPC_UTILS_H
#define UBSE_MANAGER_UBSE_IPC_UTILS_H

#include <cstdint>
#include <random>

#include "ubse_ipc_common_def.h"
#include "ubse_ipc_message.h"

namespace ubse::ipc {

/**
 * 生成16位随机ID，用于生成同步请求ID
 * @return
 */
uint64_t RandomId();

uint32_t SerializeRequestMessage(const UbseRequestMessage &requestMessage, std::vector<uint8_t> &buffer);

uint32_t SerializeResponseMessage(const UbseResponseMessage &responseMessage, std::vector<uint8_t> &buffer);

uint32_t SerializeShmFault(const UbseShmFault &shmFault, uint8_t *&buffer, size_t &size);

uint32_t DeSerializeShmFault(UbseShmFault &shmFault, uint8_t *buffer, size_t size);
} // namespace ubse::ipc

#endif // UBSE_MANAGER_UBSE_IPC_UTILS_H
