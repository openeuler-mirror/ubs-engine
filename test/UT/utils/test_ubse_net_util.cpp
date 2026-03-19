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

#include <gtest/gtest.h>
#include "ubse_error.h"
#include "ubse_net_util.h"
namespace ubse::ut::utils {
using namespace ubse::utils;

const size_t IPV4_MAX_LEN = 4;
// 测试IsPortVaLid函数
TEST(UbseNetUtilTest, IsPortVaLid)
{
    // 测试端口号在有效范围内
    EXPECT_TRUE(UbseNetUtil::IsPortVaLid(1024));
    EXPECT_TRUE(UbseNetUtil::IsPortVaLid(65535));

    // 测试端口号小于最小有效端口号
    EXPECT_FALSE(UbseNetUtil::IsPortVaLid(1023));

    // 测试端口号大于最大有效端口号
    EXPECT_FALSE(UbseNetUtil::IsPortVaLid(65536));
}

TEST(ValidIpv4AddrTest, ValidIP)
{
    std::string validIP = "192.168.1.1";
    EXPECT_TRUE(UbseNetUtil::ValidIpv4Addr(validIP));
}

// 测试用例2：测试无效的IPv4地址
TEST(ValidIpv4AddrTest, InvalidIP)
{
    std::string invalidIP = "256.256.256.256";
    EXPECT_FALSE(UbseNetUtil::ValidIpv4Addr(invalidIP));
}

// 测试用例3：测试空字符串
TEST(ValidIpv4AddrTest, EmptyString)
{
    std::string emptyString = "";
    EXPECT_FALSE(UbseNetUtil::ValidIpv4Addr(emptyString));
}

// 测试用例1：测试有效的IPv6地址
TEST(ValidIpv6AddrTest, ValidAddress)
{
    std::string validIpv6 = "2001:0db8:85a3:0000:0000:8a2e:0370:7334";
    EXPECT_TRUE(UbseNetUtil::ValidIpv6Addr(validIpv6));
}

// 测试用例2：测试无效的IPv6地址
TEST(ValidIpv6AddrTest, InvalidAddress)
{
    std::string invalidIpv6 = "2001::0db8::85a3";
    EXPECT_FALSE(UbseNetUtil::ValidIpv6Addr(invalidIpv6));
}

// 测试用例3：测试空字符串
TEST(ValidIpv6AddrTest, EmptyString)
{
    std::string emptyString = "";
    EXPECT_FALSE(UbseNetUtil::ValidIpv6Addr(emptyString));
}

// 测试用例4：测试非IP地址字符串
TEST(ValidIpv6AddrTest, NonIpString)
{
    std::string nonIpString = "Hello, World!";
    EXPECT_FALSE(UbseNetUtil::ValidIpv6Addr(nonIpString));
}

// 测试用例5：测试IPv4地址
TEST(ValidIpv6AddrTest, Ipv4Address)
{
    std::string ipv4Address = "192.168.1.1";
    EXPECT_FALSE(UbseNetUtil::ValidIpv6Addr(ipv4Address));
}
TEST(UbseNetUtilTest, Ipv4StringToArr_ValidIp)
{
    std::string ip = "192.168.1.1";
    uint8_t arr[IPV4_MAX_LEN] = {0};
    EXPECT_TRUE(UbseNetUtil::Ipv4StringToArr(ip, arr));
    EXPECT_EQ(arr[0], 192);
    EXPECT_EQ(arr[1], 168);
    EXPECT_EQ(arr[2], 1);
    EXPECT_EQ(arr[3], 1);
}

// 测试用例2：测试无效的IPv4地址
TEST(UbseNetUtilTest, Ipv4StringToArr_InvalidIp)
{
    std::string ip = "256.256.256.256";
    uint8_t arr[IPV4_MAX_LEN] = {0};
    EXPECT_FALSE(UbseNetUtil::Ipv4StringToArr(ip, arr));
}

// 测试用例3：测试空的IPv4地址
TEST(UbseNetUtilTest, Ipv4StringToArr_EmptyIp)
{
    std::string ip = "";
    uint8_t arr[IPV4_MAX_LEN] = {0};
    EXPECT_FALSE(UbseNetUtil::Ipv4StringToArr(ip, arr));
}

// 测试用例5：测试内存拷贝失败的情况
TEST(UbseNetUtilTest, Ipv4StringToArr_MemoryCopyFail)
{
    std::string ip = "192.168.1.1";
    uint8_t *arr = nullptr;
    EXPECT_FALSE(UbseNetUtil::Ipv4StringToArr(ip, arr));
}
// 测试正常IPv6地址转换
TEST(UbseNetUtilTest, Ipv6ArrToString_Normal)
{
    uint8_t arr[16] = {0x20, 0x01, 0x0d, 0xb8, 0x85, 0xa3, 0x00, 0x00, 0x00, 0x00, 0x8a, 0x2e, 0x03, 0x70, 0x73, 0x34};
    std::string expected = "2001:db8:85a3::8a2e:370:7334";
    ASSERT_EQ(UbseNetUtil::Ipv6ArrToString(arr), expected);
}

// 测试全零IPv6地址转换
TEST(UbseNetUtilTest, Ipv6ArrToString_AllZero)
{
    uint8_t arr[16] = {0};
    std::string expected = "::";
    ASSERT_EQ(UbseNetUtil::Ipv6ArrToString(arr), expected);
}

// 测试获取地址
TEST(UbseNetUtilTest, GetIpInfo)
{
    std::vector<std::string> ips{};
    EXPECT_EQ(UbseNetUtil::GetIpInfo(ips), UBSE_OK);
    EXPECT_TRUE(!ips.empty());
}
} // namespace ubse::ut::utils