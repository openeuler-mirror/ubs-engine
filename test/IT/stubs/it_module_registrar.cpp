/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 *
 * IT-only PLUGIN module to activate OPTIONAL modules (election, com, lcne, etc.)
 * in ubse_it_daemon. Without a PLUGIN module, ResolveActivation(PLUGIN) finds
 * nothing and OPTIONAL modules never get activated, so election never starts.
 *
 * In production, ssu_plugin.so (loaded via dlopen from /usr/lib64/ubse_plugin)
 * provides the PLUGIN module that pulls in OPTIONAL deps. IT cannot use
 * ssu_plugin.so due to heavy dependencies, so this stub is linked directly
 * into ubse_it_daemon via --whole-archive.
 */

#include <array>
#include <string>

#include "ubse_error.h"
#include "ubse_module.h"

namespace ubse::it {

class ItPluginModule final : public ubse::module::UbseModule {
public:
    static constexpr auto kModuleName = "ItPluginModule";

    uint32_t Initialize() override { return UBSE_OK; }
    void UnInitialize() override {}
    uint32_t Start() override { return UBSE_OK; }
    void Stop() override {}
    std::string Name() const override { return kModuleName; }
};

// Depend on all OPTIONAL modules needed for IT election scenarios.
// ActivateWithDependencies will recursively activate their own deps.
// Note: UbseMmiModule is not registered in IT build, do not list it here.
static constexpr auto G_IT_PLUGIN_DEPS = std::array<ubse::module::UbseOptionModule, 11>{
    ubse::module::UbseOptionModule::UbseElectionModule,
    ubse::module::UbseOptionModule::UbseComModule,
    ubse::module::UbseOptionModule::UbseLcneModule,
    ubse::module::UbseOptionModule::UbseNodeControllerModule,
    ubse::module::UbseOptionModule::UbseNodeMgrModule,
    ubse::module::UbseOptionModule::UbseHttpModule,
    ubse::module::UbseOptionModule::UbseUrmaUvsModule,
    ubse::module::UbseOptionModule::UbseVipModule,
    ubse::module::UbseOptionModule::UbseStorageModule,
    ubse::module::UbseOptionModule::UbseRasModule,
    ubse::module::UbseOptionModule::UbsePluginModule,
};
PLUGIN_MODULE_IMPL(ItPluginModule, G_IT_PLUGIN_DEPS);

} // namespace ubse::it
