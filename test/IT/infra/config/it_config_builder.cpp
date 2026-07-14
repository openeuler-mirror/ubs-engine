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

#include "it_config_builder.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "ubse_error.h"
#include "ubse_it_dir.h"
#include "it_console_log.h"

namespace ubse::it::infra {

ItConfigBuilder::ItConfigBuilder(const std::vector<NodeSpec>& nodeSpecs, const std::string& baseWorkDir)
    : nodeSpecs_(nodeSpecs),
      baseWorkDir_(baseWorkDir)
{
}

ItConfigBuilder& ItConfigBuilder::WithClusterIps(const std::vector<std::string>& clusterIps)
{
    clusterIps_ = clusterIps;
    return *this;
}

ItConfigBuilder& ItConfigBuilder::WithCertUse(bool useCert)
{
    certUse_ = useCert;
    return *this;
}

ItConfigBuilder& ItConfigBuilder::WithOverride(const std::string& section, const std::string& key,
                                               const std::string& value)
{
    overrides_[section][key] = value;
    return *this;
}

ItConfigBuilder& ItConfigBuilder::WithNodeOverride(const std::string& nodeId, const std::string& section,
                                                   const std::string& key, const std::string& value)
{
    nodeOverrides_[nodeId][section][key] = value;
    return *this;
}

ItConfigBuilder& ItConfigBuilder::WithMockPlugin(bool enable)
{
    mockPluginEnabled_ = enable;
    return *this;
}

UbseResult ItConfigBuilder::GenerateAllConfigs(const std::string& templatePath)
{
    for (const auto& spec : nodeSpecs_) {
        std::string outputDir = spec.workDir.empty() ? baseWorkDir_ + "/" + spec.nodeId : spec.workDir;
        UbseResult ret = GenerateConfig(spec, outputDir, templatePath);
        if (ret != UBSE_OK) {
            IT_LOG_ERROR << "Failed to generate config for node " << spec.nodeId;
            return ret;
        }
    }
    return UBSE_OK;
}

std::string ItConfigBuilder::FindTemplatePath(const std::string& templatePath)
{
    if (!templatePath.empty() && std::filesystem::exists(templatePath)) {
        return templatePath;
    }
    const char* projectRoot = std::getenv("UBSE_PROJECT_ROOT");
    if (projectRoot != nullptr && projectRoot[0] != '\0') {
        std::string path = std::string(projectRoot) + "/conf/ubse.conf";
        if (std::filesystem::exists(path)) {
            return path;
        }
    }
    return IT_DIRECTORY "/../../conf/ubse.conf";
}

std::string ItConfigBuilder::BuildClusterIpList() const
{
    if (clusterIps_.empty()) {
        return "";
    }
    std::ostringstream oss;
    for (size_t i = 0; i < clusterIps_.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << clusterIps_[i];
    }
    return oss.str();
}

std::string ItConfigBuilder::ApplyOverrides(const std::string& configContent, const std::string& nodeId)
{
    std::string content = configContent;

    // Mandatory override 1: cluster.ipList
    if (!clusterIps_.empty()) {
        std::string ipListLine = "cluster.ipList=" + BuildClusterIpList() + "\n";
        ReplaceOrInsertConfigLine(content, "cluster.ipList", ipListLine, "ubse.rpc");
    }

    // Mandatory override 2: cert.use=false (IT default)
    std::string certLine = std::string("cert.use=") + (certUse_ ? "true" : "false") + "\n";
    ReplaceOrInsertConfigLine(content, "cert.use", certLine, "ubse.rpc");

    // IT acceleration: reduce heartbeat interval to speed up election convergence.
    // Production defaults (2000ms × 3 = 6s) are unnecessarily slow for IT tests.
    // With IT override (5000ms × 3 = 15s), verify if election wait logic works correctly.
    std::string hbIntervalLine = "heartbeat.timeInterval=2000\n";
    ReplaceOrInsertConfigLine(content, "heartbeat.timeInterval", hbIntervalLine, "ubse.election");
    std::string hbLostLine = "heartbeat.lostThreshold=3\n";
    ReplaceOrInsertConfigLine(content, "heartbeat.lostThreshold", hbLostLine, "ubse.election");

    // Additional overrides via WithOverride()
    for (const auto& [section, keys] : overrides_) {
        for (const auto& [key, value] : keys) {
            std::string line = key + "=" + value + "\n";
            ReplaceOrInsertConfigLine(content, key, line, section);
        }
    }

    // Per-node overrides via WithNodeOverride() (only applied to the matching node)
    auto nodeIt = nodeOverrides_.find(nodeId);
    if (nodeIt != nodeOverrides_.end()) {
        for (const auto& [section, keys] : nodeIt->second) {
            for (const auto& [key, value] : keys) {
                std::string line = key + "=" + value + "\n";
                ReplaceOrInsertConfigLine(content, key, line, section);
            }
        }
    }

    return content;
}

void ItConfigBuilder::ReplaceOrInsertConfigLine(std::string& content, const std::string& key,
                                                const std::string& newLine, const std::string& section)
{
    std::string sectionHeader = "[" + section + "]";
    size_t sectionStart = content.find(sectionHeader);
    if (sectionStart == std::string::npos) {
        return;
    }
    size_t sectionLineEnd = content.find('\n', sectionStart);
    if (sectionLineEnd == std::string::npos) {
        return;
    }
    size_t sectionContentStart = sectionLineEnd + 1;

    size_t nextSectionStart = content.length();
    size_t searchPos = sectionContentStart;
    while (searchPos < content.length()) {
        size_t bracketPos = content.find('[', searchPos);
        if (bracketPos == std::string::npos) {
            break;
        }
        if (bracketPos == 0 || content[bracketPos - 1] == '\n') {
            nextSectionStart = bracketPos;
            break;
        }
        searchPos = bracketPos + 1;
    }

    // Replace existing uncommented key=value line
    std::string searchKey = key + "=";
    size_t pos = sectionContentStart;
    while (pos < nextSectionStart) {
        pos = content.find(searchKey, pos);
        if (pos == std::string::npos || pos >= nextSectionStart) {
            break;
        }
        bool atLineStart = (pos == 0 || content[pos - 1] == '\n');
        if (atLineStart) {
            size_t lineEnd = content.find('\n', pos);
            if (lineEnd != std::string::npos) {
                content.replace(pos, lineEnd - pos + 1, newLine);
            } else {
                content.replace(pos, content.length() - pos, newLine);
            }
            return;
        }
        pos += searchKey.length();
    }

    // Replace commented # key=value line
    std::string commentedKey = "# " + key + "=";
    pos = sectionContentStart;
    while (pos < nextSectionStart) {
        pos = content.find(commentedKey, pos);
        if (pos == std::string::npos || pos >= nextSectionStart) {
            break;
        }
        size_t lineStart = pos;
        if (pos > 0) {
            size_t prevNewline = content.rfind('\n', pos - 1);
            if (prevNewline != std::string::npos && prevNewline >= sectionContentStart) {
                lineStart = prevNewline + 1;
            } else {
                lineStart = sectionContentStart;
            }
        }
        size_t lineEnd = content.find('\n', pos);
        if (lineEnd != std::string::npos) {
            content.replace(lineStart, lineEnd - lineStart + 1, newLine);
        } else {
            content.replace(lineStart, content.length() - lineStart, newLine);
        }
        return;
    }

    // Key not found in section — insert at beginning of section content
    content.insert(sectionContentStart, newLine);
}

UbseResult ItConfigBuilder::GenerateConfig(const NodeSpec& nodeSpec, const std::string& outputDir,
                                           const std::string& templatePath)
{
    std::string tmplPath = FindTemplatePath(templatePath);
    if (!std::filesystem::exists(tmplPath)) {
        IT_LOG_ERROR << "Config template not found: " << tmplPath;
        return UBSE_ERROR_DEF(1);
    }

    std::ifstream ifs(tmplPath);
    if (!ifs.is_open()) {
        IT_LOG_ERROR << "Failed to open template: " << tmplPath;
        return UBSE_ERROR_DEF(2);
    }

    std::ostringstream contentStream;
    contentStream << ifs.rdbuf();
    ifs.close();
    std::string configContent = contentStream.str();

    configContent = ApplyOverrides(configContent, nodeSpec.nodeId);

    std::filesystem::create_directories(outputDir);
    std::string outputPath = outputDir + "/ubse.conf";
    std::ofstream ofs(outputPath);
    if (!ofs.is_open()) {
        IT_LOG_ERROR << "Failed to write config to " << outputPath;
        return UBSE_ERROR_DEF(3);
    }
    ofs << configContent;
    ofs.close();

    IT_LOG_INFO << "Generated config for node " << nodeSpec.nodeId << " at " << outputPath;

    // Overlay with mock configs from stubs directories (overwrite if exists)
    // Only copy when mockPluginEnabled_ is true. The daemon loads auxiliary configs
    // from /etc/ubse/ first, then from the work directory. We do NOT copy from the
    // source conf/ directory here because that would overwrite environment-specific
    // configs (e.g. ubse_plugin_admission.conf with all plugins commented out).
    if (mockPluginEnabled_) {
        std::filesystem::path stubConfDir = std::filesystem::path(IT_DIRECTORY) / "stubs" / "mock_plugin" / "conf";
        if (std::filesystem::exists(stubConfDir)) {
            CopyAuxiliaryConfigs(stubConfDir, outputDir);
        }
    }

    return UBSE_OK;
}

void ItConfigBuilder::CopyAuxiliaryConfigs(const std::filesystem::path& srcConfDir, const std::string& outputDir)
{
    // Patterns: plugin_*.conf and ubse_plugin_admission.conf
    if (!std::filesystem::exists(srcConfDir) || !std::filesystem::is_directory(srcConfDir)) {
        return;
    }
    for (const auto& entry : std::filesystem::directory_iterator(srcConfDir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        std::string filename = entry.path().filename().string();
        // Only copy plugin config files and admission config
        if (filename.rfind("plugin_", 0) == 0 && filename.size() >= 6 &&
            filename.substr(filename.size() - 5) == ".conf") {
            std::filesystem::copy_file(entry.path(), outputDir + "/" + filename,
                                       std::filesystem::copy_options::overwrite_existing);
            IT_LOG_INFO << "Copied plugin config: " << filename;
        } else if (filename == "ubse_plugin_admission.conf") {
            std::filesystem::copy_file(entry.path(), outputDir + "/" + filename,
                                       std::filesystem::copy_options::overwrite_existing);
            IT_LOG_INFO << "Copied admission config: " << filename;
        }
    }
}

} // namespace ubse::it::infra
