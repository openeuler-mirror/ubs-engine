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

#ifndef UBSE_MANAGER_UBSE_NET_UTIL_H
#define UBSE_MANAGER_UBSE_NET_UTIL_H
#include <string>
#include <vector>
#include "ubse_common_def.h"
#include "ubse_node_controller.h"

namespace ubse::utils {
using namespace ubse::common::def;
class UbseNetUtil {
public:
    static uint32_t ParseIpList(const std::string &ipListStr, std::vector<std::string> &ipList);

    static uint32_t FindLocalIpByRemote(const std::string &remoteIp, std::string &localIp);

    static uint32_t FindLocalIpInIpList(std::vector<std::string> ipList, std::string &localIp);

    static bool IsPortVaLid(const uint32_t port);

    static bool ValidIpv4Addr(const std::string &ip);

    static bool ValidIpv6Addr(const std::string &ip);

    static uint32_t IpV4ToInt(const std::string &ip, uint32_t &intIp);

    static std::string IntToIpV4(uint32_t ipInt);

    static void ParseIpRangeToList(const std::string &range, std::vector<std::string> &ips);

    static bool Ipv4StringToArr(const std::string &ip, uint8_t *arr);

    static std::string Ipv4ArrToString(const uint8_t *arr);

    static std::string Ipv6ArrToString(const uint8_t *arr);

    static bool IsSpecialIP(const std::string &ip);

    static uint32_t GetIpInfo(std::vector<std::string> &ipInfos);
};

// 辅助函数：解析字符串IP为UbseIpAddr结构
bool parseIpString(const std::string &ipStr, ubse::nodeController::UbseIpAddr &out);
} // namespace ubse::utils

#endif // UBSE_MANAGER_UBSE_NET_UTIL_H
