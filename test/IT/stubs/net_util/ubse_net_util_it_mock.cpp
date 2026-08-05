/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 *
 * IT link-time mock for UbseNetUtil::GetIpInfo.
 *
 * The real GetIpInfo calls getifaddrs + getnameinfo and filters out special
 * IPs (127.x.x.x, 169.254.x.x, 0.0.0.0) via IsSpecialIP. IT scenarios use
 * 127.0.0.x as node IPs, which would be filtered out, causing
 * FindLocalIpInIpList to return UBSE_ERROR_EMPTY (10002).
 *
 * This mock bypasses IsSpecialIP and returns UBSE_IT_NODE_IP directly,
 * so FindLocalIpInIpList can match the node IP in the cluster IP list.
 */

#include <cstdlib>
#include <string>
#include <vector>

#include "ubse_error.h"
#include "ubse_net_util.h"

namespace ubse::utils {

uint32_t UbseNetUtil::GetIpInfo(std::vector<std::string>& ipInfos)
{
    ipInfos.clear();
    const char* nodeIp = getenv("UBSE_IT_NODE_IP");
    if (nodeIp != nullptr && nodeIp[0] != '\0') {
        ipInfos.emplace_back(nodeIp);
    }
    return UBSE_OK;
}

} // namespace ubse::utils
