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

#include "ubse_urma_topo_config.h"

#include <cstddef>
#include <fstream>
#include <iterator>
#include <utility>

#include "rapidjson/document.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_str_util.h"

namespace ubse::urma {
using namespace ubse::common::def;
using namespace ubse::config;
using namespace ubse::context;
using namespace ubse::log;
using namespace ubse::utils;

UBSE_DEFINE_THIS_MODULE("ubse");

constexpr const char *URMA_CONFIG_SECTION = "ubse.urma";
constexpr const char *URMA_TOPO_MODE_KEY = "topo_mode";
constexpr const char *URMA_TOPO_MODE_NON_CROSS = "non-cross";
constexpr const char *URMA_TOPO_MODE_HCCS_CROSS = "hccs-cross";
constexpr const char *URMA_TOPO_CONFIG_DIR = "/etc/ubse/topo";
constexpr const char *URMA_TOPO_CONFIG_NON_CROSS_FILE = "non-cross.json";
constexpr const char *URMA_TOPO_CONFIG_HCCS_CROSS_FILE = "hccs-cross.json";
constexpr std::size_t URMA_TOPO_PORT_FIELD_NUM = 3;
constexpr std::size_t URMA_TOPO_PORT_CHIP_ID_INDEX = 0;
constexpr std::size_t URMA_TOPO_PORT_DIE_ID_INDEX = 1;
constexpr std::size_t URMA_TOPO_PORT_PORT_ID_INDEX = 2;

UbseUrmaTopoMode GetUrmaTopoMode()
{
    auto module = UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (module == nullptr) {
        UBSE_LOG_WARN << "Failed to get config module, use default URMA topo_mode=non-cross";
        return UbseUrmaTopoMode::NON_CROSS;
    }

    std::string topoMode = URMA_TOPO_MODE_NON_CROSS;
    auto ret = module->GetConf<std::string>(URMA_CONFIG_SECTION, URMA_TOPO_MODE_KEY, topoMode);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to get URMA topo_mode config, use default non-cross, ret="
                      << FormatRetCode(ret);
        return UbseUrmaTopoMode::NON_CROSS;
    }

    if (topoMode == URMA_TOPO_MODE_NON_CROSS) {
        return UbseUrmaTopoMode::NON_CROSS;
    }
    if (topoMode == URMA_TOPO_MODE_HCCS_CROSS) {
        return UbseUrmaTopoMode::HCCS_CROSS;
    }

    UBSE_LOG_WARN << "Invalid URMA topo_mode=" << topoMode << ", use default non-cross";
    return UbseUrmaTopoMode::NON_CROSS;
}

std::string GetUrmaTopoConfigFileName(UbseUrmaTopoMode topoMode)
{
    return topoMode == UbseUrmaTopoMode::HCCS_CROSS ? URMA_TOPO_CONFIG_HCCS_CROSS_FILE :
                                                      URMA_TOPO_CONFIG_NON_CROSS_FILE;
}

UbseResult ReadTextFile(const std::string &filePath, std::string &content)
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        UBSE_LOG_ERROR << "Failed to open URMA topo config file=" << filePath;
        return UBSE_ERROR_FILE_NOT_EXIST;
    }

    content.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    if (content.empty()) {
        UBSE_LOG_ERROR << "URMA topo config file is empty, file=" << filePath;
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

UbseResult ParseTopoPort(const std::string &portStr, UbseUrmaTopoPort &port)
{
    std::vector<std::string> fields;
    Split(portStr, "/", fields);
    if (fields.size() != URMA_TOPO_PORT_FIELD_NUM) {
        UBSE_LOG_ERROR << "Invalid URMA topo port=" << portStr;
        return UBSE_ERROR_INVAL;
    }

    if (ConvertStrToUint32(fields[URMA_TOPO_PORT_CHIP_ID_INDEX], port.chipId) != UBSE_OK ||
        ConvertStrToUint32(fields[URMA_TOPO_PORT_DIE_ID_INDEX], port.dieId) != UBSE_OK ||
        ConvertStrToUint32(fields[URMA_TOPO_PORT_PORT_ID_INDEX], port.portId) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to parse URMA topo port=" << portStr;
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

UbseResult GetJsonString(const rapidjson::Value &json, const char *key, std::string &value)
{
    if (!json.IsObject() || !json.HasMember(key) || !json[key].IsString()) {
        UBSE_LOG_ERROR << "Invalid URMA topo config string field=" << key;
        return UBSE_ERROR_INVAL;
    }
    value = json[key].GetString();
    return UBSE_OK;
}

UbseResult ParseTopoNodePorts(const rapidjson::Value &json, std::vector<UbseUrmaTopoPort> &nodePorts)
{
    constexpr const char *nodePortsKey = "node_ports";
    if (!json.IsObject() || !json.HasMember(nodePortsKey) || !json[nodePortsKey].IsArray()) {
        UBSE_LOG_ERROR << "Invalid URMA topo config node_ports";
        return UBSE_ERROR_INVAL;
    }

    nodePorts.clear();
    for (const auto &portValue : json[nodePortsKey].GetArray()) {
        if (!portValue.IsString()) {
            UBSE_LOG_ERROR << "Invalid URMA topo config node port item";
            return UBSE_ERROR_INVAL;
        }
        UbseUrmaTopoPort port{};
        if (auto ret = ParseTopoPort(portValue.GetString(), port); ret != UBSE_OK) {
            return ret;
        }
        nodePorts.push_back(port);
    }
    return UBSE_OK;
}

UbseResult ParseTopoLinks(const rapidjson::Value &json, std::vector<UbseUrmaTopoLink> &links)
{
    constexpr const char *linksKey = "links";
    constexpr const char *localPortKey = "local_port";
    constexpr const char *remotePortKey = "remote_port";
    if (!json.IsObject() || !json.HasMember(linksKey) || !json[linksKey].IsArray()) {
        UBSE_LOG_ERROR << "Invalid URMA topo config links";
        return UBSE_ERROR_INVAL;
    }

    links.clear();
    for (const auto &linkValue : json[linksKey].GetArray()) {
        std::string localPortStr;
        std::string remotePortStr;
        if (GetJsonString(linkValue, localPortKey, localPortStr) != UBSE_OK ||
            GetJsonString(linkValue, remotePortKey, remotePortStr) != UBSE_OK) {
            return UBSE_ERROR_INVAL;
        }

        UbseUrmaTopoLink link{};
        if (ParseTopoPort(localPortStr, link.localPort) != UBSE_OK ||
            ParseTopoPort(remotePortStr, link.remotePort) != UBSE_OK) {
            return UBSE_ERROR_INVAL;
        }
        links.push_back(link);
    }
    return UBSE_OK;
}

UbseResult ParseUrmaTopoConfig(const std::string &topoFile, UbseUrmaTopoConfig &topoConfig)
{
    std::string content;
    if (auto ret = ReadTextFile(topoFile, content); ret != UBSE_OK) {
        return ret;
    }

    rapidjson::Document doc;
    if (doc.Parse(content.c_str()).HasParseError() || !doc.IsObject()) {
        UBSE_LOG_ERROR << "Failed to parse URMA topo config json, file=" << topoFile;
        return UBSE_ERROR_INVAL;
    }

    UbseUrmaTopoConfig parsedConfig{};
    if (GetJsonString(doc, "version", parsedConfig.version) != UBSE_OK ||
        GetJsonString(doc, "node_type", parsedConfig.nodeType) != UBSE_OK ||
        GetJsonString(doc, "link_type", parsedConfig.linkType) != UBSE_OK ||
        ParseTopoNodePorts(doc, parsedConfig.nodePorts) != UBSE_OK ||
        ParseTopoLinks(doc, parsedConfig.links) != UBSE_OK) {
        return UBSE_ERROR_INVAL;
    }

    topoConfig = std::move(parsedConfig);
    return UBSE_OK;
}

UbseResult LoadUrmaTopoConfig(UbseUrmaTopoMode topoMode, UbseUrmaTopoConfig &topoConfig)
{
    const std::string fileName = GetUrmaTopoConfigFileName(topoMode);
    const std::string filePath = std::string(URMA_TOPO_CONFIG_DIR) + "/" + fileName;
    auto ret = ParseUrmaTopoConfig(filePath, topoConfig);
    if (ret != UBSE_OK) {
        return ret;
    }

    const std::string expectedLinkType = topoMode == UbseUrmaTopoMode::HCCS_CROSS ?
        URMA_TOPO_MODE_HCCS_CROSS : URMA_TOPO_MODE_NON_CROSS;
    if (topoConfig.linkType != expectedLinkType) {
        UBSE_LOG_ERROR << "URMA topo config link_type mismatch. expected=" << expectedLinkType
                       << ", actual=" << topoConfig.linkType;
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}
} // namespace ubse::urma
