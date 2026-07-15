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

uint32_t StringSize(const std::string &value)
{
    return static_cast<uint32_t>(sizeof(uint32_t) + value.size());
}

uint32_t NameSpaceSize(const UbseCliSsuNameSpaceInfo &value)
{
    uint32_t size = StringSize(value.tgtEid) + StringSize(value.tgtNqn) + StringSize(value.nsUuid) +
                    static_cast<uint32_t>(sizeof(uint32_t)) + StringSize(value.nsDevPath) +
                    static_cast<uint32_t>(sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint32_t));
    for (const auto &hostNqn : value.allowHostNqnList) {
        size += StringSize(hostNqn);
    }
    return size;
}

uint32_t AllocationSize(const UbseCliSsuAllocResult &value)
{
    uint32_t size = StringSize(value.name) + static_cast<uint32_t>(sizeof(uint8_t) + sizeof(uint32_t));
    for (const auto &nameSpace : value.nameSpaceList) {
        size += NameSpaceSize(nameSpace);
    }
    return size;
}

bool PackString(UbsePackUtil &pack, const std::string &value)
{
    return pack.UbsePackString(value, 1024);
}

bool PackNameSpace(UbsePackUtil &pack, const UbseCliSsuNameSpaceInfo &value)
{
    if (!PackString(pack, value.tgtEid) || !PackString(pack, value.tgtNqn) || !PackString(pack, value.nsUuid) ||
        !pack.UbsePackUint32(value.namespaceId) || !PackString(pack, value.nsDevPath) ||
        !pack.UbsePackUint64(value.nsSize) || !pack.UbsePackUint32(static_cast<uint32_t>(value.lbaFormat)) ||
        !pack.UbsePackUint32(static_cast<uint32_t>(value.allowHostNqnList.size()))) {
        return false;
    }
    for (const auto &hostNqn : value.allowHostNqnList) {
        if (!PackString(pack, hostNqn)) {
            return false;
        }
    }
    return true;
}

bool PackAllocation(UbsePackUtil &pack, const UbseCliSsuAllocResult &value)
{
    if (!PackString(pack, value.name) || !pack.UbsePackUint8(static_cast<uint8_t>(value.strategy)) ||
        !pack.UbsePackUint32(static_cast<uint32_t>(value.nameSpaceList.size()))) {
        return false;
    }
    for (const auto &nameSpace : value.nameSpaceList) {
        if (!PackNameSpace(pack, nameSpace)) {
            return false;
        }
    }
    return true;
}

std::vector<uint8_t> PackAllocationList(const std::vector<UbseCliSsuAllocResult> &values)
{
    uint32_t size = sizeof(uint32_t);
    for (const auto &value : values) {
        size += AllocationSize(value);
    }
    std::vector<uint8_t> payload(size);
    UbsePackUtil pack(payload.data(), payload.size());
    EXPECT_TRUE(pack.UbsePackUint32(static_cast<uint32_t>(values.size())));
    for (const auto &value : values) {
        EXPECT_TRUE(PackAllocation(pack, value));
    }
    return payload;
}

UbseCliSsuAllocResult MakeAllocation(const std::string &name, UbseSsuAllocStrategy strategy, uint32_t namespaceId)
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
UbseUnpackUtil SerializeAndOpen(const Request &request, std::vector<uint8_t> &payload)
{
    EXPECT_TRUE(request.Serialize(payload));
    return UbseUnpackUtil(payload.data(), static_cast<uint32_t>(payload.size()));
}

} // namespace

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
    EXPECT_TRUE(unpack.UnpackString(name, 48));
    EXPECT_TRUE(unpack.UnpackUint64(size));
    EXPECT_TRUE(unpack.UnpackUint32(count));
    EXPECT_TRUE(unpack.UnpackUint32(lbaFormat));
    EXPECT_TRUE(unpack.UnpackUint8(strategy));
    EXPECT_TRUE(unpack.UnpackString(tenant, 17));
    EXPECT_EQ(name, request.name);
    EXPECT_EQ(size, request.nsSize);
    EXPECT_EQ(count, request.nsNum);
    EXPECT_EQ(lbaFormat, 4096U);
    EXPECT_EQ(strategy, 0U);
    EXPECT_EQ(tenant, request.tenant);
}

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

TEST(TestUbseCliSsuStruct, ResponsesRejectTruncationAndInvalidStringLength)
{
    auto allocation = MakeAllocation("alloc-a", UbseSsuAllocStrategy::LINEAR, 1);
    auto payload = PackAllocationList({allocation});
    payload.pop_back();
    UbseCliSsuAllocListRsp response;
    EXPECT_FALSE(response.Deserialize(payload.data(), static_cast<uint32_t>(payload.size())));

}

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

TEST(TestUbseCliSsuStruct, ResponsesRejectOversizedAllowedHostListAndInvalidEnums)
{
    auto allocation = MakeAllocation("alloc-a", UbseSsuAllocStrategy::LINEAR, 1);
    allocation.nameSpaceList.resize(1);
    allocation.nameSpaceList[0].allowHostNqnList.clear();
    auto payload = PackAllocationList({allocation});
    uint32_t tooManyHosts = SSU_CLI_MAX_NS_NUM + 1;
    std::memcpy(payload.data() + payload.size() - sizeof(uint32_t), &tooManyHosts, sizeof(tooManyHosts));
    UbseCliSsuAllocListRsp response;
    EXPECT_FALSE(response.Deserialize(payload.data(), static_cast<uint32_t>(payload.size())));

    payload = PackAllocationList({allocation});
    const size_t strategyOffset = sizeof(uint32_t) + StringSize(allocation.name);
    payload[strategyOffset] = UINT8_MAX;
    EXPECT_FALSE(response.Deserialize(payload.data(), static_cast<uint32_t>(payload.size())));

    payload = PackAllocationList({allocation});
    const auto &nameSpace = allocation.nameSpaceList[0];
    const size_t lbaOffset = sizeof(uint32_t) + StringSize(allocation.name) + sizeof(uint8_t) + sizeof(uint32_t) +
                             StringSize(nameSpace.tgtEid) + StringSize(nameSpace.tgtNqn) +
                             StringSize(nameSpace.nsUuid) + sizeof(uint32_t) + StringSize(nameSpace.nsDevPath) +
                             sizeof(uint64_t);
    uint32_t invalidLba = 1234;
    std::memcpy(payload.data() + lbaOffset, &invalidLba, sizeof(invalidLba));
    EXPECT_FALSE(response.Deserialize(payload.data(), static_cast<uint32_t>(payload.size())));
}

TEST(TestUbseCliSsuStruct, RequestsRejectOverlongStringsAndInvalidEnums)
{
    UbseCliSsuAllocDetailReq detail{std::string(49, 'a')};
    std::vector<uint8_t> payload;
    EXPECT_FALSE(detail.Serialize(payload));

    UbseCliSsuAllocCreateReq create;
    create.name = "alloc-a";
    create.lbaFormat = static_cast<UbseSsuLBAFormat>(1234);
    EXPECT_FALSE(create.Serialize(payload));
    create.lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_512;
    create.strategy = static_cast<UbseSsuAllocStrategy>(9);
    EXPECT_FALSE(create.Serialize(payload));

}

TEST(TestUbseCliSsuStruct, PublicConstantsReuseSsuLimits)
{
    EXPECT_EQ(SSU_CLI_MAX_NAME_LENGTH, 48U);
    EXPECT_EQ(SSU_CLI_MAX_NS_NUM, ubse::adapter_plugins::ssu::def::UBSE_SSU_MAX_HOST_NUM);
    EXPECT_EQ(SSU_CLI_MIN_SIZE_BYTES, 1024ULL * 1024ULL * 1024ULL);
}
} // namespace ubse::ut::cli
