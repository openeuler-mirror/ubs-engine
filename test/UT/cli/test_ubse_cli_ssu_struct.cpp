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

#include <gtest/gtest.h>

#include "adapter_plugins/ssu/ubse_ssu_def.h"
#include "ubse_cli_ssu_struct.h"

namespace ubse::ut::cli {
using namespace ubse::cli::reg;
using namespace ubse::plugin::service::ssu;
namespace ssuDef = ubse::adapter_plugins::ssu::def;
namespace {
// 序列化往返辅助：将输入序列化后立即反序列化，用于验证各字段在编解码后保持一致。
// 期望在内部断言序列化/反序列化均成功，仅返回还原后的对象供用例逐字段比对。
template <typename T>
T RoundTrip(const T &input)
{
    ubse::serial::UbseSerialization serializer;
    EXPECT_TRUE(input.Serialize(serializer));
    T output;
    ubse::serial::UbseDeSerialization deserializer(serializer.GetBuffer(), serializer.GetLength());
    EXPECT_TRUE(output.Deserialize(deserializer));
    return output;
}

// 构造一份带运行用户身份的 identity，供请求往返用例复用。
UbseSsuAllocIdentityInfo MakeIdentity(const std::string &userName, uid_t uid)
{
    return UbseSsuAllocIdentityInfo{userName, uid};
}
} // namespace

// 摘要请求往返：identityInfo 的 userName/uid 编码后再解码应保持一致。
TEST(TestUbseCliSsuStruct, SummaryReqRoundTripKeepsIdentity)
{
    UbseCliSsuAllocSummaryReq input;
    input.identityInfo = MakeIdentity("ubse", 1000);

    auto output = RoundTrip(input);

    EXPECT_EQ(output.identityInfo.userName, input.identityInfo.userName);
    EXPECT_EQ(output.identityInfo.uid, input.identityInfo.uid);
}

// 详情请求往返：name 与 identityInfo 编码后再解码应保持一致。
TEST(TestUbseCliSsuStruct, DetailReqRoundTripKeepsNameAndIdentity)
{
    UbseCliSsuAllocDetailReq input;
    input.name = "alloc-space-1";
    input.identityInfo = MakeIdentity("ubse", 1000);

    auto output = RoundTrip(input);

    EXPECT_EQ(output.name, input.name);
    EXPECT_EQ(output.identityInfo.userName, input.identityInfo.userName);
    EXPECT_EQ(output.identityInfo.uid, input.identityInfo.uid);
}

// 创建请求往返：仅填必填/运行态字段时，可选字段应保留默认值且运行态字段不丢失，tenant 默认空。
TEST(TestUbseCliSsuStruct, CreateReqRoundTripKeepsDefaultsAndRuntimeFields)
{
    UbseCliSsuAllocCreateReq input;
    input.name = "alloc-space-1";
    input.nsSize = 10ULL * 1024 * 1024 * 1024;
    input.identityInfo = MakeIdentity("ubse", 1000);

    auto output = RoundTrip(input);

    EXPECT_EQ(output.name, input.name);
    EXPECT_EQ(output.nsSize, input.nsSize);
    EXPECT_EQ(output.nsNum, SSU_CLI_DEFAULT_NS_NUM);
    EXPECT_EQ(output.lbaFormat, UbseSsuLBAFormat::LBA_FORMAT_512);
    EXPECT_EQ(output.strategy, UbseSsuAllocStrategy::LINEAR);
    EXPECT_EQ(output.identityInfo.userName, input.identityInfo.userName);
    EXPECT_EQ(output.identityInfo.uid, input.identityInfo.uid);
    EXPECT_EQ(output.tenant, "");
}

// 创建请求往返：显式设置全部可选字段后，往返应逐一保持原值（含 tenant）。
TEST(TestUbseCliSsuStruct, CreateReqRoundTripKeepsExplicitOptions)
{
    UbseCliSsuAllocCreateReq input;
    input.name = "alloc-space-2";
    input.nsSize = 20ULL * 1024 * 1024 * 1024;
    input.nsNum = 2;
    input.lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_4K;
    input.strategy = UbseSsuAllocStrategy::STRIPED;
    input.identityInfo = MakeIdentity("tester", 1001);
    input.tenant = "tenant-a";

    auto output = RoundTrip(input);

    EXPECT_EQ(output.name, input.name);
    EXPECT_EQ(output.nsSize, input.nsSize);
    EXPECT_EQ(output.nsNum, input.nsNum);
    EXPECT_EQ(output.lbaFormat, input.lbaFormat);
    EXPECT_EQ(output.strategy, input.strategy);
    EXPECT_EQ(output.identityInfo.userName, input.identityInfo.userName);
    EXPECT_EQ(output.identityInfo.uid, input.identityInfo.uid);
    EXPECT_EQ(output.tenant, input.tenant);
}

// 摘要响应往返：多条分配（含不同策略）的 name/strategy/nameSpaceList 应保持一致。
TEST(TestUbseCliSsuStruct, ListRspRoundTripKeepsAllocations)
{
    UbseCliSsuAllocListRsp input;
    UbseCliSsuAllocResult alloc1;
    alloc1.name = "alloc-space-1";
    alloc1.strategy = UbseSsuAllocStrategy::LINEAR;
    alloc1.nameSpaceList = {
        {"e2", "nqn.2024-01:target", "uuid-aa", 1, "/dev/nvme0n1", 10ULL * 1024 * 1024 * 1024,
         UbseSsuLBAFormat::LBA_FORMAT_4K}};
    UbseCliSsuAllocResult alloc2;
    alloc2.name = "alloc-space-2";
    alloc2.strategy = UbseSsuAllocStrategy::STRIPED;
    alloc2.nameSpaceList = {
        {"e3", "nqn.2024-01:target", "uuid-bb", 1, "/dev/nvme0n2", 20ULL * 1024 * 1024 * 1024,
         UbseSsuLBAFormat::LBA_FORMAT_512}};
    input.allocations = {alloc1, alloc2};

    auto output = RoundTrip(input);

    ASSERT_EQ(output.allocations.size(), input.allocations.size());
    EXPECT_EQ(output.allocations[0].name, "alloc-space-1");
    EXPECT_EQ(output.allocations[0].strategy, UbseSsuAllocStrategy::LINEAR);
    ASSERT_EQ(output.allocations[0].nameSpaceList.size(), 1U);
    EXPECT_EQ(output.allocations[0].nameSpaceList[0].nsSize, 10ULL * 1024 * 1024 * 1024);
    EXPECT_EQ(output.allocations[1].name, "alloc-space-2");
    EXPECT_EQ(output.allocations[1].strategy, UbseSsuAllocStrategy::STRIPED);
    ASSERT_EQ(output.allocations[1].nameSpaceList.size(), 1U);
    EXPECT_EQ(output.allocations[1].nameSpaceList[0].nsSize, 20ULL * 1024 * 1024 * 1024);
}

// 分配结果往返：命名空间列表中 tgtEid/tgtNqn/nsUuid/namespaceId/nsDevPath/nsSize/lbaFormat 应完整保留。
TEST(TestUbseCliSsuStruct, AllocResultRoundTripKeepsNamespaces)
{
    UbseCliSsuAllocResult input;
    input.name = "alloc-space-1";
    input.strategy = UbseSsuAllocStrategy::LINEAR;
    input.nameSpaceList = {
        {"e2", "nqn.2024-01:target", "uuid-aa", 1, "/dev/nvme0n1", 10ULL * 1024 * 1024 * 1024,
         UbseSsuLBAFormat::LBA_FORMAT_4K},
        {"e2", "nqn.2024-01:target", "uuid-bb", 2, "/dev/nvme0n2", 10ULL * 1024 * 1024 * 1024,
         UbseSsuLBAFormat::LBA_FORMAT_512},
    };

    auto output = RoundTrip(input);

    EXPECT_EQ(output.name, input.name);
    EXPECT_EQ(output.strategy, input.strategy);
    ASSERT_EQ(output.nameSpaceList.size(), input.nameSpaceList.size());
    EXPECT_EQ(output.nameSpaceList[0].tgtEid, "e2");
    EXPECT_EQ(output.nameSpaceList[0].tgtNqn, "nqn.2024-01:target");
    EXPECT_EQ(output.nameSpaceList[0].nsUuid, "uuid-aa");
    EXPECT_EQ(output.nameSpaceList[0].namespaceId, 1U);
    EXPECT_EQ(output.nameSpaceList[0].nsDevPath, "/dev/nvme0n1");
    EXPECT_EQ(output.nameSpaceList[0].nsSize, 10ULL * 1024 * 1024 * 1024);
    EXPECT_EQ(output.nameSpaceList[0].lbaFormat, UbseSsuLBAFormat::LBA_FORMAT_4K);
    EXPECT_EQ(output.nameSpaceList[1].nsUuid, "uuid-bb");
    EXPECT_EQ(output.nameSpaceList[1].namespaceId, 2U);
    EXPECT_EQ(output.nameSpaceList[1].nsDevPath, "/dev/nvme0n2");
    EXPECT_EQ(output.nameSpaceList[1].lbaFormat, UbseSsuLBAFormat::LBA_FORMAT_512);
}

// 损坏缓冲应使反序列化失败：构造非预期布局的缓冲，验证 Deserialize 不静默成功。
TEST(TestUbseCliSsuStruct, BadBufferFailsDeserialize)
{
    ubse::serial::UbseSerialization serializer;
    serializer << std::string("bad-response");

    UbseCliSsuAllocResult output;
    ubse::serial::UbseDeSerialization deserializer(serializer.GetBuffer(), serializer.GetLength());

    EXPECT_FALSE(output.Deserialize(deserializer));
}

// 锁定枚举线报文数值：LBA/Strategy 的底层值变化即告警，防回退破坏与适配器层的对齐。
// 服务层已无 UbseSsuUsingType，此处不再断言该枚举。
TEST(TestUbseCliSsuStruct, UsesPluginServiceEnumWireValues)
{
    EXPECT_EQ(static_cast<uint32_t>(UbseSsuLBAFormat::LBA_FORMAT_512), 512U);
    EXPECT_EQ(static_cast<uint32_t>(UbseSsuLBAFormat::LBA_FORMAT_4K), 4096U);
    // 服务层 UbseSsuAllocStrategy 与适配器层 UbseSsuAddressingType 数值保持一致：
    // STRIPED = 0, LINEAR = 1
    EXPECT_EQ(static_cast<uint32_t>(UbseSsuAllocStrategy::STRIPED), 0U);
    EXPECT_EQ(static_cast<uint32_t>(UbseSsuAllocStrategy::LINEAR), 1U);
}

// CLI 上限 SSU_CLI_MAX_NS_NUM 必须复用底层 UBSE_SSU_MAX_HOST_NUM，保持单一来源
TEST(TestUbseCliSsuStruct, CliMaxNsNumReusesUnderlyingHostNum)
{
    EXPECT_EQ(SSU_CLI_MAX_NS_NUM, static_cast<uint32_t>(ssuDef::UBSE_SSU_MAX_HOST_NUM));
}

// ParseSize 的 1G 下限应显式绑定契约常量 SSU_CLI_MIN_SIZE_BYTES
TEST(TestUbseCliSsuStruct, MinSizeBytesConstantIsOneGib)
{
    constexpr uint64_t oneGib = 1024ULL * 1024ULL * 1024ULL;
    EXPECT_EQ(SSU_CLI_MIN_SIZE_BYTES, oneGib);
}
} // namespace ubse::ut::cli
