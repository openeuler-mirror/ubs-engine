/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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
#include <cstring>
#include "ubse_mem_common_utils.h"
#include "obmm_v1_fixture.h"

namespace ubse::ut::mmi {
using namespace ubse::mmi;

TEST(TestObmmCustomMetaCodec, RegionMaskRoundTripsBoundaryIndices)
{
    UbseMemLocalObmmCustomMeta meta{};
    ASSERT_EQ(SetMaskFromRegionIndex({0, 31, 32, 63, 64, 127, 128, 255}, meta.regionMask), UBSE_OK);
    EXPECT_EQ(SetMaskFromRegionIndex({256}, meta.regionMask), UBSE_ERROR_INVAL);
    EXPECT_EQ(IsBitSet(meta.regionMask, -1), false);
    EXPECT_EQ(IsBitSet(meta.regionMask, 256), false);
    ObmmEncodedMeta encoded{};
    uint32_t length{};
    ASSERT_EQ(EncodeObmmCustomMeta(meta, {}, encoded, length), UBSE_OK);
    EXPECT_EQ(length, 389);
    UbseMemLocalObmmCustomMeta restored{};
    UbMemPrivData prefix{};
    ASSERT_EQ(DecodeObmmCustomMeta(encoded.data(), length, restored, prefix), UBSE_OK);
    for (int index = 0; index < MAX_NODE_NUM; ++index) {
        const bool expected = index == 0 || index == 31 || index == 32 || index == 63 || index == 64 || index == 127 ||
                              index == 128 || index == 255;
        EXPECT_EQ(IsBitSet(restored.regionMask, index), expected);
    }
}

TEST(TestObmmCustomMetaCodec, V1RegionMaskPreservesHighestBitAndClearsUpperBits)
{
    auto bytes = OBMM_V1_FIXTURE;
    const uint32_t legacyMask = 0x80000001u;
    const auto offset = sizeof(UbMemPrivData) + offsetof(ObmmCustomMetaV1, regionMask);
    ASSERT_EQ(memcpy_s(bytes.data() + offset, bytes.size() - offset, &legacyMask, sizeof(legacyMask)), EOK);
    UbseMemLocalObmmCustomMeta restored{};
    std::memset(restored.regionMask, 0xff, sizeof(restored.regionMask));
    UbMemPrivData prefix{};
    ASSERT_EQ(DecodeObmmCustomMeta(bytes.data(), bytes.size(), restored, prefix), UBSE_OK);
    for (int index = 0; index < MAX_NODE_NUM; ++index) {
        EXPECT_EQ(IsBitSet(restored.regionMask, index), index == 0 || index == 31);
    }
}

TEST(TestObmmCustomMetaCodec, RestoresHistoricalBinaryFixture)
{
    UbseMemLocalObmmCustomMeta meta{};
    UbMemPrivData prefix{};
    ASSERT_EQ(DecodeObmmCustomMeta(OBMM_V1_FIXTURE.data(), OBMM_V1_FIXTURE.size(), meta, prefix), UBSE_OK);
    EXPECT_EQ(meta.version, 1);
    EXPECT_EQ(meta.type, 2);
    EXPECT_EQ(meta.uid, 1000);
    EXPECT_EQ(meta.gid, 1001);
    EXPECT_EQ(meta.pid, 1234);
    EXPECT_STREQ(meta.name, "legacy-shared-memory");
    EXPECT_STREQ(meta.username, "legacy-user");
    EXPECT_STREQ(meta.importNodeId, "12");
    EXPECT_STREQ(meta.exportNodeId, "34");
    EXPECT_STREQ(meta.requestNodeId, "56");
    EXPECT_EQ(meta.srcPid, 111);
    EXPECT_EQ(meta.dstPid, 222);
    EXPECT_EQ(meta.virAddr, 0x100000000ULL);
    EXPECT_EQ(meta.exportMemid, 12345);
    EXPECT_EQ(meta.attachSocket, 1);
    EXPECT_EQ(meta.memidCount, 2);
    EXPECT_EQ(meta.regionMask[0], 3);
    for (size_t index = 1; index < OBMM_REGION_MASK_BYTES; ++index) {
        EXPECT_EQ(meta.regionMask[index], 0);
    }
    EXPECT_EQ(meta.importNumaIds[0], 2);
    EXPECT_EQ(meta.exportNumaIds[0], 3);
    EXPECT_EQ(meta.importSocket, 1);
    EXPECT_EQ(meta.exportSocket, 0);
    EXPECT_EQ(meta.chipId, 7);
    EXPECT_EQ(meta.portId, 8);
    EXPECT_EQ(meta.numaSizes[0], 4194304);
    EXPECT_EQ(meta.usrInfo[0], 0x80);
    EXPECT_EQ(meta.usrInfo[31], 0xff);
    EXPECT_EQ(meta.dstSocket, -1);
    EXPECT_EQ(meta.dstNuma, -1);
    EXPECT_EQ(meta.requestSize, 8388608);
    EXPECT_EQ(meta.decoderResult.marId, 9);
    EXPECT_EQ(meta.decoderResult.hpa, 0x200000000ULL);
    EXPECT_EQ(meta.decoderResult.handle, 10);
    EXPECT_EQ(meta.decoderResult.staticHandle, 11);
    EXPECT_EQ(static_cast<uint8_t>(meta.decoderResult.state), 2);
    EXPECT_EQ(prefix.cacheable_flag, 1);
    EXPECT_EQ(prefix.mar_id, 3);
}

TEST(TestObmmCustomMetaCodec, ReencodingLegacyMetadataAlwaysWritesV2)
{
    UbseMemLocalObmmCustomMeta meta{};
    UbMemPrivData prefix{};
    ASSERT_EQ(DecodeObmmCustomMeta(OBMM_V1_FIXTURE.data(), OBMM_V1_FIXTURE.size(), meta, prefix), UBSE_OK);
    ASSERT_EQ(meta.version, OBMM_CUSTOM_META_V1);
    ObmmEncodedMeta encoded{};
    uint32_t length{};
    ASSERT_EQ(EncodeObmmCustomMeta(meta, prefix, encoded, length), UBSE_OK);
    ASSERT_EQ(length, sizeof(UbMemPrivData) + sizeof(ObmmCustomMetaV2));
    EXPECT_EQ(encoded[sizeof(UbMemPrivData)], OBMM_CUSTOM_META_V2);
    UbseMemLocalObmmCustomMeta restored{};
    UbMemPrivData restoredPrefix{};
    ASSERT_EQ(DecodeObmmCustomMeta(encoded.data(), length, restored, restoredPrefix), UBSE_OK);
    EXPECT_EQ(restored.version, OBMM_CUSTOM_META_V2);
    EXPECT_STREQ(restored.name, meta.name);
    EXPECT_STREQ(restored.username, meta.username);
    EXPECT_EQ(std::memcmp(restored.usrInfo, meta.usrInfo, sizeof(meta.usrInfo)), 0);
    EXPECT_EQ(restored.decoderResult.marId, meta.decoderResult.marId);
    EXPECT_EQ(restored.decoderResult.hpa, meta.decoderResult.hpa);
    EXPECT_EQ(restored.decoderResult.handle, meta.decoderResult.handle);
    EXPECT_EQ(restored.decoderResult.state, meta.decoderResult.state);
    EXPECT_EQ(restored.decoderResult.staticHandle, meta.decoderResult.staticHandle);
    EXPECT_EQ(restoredPrefix.cacheable_flag, prefix.cacheable_flag);
    EXPECT_EQ(restoredPrefix.mar_id, prefix.mar_id);
    EXPECT_EQ(OBMM_V1_FIXTURE[sizeof(UbMemPrivData)], OBMM_CUSTOM_META_V1);
}

TEST(TestObmmCustomMetaCodec, V1ConversionPreservesAllOtherFields)
{
    auto bytes = OBMM_V1_FIXTURE;
    // Conversion must not add business-field validation to historical records.
    bytes[sizeof(UbMemPrivData) + offsetof(ObmmCustomMetaV1, type)] = 0xff;
    UbseMemLocalObmmCustomMeta restored{};
    UbMemPrivData prefix{};
    ASSERT_EQ(DecodeObmmCustomMeta(bytes.data(), bytes.size(), restored, prefix), UBSE_OK);
    const auto* source = bytes.data() + sizeof(UbMemPrivData);
    const auto* target = reinterpret_cast<const uint8_t*>(&restored);
    constexpr size_t oldTail = offsetof(ObmmCustomMetaV1, importNumaIds);
    constexpr size_t newTail = offsetof(UbseMemLocalObmmCustomMeta, importNumaIds);
    EXPECT_EQ(std::memcmp(source, target, offsetof(ObmmCustomMetaV1, name)), 0);
    EXPECT_EQ(std::memcmp(source + offsetof(ObmmCustomMetaV1, name), restored.name, 52), 0);
    for (size_t index = 52; index < sizeof(restored.name); ++index) {
        EXPECT_EQ(restored.name[index], '\0');
    }
    constexpr size_t tailLength = offsetof(ObmmCustomMetaV1, requestSize) + sizeof(restored.requestSize) - oldTail;
    EXPECT_EQ(std::memcmp(source + oldTail, target + newTail, tailLength), 0);
    ObmmCustomMetaV1 original{};
    ASSERT_EQ(memcpy_s(&original, sizeof(original), source, sizeof(original)), EOK);
    EXPECT_EQ(restored.memidCount, original.memidCount);
    EXPECT_EQ(restored.decoderResult.marId, original.decoderResult.marId);
    EXPECT_EQ(restored.decoderResult.hpa, original.decoderResult.hpa);
    EXPECT_EQ(restored.decoderResult.handle, original.decoderResult.handle);
    EXPECT_EQ(restored.decoderResult.staticHandle, original.decoderResult.staticHandle);
    EXPECT_EQ(restored.decoderResult.state, original.decoderResult.state);
}

TEST(TestObmmCustomMetaCodec, V2RoundTripsAllBorrowTypesAndMaximumDerivedKeys)
{
    for (uint8_t type = 0; type <= 3; ++type) {
        for (const size_t nameLength : {47UL, 48UL, 63UL, 64UL, 67UL, 68UL}) {
            UbseMemLocalObmmCustomMeta meta{};
            UbMemPrivData prefix{};
            ASSERT_EQ(DecodeObmmCustomMeta(OBMM_V1_FIXTURE.data(), OBMM_V1_FIXTURE.size(), meta, prefix), UBSE_OK);
            meta.type = type;
            const auto name = nameLength >= 67 ? GenerateExportKey(std::string(nameLength - 4, 'n'), "123") :
                                                 std::string(nameLength, 'a');
            ASSERT_EQ(name.size(), nameLength);
            ASSERT_EQ(strcpy_s(meta.name, sizeof(meta.name), name.c_str()), EOK);
            ObmmEncodedMeta encoded{};
            uint32_t length{};
            ASSERT_EQ(EncodeObmmCustomMeta(meta, prefix, encoded, length), UBSE_OK);
            ASSERT_EQ(length, sizeof(UbMemPrivData) + sizeof(ObmmCustomMetaV2));
            UbseMemLocalObmmCustomMeta restored{};
            ASSERT_EQ(DecodeObmmCustomMeta(encoded.data(), length, restored, prefix), UBSE_OK);
            EXPECT_EQ(restored.version, 2);
            EXPECT_EQ(restored.type, type);
            EXPECT_STREQ(restored.name, meta.name);
            EXPECT_EQ(std::memcmp(restored.usrInfo, meta.usrInfo, sizeof(meta.usrInfo)), 0);
            EXPECT_EQ(restored.decoderResult.staticHandle, meta.decoderResult.staticHandle);
            EXPECT_EQ(restored.requestSize, meta.requestSize);
        }
    }
}

TEST(TestObmmCustomMetaCodec, RejectsMismatchedVersionAndLengthWithoutChangingOutputs)
{
    UbseMemLocalObmmCustomMeta meta{};
    UbMemPrivData prefix{};
    ASSERT_EQ(DecodeObmmCustomMeta(OBMM_V1_FIXTURE.data(), OBMM_V1_FIXTURE.size(), meta, prefix), UBSE_OK);
    ObmmEncodedMeta encoded{};
    uint32_t length{};
    ASSERT_EQ(EncodeObmmCustomMeta(meta, prefix, encoded, length), UBSE_OK);
    auto legacy = OBMM_V1_FIXTURE;
    legacy[sizeof(UbMemPrivData)] = OBMM_CUSTOM_META_V2;
    encoded[sizeof(UbMemPrivData)] = OBMM_CUSTOM_META_V1;
    meta.pid = 999;
    prefix.mar_id = 7;
    prefix.cacheable_flag = 0;
    EXPECT_EQ(DecodeObmmCustomMeta(legacy.data(), legacy.size(), meta, prefix), UBSE_ERROR_INVAL);
    EXPECT_EQ(meta.pid, 999);
    EXPECT_EQ(prefix.mar_id, 7);
    EXPECT_EQ(prefix.cacheable_flag, 0);
    EXPECT_EQ(DecodeObmmCustomMeta(encoded.data(), length, meta, prefix), UBSE_ERROR_INVAL);
    EXPECT_EQ(meta.pid, 999);
    EXPECT_EQ(prefix.mar_id, 7);
    EXPECT_EQ(prefix.cacheable_flag, 0);
}

TEST(TestObmmCustomMetaCodec, FreshV2RoundTripPreservesDefaultsAndTail)
{
    UbseMemLocalObmmCustomMeta meta{};
    const auto name = GenerateExportKey(std::string(64, 'n'), "123");
    ASSERT_EQ(strcpy_s(meta.name, sizeof(meta.name), name.c_str()), EOK);
    meta.pid = 4321;
    meta.usrInfo[31] = 0xa5;
    meta.requestSize = 8388608;
    meta.decoderResult.staticHandle = 0x123456789ULL;
    UbMemPrivData prefix{};
    prefix.mar_id = 7;
    ObmmEncodedMeta encoded{};
    uint32_t length{};
    ASSERT_EQ(EncodeObmmCustomMeta(meta, prefix, encoded, length), UBSE_OK);
    EXPECT_EQ(meta.version, 0); // Encoding must not change the caller's object.
    UbseMemLocalObmmCustomMeta restored{};
    UbMemPrivData restoredPrefix{};
    ASSERT_EQ(DecodeObmmCustomMeta(encoded.data(), length, restored, restoredPrefix), UBSE_OK);
    EXPECT_EQ(restored.version, OBMM_CUSTOM_META_V2);
    EXPECT_STREQ(restored.name, name.c_str());
    EXPECT_EQ(restored.pid, meta.pid);
    EXPECT_EQ(restored.dstSocket, -1);
    EXPECT_EQ(restored.dstNuma, -1);
    EXPECT_EQ(restored.usrInfo[31], meta.usrInfo[31]);
    EXPECT_EQ(restored.requestSize, meta.requestSize);
    EXPECT_EQ(restored.decoderResult.staticHandle, meta.decoderResult.staticHandle);
    EXPECT_EQ(restoredPrefix.mar_id, prefix.mar_id);
}

TEST(TestObmmCustomMetaCodec, AlternatingV1AndV2DecodesPreserveSelectedVersion)
{
    UbseMemLocalObmmCustomMeta legacy{};
    UbMemPrivData legacyPrefix{};
    ASSERT_EQ(DecodeObmmCustomMeta(OBMM_V1_FIXTURE.data(), OBMM_V1_FIXTURE.size(), legacy, legacyPrefix), UBSE_OK);
    auto current = legacy;
    const auto name = GenerateExportKey(std::string(64, 'v'), "123");
    ASSERT_EQ(strcpy_s(current.name, sizeof(current.name), name.c_str()), EOK);
    current.pid = 5678;
    current.dstSocket = 2;
    current.usrInfo[31] = 0x42;
    current.decoderResult.staticHandle = 9876;
    UbMemPrivData currentPrefix{};
    currentPrefix.mar_id = 7;
    ObmmEncodedMeta encoded{};
    uint32_t length{};
    ASSERT_EQ(EncodeObmmCustomMeta(current, currentPrefix, encoded, length), UBSE_OK);
    UbseMemLocalObmmCustomMeta restored{};
    UbMemPrivData restoredPrefix{};
    for (const bool useV2 : {true, false, true, false}) {
        const auto* bytes = useV2 ? encoded.data() : OBMM_V1_FIXTURE.data();
        const auto byteCount = useV2 ? length : OBMM_V1_FIXTURE.size();
        ASSERT_EQ(DecodeObmmCustomMeta(bytes, byteCount, restored, restoredPrefix), UBSE_OK);
        const auto& expected = useV2 ? current : legacy;
        EXPECT_EQ(restored.version, useV2 ? OBMM_CUSTOM_META_V2 : OBMM_CUSTOM_META_V1);
        EXPECT_STREQ(restored.name, expected.name);
        EXPECT_EQ(restored.pid, expected.pid);
        EXPECT_EQ(restored.dstSocket, expected.dstSocket);
        EXPECT_EQ(restored.usrInfo[31], expected.usrInfo[31]);
        EXPECT_EQ(restored.decoderResult.staticHandle, expected.decoderResult.staticHandle);
        EXPECT_EQ(restoredPrefix.mar_id, useV2 ? currentPrefix.mar_id : legacyPrefix.mar_id);
    }
}

TEST(TestObmmCustomMetaCodec, RejectsInvalidWireWithoutChangingOutput)
{
    UbseMemLocalObmmCustomMeta meta{};
    meta.pid = 999;
    UbMemPrivData prefix{};
    EXPECT_EQ(DecodeObmmCustomMeta(nullptr, 0, meta, prefix), UBSE_ERROR_NULLPTR);
    for (const size_t length : {0UL, 2UL, 361UL, 363UL, 513UL}) {
        EXPECT_EQ(DecodeObmmCustomMeta(OBMM_V1_FIXTURE.data(), length, meta, prefix), UBSE_ERROR_INVAL);
    }
    auto bytes = OBMM_V1_FIXTURE;
    bytes[2] = 99;
    EXPECT_EQ(DecodeObmmCustomMeta(bytes.data(), bytes.size(), meta, prefix), UBSE_ERROR_INVAL);
    for (const auto offset : {offsetof(ObmmCustomMetaV1, name)}) {
        bytes = OBMM_V1_FIXTURE;
        std::memset(bytes.data() + 2 + offset, 'x', 52);
        EXPECT_EQ(DecodeObmmCustomMeta(bytes.data(), bytes.size(), meta, prefix), UBSE_ERROR_INVAL);
    }
    EXPECT_EQ(meta.pid, 999);
}

TEST(TestObmmCustomMetaCodec, RejectsUnterminatedNamesBeforeWritingV2)
{
    UbseMemLocalObmmCustomMeta meta{};
    UbMemPrivData prefix{};
    ASSERT_EQ(DecodeObmmCustomMeta(OBMM_V1_FIXTURE.data(), OBMM_V1_FIXTURE.size(), meta, prefix), UBSE_OK);
    ObmmEncodedMeta encoded{};
    uint32_t length = 123;
    std::memset(meta.name, 'a', sizeof(meta.name));
    EXPECT_EQ(EncodeObmmCustomMeta(meta, prefix, encoded, length), UBSE_ERROR_INVAL);
    EXPECT_EQ(length, 0);
}

TEST(TestObmmCustomMetaCodec, RejectsInvalidV2WithoutChangingOutput)
{
    UbseMemLocalObmmCustomMeta meta{};
    UbMemPrivData prefix{};
    ASSERT_EQ(DecodeObmmCustomMeta(OBMM_V1_FIXTURE.data(), OBMM_V1_FIXTURE.size(), meta, prefix), UBSE_OK);
    ObmmEncodedMeta encoded{};
    uint32_t length{};
    ASSERT_EQ(EncodeObmmCustomMeta(meta, prefix, encoded, length), UBSE_OK);
    meta.pid = 999;
    EXPECT_EQ(DecodeObmmCustomMeta(encoded.data(), length - 1, meta, prefix), UBSE_ERROR_INVAL);
    EXPECT_EQ(DecodeObmmCustomMeta(encoded.data(), length + 1, meta, prefix), UBSE_ERROR_INVAL);
    std::memset(encoded.data() + sizeof(UbMemPrivData) + offsetof(ObmmCustomMetaV2, name), 'x',
                sizeof(ObmmCustomMetaV2::name));
    EXPECT_EQ(DecodeObmmCustomMeta(encoded.data(), length, meta, prefix), UBSE_ERROR_INVAL);
    EXPECT_EQ(meta.pid, 999);
}
} // namespace ubse::ut::mmi
