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

#include "test_ubse_npu_source_def.h"
#include "ubse_npu_source_def.cpp"

namespace ubse::npu::controller::ut {

void TestUbseNpuSourceDef::SetUp() {}

void TestUbseNpuSourceDef::TearDown()
{
    GlobalMockObject::verify();
}

TEST_F(TestUbseNpuSourceDef, StringToArrayForGuidEmpty)
{
    auto arr = StringToArrayForGuid("");
    for (auto v : arr) {
        EXPECT_EQ(v, 0);
    }
}

TEST_F(TestUbseNpuSourceDef, StringToArrayForGuidShort)
{
    auto arr = StringToArrayForGuid("abc");
    EXPECT_EQ(arr[0], 'a');
    EXPECT_EQ(arr[1], 'b');
    EXPECT_EQ(arr[2], 'c');
    EXPECT_EQ(arr[3], 0);
}

TEST_F(TestUbseNpuSourceDef, StringToArrayForGuidExactSize)
{
    std::string str(UBSE_UB_DEVICE_GUID_SIZE, 'x');
    auto arr = StringToArrayForGuid(str);
    for (size_t i = 0; i < UBSE_UB_DEVICE_GUID_SIZE; i++) {
        EXPECT_EQ(arr[i], 'x');
    }
}

TEST_F(TestUbseNpuSourceDef, StringToArrayForGuidOverflow)
{
    std::string str(UBSE_UB_DEVICE_GUID_SIZE + 10, 'y');
    auto arr = StringToArrayForGuid(str);
    for (size_t i = 0; i < UBSE_UB_DEVICE_GUID_SIZE; i++) {
        EXPECT_EQ(arr[i], 'y');
    }
}

TEST_F(TestUbseNpuSourceDef, NpuResourceGetType)
{
    NpuResource res;
    EXPECT_EQ(res.GetType(), ResourceType::NPU);
}

TEST_F(TestUbseNpuSourceDef, NpuResourceCalculateSizeEmpty)
{
    NpuResource res;
    res.SetLoc(1, 2);
    res.SetGuid("guid");
    res.SetBusInstanceGuid("busi");
    size_t sz = res.CalculateSize();
    EXPECT_GT(sz, 0);
}

TEST_F(TestUbseNpuSourceDef, NpuResourceCalculateSizeWithAffinity)
{
    NpuResource res;
    res.SetLoc(1, 2);
    size_t sizeBefore = res.CalculateSize();
    UbDevice dev;
    dev.type = ResourceType::NIC_PFE;
    dev.slotId = 1;
    res.AddAffinityDevice(dev);
    res.AddAffinityDevice(dev);
    size_t sizeAfter = res.CalculateSize();
    EXPECT_GT(sizeAfter, sizeBefore);
}

TEST_F(TestUbseNpuSourceDef, NpuResourcePackSuccess)
{
    NpuResource res;
    res.SetLoc(1, 2);
    res.SetGuid("testguid");
    res.SetBusInstanceGuid("busiguid");
    UbDevice dev;
    dev.type = ResourceType::NIC_PFE;
    dev.slotId = 1;
    dev.chipId = 2;
    dev.dieId = 3;
    dev.pfId = 100;
    dev.vfId = 200;
    res.AddAffinityDevice(dev);

    size_t bufSize = res.CalculateSize() + 64;
    std::vector<uint8_t> buffer(bufSize, 0);
    UbsePackUtil packUtil(buffer.data(), bufSize);
    UbseResult ret = res.Pack(packUtil);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNpuSourceDef, NpuResourcePackNoAffinitySuccess)
{
    NpuResource res;
    res.SetLoc(3, 4);
    res.SetGuid("g");
    res.SetBusInstanceGuid("b");

    size_t bufSize = res.CalculateSize() + 32;
    std::vector<uint8_t> buffer(bufSize, 0);
    UbsePackUtil packUtil(buffer.data(), bufSize);
    UbseResult ret = res.Pack(packUtil);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNpuSourceDef, NpuResourcePackBufferTooSmall)
{
    NpuResource res;
    res.SetLoc(1, 2);
    res.SetGuid("guid");
    res.SetBusInstanceGuid("busi");

    std::vector<uint8_t> buffer(1, 0);
    UbsePackUtil packUtil(buffer.data(), 1);
    UbseResult ret = res.Pack(packUtil);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNpuSourceDef, NpuResourceUnpackReturnsOk)
{
    NpuResource res;
    std::vector<uint8_t> buffer(256, 0);
    UbsePackUtil packUtil(buffer.data(), 256);
    UbseResult ret = res.Unpack(packUtil);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNpuSourceDef, NpuResourceSetLoc)
{
    NpuResource res;
    res.SetLoc(5, 10);
}

TEST_F(TestUbseNpuSourceDef, NpuResourceSetGuid)
{
    NpuResource res;
    res.SetGuid("myguid");
}

TEST_F(TestUbseNpuSourceDef, NpuResourceSetBusInstanceGuid)
{
    NpuResource res;
    res.SetBusInstanceGuid("mybusi");
}

TEST_F(TestUbseNpuSourceDef, NpuResourceAddAffinityDevice)
{
    NpuResource res;
    UbDevice dev;
    dev.type = ResourceType::NIC_PFE;
    dev.slotId = 1;
    res.AddAffinityDevice(dev);
}

TEST_F(TestUbseNpuSourceDef, BusiResourceGetType)
{
    BusiResource res;
    EXPECT_EQ(res.GetType(), ResourceType::BUSINSTANCE);
}

TEST_F(TestUbseNpuSourceDef, BusiResourceCalculateSizeEmpty)
{
    BusiResource res;
    res.SetGuid("guid");
    EXPECT_GT(res.CalculateSize(), 0);
}

TEST_F(TestUbseNpuSourceDef, BusiResourceCalculateSizeWithSubDevices)
{
    BusiResource res;
    res.SetGuid("guid");
    UbDevice dev;
    dev.type = ResourceType::NPU;
    dev.slotId = 1;
    res.AddSubDevice(dev);
    res.AddSubDevice(dev);
    size_t sz = res.CalculateSize();
    EXPECT_GT(sz, 0);
}

TEST_F(TestUbseNpuSourceDef, BusiResourcePackSuccess)
{
    BusiResource res;
    res.SetGuid("busiguid");
    UbDevice dev;
    dev.type = ResourceType::NPU;
    dev.slotId = 1;
    dev.chipId = 2;
    dev.dieId = 3;
    dev.pfId = 100;
    dev.vfId = 200;
    res.AddSubDevice(dev);

    size_t bufSize = res.CalculateSize() + 64;
    std::vector<uint8_t> buffer(bufSize, 0);
    UbsePackUtil packUtil(buffer.data(), bufSize);
    UbseResult ret = res.Pack(packUtil);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNpuSourceDef, BusiResourcePackNoSubDevicesSuccess)
{
    BusiResource res;
    res.SetGuid("g");

    size_t bufSize = res.CalculateSize() + 32;
    std::vector<uint8_t> buffer(bufSize, 0);
    UbsePackUtil packUtil(buffer.data(), bufSize);
    UbseResult ret = res.Pack(packUtil);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNpuSourceDef, BusiResourcePackBufferTooSmall)
{
    BusiResource res;
    res.SetGuid("guid");

    std::vector<uint8_t> buffer(1, 0);
    UbsePackUtil packUtil(buffer.data(), 1);
    UbseResult ret = res.Pack(packUtil);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNpuSourceDef, BusiResourceUnpackReturnsOk)
{
    BusiResource res;
    std::vector<uint8_t> buffer(256, 0);
    UbsePackUtil packUtil(buffer.data(), 256);
    EXPECT_EQ(res.Unpack(packUtil), UBSE_OK);
}

TEST_F(TestUbseNpuSourceDef, BusiResourceSetGuid)
{
    BusiResource res;
    res.SetGuid("myguid");
}

TEST_F(TestUbseNpuSourceDef, BusiResourceAddSubDevice)
{
    BusiResource res;
    UbDevice dev;
    dev.type = ResourceType::NPU;
    dev.slotId = 1;
    res.AddSubDevice(dev);
}

TEST_F(TestUbseNpuSourceDef, NicPfeResourceGetType)
{
    NicPfeResource res;
    EXPECT_EQ(res.GetType(), ResourceType::NIC_PFE);
}

TEST_F(TestUbseNpuSourceDef, NicPfeResourceCalculateSizeEmpty)
{
    NicPfeResource res;
    res.SetLoc(1, 2, 3);
    res.SetGuid("guid");
    res.SetBusInstanceGuid("busi");
    EXPECT_GT(res.CalculateSize(), 0);
}

TEST_F(TestUbseNpuSourceDef, NicPfeResourceCalculateSizeWithAffinity)
{
    NicPfeResource res;
    res.SetLoc(1, 2, 3);
    res.SetGuid("guid");
    res.SetBusInstanceGuid("busi");
    UbDevice dev;
    dev.type = ResourceType::NIC_VFE;
    dev.slotId = 1;
    res.AddAffinityDevice(dev);
    EXPECT_GT(res.CalculateSize(), 0);
}

TEST_F(TestUbseNpuSourceDef, NicPfeResourcePackSuccess)
{
    NicPfeResource res;
    res.SetLoc(1, 2, 100);
    res.SetGuid("nicguid");
    res.SetBusInstanceGuid("busiguid");
    UbDevice dev;
    dev.type = ResourceType::NIC_VFE;
    dev.slotId = 1;
    dev.chipId = 2;
    dev.dieId = 3;
    dev.pfId = 50;
    dev.vfId = 60;
    res.AddAffinityDevice(dev);

    size_t bufSize = res.CalculateSize() + 64;
    std::vector<uint8_t> buffer(bufSize, 0);
    UbsePackUtil packUtil(buffer.data(), bufSize);
    UbseResult ret = res.Pack(packUtil);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNpuSourceDef, NicPfeResourcePackNoAffinitySuccess)
{
    NicPfeResource res;
    res.SetLoc(3, 4, 200);
    res.SetGuid("g");
    res.SetBusInstanceGuid("b");

    size_t bufSize = res.CalculateSize() + 32;
    std::vector<uint8_t> buffer(bufSize, 0);
    UbsePackUtil packUtil(buffer.data(), bufSize);
    UbseResult ret = res.Pack(packUtil);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNpuSourceDef, NicPfeResourcePackBufferTooSmall)
{
    NicPfeResource res;
    res.SetLoc(1, 2, 3);
    res.SetGuid("guid");
    res.SetBusInstanceGuid("busi");

    std::vector<uint8_t> buffer(1, 0);
    UbsePackUtil packUtil(buffer.data(), 1);
    UbseResult ret = res.Pack(packUtil);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNpuSourceDef, NicPfeResourceUnpackReturnsOk)
{
    NicPfeResource res;
    std::vector<uint8_t> buffer(256, 0);
    UbsePackUtil packUtil(buffer.data(), 256);
    EXPECT_EQ(res.Unpack(packUtil), UBSE_OK);
}

TEST_F(TestUbseNpuSourceDef, NicPfeResourceSetLoc)
{
    NicPfeResource res;
    res.SetLoc(1, 2, 3);
}

TEST_F(TestUbseNpuSourceDef, NicPfeResourceSetGuid)
{
    NicPfeResource res;
    res.SetGuid("myguid");
}

TEST_F(TestUbseNpuSourceDef, NicPfeResourceSetBusInstanceGuid)
{
    NicPfeResource res;
    res.SetBusInstanceGuid("mybusi");
}

TEST_F(TestUbseNpuSourceDef, NicPfeResourceAddAffinityDevice)
{
    NicPfeResource res;
    UbDevice dev;
    dev.type = ResourceType::NIC_VFE;
    dev.slotId = 1;
    res.AddAffinityDevice(dev);
}

TEST_F(TestUbseNpuSourceDef, NicVfeResourceGetType)
{
    NicVfeResource res;
    EXPECT_EQ(res.GetType(), ResourceType::NIC_VFE);
}

TEST_F(TestUbseNpuSourceDef, NicVfeResourceCalculateSizeEmpty)
{
    NicVfeResource res;
    res.SetLoc(1, 2, 3, 4);
    res.SetGuid("guid");
    res.SetBusInstanceGuid("busi");
    EXPECT_GT(res.CalculateSize(), 0);
}

TEST_F(TestUbseNpuSourceDef, NicVfeResourceCalculateSizeWithAffinity)
{
    NicVfeResource res;
    res.SetLoc(1, 2, 3, 4);
    res.SetGuid("guid");
    res.SetBusInstanceGuid("busi");
    UbDevice dev;
    dev.type = ResourceType::NPU;
    dev.slotId = 1;
    res.AddAffinityDevice(dev);
    EXPECT_GT(res.CalculateSize(), 0);
}

TEST_F(TestUbseNpuSourceDef, NicVfeResourcePackSuccess)
{
    NicVfeResource res;
    res.SetLoc(1, 2, 100, 200);
    res.SetGuid("nicguid");
    res.SetBusInstanceGuid("busiguid");
    UbDevice dev;
    dev.type = ResourceType::NPU;
    dev.slotId = 1;
    dev.chipId = 2;
    dev.dieId = 3;
    dev.pfId = 50;
    dev.vfId = 60;
    res.AddAffinityDevice(dev);

    size_t bufSize = res.CalculateSize() + 64;
    std::vector<uint8_t> buffer(bufSize, 0);
    UbsePackUtil packUtil(buffer.data(), bufSize);
    UbseResult ret = res.Pack(packUtil);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNpuSourceDef, NicVfeResourcePackNoAffinitySuccess)
{
    NicVfeResource res;
    res.SetLoc(3, 4, 300, 400);
    res.SetGuid("g");
    res.SetBusInstanceGuid("b");

    size_t bufSize = res.CalculateSize() + 32;
    std::vector<uint8_t> buffer(bufSize, 0);
    UbsePackUtil packUtil(buffer.data(), bufSize);
    UbseResult ret = res.Pack(packUtil);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNpuSourceDef, NicVfeResourcePackBufferTooSmall)
{
    NicVfeResource res;
    res.SetLoc(1, 2, 3, 4);
    res.SetGuid("guid");
    res.SetBusInstanceGuid("busi");

    std::vector<uint8_t> buffer(1, 0);
    UbsePackUtil packUtil(buffer.data(), 1);
    UbseResult ret = res.Pack(packUtil);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNpuSourceDef, NicVfeResourceUnpackReturnsOk)
{
    NicVfeResource res;
    std::vector<uint8_t> buffer(256, 0);
    UbsePackUtil packUtil(buffer.data(), 256);
    EXPECT_EQ(res.Unpack(packUtil), UBSE_OK);
}

TEST_F(TestUbseNpuSourceDef, NicVfeResourceSetLoc)
{
    NicVfeResource res;
    res.SetLoc(1, 2, 3, 4);
}

TEST_F(TestUbseNpuSourceDef, NicVfeResourceSetGuid)
{
    NicVfeResource res;
    res.SetGuid("myguid");
}

TEST_F(TestUbseNpuSourceDef, NicVfeResourceSetBusInstanceGuid)
{
    NicVfeResource res;
    res.SetBusInstanceGuid("mybusi");
}

TEST_F(TestUbseNpuSourceDef, NicVfeResourceAddAffinityDevice)
{
    NicVfeResource res;
    UbDevice dev;
    dev.type = ResourceType::NPU;
    dev.slotId = 1;
    res.AddAffinityDevice(dev);
}

TEST_F(TestUbseNpuSourceDef, UbCtrlResourceGetType)
{
    UbCtrlResource res;
    EXPECT_EQ(res.GetType(), ResourceType::UBCONTROLLER);
}

TEST_F(TestUbseNpuSourceDef, UbCtrlResourceCalculateSize)
{
    UbCtrlResource res;
    EXPECT_GT(res.CalculateSize(), 0);
}

TEST_F(TestUbseNpuSourceDef, UbCtrlResourcePackSuccess)
{
    UbCtrlResource res;

    size_t bufSize = res.CalculateSize() + 32;
    std::vector<uint8_t> buffer(bufSize, 0);
    UbsePackUtil packUtil(buffer.data(), bufSize);
    UbseResult ret = res.Pack(packUtil);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNpuSourceDef, UbCtrlResourcePackBufferTooSmall)
{
    UbCtrlResource res;

    std::vector<uint8_t> buffer(1, 0);
    UbsePackUtil packUtil(buffer.data(), 1);
    UbseResult ret = res.Pack(packUtil);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNpuSourceDef, UbCtrlResourceUnpackReturnsOk)
{
    UbCtrlResource res;
    std::vector<uint8_t> buffer(256, 0);
    UbsePackUtil packUtil(buffer.data(), 256);
    EXPECT_EQ(res.Unpack(packUtil), UBSE_OK);
}

} // namespace ubse::npu::controller::ut
