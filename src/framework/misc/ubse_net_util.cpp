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

#include "ubse_net_util.h"
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <securec.h>
#include <regex>

#include <fstream>
#include <sstream>
#include <unordered_set>
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_str_util.h"

UBSE_DEFINE_THIS_MODULE("ubse");
namespace ubse::utils {
using namespace ubse::common::def;
const size_t IPV4_MAX_LEN = 4;
const size_t IPV6_MAX_LEN = 16;
const uint32_t IPV4_INT_MAX = 255;

bool UbseNetUtil::IsPortVaLid(const uint32_t port)
{
    return port >= MIN_PORT && port <= MAX_PORT;
}

bool UbseNetUtil::ValidIpv4Addr(const std::string& ip)
{
    in_addr ipv4{};
    if (inet_pton(AF_INET, ip.c_str(), &ipv4) == 1) {
        return true;
    }
    return false;
}

bool UbseNetUtil::ValidIpv6Addr(const std::string& ip)
{
    in6_addr ipv6{};
    if (inet_pton(AF_INET6, ip.c_str(), &ipv6) == 1) {
        return true;
    }
    return false;
}

// 将点分十进制IP转为32位整数
uint32_t UbseNetUtil::IpV4ToInt(const std::string& ip, uint32_t& intIp)
{
    std::istringstream iss(ip);
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    char dot1;
    char dot2;
    char dot3;
    if (!(iss >> a >> dot1 >> b >> dot2 >> c >> dot3 >> d) || dot1 != '.' || dot2 != '.' || dot3 != '.' ||
        a > IPV4_INT_MAX || b > IPV4_INT_MAX || c > IPV4_INT_MAX || d > IPV4_INT_MAX) {
        return UBSE_ERROR;
    }
    // 分别左移24、16、8和0位，然后将它们用位或操作符 '|' 合并成一个32位的整数
    intIp = (a << 24) | (b << 16) | (c << 8) | d;
    return UBSE_OK;
}

// 将32位整数转为点分十进制IP
std::string UbseNetUtil::IntToIpV4(uint32_t ip_int)
{
    // 右移24位得到ip地址第一位，右移16位得到第二位
    return std::to_string((ip_int >> 24) & 0xFF) + "." + std::to_string((ip_int >> 16) & 0xFF) + "." +
           std::to_string((ip_int >> 8) & 0xFF) + "." + std::to_string(ip_int & 0xFF); // 右移8位得到第三位
}

// 解析IP范围
void UbseNetUtil::ParseIpRangeToList(const std::string& range, std::vector<std::string>& ips)
{
    std::vector<std::string> ipList;
    ubse::utils::Split(range, "-", ipList);
    if (ipList.size() != NO_2) { // 如果nodeId/ip/端口没有用冒号分隔，返回
        return;
    }
    uint32_t startIntIp;
    uint32_t endIntIp;
    auto ret = IpV4ToInt(ipList[0], startIntIp);
    if (ret != UBSE_OK) {
        return;
    }
    ret = IpV4ToInt(ipList[1], endIntIp);
    if (ret != UBSE_OK) {
        return;
    }
    if (startIntIp > endIntIp) {
        return;
    }
    for (uint32_t ip = startIntIp; ip <= endIntIp; ip++) {
        ips.emplace_back(IntToIpV4(ip));
        if (ip == UINT32_MAX) {
            break;
        }
    }
}

bool UbseNetUtil::Ipv4StringToArr(const std::string& ip, uint8_t* arr)
{
    in_addr ipv4{};
    if (inet_pton(AF_INET, ip.c_str(), &ipv4) == 1) {
        if (memcpy_s(arr, IPV4_MAX_LEN, &ipv4, IPV4_MAX_LEN) != EOK) {
            return false;
        }
        return true;
    }
    return false;
}

std::string UbseNetUtil::Ipv4ArrToString(const uint8_t* arr)
{
    if (arr == nullptr) {
        return "";
    }
    char ipBuf[INET_ADDRSTRLEN]; // 足够存放IPv4地址的字符串
    const char* result = inet_ntop(AF_INET, arr, ipBuf, INET_ADDRSTRLEN);
    if (result == nullptr) {
        // 错误处理，例如返回空字符串或抛出异常
        return "";
    }
    return std::string(ipBuf);
}

std::string UbseNetUtil::Ipv6ArrToString(const uint8_t* arr)
{
    char buffer[INET6_ADDRSTRLEN];                // IPv6地址最大长度缓冲区
    const char* result = inet_ntop(AF_INET6,      // IPv6地址族
                                   arr,           // 输入地址结构体
                                   buffer,        // 输出缓冲区
                                   sizeof(buffer) // 缓冲区大小
    );
    // 检查转换结果
    if (result == nullptr) {
        return ""; // 转换失败返回空字符串
    }
    return std::string(buffer);
}

bool UbseNetUtil::IsSpecialIP(const std::string& ip)
{
    // 排除特殊 IP 地址：0.0.0.0, 127.x.x.x, 169.254.x.x
    return std::regex_match(ip, std::regex("^(0\\.0\\.0\\.0|127\\..*|169\\.254\\..*)$"));
}

uint32_t UbseNetUtil::GetIpInfo(std::vector<std::string>& ipInfos)
{
    struct ifaddrs* ifaddr;
    struct ifaddrs* ifa;
    int family;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1) {
        UBSE_LOG_ERROR << "Error getting interface addresses";
        return UBSE_ERROR;
    }

    ipInfos.clear();

    std::unordered_set<std::string> uniqueIPs;
    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) {
            continue;
        }

        family = ifa->ifa_addr->sa_family;
        if (family == AF_INET) {
            // 获取IP地址字符串
            if (getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST) !=
                0) {
                continue;
            }
            std::string ip(host);
            if (!IsSpecialIP(ip)) {
                uniqueIPs.insert(ip);
            }
        }
    }

    freeifaddrs(ifaddr);
    ipInfos.insert(ipInfos.end(), uniqueIPs.begin(), uniqueIPs.end());
    return UBSE_OK;
}

bool parseIpString(const std::string& ipStr, ubse::nodeController::UbseIpAddr& out)
{
    // 尝试解析为IPv4
    in_addr ipv4{};
    if (inet_pton(AF_INET, ipStr.c_str(), &ipv4) == 1) {
        out.type = ubse::nodeController::UbseIpType::UBSE_IP_V4;
        auto err = memcpy_s(out.ipv4.addr, sizeof(out.ipv4.addr), &ipv4, NO_4); // ipv4
        if (err != EOK) {
            UBSE_LOG_ERROR << "Mem copy failed, errno_t=" << err << ".";
            return false;
        }

        return true;
    }

    // 尝试解析为IPv6
    in6_addr ipv6{};
    if (inet_pton(AF_INET6, ipStr.c_str(), &ipv6) == 1) {
        out.type = ubse::nodeController::UbseIpType::UBSE_IP_V6;
        auto err = memcpy_s(out.ipv6.addr, sizeof(out.ipv6.addr), &ipv6, NO_16);
        if (err != EOK) {
            UBSE_LOG_ERROR << "Mem copy failed, errno_t=" << err << ".";
            return false;
        }
        return true;
    }
    return false; // 无效IP格式
}

} // namespace ubse::utils