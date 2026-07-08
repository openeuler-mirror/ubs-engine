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
#include "it_assertion.h"
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

/**
 * @brief Macro to define a scenario class with minimal boilerplate.
 *
 * Usage:
 *   IT_DEFINE_SCENARIO(Tongsuan1dFullMeshSingleNodeScenario,
 *       MakeBuilder().SingleNode().Start(cluster_))
 *
 * The macro generates a class inheriting ItScenarioFixture with:
 *   - SetUpTestSuite() that executes the startup expression and captures workDir_
 *   - TearDownTestSuite() that stops the cluster and cleans up the work directory
 *   - Cluster() accessor that returns a reference to the shared ItCluster instance
 *
 * Each scenario is compiled as a separate binary, so static member definitions
 * in the header do not cause ODR violations.
 *
 * @param ClassName  Name of the generated scenario class.
 * @param startup_expr  Expression that builds and starts the cluster.
 *                      Must populate `cluster_` (a std::unique_ptr<ItCluster>&).
 */
#define IT_DEFINE_SCENARIO(ClassName, startup_expr)                \
    namespace ubse::it::infra {                                    \
    class ClassName : public ItScenarioFixture {                   \
    public:                                                        \
        static void SetUpTestSuite()                               \
        {                                                          \
            IT_LOG_INFO << #ClassName ": starting cluster...";     \
            auto ret = startup_expr;                               \
            ASSERT_IT_OK(ret);                                     \
            if (cluster_) {                                        \
                workDir_ = cluster_->GetBaseWorkDir();             \
            }                                                      \
            IT_LOG_INFO << #ClassName ": cluster started";         \
        }                                                          \
        static void TearDownTestSuite()                            \
        {                                                          \
            if (cluster_) {                                        \
                IT_LOG_INFO << #ClassName ": stopping cluster..."; \
                cluster_->StopCluster();                           \
                cluster_.reset();                                  \
            }                                                      \
            CleanupWorkDir(workDir_);                              \
        }                                                          \
        static ItCluster& Cluster()                                \
        {                                                          \
            return *cluster_;                                      \
        }                                                          \
                                                                   \
    private:                                                       \
        static std::unique_ptr<ItCluster> cluster_;                \
        static std::string workDir_;                               \
    };                                                             \
    std::unique_ptr<ItCluster> ClassName::cluster_;                \
    std::string ClassName::workDir_;                               \
    } /* namespace ubse::it::infra */

} // namespace ubse::it::infra

#endif // IT_SCENARIO_FIXTURE_H
