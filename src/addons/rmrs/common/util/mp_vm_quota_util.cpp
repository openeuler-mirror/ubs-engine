/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
  
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "mp_vm_quota_util.h"

#include <fstream>
#include <regex>
#include <sstream>

#include "ubse_logger.h"
#include "LibvirtHelper.h"
#include "mempooling_interface.h"
#include "mp_configuration.h"
#include "mp_error.h"

namespace mempooling {
using std::map;
using std::string;

namespace {
constexpr size_t GUEST_PREFIX_LENGTH = 6;
constexpr size_t NUMATUNE_CLOSE_TAG_LENGTH = 11;
constexpr size_t NODE_PREFIX_LENGTH = 4;
constexpr uint64_t MB_TO_KB = 1024ULL;
constexpr int INVALID_NODE_ID = -1;

static bool ParseTokenToNodeId(const std::string& token, int& nodeId, uint64_t& valueMB)
{
    size_t dashPos = token.find('-');
    if (dashPos == std::string::npos) {
        return false;
    }

    std::string valStr = token.substr(0, dashPos);
    std::string nodeName = token.substr(dashPos + 1);

    try {
        valueMB = std::stoull(valStr);
        nodeId = INVALID_NODE_ID;

        if (nodeName.length() > NODE_PREFIX_LENGTH && nodeName.substr(0, NODE_PREFIX_LENGTH) == "node") {
            nodeId = std::stoi(nodeName.substr(NODE_PREFIX_LENGTH));
        }

        return nodeId >= 0;
    } catch (const std::exception& e) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[ParseToken] Parse token failed: " << token << ", exception: " << e.what();
        return false;
    } catch (...) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[ParseToken] Parse token failed: " << token << ", unknown exception";
        return false;
    }
}

static void ParseProportionString(const std::string& proportion, std::map<int, uint64_t>& numaLimits)
{
    std::stringstream ss(proportion);
    std::string token;

    while (std::getline(ss, token, ':')) {
        int nodeId = INVALID_NODE_ID;
        uint64_t valueMB = 0;

        if (ParseTokenToNodeId(token, nodeId, valueMB)) {
            numaLimits[nodeId] += valueMB * MB_TO_KB;
        }
    }
}
} // namespace

std::string MpVmQuotaUtil::GetVmNameFromPid(pid_t pid)
{
    std::string path = "/proc/" + std::to_string(pid) + "/cmdline";
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[GetVmNameFromPid] Cannot open " << path;
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string cmdline = buffer.str();
    for (char& c : cmdline) {
        if (c == '\0')
            c = ' ';
    }

    std::string targetFlag = "-name";
    size_t pos = cmdline.find(targetFlag);
    if (pos == std::string::npos) {
        return "";
    }
    pos += targetFlag.length();
    while (pos < cmdline.length() && (cmdline[pos] == ' ' || cmdline[pos] == '=')) {
        pos++;
    }

    if (cmdline.substr(pos, GUEST_PREFIX_LENGTH) != "guest=") {
        return "";
    }
    pos += GUEST_PREFIX_LENGTH;

    size_t end = pos;
    while (end < cmdline.length() && cmdline[end] != ',' && cmdline[end] != ' ') {
        end++;
    }

    return cmdline.substr(pos, end - pos);
}

std::map<int, uint64_t> MpVmQuotaUtil::ParseNumatuneXml(const std::string& xmlStr)
{
    std::map<int, uint64_t> numaLimits;

    size_t startTag = xmlStr.find("<numatune>");
    size_t endTag = xmlStr.find("</numatune>");
    if (startTag == std::string::npos || endTag == std::string::npos) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[ParseNumatuneXml] No <numatune> section found";
        return numaLimits;
    }

    std::string numatuneBlock = xmlStr.substr(startTag, endTag - startTag + NUMATUNE_CLOSE_TAG_LENGTH);

    std::regex memnodeRegex(R"(<memnode\s+cellid='([^']+)'\s+proportion='([^']+)')");
    std::sregex_iterator it(numatuneBlock.begin(), numatuneBlock.end(), memnodeRegex);
    std::sregex_iterator end;

    for (; it != end; ++it) {
        std::smatch match = *it;
        std::string proportion = match[2].str();
        ParseProportionString(proportion, numaLimits);
    }

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[ParseNumatuneXml] Found " << numaLimits.size() << " NUMA limits";

    return numaLimits;
}

uint32_t MpVmQuotaUtil::GetVmNumaLimits(pid_t pid, std::map<int, uint64_t>& numaLimits)
{
    std::string vmName = MpVmQuotaUtil::GetVmNameFromPid(pid);
    if (vmName.empty()) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[GetVmNumaLimits] Cannot get VM name from pid=" << pid << ", skip validation";
        return MEM_POOLING_ERROR;
    }

    auto& helper = mempooling::exportV2::LibvirtHelper::GetInstance();
    if (helper.CheckConnectAndReconnect() != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[GetVmNumaLimits] Libvirt connection failed";
        return MEM_POOLING_ERROR;
    }

    mempooling::libvirt::VirDomainPtr domain = nullptr;
    if (helper.GetDomainByName(vmName, domain) != MEM_POOLING_OK) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[GetVmNumaLimits] Cannot find VM domain: " << vmName;
        return MEM_POOLING_ERROR;
    }

    std::string xmlStr;
    bool getXmlSuccess = (helper.GetDomainXML(domain, xmlStr) == MEM_POOLING_OK);
    helper.FreeDomain(domain);

    if (!getXmlSuccess) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[GetVmNumaLimits] Cannot get domain XML, skip validation";
        return MEM_POOLING_OK;
    }

    numaLimits = MpVmQuotaUtil::ParseNumatuneXml(xmlStr);
    if (numaLimits.empty()) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[GetVmNumaLimits] No numatune config found, skip validation";
    }

    return MEM_POOLING_OK;
}

uint32_t MpVmQuotaUtil::CheckQuotaExceedsLimit(const std::vector<mempooling::outinterface::PageSwapPair>& pageSwapPairs,
                                               const std::map<int, uint64_t>& numaLimits)
{
    for (const auto& pair : pageSwapPairs) {
        for (const auto& local : pair.localNumas) {
            int numaId = static_cast<int>(local.numaId);
            uint64_t quotaKB = local.quota;
            auto it = numaLimits.find(numaId);
            if (it != numaLimits.end()) {
                uint64_t limitKB = it->second;
                if (quotaKB > limitKB) {
                    UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                        << "[CheckQuota] quota exceeds limit: numaId=" << numaId << ", quota=" << quotaKB
                        << "KB, limit=" << limitKB << "KB";
                    return MEM_POOLING_ERROR;
                }
            } else {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[CheckQuota] Not Found Numa " << numaId << " in <numatune>";
                return MEM_POOLING_ERROR;
            }
        }
    }
    return MEM_POOLING_OK;
}

uint32_t MpVmQuotaUtil::ValidateNumaQuota(pid_t pid,
                                          const std::vector<mempooling::outinterface::PageSwapPair>& pageSwapPairs)
{
    std::map<int, uint64_t> numaLimits;
    uint32_t ret = GetVmNumaLimits(pid, numaLimits);
    if (ret != MEM_POOLING_OK) {
        return ret;
    }

    ret = CheckQuotaExceedsLimit(pageSwapPairs, numaLimits);
    if (ret != MEM_POOLING_OK) {
        return ret;
    }

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[ValidateNumaQuota] Validation passed for pid=" << pid;
    return MEM_POOLING_OK;
}

} // namespace mempooling