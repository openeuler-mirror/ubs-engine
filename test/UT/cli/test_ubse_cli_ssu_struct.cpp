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

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ubse_cli_ssu_struct.h"
#include "ubse_pack_util.h"

namespace ubse::ut::cli {
using namespace ubse::cli::reg;
using namespace ubse::plugin::service::ssu;
using ubse::utils::UbsePackUtil;
using ubse::utils::UbseUnpackUtil;

namespace {
template <typename T, typename = void>
struct HasIdentityInfo : std::false_type {};

template <typename T>
struct HasIdentityInfo<T, std::void_t<decltype(std::declval<T>().identityInfo)>> : std::true_type {};

static_assert(!HasIdentityInfo<UbseCliSsuAllocDetailReq>::value);
static_assert(!HasIdentityInfo<UbseCliSsuAllocCreateReq>::value);
static_assert(!HasIdentityInfo<UbseCliSsuAttachSpaceReq>::value);
static_assert(!HasIdentityInfo<UbseCliSsuAttachLinearReq>::value);
static_assert(!HasIdentityInfo<UbseCliSsuAttachStripedReq>::value);
static_assert(!HasIdentityInfo<UbseCliSsuDetachSpaceReq>::value);
static_assert(!HasIdentityInfo<UbseCliSsuDetachLinearReq>::value);
static_assert(!HasIdentityInfo<UbseCliSsuDetachStripedReq>::value);

uint32_t StringSize(const std::string& value)
{
    return static_cast<uint32_t>(sizeof(uint32_t) + value.size());
}

uint32_t NameSpaceSize(const UbseCliSsuNameSpaceInfo& value)
{
    uint32_t size = StringSize(value.tgtEid) + StringSize(value.tgtNqn) + StringSize(value.nsUuid) +
                    static_cast<uint32_t>(sizeof(uint32_t)) + StringSize(value.nsDevPath) +
                    static_cast<uint32_t>(sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint32_t));
    for (const auto& hostNqn : value.allowHostNqnList) {
        size += StringSize(hostNqn);
    }
    return size;
}

uint32_t AllocationSize(const UbseCliSsuAllocResult& value)
{
    uint32_t size = StringSize(value.name) + static_cast<uint32_t>(sizeof(uint8_t) + sizeof(uint32_t));
    for (const auto& nameSpace : value.nameSpaceList) {
        size += NameSpaceSize(nameSpace);
    }
    return size;
}

bool PackString(UbsePackUtil& pack, const std::string& value)
{
    return pack.UbsePackString(value, 1024);
}

bool PackNameSpace(UbsePackUtil& pack, const UbseCliSsuNameSpaceInfo& value)
{
    if (!PackString(pack, value.tgtEid) || !PackString(pack, value.tgtNqn) || !PackString(pack, value.nsUuid) ||
        !pack.UbsePackUint32(value.namespaceId) || !PackString(pack, value.nsDevPath) ||
        !pack.UbsePackUint64(value.nsSize) || !pack.UbsePackUint32(static_cast<uint32_t>(value.lbaFormat)) ||
        !pack.UbsePackUint32(static_cast<uint32_t>(value.allowHostNqnList.size()))) {
        return false;
    }
    for (const auto& hostNqn : value.allowHostNqnList) {
        if (!PackString(pack, hostNqn)) {
            return false;
        }
    }
    return true;
}

bool PackAllocation(UbsePackUtil& pack, const UbseCliSsuAllocResult& value)
{
    if (!PackString(pack, value.name) || !pack.UbsePackUint8(static_cast<uint8_t>(value.strategy)) ||
        !pack.UbsePackUint32(static_cast<uint32_t>(value.nameSpaceList.size()))) {
        return false;
    }
    for (const auto& nameSpace : value.nameSpaceList) {
        if (!PackNameSpace(pack, nameSpace)) {
            return false;
        }
    }
    return true;
}

std::vector<uint8_t> PackAllocationList(const std::vector<UbseCliSsuAllocResult>& values)
{
    uint32_t size = sizeof(uint32_t);
    for (const auto& value : values) {
        size += AllocationSize(value);
    }
    std::vector<uint8_t> payload(size);
    UbsePackUtil pack(payload.data(), payload.size());
    EXPECT_TRUE(pack.UbsePackUint32(static_cast<uint32_t>(values.size())));
    for (const auto& value : values) {
        EXPECT_TRUE(PackAllocation(pack, value));
    }
    return payload;
}

std::vector<uint8_t> PackAttachSpaceResponse(const std::vector<std::string>& nsDevPaths)
{
    uint32_t size = sizeof(uint32_t);
    for (const auto& path : nsDevPaths) {
        size += StringSize(path);
    }
    std::vector<uint8_t> payload(size);
    UbsePackUtil pack(payload.data(), payload.size());
    EXPECT_TRUE(pack.UbsePackUint32(static_cast<uint32_t>(nsDevPaths.size())));
    for (const auto& path : nsDevPaths) {
        EXPECT_TRUE(PackString(pack, path));
    }
    return payload;
}

std::vector<uint8_t> PackAttachAggregatedResponse(const std::vector<std::string>& nsDevPaths,
                                                  const std::string& devPath)
{
    auto payload = PackAttachSpaceResponse(nsDevPaths);
    payload.resize(payload.size() + StringSize(devPath));
    UbsePackUtil pack(payload.data(), payload.size());
    EXPECT_TRUE(pack.UbsePackUint32(static_cast<uint32_t>(nsDevPaths.size())));
    for (const auto& path : nsDevPaths) {
        EXPECT_TRUE(PackString(pack, path));
    }
    EXPECT_TRUE(PackString(pack, devPath));
    return payload;
}

UbseCliSsuAllocResult MakeAllocation(const std::string& name, UbseSsuAllocStrategy strategy, uint32_t namespaceId)
{
    UbseCliSsuAllocResult value;
    value.name = name;
    value.strategy = strategy;
    value.nameSpaceList = {
        {"eid-1",
         "nqn.2026-07:target",
         "uuid-1",
         namespaceId,
         "/dev/nvme0n1",
         10ULL * 1024ULL * 1024ULL,
         UbseSsuLBAFormat::LBA_FORMAT_4K,
         {"nqn.2026-07:host-a", "nqn.2026-07:host-b"}},
        {"eid-2",
         "nqn.2026-07:target",
         "uuid-2",
         namespaceId + 1,
         "/dev/nvme0n2",
         20ULL * 1024ULL * 1024ULL,
         UbseSsuLBAFormat::LBA_FORMAT_512,
         {"nqn.2026-07:host-c"}},
    };
    return value;
}

template <typename Request>
UbseUnpackUtil SerializeAndOpen(const Request& request, std::vector<uint8_t>& payload)
{
    EXPECT_TRUE(request.Serialize(payload));
    return UbseUnpackUtil(payload.data(), static_cast<uint32_t>(payload.size()));
}

void ExpectSpaceFields(UbseUnpackUtil& unpack, const std::string& name, const std::string& hostNqn,
                       const std::string& srcEid)
{
    std::string actualName;
    std::string actualHostNqn;
    std::string actualSrcEid;
    EXPECT_TRUE(unpack.UnpackString(actualName, SSU_CLI_MAX_NAME_LENGTH));
    EXPECT_TRUE(unpack.UnpackString(actualHostNqn, SSU_CLI_MAX_HOST_NQN_LENGTH));
    EXPECT_TRUE(unpack.UnpackString(actualSrcEid, SSU_CLI_MAX_SRC_EID_LENGTH));
    EXPECT_EQ(actualName, name);
    EXPECT_EQ(actualHostNqn, hostNqn);
    EXPECT_EQ(actualSrcEid, srcEid);
}
} // namespace

// 详情请求应编码为裸长度前缀字符串，payload 不携带 C 字符串结尾的空字符。
TEST(TestUbseCliSsuStruct, DetailRequestIsBareLengthPrefixedStringWithoutNullTerminator)
{
    UbseCliSsuAllocDetailReq request{"alloc-a"};
    std::vector<uint8_t> payload;
    ASSERT_TRUE(request.Serialize(payload));
    ASSERT_EQ(payload.size(), sizeof(uint32_t) + request.name.size());
    uint32_t length = 0;
    std::memcpy(&length, payload.data(), sizeof(length));
    EXPECT_EQ(length, request.name.size());
    EXPECT_EQ(std::memcmp(payload.data() + sizeof(uint32_t), request.name.data(), request.name.size()), 0);
}

// 释放请求的 name 应接受协议上限；正向值覆盖大小写字母、数字及每个合法符号等价类。
// 在该合法边界值上再增加一个字符应直接失败且不改写已有 payload。
TEST(TestUbseCliSsuStruct, FreeSpaceRequestEnforcesNameLengthBoundary)
{
    std::string maxLengthName = "Aa0.:-_";
    maxLengthName.append(SSU_CLI_MAX_NAME_LENGTH - maxLengthName.size(), 'a');
    ASSERT_EQ(maxLengthName.size(), SSU_CLI_MAX_NAME_LENGTH);
    const UbseCliSsuFreeSpaceReq validRequest{maxLengthName};
    std::vector<uint8_t> payload;
    auto unpack = SerializeAndOpen(validRequest, payload);
    std::string actualName;
    ASSERT_TRUE(unpack.UnpackString(actualName, SSU_CLI_MAX_NAME_LENGTH));
    EXPECT_EQ(actualName, maxLengthName);
    EXPECT_EQ(payload.size(), sizeof(uint32_t) + maxLengthName.size());

    const UbseCliSsuFreeSpaceReq overlongRequest{maxLengthName + "a"};
    const auto validPayload = payload;
    EXPECT_FALSE(overlongRequest.Serialize(payload));
    EXPECT_EQ(payload, validPayload);
}

// create 请求的字段顺序及枚举宽度必须与服务端 handler 解包契约逐字段一致。
TEST(TestUbseCliSsuStruct, CreateRequestMatchesHandlerFieldOrderAndEnumWidths)
{
    UbseCliSsuAllocCreateReq request;
    request.name = "alloc-a";
    request.nsSize = 10ULL * 1024ULL * 1024ULL * 1024ULL;
    request.nsNum = 2;
    request.lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_4K;
    request.strategy = UbseSsuAllocStrategy::STRIPED;
    request.tenant = "tenant-a";
    std::vector<uint8_t> payload;
    auto unpack = SerializeAndOpen(request, payload);
    std::string name;
    std::string tenant;
    uint64_t size = 0;
    uint32_t count = 0;
    uint32_t lbaFormat = 0;
    uint8_t strategy = UINT8_MAX;
    EXPECT_TRUE(unpack.UnpackString(name, SSU_CLI_MAX_NAME_LENGTH));
    EXPECT_TRUE(unpack.UnpackUint64(size));
    EXPECT_TRUE(unpack.UnpackUint32(count));
    EXPECT_TRUE(unpack.UnpackUint32(lbaFormat));
    EXPECT_TRUE(unpack.UnpackUint8(strategy));
    EXPECT_TRUE(unpack.UnpackString(tenant, SSU_CLI_MAX_TENANT_LENGTH));
    EXPECT_EQ(name, request.name);
    EXPECT_EQ(size, request.nsSize);
    EXPECT_EQ(count, request.nsNum);
    EXPECT_EQ(lbaFormat, 4096U);
    EXPECT_EQ(strategy, 0U);
    EXPECT_EQ(tenant, request.tenant);
}

// 普通、Linear、Striped 三类 attach 请求应按各自服务端字段顺序和枚举宽度编码。
TEST(TestUbseCliSsuStruct, AttachRequestsMatchSpaceLinearAndStripedUnpackers)
{
    UbseCliSsuAttachSpaceReq space{"alloc-a", "host-a", "eid-a"};
    std::vector<uint8_t> payload;
    auto spaceUnpack = SerializeAndOpen(space, payload);
    ExpectSpaceFields(spaceUnpack, "alloc-a", "host-a", "eid-a");

    UbseCliSsuAttachLinearReq linear{"alloc-a", "host-a", "eid-a", "linear0"};
    auto linearUnpack = SerializeAndOpen(linear, payload);
    ExpectSpaceFields(linearUnpack, "alloc-a", "host-a", "eid-a");
    std::string devName;
    EXPECT_TRUE(linearUnpack.UnpackString(devName, SSU_CLI_MAX_DEV_NAME_LENGTH));
    EXPECT_EQ(devName, "linear0");

    UbseCliSsuAttachStripedReq striped{"alloc-a",
                                       "host-a",
                                       "eid-a",
                                       "striped0",
                                       UbseSsuAggregationRaidLevel::RAID5,
                                       UbseSsuChunkSize::CHUNK_SIZE_256K};
    auto stripedUnpack = SerializeAndOpen(striped, payload);
    ExpectSpaceFields(stripedUnpack, "alloc-a", "host-a", "eid-a");
    uint8_t level = 0;
    uint32_t chunkSize = 0;
    EXPECT_TRUE(stripedUnpack.UnpackString(devName, SSU_CLI_MAX_DEV_NAME_LENGTH));
    EXPECT_TRUE(stripedUnpack.UnpackUint8(level));
    EXPECT_TRUE(stripedUnpack.UnpackUint32(chunkSize));
    EXPECT_EQ(devName, "striped0");
    EXPECT_EQ(level, 5U);
    EXPECT_EQ(chunkSize, 256U);
}

// 三类 detach 请求应携带服务端仍要求的 src_eid、level、chunk_size 协议占位默认值。
TEST(TestUbseCliSsuStruct, DetachRequestsIncludeHandlerPlaceholders)
{
    std::vector<uint8_t> payload;
    UbseCliSsuDetachSpaceReq space;
    space.name = "alloc-a";
    space.hostNqn = "host-a";
    auto spaceUnpack = SerializeAndOpen(space, payload);
    ExpectSpaceFields(spaceUnpack, "alloc-a", "host-a", "");

    UbseCliSsuDetachLinearReq linear;
    linear.name = "alloc-a";
    linear.hostNqn = "host-a";
    linear.devName = "linear0";
    auto linearUnpack = SerializeAndOpen(linear, payload);
    ExpectSpaceFields(linearUnpack, "alloc-a", "host-a", "");
    std::string devName;
    EXPECT_TRUE(linearUnpack.UnpackString(devName, SSU_CLI_MAX_DEV_NAME_LENGTH));
    EXPECT_EQ(devName, "linear0");

    UbseCliSsuDetachStripedReq striped;
    striped.name = "alloc-a";
    striped.hostNqn = "host-a";
    striped.devName = "striped0";
    auto stripedUnpack = SerializeAndOpen(striped, payload);
    ExpectSpaceFields(stripedUnpack, "alloc-a", "host-a", "");
    uint8_t level = UINT8_MAX;
    uint32_t chunkSize = 0;
    EXPECT_TRUE(stripedUnpack.UnpackString(devName, SSU_CLI_MAX_DEV_NAME_LENGTH));
    EXPECT_TRUE(stripedUnpack.UnpackUint8(level));
    EXPECT_TRUE(stripedUnpack.UnpackUint32(chunkSize));
    EXPECT_EQ(level, static_cast<uint8_t>(UbseSsuAggregationRaidLevel::RAID0));
    EXPECT_EQ(chunkSize, static_cast<uint32_t>(UbseSsuChunkSize::CHUNK_SIZE_4K));
}

// 普通 attach 响应应按服务输出顺序完整解码命名空间设备路径列表。
TEST(TestUbseCliSsuStruct, AttachSpaceResponseDecodesNamespacePathsInServiceOutputOrder)
{
    const std::vector<std::string> nsDevPaths = {"/dev/nvme0n1", "/dev/nvme1n1"};
    const auto payload = PackAttachSpaceResponse(nsDevPaths);
    UbseCliSsuAttachSpaceRsp response;
    ASSERT_TRUE(response.Deserialize(payload.data(), static_cast<uint32_t>(payload.size())));
    EXPECT_EQ(response.nsDevPaths, nsDevPaths);
}

// 聚合 attach 响应应先解码命名空间路径列表，再解码最终聚合设备路径。
TEST(TestUbseCliSsuStruct, AggregatedAttachResponseDecodesNamespacePathsBeforeDevicePath)
{
    const std::vector<std::string> nsDevPaths = {"/dev/nvme0n1", "/dev/nvme1n1"};
    const std::string devPath = "/dev/ubse_ssu0";
    const auto payload = PackAttachAggregatedResponse(nsDevPaths, devPath);
    UbseCliSsuAttachAggregatedRsp response;
    ASSERT_TRUE(response.Deserialize(payload.data(), static_cast<uint32_t>(payload.size())));
    EXPECT_EQ(response.nsDevPaths, nsDevPaths);
    EXPECT_EQ(response.devPath, devPath);
}

// allocation 列表应完整消费多个分配、命名空间和允许主机 NQN，并保持各字段归属关系。
TEST(TestUbseCliSsuStruct, AllocationListConsumesMultipleNamespacesAndAllowedHostNqns)
{
    const auto first = MakeAllocation("alloc-a", UbseSsuAllocStrategy::LINEAR, 1);
    const auto second = MakeAllocation("alloc-b", UbseSsuAllocStrategy::STRIPED, 3);
    const auto payload = PackAllocationList({first, second});
    UbseCliSsuAllocListRsp response;
    ASSERT_TRUE(response.Deserialize(payload.data(), static_cast<uint32_t>(payload.size())));
    ASSERT_EQ(response.allocations.size(), 2U);
    EXPECT_EQ(response.allocations[0].nameSpaceList[0].allowHostNqnList,
              (std::vector<std::string>{"nqn.2026-07:host-a", "nqn.2026-07:host-b"}));
    EXPECT_EQ(response.allocations[0].nameSpaceList[1].nsUuid, "uuid-2");
    EXPECT_EQ(response.allocations[1].name, "alloc-b");
    EXPECT_EQ(response.allocations[1].strategy, UbseSsuAllocStrategy::STRIPED);
}

// 响应截断、伪造字符串长度和空成功响应都应在反序列化阶段拒绝。
TEST(TestUbseCliSsuStruct, ResponsesRejectTruncationAndInvalidStringLength)
{
    auto allocation = MakeAllocation("alloc-a", UbseSsuAllocStrategy::LINEAR, 1);
    auto payload = PackAllocationList({allocation});
    payload.pop_back();
    UbseCliSsuAllocListRsp response;
    EXPECT_FALSE(response.Deserialize(payload.data(), static_cast<uint32_t>(payload.size())));

    std::vector<uint8_t> invalidLength(sizeof(uint32_t));
    UbsePackUtil pack(invalidLength.data(), invalidLength.size());
    ASSERT_TRUE(pack.UbsePackUint32(64));
    UbseCliSsuAttachSpaceRsp attachResponse;
    EXPECT_FALSE(attachResponse.Deserialize(invalidLength.data(), static_cast<uint32_t>(invalidLength.size())));
    EXPECT_FALSE(attachResponse.Deserialize(nullptr, 0));
}

// 聚合 attach 响应缺少尾部 dev_path 字段时不得被当作普通 attach 成功响应接受。
TEST(TestUbseCliSsuStruct, AggregatedAttachResponseRejectsMissingDevicePath)
{
    auto payload = PackAttachSpaceResponse({"/dev/nvme0n1", "/dev/nvme1n1"});
    UbseCliSsuAttachAggregatedRsp response;
    EXPECT_FALSE(response.Deserialize(payload.data(), static_cast<uint32_t>(payload.size())));
}

// 响应字符串超过设备路径或分配名称上限一个字符时应拒绝，固化 max+1 失败边界。
TEST(TestUbseCliSsuStruct, ResponsesRejectStringsOnePastCliLimits)
{
    const auto pathPayload = PackAttachSpaceResponse({std::string(SSU_CLI_MAX_DEV_PATH_LENGTH + 1, 'a')});
    UbseCliSsuAttachSpaceRsp attachResponse;
    EXPECT_FALSE(attachResponse.Deserialize(pathPayload.data(), static_cast<uint32_t>(pathPayload.size())));

    auto allocation = MakeAllocation(std::string(SSU_CLI_MAX_NAME_LENGTH + 1, 'a'), UbseSsuAllocStrategy::LINEAR, 1);
    const auto allocationPayload = PackAllocationList({allocation});
    UbseCliSsuAllocListRsp allocationResponse;
    EXPECT_FALSE(
        allocationResponse.Deserialize(allocationPayload.data(), static_cast<uint32_t>(allocationPayload.size())));
}

// allocation 数量超过防御上限、namespace 数量超过业务上限时应在预分配前拒绝。
TEST(TestUbseCliSsuStruct, ResponsesRejectOversizedLists)
{
    std::vector<uint8_t> payload(sizeof(uint32_t));
    UbsePackUtil allocationCountPack(payload.data(), payload.size());
    ASSERT_TRUE(allocationCountPack.UbsePackUint32((1U << 16) + 1));
    UbseCliSsuAllocListRsp listResponse;
    EXPECT_FALSE(listResponse.Deserialize(payload.data(), static_cast<uint32_t>(payload.size())));

    const std::string name = "alloc-a";
    payload.resize(StringSize(name) + sizeof(uint8_t) + sizeof(uint32_t));
    UbsePackUtil namespaceCountPack(payload.data(), payload.size());
    ASSERT_TRUE(PackString(namespaceCountPack, name));
    ASSERT_TRUE(namespaceCountPack.UbsePackUint8(static_cast<uint8_t>(UbseSsuAllocStrategy::LINEAR)));
    ASSERT_TRUE(namespaceCountPack.UbsePackUint32(SSU_CLI_MAX_NS_NUM + 1));
    UbseCliSsuAllocResult allocationResponse;
    EXPECT_FALSE(allocationResponse.Deserialize(payload.data(), static_cast<uint32_t>(payload.size())));
}

// allowHostNqnList 使用独立防御上限，不应错误复用 128 个 namespace 的业务上限。
TEST(TestUbseCliSsuStruct, ResponsesAcceptAllowedHostListBeyondNamespaceLimit)
{
    auto allocation = MakeAllocation("alloc-a", UbseSsuAllocStrategy::LINEAR, 1);
    allocation.nameSpaceList.resize(1);
    auto& allowedHosts = allocation.nameSpaceList[0].allowHostNqnList;
    allowedHosts.assign(SSU_CLI_MAX_NS_NUM + 1, "nqn.2026-07:host-a");

    const auto payload = PackAllocationList({allocation});
    UbseCliSsuAllocListRsp response;
    ASSERT_TRUE(response.Deserialize(payload.data(), static_cast<uint32_t>(payload.size())));
    ASSERT_EQ(response.allocations.size(), 1U);
    ASSERT_EQ(response.allocations[0].nameSpaceList.size(), 1U);
    EXPECT_EQ(response.allocations[0].nameSpaceList[0].allowHostNqnList, allowedHosts);
}

// 允许主机列表超过防御上限，以及响应 strategy/LBA 原始值不在枚举集合时均应拒绝。
TEST(TestUbseCliSsuStruct, ResponsesRejectDefensiveHostLimitAndInvalidEnums)
{
    auto allocation = MakeAllocation("alloc-a", UbseSsuAllocStrategy::LINEAR, 1);
    allocation.nameSpaceList.resize(1);
    allocation.nameSpaceList[0].allowHostNqnList.clear();
    auto payload = PackAllocationList({allocation});
    uint32_t tooManyHosts = SSU_CLI_MAX_DESERIALIZED_HOST_NQNS + 1;
    std::memcpy(payload.data() + payload.size() - sizeof(uint32_t), &tooManyHosts, sizeof(tooManyHosts));
    UbseCliSsuAllocListRsp response;
    EXPECT_FALSE(response.Deserialize(payload.data(), static_cast<uint32_t>(payload.size())));

    payload = PackAllocationList({allocation});
    const size_t strategyOffset = sizeof(uint32_t) + StringSize(allocation.name);
    payload[strategyOffset] = UINT8_MAX;
    EXPECT_FALSE(response.Deserialize(payload.data(), static_cast<uint32_t>(payload.size())));

    payload = PackAllocationList({allocation});
    const auto& nameSpace = allocation.nameSpaceList[0];
    const size_t lbaOffset = sizeof(uint32_t) + StringSize(allocation.name) + sizeof(uint8_t) + sizeof(uint32_t) +
                             StringSize(nameSpace.tgtEid) + StringSize(nameSpace.tgtNqn) +
                             StringSize(nameSpace.nsUuid) + sizeof(uint32_t) + StringSize(nameSpace.nsDevPath) +
                             sizeof(uint64_t);
    uint32_t invalidLba = 1234;
    std::memcpy(payload.data() + lbaOffset, &invalidLba, sizeof(invalidLba));
    EXPECT_FALSE(response.Deserialize(payload.data(), static_cast<uint32_t>(payload.size())));
}

// 请求字符串 max+1 和各协议枚举非法原始值应使序列化失败，不产出可发送请求。
TEST(TestUbseCliSsuStruct, RequestsRejectOverlongStringsAndInvalidEnums)
{
    UbseCliSsuAllocDetailReq detail{std::string(SSU_CLI_MAX_NAME_LENGTH + 1, 'a')};
    std::vector<uint8_t> payload;
    EXPECT_FALSE(detail.Serialize(payload));

    UbseCliSsuAllocCreateReq create;
    create.name = "alloc-a";
    create.tenant = std::string(SSU_CLI_MAX_TENANT_LENGTH + 1, 'a');
    EXPECT_FALSE(create.Serialize(payload));
    create.tenant.clear();
    create.lbaFormat = static_cast<UbseSsuLBAFormat>(1234);
    EXPECT_FALSE(create.Serialize(payload));
    create.lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_512;
    create.strategy = static_cast<UbseSsuAllocStrategy>(9);
    EXPECT_FALSE(create.Serialize(payload));

    UbseCliSsuAttachSpaceReq space;
    space.name = "alloc-a";
    space.hostNqn = std::string(SSU_CLI_MAX_HOST_NQN_LENGTH + 1, 'a');
    EXPECT_FALSE(space.Serialize(payload));
    space.hostNqn.clear();
    space.srcEid = std::string(SSU_CLI_MAX_SRC_EID_LENGTH + 1, 'a');
    EXPECT_FALSE(space.Serialize(payload));

    UbseCliSsuAttachLinearReq linear;
    linear.name = "alloc-a";
    linear.devName = std::string(SSU_CLI_MAX_DEV_NAME_LENGTH + 1, 'a');
    EXPECT_FALSE(linear.Serialize(payload));

    UbseCliSsuAttachStripedReq striped;
    striped.name = "alloc-a";
    striped.level = static_cast<UbseSsuAggregationRaidLevel>(9);
    EXPECT_FALSE(striped.Serialize(payload));
    striped.level = UbseSsuAggregationRaidLevel::RAID0;
    striped.chunkSize = static_cast<UbseSsuChunkSize>(8);
    EXPECT_FALSE(striped.Serialize(payload));
}

// 所有请求字符串字段恰好等于各自线协议上限时都应成功序列化并原样解包。
TEST(TestUbseCliSsuStruct, RequestsAcceptStringsAtCliLimits)
{
    std::vector<uint8_t> payload;
    const std::string name(SSU_CLI_MAX_NAME_LENGTH, 'n');
    const std::string tenant(SSU_CLI_MAX_TENANT_LENGTH, 't');
    const std::string hostNqn(SSU_CLI_MAX_HOST_NQN_LENGTH, 'h');
    const std::string srcEid(SSU_CLI_MAX_SRC_EID_LENGTH, 'e');
    const std::string devName(SSU_CLI_MAX_DEV_NAME_LENGTH, 'd');

    const UbseCliSsuAllocDetailReq detail{name};
    auto detailUnpack = SerializeAndOpen(detail, payload);
    std::string actualName;
    ASSERT_TRUE(detailUnpack.UnpackString(actualName, SSU_CLI_MAX_NAME_LENGTH));
    EXPECT_EQ(actualName, name);

    UbseCliSsuAllocCreateReq create;
    create.name = name;
    create.tenant = tenant;
    auto createUnpack = SerializeAndOpen(create, payload);
    std::string actualTenant;
    uint64_t actualSize = 0;
    uint32_t actualNsNum = 0;
    uint32_t actualLba = 0;
    uint8_t actualStrategy = UINT8_MAX;
    ASSERT_TRUE(createUnpack.UnpackString(actualName, SSU_CLI_MAX_NAME_LENGTH));
    ASSERT_TRUE(createUnpack.UnpackUint64(actualSize));
    ASSERT_TRUE(createUnpack.UnpackUint32(actualNsNum));
    ASSERT_TRUE(createUnpack.UnpackUint32(actualLba));
    ASSERT_TRUE(createUnpack.UnpackUint8(actualStrategy));
    ASSERT_TRUE(createUnpack.UnpackString(actualTenant, SSU_CLI_MAX_TENANT_LENGTH));
    EXPECT_EQ(actualName, create.name);
    EXPECT_EQ(actualSize, create.nsSize);
    EXPECT_EQ(actualNsNum, create.nsNum);
    EXPECT_EQ(actualLba, static_cast<uint32_t>(create.lbaFormat));
    EXPECT_EQ(actualStrategy, static_cast<uint8_t>(create.strategy));
    EXPECT_EQ(actualTenant, create.tenant);

    const UbseCliSsuAttachSpaceReq attachSpace{name, hostNqn, srcEid};
    auto attachSpaceUnpack = SerializeAndOpen(attachSpace, payload);
    ExpectSpaceFields(attachSpaceUnpack, name, hostNqn, srcEid);

    const UbseCliSsuAttachLinearReq attachLinear{name, hostNqn, srcEid, devName};
    auto attachLinearUnpack = SerializeAndOpen(attachLinear, payload);
    ExpectSpaceFields(attachLinearUnpack, name, hostNqn, srcEid);
    std::string actualDevName;
    ASSERT_TRUE(attachLinearUnpack.UnpackString(actualDevName, SSU_CLI_MAX_DEV_NAME_LENGTH));
    EXPECT_EQ(actualDevName, devName);

    const UbseCliSsuAttachStripedReq attachStriped{name, hostNqn, srcEid, devName};
    auto attachStripedUnpack = SerializeAndOpen(attachStriped, payload);
    ExpectSpaceFields(attachStripedUnpack, name, hostNqn, srcEid);
    ASSERT_TRUE(attachStripedUnpack.UnpackString(actualDevName, SSU_CLI_MAX_DEV_NAME_LENGTH));
    EXPECT_EQ(actualDevName, devName);

    const UbseCliSsuDetachSpaceReq detachSpace{name, hostNqn};
    auto detachSpaceUnpack = SerializeAndOpen(detachSpace, payload);
    ExpectSpaceFields(detachSpaceUnpack, name, hostNqn, "");

    const UbseCliSsuDetachLinearReq detachLinear{name, hostNqn, "", devName};
    auto detachLinearUnpack = SerializeAndOpen(detachLinear, payload);
    ExpectSpaceFields(detachLinearUnpack, name, hostNqn, "");
    ASSERT_TRUE(detachLinearUnpack.UnpackString(actualDevName, SSU_CLI_MAX_DEV_NAME_LENGTH));
    EXPECT_EQ(actualDevName, devName);

    const UbseCliSsuDetachStripedReq detachStriped{name, hostNqn, "", devName};
    auto detachStripedUnpack = SerializeAndOpen(detachStriped, payload);
    ExpectSpaceFields(detachStripedUnpack, name, hostNqn, "");
    ASSERT_TRUE(detachStripedUnpack.UnpackString(actualDevName, SSU_CLI_MAX_DEV_NAME_LENGTH));
    EXPECT_EQ(actualDevName, devName);
}

// 响应中的 name/EID/NQN/UUID/设备路径/允许主机 NQN 恰好达到字符串上限时应完整反序列化。
TEST(TestUbseCliSsuStruct, ResponsesAcceptStringsAtCliLimits)
{
    const std::string devPath(SSU_CLI_MAX_DEV_PATH_LENGTH, 'p');
    auto allocation = MakeAllocation(std::string(SSU_CLI_MAX_NAME_LENGTH, 'n'), UbseSsuAllocStrategy::LINEAR, 1);
    allocation.nameSpaceList.resize(1);
    auto& nameSpace = allocation.nameSpaceList[0];
    nameSpace.tgtEid = std::string(SSU_CLI_MAX_SRC_EID_LENGTH, 'e');
    nameSpace.tgtNqn = std::string(SSU_CLI_MAX_HOST_NQN_LENGTH, 'q');
    nameSpace.nsUuid = std::string(SSU_CLI_MAX_UUID_LENGTH, 'u');
    nameSpace.nsDevPath = devPath;
    nameSpace.allowHostNqnList = {std::string(SSU_CLI_MAX_HOST_NQN_LENGTH, 'h')};

    const auto allocationPayload = PackAllocationList({allocation});
    UbseCliSsuAllocListRsp allocationResponse;
    ASSERT_TRUE(
        allocationResponse.Deserialize(allocationPayload.data(), static_cast<uint32_t>(allocationPayload.size())));
    ASSERT_EQ(allocationResponse.allocations.size(), 1U);
    const auto& actualAllocation = allocationResponse.allocations[0];
    EXPECT_EQ(actualAllocation.name, allocation.name);
    EXPECT_EQ(actualAllocation.strategy, allocation.strategy);
    ASSERT_EQ(actualAllocation.nameSpaceList.size(), 1U);
    const auto& actualNameSpace = actualAllocation.nameSpaceList[0];
    EXPECT_EQ(actualNameSpace.tgtEid, nameSpace.tgtEid);
    EXPECT_EQ(actualNameSpace.tgtNqn, nameSpace.tgtNqn);
    EXPECT_EQ(actualNameSpace.nsUuid, nameSpace.nsUuid);
    EXPECT_EQ(actualNameSpace.namespaceId, nameSpace.namespaceId);
    EXPECT_EQ(actualNameSpace.nsDevPath, nameSpace.nsDevPath);
    EXPECT_EQ(actualNameSpace.nsSize, nameSpace.nsSize);
    EXPECT_EQ(actualNameSpace.lbaFormat, nameSpace.lbaFormat);
    EXPECT_EQ(actualNameSpace.allowHostNqnList, nameSpace.allowHostNqnList);

    const auto attachPayload = PackAttachSpaceResponse({devPath});
    UbseCliSsuAttachSpaceRsp attachResponse;
    ASSERT_TRUE(attachResponse.Deserialize(attachPayload.data(), static_cast<uint32_t>(attachPayload.size())));
    EXPECT_EQ(attachResponse.nsDevPaths, (std::vector<std::string>{devPath}));

    const auto aggregatedPayload = PackAttachAggregatedResponse({devPath}, devPath);
    UbseCliSsuAttachAggregatedRsp aggregatedResponse;
    ASSERT_TRUE(
        aggregatedResponse.Deserialize(aggregatedPayload.data(), static_cast<uint32_t>(aggregatedPayload.size())));
    EXPECT_EQ(aggregatedResponse.nsDevPaths, (std::vector<std::string>{devPath}));
    EXPECT_EQ(aggregatedResponse.devPath, devPath);
}

// 命名空间列表与 attach 设备路径列表恰好为业务上限 128 时应被接受，和 129 拒绝路径成对。
TEST(TestUbseCliSsuStruct, ResponsesAcceptNamespaceAndDevicePathListLimits)
{
    auto allocation = MakeAllocation("alloc-a", UbseSsuAllocStrategy::LINEAR, 1);
    allocation.nameSpaceList.resize(SSU_CLI_MAX_NS_NUM);
    const auto allocationPayload = PackAllocationList({allocation});
    UbseCliSsuAllocListRsp allocationResponse;
    ASSERT_TRUE(
        allocationResponse.Deserialize(allocationPayload.data(), static_cast<uint32_t>(allocationPayload.size())));
    EXPECT_EQ(allocationResponse.allocations[0].nameSpaceList.size(), SSU_CLI_MAX_NS_NUM);

    const std::vector<std::string> paths(SSU_CLI_MAX_NS_NUM, "/dev/nvme0n1");
    const auto attachPayload = PackAttachSpaceResponse(paths);
    UbseCliSsuAttachSpaceRsp attachResponse;
    ASSERT_TRUE(attachResponse.Deserialize(attachPayload.data(), static_cast<uint32_t>(attachPayload.size())));
    EXPECT_EQ(attachResponse.nsDevPaths.size(), SSU_CLI_MAX_NS_NUM);
}

// 防御性 allocation 数量恰好为 65536 时应成功，证明保护条件是严格的大于上限才拒绝。
TEST(TestUbseCliSsuStruct, ResponseAcceptsAllocationListAtDefensiveLimit)
{
    UbseCliSsuAllocResult allocation;
    allocation.name = "a";
    const std::vector<UbseCliSsuAllocResult> allocations(SSU_CLI_MAX_DESERIALIZED_ALLOCATIONS, allocation);
    const auto payload = PackAllocationList(allocations);
    UbseCliSsuAllocListRsp response;
    ASSERT_TRUE(response.Deserialize(payload.data(), static_cast<uint32_t>(payload.size())));
    EXPECT_EQ(response.allocations.size(), SSU_CLI_MAX_DESERIALIZED_ALLOCATIONS);
}

// 防御性允许主机 NQN 数量恰好为 65536 时应成功，证明 host 列表上限是闭区间。
TEST(TestUbseCliSsuStruct, ResponseAcceptsAllowedHostListAtDefensiveLimit)
{
    auto allocation = MakeAllocation("alloc-a", UbseSsuAllocStrategy::LINEAR, 1);
    allocation.nameSpaceList.resize(1);
    allocation.nameSpaceList[0].allowHostNqnList.assign(SSU_CLI_MAX_DESERIALIZED_HOST_NQNS, "h");
    const auto payload = PackAllocationList({allocation});
    UbseCliSsuAllocListRsp response;
    ASSERT_TRUE(response.Deserialize(payload.data(), static_cast<uint32_t>(payload.size())));
    EXPECT_EQ(response.allocations[0].nameSpaceList[0].allowHostNqnList.size(), SSU_CLI_MAX_DESERIALIZED_HOST_NQNS);
}

// 对外使用的核心边界常量应保持 name=47、ns_num=128、最小 size=1GiB 的产品契约。
TEST(TestUbseCliSsuStruct, PublicConstantsReuseSsuLimits)
{
    EXPECT_EQ(SSU_CLI_MAX_NAME_LENGTH, 47U);
    EXPECT_EQ(SSU_CLI_MAX_NS_NUM, 128U);
    EXPECT_EQ(SSU_CLI_MIN_SIZE_BYTES, 1024ULL * 1024ULL * 1024ULL);
}
} // namespace ubse::ut::cli
