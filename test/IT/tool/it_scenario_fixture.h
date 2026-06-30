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

#ifndef IT_SCENARIO_FIXTURE_H
#define IT_SCENARIO_FIXTURE_H

#include <filesystem>
#include <string>

#include "config.h"
#include "gtest/gtest.h"
#include "it_cluster_builder.h"
#include "it_console_log.h"

namespace ubse::it::infra {

/**
 * @brief Base class for scenario-driven IT test fixtures.
 *
 * Each scenario (e.g. SingleNodeScenario, TwoNodeScenario) inherits from this
 * class and manages its own ItCluster instance via SetUpTestSuite/TearDownTestSuite.
 * Test cases use TEST_F(ScenarioName, TestName) and access the cluster via
 * the scenario's static Cluster() method.
 *
 * Lifecycle:
 *   SetUpTestSuite()   -> start cluster (once per scenario)
 *   TEST_F cases       -> run with shared cluster
 *   TearDownTestSuite() -> stop cluster (once per scenario)
 */
class ItScenarioFixture : public testing::Test {
protected:
    /**
     * @brief Scenario name for logging and diagnostics.
     *
     * Each scenario subclass must provide its own Name() implementation.
     * Used in IT_LOG_INFO / IT_LOG_WARN etc. to identify which scenario
     * is producing the output.
     */
    static constexpr const char* Name()
    {
        return "UnknownScenario";
    }

    /**
     * @brief Get runtime paths for building clusters within scenarios.
     *
     * Scenarios use this internally in their SetUpTestSuite to construct
     * an ItClusterBuilder. Test cases should NOT call this directly —
     * they access the cluster via the scenario's Cluster() method.
     */
    static ItClusterBuilder MakeBuilder()
    {
        auto workDir = "/tmp/ubs_engine_it_" + std::to_string(::getpid());
        auto binaryPath = std::filesystem::path(BUILD_DIRECTORY) / "bin" / "ubse_it_daemon";
        auto stubLibDir = std::filesystem::path(BUILD_DIRECTORY) / "lib";
        return ItClusterBuilder::FromRuntimePaths(binaryPath, workDir, stubLibDir);
    }

    /**
     * @brief Clean up the work directory for a scenario.
     *
     * Called by each scenario's TearDownTestSuite after stopping the cluster.
     */
    static void CleanupWorkDir(const std::string& workDir)
    {
        if (!workDir.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(workDir, ec);
            if (ec) {
                IT_LOG_WARN << "Failed to clean up work directory " << workDir << ": " << ec.message();
            }
        }
    }
};

} // namespace ubse::it::infra

#endif // IT_SCENARIO_FIXTURE_H
