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

#ifndef IT_CONFIG_BUILDER_H
#define IT_CONFIG_BUILDER_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "ubse_common_def.h"
#include "ubse_error.h"

namespace ubse::it::infra {

using ubse::common::def::UbseResult;

struct NodeConfig {
    std::string nodeId;
    std::string ip;
    uint16_t port;
    uint32_t slotId = 1;
};

/**
 * @brief Unified config generator for IT tests.
 *
 * Generates per-node ubse.conf files from the project template,
 * applying only the two mandatory IT overrides:
 * - cluster.ipList (RPC cluster IP addresses)
 * - cert.use=false (disable TLS for IT tests)
 *
 * Plus any additional overrides via WithOverride().
 *
 * The daemon loads config via two-stage Init:
 * 1. /etc/ubse/ (system defaults — may not exist on CI)
 * 2. -f <workDir> (IT-generated full config overrides everything)
 *
 * Since CI environments lack /etc/ubse/ubse.conf, this builder
 * produces a FULL config file from the project template,
 * not a minimal overlay.
 */
class ItConfigBuilder {
public:
    explicit ItConfigBuilder(const std::vector<NodeConfig>& nodeConfigs, const std::string& baseWorkDir);

    /** @brief Set cluster IP list for [ubse.rpc] cluster.ipList. Mandatory. */
    ItConfigBuilder& WithClusterIps(const std::vector<std::string>& clusterIps);

    /** @brief Set cert.use in [ubse.rpc]. Defaults to false for IT. */
    ItConfigBuilder& WithCertUse(bool useCert = false);

    /** @brief Add an arbitrary key=value override in a specific section. */
    ItConfigBuilder& WithOverride(const std::string& section, const std::string& key, const std::string& value);

    /** @brief Generate config files for all nodes. Returns UBSE_OK on success. */
    UbseResult GenerateAllConfigs(const std::string& templatePath = "");

private:
    UbseResult GenerateConfig(const NodeConfig& nodeConfig, const std::string& outputDir,
                              const std::string& templatePath);

    std::string FindTemplatePath(const std::string& templatePath);
    std::string BuildClusterIpList() const;
    std::string ApplyOverrides(const std::string& configContent);
    void ReplaceOrInsertConfigLine(std::string& content, const std::string& key, const std::string& newLine,
                                   const std::string& section);

    std::vector<NodeConfig> nodeConfigs_;
    std::string baseWorkDir_;
    std::vector<std::string> clusterIps_;
    bool certUse_ = false;
    std::map<std::string, std::map<std::string, std::string>> overrides_;
};

} // namespace ubse::it::infra

#endif // IT_CONFIG_BUILDER_H