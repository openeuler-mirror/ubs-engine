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

#include "test_ubse_ssu_adapter_impl.h"

#include <cstring>
#include <securec.h>

#include "ubse_common_def.h"
#include "ubse_error.h"

namespace ubse::adapter_plugins::ssu::ut {
using namespace ubse::common::def;
using namespace ubse::adapter_plugins::ssu::def;

std::string TestUbseSsuAdapterImpl::MakeEid(char c)
{
    return std::string(EID_SIZE, c);
}

UbseSsuDevInfo TestUbseSsuAdapterImpl::MakeDevInfo(const std::string &eid, const std::string &subNqn)
{
    UbseSsuDevInfo info;
    info.subSystem.eid = eid;
    info.subSystem.subNqn = subNqn;
    return info;
}

UbseSsuDevNameSpace TestUbseSsuAdapterImpl::MakeNameSpaceForCreate(const std::string &eid,
                                                                    const std::string &subNqn,
                                                                    uint64_t nsze,
                                                                    uint64_t ncap)
{
    UbseSsuDevNameSpace ns;
    ns.subSystem.eid = eid;
    ns.subSystem.subNqn = subNqn;
    ns.nsze = nsze;
    ns.ncap = ncap;
    return ns;
}

UbseSsuDevNameSpace TestUbseSsuAdapterImpl::MakeNameSpaceForBasic(const std::string &eid,
                                                                   const std::string &subNqn,
                                                                   uint32_t namespaceId,
                                                                   const std::string &guid)
{
    UbseSsuDevNameSpace ns;
    ns.subSystem.eid = eid;
    ns.subSystem.subNqn = subNqn;
    ns.namespaceId = namespaceId;
    ns.guid = guid;
    return ns;
}

DevInfoT TestUbseSsuAdapterImpl::MakeDevInfoT(const std::string &eid, const std::string &subNqn,
                                               DevStatusT state, uint32_t nsCount)
{
    DevInfoT devInfo{};
    devInfo.state = state;
    devInfo.nsCount = nsCount;
    devInfo.tnvmcap = 1000;
    devInfo.unvmcap = 200;
    devInfo.cntlId = 1;
    strncpy_s(devInfo.devPath, DEV_PATH_SIZE, "/dev/nvme0", strlen("/dev/nvme0"));
    strncpy_s(devInfo.sn, SN_SIZE, "SN12345", strlen("SN12345"));
    strncpy_s(devInfo.mn, MN_SIZE, "MN67890", strlen("MN67890"));
    if (eid.size() == EID_SIZE) {
        memcpy_s(devInfo.devAddr.tgtEid.raw, EID_SIZE, eid.c_str(), EID_SIZE);
    }
    if (!subNqn.empty()) {
        strncpy_s(devInfo.devAddr.subNqn, SUBNQN_SIZE, subNqn.c_str(), subNqn.size());
    }
    return devInfo;
}

void TestUbseSsuAdapterImpl::SetUp()
{
    Test::SetUp();
}

void TestUbseSsuAdapterImpl::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

// ==================== BuildDevAddrList ====================

TEST_F(TestUbseSsuAdapterImpl, BuildDevAddrList_EmptyList)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::vector<UbseSsuDevInfo> ssuInfoList;
    std::vector<DevAddrT> devList;
    uint32_t ret = impl.BuildDevAddrList(ssuInfoList, devList);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(devList.size(), 0u);
}

TEST_F(TestUbseSsuAdapterImpl, BuildDevAddrList_SingleValidDev)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('A');
    std::vector<UbseSsuDevInfo> ssuInfoList = {MakeDevInfo(eid, "nqn.test")};
    std::vector<DevAddrT> devList;
    uint32_t ret = impl.BuildDevAddrList(ssuInfoList, devList);
    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(devList.size(), 1u);
    EXPECT_EQ(memcmp(devList[0].tgtEid.raw, eid.c_str(), EID_SIZE), 0);
    EXPECT_EQ(std::string(devList[0].subNqn), "nqn.test");
    EXPECT_EQ(devList[0].devIp, nullptr);
    EXPECT_FALSE(devList[0].useUb);
}

TEST_F(TestUbseSsuAdapterImpl, BuildDevAddrList_MultipleValidDevs)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid1 = MakeEid('A');
    std::string eid2 = MakeEid('B');
    std::vector<UbseSsuDevInfo> ssuInfoList = {
        MakeDevInfo(eid1, "nqn.test1"),
        MakeDevInfo(eid2, "nqn.test2"),
    };
    std::vector<DevAddrT> devList;
    uint32_t ret = impl.BuildDevAddrList(ssuInfoList, devList);
    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(devList.size(), 2u);
    EXPECT_EQ(memcmp(devList[0].tgtEid.raw, eid1.c_str(), EID_SIZE), 0);
    EXPECT_EQ(memcmp(devList[1].tgtEid.raw, eid2.c_str(), EID_SIZE), 0);
}

TEST_F(TestUbseSsuAdapterImpl, BuildDevAddrList_InvalidEidLength)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::vector<UbseSsuDevInfo> ssuInfoList = {MakeDevInfo("short_eid", "nqn.test")};
    std::vector<DevAddrT> devList;
    uint32_t ret = impl.BuildDevAddrList(ssuInfoList, devList);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseSsuAdapterImpl, BuildDevAddrList_EmptySubNqn)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('A');
    std::vector<UbseSsuDevInfo> ssuInfoList = {MakeDevInfo(eid, "")};
    std::vector<DevAddrT> devList;
    uint32_t ret = impl.BuildDevAddrList(ssuInfoList, devList);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseSsuAdapterImpl, BuildDevAddrList_EidTooLong)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid(EID_SIZE + 5, 'X');
    std::vector<UbseSsuDevInfo> ssuInfoList = {MakeDevInfo(eid, "nqn.test")};
    std::vector<DevAddrT> devList;
    uint32_t ret = impl.BuildDevAddrList(ssuInfoList, devList);
    EXPECT_NE(ret, UBSE_OK);
}

// ==================== BuildNamespaceInfoForCreate ====================

TEST_F(TestUbseSsuAdapterImpl, BuildNamespaceInfoForCreate_ValidInput)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('C');
    auto ns = MakeNameSpaceForCreate(eid, "nqn.create", 1024, 512);
    ns.nsOptions.flbas = 1;
    ns.nsOptions.dps = 0;
    ns.nsOptions.anagrpid = 2;
    ns.nsOptions.nvmsetid = 3;
    ns.nsOptions.nmic = 1;
    DevNamespaceInfoT nsInfo{};
    uint32_t ret = impl.BuildNamespaceInfoForCreate(ns, nsInfo);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(memcmp(nsInfo.devAddr.tgtEid.raw, eid.c_str(), EID_SIZE), 0);
    EXPECT_EQ(std::string(nsInfo.devAddr.subNqn), "nqn.create");
    EXPECT_EQ(nsInfo.baseAttr.nsze, 1024u);
    EXPECT_EQ(nsInfo.baseAttr.ncap, 512u);
    EXPECT_EQ(nsInfo.baseAttr.flbas, 1u);
    EXPECT_EQ(nsInfo.baseAttr.dps, 0u);
    EXPECT_EQ(nsInfo.baseAttr.anagrpid, 2u);
    EXPECT_EQ(nsInfo.baseAttr.nvmsetid, 3u);
    EXPECT_TRUE(nsInfo.baseAttr.nmic);
    EXPECT_EQ(nsInfo.devAddr.devIp, nullptr);
    EXPECT_FALSE(nsInfo.devAddr.useUb);
}

TEST_F(TestUbseSsuAdapterImpl, BuildNamespaceInfoForCreate_InvalidEidLength)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    auto ns = MakeNameSpaceForCreate("short", "nqn.create", 1024, 512);
    DevNamespaceInfoT nsInfo{};
    uint32_t ret = impl.BuildNamespaceInfoForCreate(ns, nsInfo);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseSsuAdapterImpl, BuildNamespaceInfoForCreate_EmptySubNqn)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('C');
    auto ns = MakeNameSpaceForCreate(eid, "", 1024, 512);
    DevNamespaceInfoT nsInfo{};
    uint32_t ret = impl.BuildNamespaceInfoForCreate(ns, nsInfo);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseSsuAdapterImpl, BuildNamespaceInfoForCreate_ZeroNsze)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('C');
    auto ns = MakeNameSpaceForCreate(eid, "nqn.create", 0, 512);
    DevNamespaceInfoT nsInfo{};
    uint32_t ret = impl.BuildNamespaceInfoForCreate(ns, nsInfo);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseSsuAdapterImpl, BuildNamespaceInfoForCreate_ZeroNcap)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('C');
    auto ns = MakeNameSpaceForCreate(eid, "nqn.create", 1024, 0);
    DevNamespaceInfoT nsInfo{};
    uint32_t ret = impl.BuildNamespaceInfoForCreate(ns, nsInfo);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseSsuAdapterImpl, BuildNamespaceInfoForCreate_JettyIdSet)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('C');
    auto ns = MakeNameSpaceForCreate(eid, "nqn.create", 1024, 512);
    ns.subSystem.jettyId = 42;
    DevNamespaceInfoT nsInfo{};
    uint32_t ret = impl.BuildNamespaceInfoForCreate(ns, nsInfo);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(nsInfo.devAddr.jettyId, 42u);
}

// ==================== BuildNamespaceInfoForBasic ====================

TEST_F(TestUbseSsuAdapterImpl, BuildNamespaceInfoForBasic_ValidInput)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('D');
    std::string guid(GUID_SIZE, 'G');
    auto ns = MakeNameSpaceForBasic(eid, "nqn.basic", 1, guid);
    DevNamespaceInfoT nsInfo{};
    uint32_t ret = impl.BuildNamespaceInfoForBasic(ns, nsInfo);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(nsInfo.namespaceId, 1u);
    EXPECT_EQ(memcmp(nsInfo.devAddr.tgtEid.raw, eid.c_str(), EID_SIZE), 0);
    EXPECT_EQ(nsInfo.devAddr.devIp, nullptr);
    EXPECT_FALSE(nsInfo.devAddr.useUb);
}

TEST_F(TestUbseSsuAdapterImpl, BuildNamespaceInfoForBasic_InvalidEidLength)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    auto ns = MakeNameSpaceForBasic("short", "nqn.basic", 1, "guid");
    DevNamespaceInfoT nsInfo{};
    uint32_t ret = impl.BuildNamespaceInfoForBasic(ns, nsInfo);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseSsuAdapterImpl, BuildNamespaceInfoForBasic_ZeroNamespaceId)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('D');
    auto ns = MakeNameSpaceForBasic(eid, "nqn.basic", 0, "guid");
    DevNamespaceInfoT nsInfo{};
    uint32_t ret = impl.BuildNamespaceInfoForBasic(ns, nsInfo);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseSsuAdapterImpl, BuildNamespaceInfoForBasic_WithGuid)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('D');
    std::string guid(GUID_SIZE, 'G');
    auto ns = MakeNameSpaceForBasic(eid, "nqn.basic", 5, guid);
    DevNamespaceInfoT nsInfo{};
    uint32_t ret = impl.BuildNamespaceInfoForBasic(ns, nsInfo);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(memcmp(nsInfo.guid, guid.c_str(), GUID_SIZE), 0);
}

TEST_F(TestUbseSsuAdapterImpl, BuildNamespaceInfoForBasic_EmptyGuid)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('D');
    auto ns = MakeNameSpaceForBasic(eid, "nqn.basic", 5, "");
    DevNamespaceInfoT nsInfo{};
    uint32_t ret = impl.BuildNamespaceInfoForBasic(ns, nsInfo);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseSsuAdapterImpl, BuildNamespaceInfoForBasic_JettyIdSet)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('D');
    auto ns = MakeNameSpaceForBasic(eid, "nqn.basic", 5, "guid");
    ns.subSystem.jettyId = 99;
    DevNamespaceInfoT nsInfo{};
    uint32_t ret = impl.BuildNamespaceInfoForBasic(ns, nsInfo);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(nsInfo.devAddr.jettyId, 99u);
}

// ==================== ConvertDevInfo ====================

TEST_F(TestUbseSsuAdapterImpl, ConvertDevInfo_OnlineDevice)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('E');
    DevInfoT devInfo = MakeDevInfoT(eid, "nqn.convert", DevStatusT::DEV_ONLINE, 0);
    UbseSsuDevInfo info;
    impl.ConvertDevInfo(devInfo, info);
    EXPECT_EQ(info.subSystem.eid, eid);
    EXPECT_EQ(info.subSystem.subNqn, "nqn.convert");
    EXPECT_EQ(info.serialNumber, "SN12345");
    EXPECT_EQ(info.firmware, "MN67890");
    EXPECT_EQ(info.totalBytes, 1000u);
    EXPECT_EQ(info.usedBytes, 800u);
    EXPECT_EQ(info.state, UbseSsuState::ONLINE);
    ASSERT_EQ(info.ctrlList.size(), 1u);
    EXPECT_EQ(info.ctrlList[0].eid, eid);
    EXPECT_EQ(info.ctrlList[0].devPath, "/dev/nvme0");
    EXPECT_EQ(info.ctrlList[0].cntlid, 1u);
}

TEST_F(TestUbseSsuAdapterImpl, ConvertDevInfo_OfflineDevice)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('E');
    DevInfoT devInfo = MakeDevInfoT(eid, "nqn.convert", DevStatusT::DEV_CONNECT_ERROR, 0);
    UbseSsuDevInfo info;
    impl.ConvertDevInfo(devInfo, info);
    EXPECT_EQ(info.state, UbseSsuState::OFFLINE);
}

TEST_F(TestUbseSsuAdapterImpl, ConvertDevInfo_DiscoverError)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('E');
    DevInfoT devInfo = MakeDevInfoT(eid, "nqn.convert", DevStatusT::DEV_DISCOVER_ERROR, 0);
    UbseSsuDevInfo info;
    impl.ConvertDevInfo(devInfo, info);
    EXPECT_EQ(info.state, UbseSsuState::OFFLINE);
}

TEST_F(TestUbseSsuAdapterImpl, ConvertDevInfo_IdentifyError)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('E');
    DevInfoT devInfo = MakeDevInfoT(eid, "nqn.convert", DevStatusT::DEV_IDENTIFY_ERROR, 0);
    UbseSsuDevInfo info;
    impl.ConvertDevInfo(devInfo, info);
    EXPECT_EQ(info.state, UbseSsuState::OFFLINE);
}

TEST_F(TestUbseSsuAdapterImpl, ConvertDevInfo_WithNamespaces)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('E');
    DevInfoT devInfo = MakeDevInfoT(eid, "nqn.convert", DevStatusT::DEV_ONLINE, 2);

    auto &ns0 = devInfo.namespaces[0];
    ns0.namespaceId = 1;
    ns0.baseAttr.nsze = 100;
    ns0.baseAttr.ncap = 80;
    ns0.usedBytes = 50;
    strncpy_s(ns0.devPath, DEV_PATH_SIZE, "/dev/nvme0n1", strlen("/dev/nvme0n1"));
    memset(ns0.guid, 0xAA, GUID_SIZE);
    memset(ns0.uuid, 0xBB, UUID_SIZE);
    ns0.baseAttr.flbas = 1;
    ns0.baseAttr.dps = 0;
    ns0.baseAttr.anagrpid = 3;
    ns0.baseAttr.nvmsetid = 4;
    ns0.baseAttr.nmic = true;

    auto &ns1 = devInfo.namespaces[1];
    ns1.namespaceId = 2;
    ns1.baseAttr.nsze = 200;
    ns1.baseAttr.ncap = 160;
    ns1.usedBytes = 100;
    strncpy_s(ns1.devPath, DEV_PATH_SIZE, "/dev/nvme0n2", strlen("/dev/nvme0n2"));
    ns1.baseAttr.nmic = false;

    UbseSsuDevInfo info;
    impl.ConvertDevInfo(devInfo, info);
    ASSERT_EQ(info.nameSpaces.size(), 2u);

    EXPECT_EQ(info.nameSpaces[0].namespaceId, 1u);
    EXPECT_EQ(info.nameSpaces[0].nsze, 100u);
    EXPECT_EQ(info.nameSpaces[0].ncap, 80u);
    EXPECT_EQ(info.nameSpaces[0].nuse, 50u);
    EXPECT_EQ(info.nameSpaces[0].nsDevPath, "/dev/nvme0n1");
    EXPECT_EQ(info.nameSpaces[0].nsOptions.flbas, 1u);
    EXPECT_EQ(info.nameSpaces[0].nsOptions.dps, 0u);
    EXPECT_EQ(info.nameSpaces[0].nsOptions.anagrpid, 3u);
    EXPECT_EQ(info.nameSpaces[0].nsOptions.nvmsetid, 4u);
    EXPECT_EQ(info.nameSpaces[0].nsOptions.nmic, 1u);
    EXPECT_EQ(info.nameSpaces[0].subSystem.eid, eid);

    EXPECT_EQ(info.nameSpaces[1].namespaceId, 2u);
    EXPECT_EQ(info.nameSpaces[1].nsze, 200u);
    EXPECT_EQ(info.nameSpaces[1].ncap, 160u);
    EXPECT_EQ(info.nameSpaces[1].nuse, 100u);
    EXPECT_EQ(info.nameSpaces[1].nsDevPath, "/dev/nvme0n2");
    EXPECT_EQ(info.nameSpaces[1].nsOptions.nmic, 0u);
}

TEST_F(TestUbseSsuAdapterImpl, ConvertDevInfo_ZeroNamespaces)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('E');
    DevInfoT devInfo = MakeDevInfoT(eid, "nqn.convert", DevStatusT::DEV_ONLINE, 0);
    UbseSsuDevInfo info;
    impl.ConvertDevInfo(devInfo, info);
    EXPECT_EQ(info.nameSpaces.size(), 0u);
}

// ==================== GetSrcEid ====================

TEST_F(TestUbseSsuAdapterImpl, GetSrcEid_Success)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    DevEidT srcEid{};
    memset_s(srcEid.raw, EID_SIZE, 0xFF, EID_SIZE);
    uint32_t ret = impl.GetSrcEid(srcEid);
    EXPECT_EQ(ret, UBSE_OK);
    for (int i = 0; i < EID_SIZE; ++i) {
        EXPECT_EQ(srcEid.raw[i], 0);
    }
}

// ==================== ValidatePersistentPaths ====================

TEST_F(TestUbseSsuAdapterImpl, ValidatePersistentPaths_ValidByPaths)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::vector<std::string> paths = {
        "/dev/disk/by-id/nvme-eui.0011223344556677",
        "/dev/disk/by-id/nvme-eui.aabbccddeeff0011",
    };
    uint32_t ret = impl.ValidatePersistentPaths(paths);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseSsuAdapterImpl, ValidatePersistentPaths_SingleValidPath)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::vector<std::string> paths = {"/dev/disk/by-id/nvme-eui.0011223344556677"};
    uint32_t ret = impl.ValidatePersistentPaths(paths);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseSsuAdapterImpl, ValidatePersistentPaths_EmptyList)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::vector<std::string> paths;
    uint32_t ret = impl.ValidatePersistentPaths(paths);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseSsuAdapterImpl, ValidatePersistentPaths_InvalidPathDevNvme)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::vector<std::string> paths = {"/dev/nvme0n1"};
    uint32_t ret = impl.ValidatePersistentPaths(paths);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseSsuAdapterImpl, ValidatePersistentPaths_MixedValidInvalid)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::vector<std::string> paths = {
        "/dev/disk/by-id/nvme-eui.0011223344556677",
        "/dev/nvme0n1",
    };
    uint32_t ret = impl.ValidatePersistentPaths(paths);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseSsuAdapterImpl, ValidatePersistentPaths_ByPathInsteadOfById)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::vector<std::string> paths = {"/dev/disk/by-path/pci-0000:01:00.0-nvme-1"};
    uint32_t ret = impl.ValidatePersistentPaths(paths);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseSsuAdapterImpl, ValidatePersistentPaths_RelativePath)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::vector<std::string> paths = {"relative/path"};
    uint32_t ret = impl.ValidatePersistentPaths(paths);
    EXPECT_NE(ret, UBSE_OK);
}

// ==================== DlOpenLib ====================

TEST_F(TestUbseSsuAdapterImpl, DlOpenLib_FailWhenLibNotFound)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    impl.dlManager_.Close();
    impl.acquireDevInfo_ = nullptr;
    impl.createNamespace_ = nullptr;
    impl.deleteNamespace_ = nullptr;
    impl.attachNamespace_ = nullptr;
    impl.detachNamespace_ = nullptr;
    impl.addNamespaceAllowHost_ = nullptr;
    impl.removeNamespaceAllowHost_ = nullptr;
    impl.getNamespaceAllowHosts_ = nullptr;
    impl.freeAllowHostsMem_ = nullptr;

    UbseResult ret = impl.DlOpenLib();
    EXPECT_NE(ret, UBSE_OK);
}

// ==================== GetDevList ====================

TEST_F(TestUbseSsuAdapterImpl, GetDevList_EmptyInputList)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::vector<UbseSsuDevInfo> ssuInfoList;
    uint32_t ret = impl.GetDevList(ssuInfoList);
    EXPECT_NE(ret, UBSE_OK);
}

// ==================== AttachDevNameSpace ====================

TEST_F(TestUbseSsuAdapterImpl, AttachDevNameSpace_EmptyHostNqn)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('F');
    auto ns = MakeNameSpaceForBasic(eid, "nqn.attach", 1, "guid");
    uint32_t ret = impl.AttachDevNameSpace("", ns);
    EXPECT_NE(ret, UBSE_OK);
}

// ==================== DetachDevNameSpace ====================

TEST_F(TestUbseSsuAdapterImpl, DetachDevNameSpace_EmptyHostNqn)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('F');
    auto ns = MakeNameSpaceForBasic(eid, "nqn.detach", 1, "guid");
    uint32_t ret = impl.DetachDevNameSpace("", ns);
    EXPECT_NE(ret, UBSE_OK);
}

// ==================== AddNameSpaceAllowHost ====================

TEST_F(TestUbseSsuAdapterImpl, AddNameSpaceAllowHost_EmptyHostNqn)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('F');
    auto ns = MakeNameSpaceForBasic(eid, "nqn.allow", 1, "guid");
    uint32_t ret = impl.AddNameSpaceAllowHost(ns, "");
    EXPECT_NE(ret, UBSE_OK);
}

// ==================== RemoveNameSpaceAllowHost ====================

TEST_F(TestUbseSsuAdapterImpl, RemoveNameSpaceAllowHost_EmptyHostNqn)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('F');
    auto ns = MakeNameSpaceForBasic(eid, "nqn.allow", 1, "guid");
    uint32_t ret = impl.RemoveNameSpaceAllowHost(ns, "");
    EXPECT_NE(ret, UBSE_OK);
}

// ==================== CreateBlockDevice ====================

TEST_F(TestUbseSsuAdapterImpl, CreateBlockDevice_InvalidPathNotById)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::vector<std::string> paths = {"/dev/nvme0n1"};
    UbseCreateBlockDeviceOptions opts;
    std::string devicePath;
    uint32_t ret = impl.CreateBlockDevice("testdev", paths, opts, devicePath);
    EXPECT_NE(ret, UBSE_OK);
}

// ==================== VerifyNamespaceGuid ====================

TEST_F(TestUbseSsuAdapterImpl, VerifyNamespaceGuid_EmptyEid)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    UbseSsuDevNameSpace ns;
    ns.subSystem.eid = "";
    ns.guid = "someguid";
    bool ret = impl.VerifyNamespaceGuid(ns);
    EXPECT_FALSE(ret);
}

TEST_F(TestUbseSsuAdapterImpl, VerifyNamespaceGuid_EmptyGuid)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    UbseSsuDevNameSpace ns;
    ns.subSystem.eid = MakeEid('V');
    ns.guid = "";
    bool ret = impl.VerifyNamespaceGuid(ns);
    EXPECT_FALSE(ret);
}

// ==================== ConvertDevInfo Edge Cases ====================

TEST_F(TestUbseSsuAdapterImpl, ConvertDevInfo_EmptyEidInRaw)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    DevInfoT devInfo{};
    devInfo.state = DevStatusT::DEV_ONLINE;
    devInfo.nsCount = 0;
    devInfo.tnvmcap = 500;
    devInfo.unvmcap = 100;
    devInfo.cntlId = 2;
    strncpy_s(devInfo.devPath, DEV_PATH_SIZE, "/dev/nvme1", strlen("/dev/nvme1"));
    strncpy_s(devInfo.sn, SN_SIZE, "SN_EMPTY", strlen("SN_EMPTY"));
    strncpy_s(devInfo.mn, MN_SIZE, "MN_EMPTY", strlen("MN_EMPTY"));
    memset(devInfo.devAddr.tgtEid.raw, 0, EID_SIZE);
    strncpy_s(devInfo.devAddr.subNqn, SUBNQN_SIZE, "nqn.empty_eid", strlen("nqn.empty_eid"));
    UbseSsuDevInfo info;
    impl.ConvertDevInfo(devInfo, info);
    EXPECT_TRUE(info.subSystem.eid.empty());
    EXPECT_EQ(info.subSystem.subNqn, "nqn.empty_eid");
    EXPECT_EQ(info.totalBytes, 500u);
    EXPECT_EQ(info.usedBytes, 400u);
}

TEST_F(TestUbseSsuAdapterImpl, ConvertDevInfo_LargeCapacity)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    DevInfoT devInfo{};
    devInfo.state = DevStatusT::DEV_ONLINE;
    devInfo.nsCount = 0;
    devInfo.tnvmcap = 1099511627776ULL;
    devInfo.unvmcap = 109951162777ULL;
    devInfo.cntlId = 3;
    strncpy_s(devInfo.devPath, DEV_PATH_SIZE, "/dev/nvme2", strlen("/dev/nvme2"));
    strncpy_s(devInfo.devAddr.subNqn, SUBNQN_SIZE, "nqn.large", strlen("nqn.large"));
    UbseSsuDevInfo info;
    impl.ConvertDevInfo(devInfo, info);
    EXPECT_EQ(info.totalBytes, 1099511627776ULL);
    EXPECT_EQ(info.usedBytes, 1099511627776ULL - 109951162777ULL);
}

// ==================== BuildDevAddrList Edge Cases ====================

TEST_F(TestUbseSsuAdapterImpl, BuildDevAddrList_SubNqnAtMaxSize)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('M');
    std::string maxSubNqn(SUBNQN_SIZE - 1, 'n');
    std::vector<UbseSsuDevInfo> ssuInfoList = {MakeDevInfo(eid, maxSubNqn)};
    std::vector<DevAddrT> devList;
    uint32_t ret = impl.BuildDevAddrList(ssuInfoList, devList);
    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(devList.size(), 1u);
    EXPECT_EQ(std::string(devList[0].subNqn), maxSubNqn);
}

TEST_F(TestUbseSsuAdapterImpl, BuildDevAddrList_SrcEidZeroed)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('S');
    std::vector<UbseSsuDevInfo> ssuInfoList = {MakeDevInfo(eid, "nqn.src")};
    std::vector<DevAddrT> devList;
    uint32_t ret = impl.BuildDevAddrList(ssuInfoList, devList);
    EXPECT_EQ(ret, UBSE_OK);
    for (int i = 0; i < EID_SIZE; ++i) {
        EXPECT_EQ(devList[0].srcEid.raw[i], 0);
    }
}

// ==================== BuildNamespaceInfoForCreate Edge Cases ====================

TEST_F(TestUbseSsuAdapterImpl, BuildNamespaceInfoForCreate_SrcEidZeroed)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('C');
    auto ns = MakeNameSpaceForCreate(eid, "nqn.srccheck", 100, 50);
    DevNamespaceInfoT nsInfo{};
    uint32_t ret = impl.BuildNamespaceInfoForCreate(ns, nsInfo);
    EXPECT_EQ(ret, UBSE_OK);
    for (int i = 0; i < EID_SIZE; ++i) {
        EXPECT_EQ(nsInfo.devAddr.srcEid.raw[i], 0);
    }
}

TEST_F(TestUbseSsuAdapterImpl, BuildNamespaceInfoForCreate_NmicFalse)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('C');
    auto ns = MakeNameSpaceForCreate(eid, "nqn.nmic", 100, 50);
    ns.nsOptions.nmic = 0;
    DevNamespaceInfoT nsInfo{};
    uint32_t ret = impl.BuildNamespaceInfoForCreate(ns, nsInfo);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_FALSE(nsInfo.baseAttr.nmic);
}

// ==================== BuildNamespaceInfoForBasic Edge Cases ====================

TEST_F(TestUbseSsuAdapterImpl, BuildNamespaceInfoForBasic_SrcEidZeroed)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('D');
    auto ns = MakeNameSpaceForBasic(eid, "nqn.srccheck", 3, "guid");
    DevNamespaceInfoT nsInfo{};
    uint32_t ret = impl.BuildNamespaceInfoForBasic(ns, nsInfo);
    EXPECT_EQ(ret, UBSE_OK);
    for (int i = 0; i < EID_SIZE; ++i) {
        EXPECT_EQ(nsInfo.devAddr.srcEid.raw[i], 0);
    }
}

TEST_F(TestUbseSsuAdapterImpl, BuildNamespaceInfoForBasic_GuidShorterThanMaxSize)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('D');
    std::string shortGuid = "short";
    auto ns = MakeNameSpaceForBasic(eid, "nqn.basic", 5, shortGuid);
    DevNamespaceInfoT nsInfo{};
    uint32_t ret = impl.BuildNamespaceInfoForBasic(ns, nsInfo);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(memcmp(nsInfo.guid, shortGuid.c_str(), shortGuid.size()), 0);
}

// ==================== ValidatePersistentPaths Edge Cases ====================

TEST_F(TestUbseSsuAdapterImpl, ValidatePersistentPaths_EmptyString)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::vector<std::string> paths = {""};
    uint32_t ret = impl.ValidatePersistentPaths(paths);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseSsuAdapterImpl, ValidatePersistentPaths_OnlyByIdPrefix)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::vector<std::string> paths = {"/dev/disk/by-id/"};
    uint32_t ret = impl.ValidatePersistentPaths(paths);
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== ConvertDevInfo Namespace with userData ====================

TEST_F(TestUbseSsuAdapterImpl, ConvertDevInfo_NamespaceUserDataCopied)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('E');
    DevInfoT devInfo = MakeDevInfoT(eid, "nqn.userdata", DevStatusT::DEV_ONLINE, 1);
    auto &ns0 = devInfo.namespaces[0];
    ns0.namespaceId = 10;
    ns0.baseAttr.nsze = 50;
    ns0.baseAttr.ncap = 40;
    ns0.usedBytes = 30;
    memset(ns0.userData, 0xCD, USER_DATA_SIZE);
    UbseSsuDevInfo info;
    impl.ConvertDevInfo(devInfo, info);
    ASSERT_EQ(info.nameSpaces.size(), 1u);
    EXPECT_EQ(info.nameSpaces[0].namespaceId, 10u);
    EXPECT_EQ(memcmp(&info.nameSpaces[0].customData, ns0.userData, sizeof(info.nameSpaces[0].customData)), 0);
}

} // namespace ubse::adapter_plugins::ssu::ut
