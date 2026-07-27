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
#ifndef UBSE_MEM_CONTROLLER_DEF_SERIAL_H
#define UBSE_MEM_CONTROLLER_DEF_SERIAL_H

#include <vector>
#include "ubse_mem_controller_def.h"
#include "ubse_serial_util.h"

namespace ubse::mem::controller::message {
using ubse::serial::UbseDeSerialization;
using ubse::serial::UbseSerialization;

bool UbseNodeBorrowInfosSerialize(UbseSerialization& serialization,
                                  const std::vector<def::UbseNodeBorrowInfo>& nodeBorrowInfos);

bool UbseNodeBorrowInfosDeserialize(UbseDeSerialization& deSerialization,
                                    std::vector<def::UbseNodeBorrowInfo>& nodeBorrowInfos);

bool UbseCliShmDescSerialize(const ubse::mem::def::UbseMemShmDesc& shmDesc, const std::string& importNodeId,
                             UbseSerialization& serialization);
} // namespace ubse::mem::controller::message
#endif // UBSE_MEM_CONTROLLER_DEF_SERIAL_H
