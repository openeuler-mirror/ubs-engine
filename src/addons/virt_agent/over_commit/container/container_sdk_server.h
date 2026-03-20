/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef CONTAINER_SDK_SERVER_H
#define CONTAINER_SDK_SERVER_H

#include <vm_error.h>
#include <ubse_api_server_def.h>
#include "vm_mem_manager.h"
#include "ubs_virt_agent_container.h"

namespace vm {
using namespace api::server;

class VirtContainerSdk {
public:
  static VmResult Register();

private:
    static uint32_t GetMemInfoForPid(const UbseIpcMessage &req, const UbseRequestContext &context);
    static uint32_t InjectWaterLineHandler(const UbseIpcMessage &req, const UbseRequestContext &context);
    static uint32_t GetContainerPidsHandler(const UbseIpcMessage &req, const UbseRequestContext &context);
    static VmResult WaterLineMemBorrow(const UbseIpcMessage &req, const UbseRequestContext &context);
    static VmResult WaterLineMemMigrate(const UbseIpcMessage &req, const UbseRequestContext &context);
    static VmResult WaterLineMemReturn(const UbseIpcMessage &req, const UbseRequestContext &context);
};
} // namespace vm

#endif // CONTAINER_SDK_SERVER_H
