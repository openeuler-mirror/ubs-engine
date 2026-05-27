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

#ifndef UBSE_NODE_CONTROLLER_MODULE_H
#define UBSE_NODE_CONTROLLER_MODULE_H

#include "ubse_context.h" // for context
#include "ubse_election.h"
#include "ubse_logger_module.h"
#include "ubse_module.h"
#include "ubse_node_controller.h"
#include "ubse_timer.h"
#include "adapter_plugins/mti/ubse_topology_interface.h"

namespace ubse::nodeController {
using ubse::common::def::UbseResult;
using ubse::module::UbseModule;

class UbseNodeControllerModule : public UbseModule {
public:
    UbseResult Initialize() override;

    void UnInitialize() override;

    UbseResult Start() override;

    void Stop() override;
};
} // namespace ubse::nodeController
#endif // UBSE_NODE_CONTROLLER_MODULE_H
