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

#include "ubse_module.h"
#include "ubse_timer.h"
#include "ubse_context.h" // for context
#include "ubse_election.h"
#include "ubse_logger_module.h"
#include "ubse_module.h"
#include "adapter_plugins/mti/ubse_topology_interface.h"
#include "ubse_node_controller.h"

namespace ubse::nodeController {
using namespace ubse::mti;
using namespace ubse::context;
using namespace ubse::log;
using namespace ubse::module;
using namespace ubse::timer;
using namespace ubse::election;
using namespace ubse::common::def;
class UbseNodeControllerModule : public UbseModule {
public:
    static constexpr const char *kModuleName = "UbseNodeControllerModule";
    std::string Name() const override
    {
        return kModuleName;
    }
    UbseResult Initialize() override;

    void UnInitialize() override;

    UbseResult Start() override;

    void Stop() override;
};
} // namespace ubse::nodeController
#endif // UBSE_NODE_CONTROLLER_MODULE_H
