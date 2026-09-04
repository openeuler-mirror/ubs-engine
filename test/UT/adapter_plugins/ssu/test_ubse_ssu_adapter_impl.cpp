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

#include <cstdlib>
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
    // 空列表时 BuildDevAddrList 会从环境变量 SSU_NVME_SERVER_IP_LIST 读取；
    // 测试环境未设置该环境变量，因此应返回失败。
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::vector<UbseSsuDevInfo> ssuInfoList;
    std::vector<DevAddrT> devList;
    uint32_t ret = impl.BuildDevAddrList(ssuInfoList, devList);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseSsuAdapterImpl, BuildDevAddrList_EmptyListReadsFromEnv)
{
    // 空列表时从环境变量 SSU_NVME_SERVER_IP_LIST 读取并构建 devList
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid1 = MakeEid('A');
    std::string eid2 = MakeEid('B');
    std::string entry = "192.168.1.10:8080/" + eid1 + ",192.168.1.11:9090/" + eid2;
    setenv("SSU_NVME_SERVER_IP_LIST", entry.c_str(), 1);
    std::vector<UbseSsuDevInfo> ssuInfoList;
    std::vector<DevAddrT> devList;
    uint32_t ret = impl.BuildDevAddrList(ssuInfoList, devList);
    unsetenv("SSU_NVME_SERVER_IP_LIST");
    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(devList.size(), 2u);
    EXPECT_EQ(memcmp(devList[0].tgtEid.raw, eid1.c_str(), EID_SIZE), 0);
    EXPECT_EQ(std::string(devList[0].devIp), "192.168.1.10");
    EXPECT_EQ(devList[0].jettyId, 8080u);
    EXPECT_FALSE(devList[0].useUb);
    EXPECT_EQ(memcmp(devList[1].tgtEid.raw, eid2.c_str(), EID_SIZE), 0);
    EXPECT_EQ(std::string(devList[1].devIp), "192.168.1.11");
    EXPECT_EQ(devList[1].jettyId, 9090u);
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
    // devIp 是固定数组；环境变量未设置时 GetDevAddrByEid 查不到，devIp 保持空字符串
    EXPECT_EQ(devList[0].devIp[0], 0);
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

TEST_F(TestUbseSsuAdapterImpl, BuildDevAddrList_ShortEidCopiedSafely)
{
    // eid.size() < EID_SIZE 时按实际长度拷贝，剩余字节保持 0（不越界读取 eid 缓冲区）
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string shortEid = "short_eid"; // 9 字节 < EID_SIZE(16)
    std::vector<UbseSsuDevInfo> ssuInfoList = {MakeDevInfo(shortEid, "nqn.test")};
    std::vector<DevAddrT> devList;
    uint32_t ret = impl.BuildDevAddrList(ssuInfoList, devList);
    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(devList.size(), 1u);
    EXPECT_EQ(memcmp(devList[0].tgtEid.raw, shortEid.c_str(), shortEid.size()), 0);
    // 尾部剩余字节应为 0
    for (size_t i = shortEid.size(); i < EID_SIZE; ++i) {
        EXPECT_EQ(devList[0].tgtEid.raw[i], 0);
    }
}

// 注：BuildDevAddrList 不再校验 EID 长度/subNqn 是否为空（校验已移至 BuildNamespaceInfoForCreate/Basic，
// 由对应的 *InvalidEidLength*/*EmptySubNqn* 等用例覆盖），故不再保留此类用例。

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
    // devIp 是固定数组；环境变量未设置时 GetDevAddrByEid 查不到，devIp 保持空字符串
    EXPECT_EQ(nsInfo.devAddr.devIp[0], 0);
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

TEST_F(TestUbseSsuAdapterImpl, BuildNamespaceInfoForCreate_EidTooLong)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid(EID_SIZE + 5, 'X');
    auto ns = MakeNameSpaceForCreate(eid, "nqn.create", 1024, 512);
    DevNamespaceInfoT nsInfo{};
    uint32_t ret = impl.BuildNamespaceInfoForCreate(ns, nsInfo);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseSsuAdapterImpl, BuildNamespaceInfoForCreate_JettyIdSet)
{
    // jettyId 现在按 EID 从环境变量 SSU_NVME_SERVER_IP_LIST 查找得到，
    // 不再从 ns.subSystem.jettyId 取值。设置环境变量以验证查找逻辑。
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('C');
    std::string entry = "192.168.1.100:42/" + eid;
    setenv("SSU_NVME_SERVER_IP_LIST", entry.c_str(), 1);
    auto ns = MakeNameSpaceForCreate(eid, "nqn.create", 1024, 512);
    DevNamespaceInfoT nsInfo{};
    uint32_t ret = impl.BuildNamespaceInfoForCreate(ns, nsInfo);
    unsetenv("SSU_NVME_SERVER_IP_LIST");
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(nsInfo.devAddr.jettyId, 42u);
    EXPECT_EQ(std::string(nsInfo.devAddr.devIp), "192.168.1.100");
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
    // devIp 是固定数组；环境变量未设置时 GetDevAddrByEid 查不到，devIp 保持空字符串
    EXPECT_EQ(nsInfo.devAddr.devIp[0], 0);
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

TEST_F(TestUbseSsuAdapterImpl, BuildNamespaceInfoForBasic_EidTooLong)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid(EID_SIZE + 5, 'X');
    auto ns = MakeNameSpaceForBasic(eid, "nqn.basic", 1, "guid");
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
    // jettyId 现在按 EID 从环境变量 SSU_NVME_SERVER_IP_LIST 查找得到，
    // 不再从 ns.subSystem.jettyId 取值。设置环境变量以验证查找逻辑。
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('D');
    std::string entry = "192.168.2.200:99/" + eid;
    setenv("SSU_NVME_SERVER_IP_LIST", entry.c_str(), 1);
    auto ns = MakeNameSpaceForBasic(eid, "nqn.basic", 5, "guid");
    DevNamespaceInfoT nsInfo{};
    uint32_t ret = impl.BuildNamespaceInfoForBasic(ns, nsInfo);
    unsetenv("SSU_NVME_SERVER_IP_LIST");
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(nsInfo.devAddr.jettyId, 99u);
    EXPECT_EQ(std::string(nsInfo.devAddr.devIp), "192.168.2.200");
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
    // usedBytes为字节，flbas=1对应4K LBA，nuse应换算为LBA数量：8192/4096=2
    ns0.usedBytes = 8192;
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
    // usedBytes为字节，flbas默认0对应512B LBA，nuse应换算为LBA数量：51200/512=100
    ns1.usedBytes = 51200;
    strncpy_s(ns1.devPath, DEV_PATH_SIZE, "/dev/nvme0n2", strlen("/dev/nvme0n2"));
    ns1.baseAttr.nmic = false;

    UbseSsuDevInfo info;
    impl.ConvertDevInfo(devInfo, info);
    ASSERT_EQ(info.nameSpaces.size(), 2u);

    EXPECT_EQ(info.nameSpaces[0].namespaceId, 1u);
    EXPECT_EQ(info.nameSpaces[0].nsze, 100u);
    EXPECT_EQ(info.nameSpaces[0].ncap, 80u);
    // 8192字节 / 4096 LBA Size = 2 LBA
    EXPECT_EQ(info.nameSpaces[0].nuse, 2u);
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
    // 51200字节 / 512 LBA Size = 100 LBA
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

TEST_F(TestUbseSsuAdapterImpl, ConvertDevInfo_NamespaceGuidUuid)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('E');
    DevInfoT devInfo = MakeDevInfoT(eid, "nqn.guidtest", DevStatusT::DEV_ONLINE, 1);
    auto &ns0 = devInfo.namespaces[0];
    ns0.namespaceId = 5;
    ns0.baseAttr.nsze = 200;
    ns0.baseAttr.ncap = 150;
    ns0.usedBytes = 80;
    memset(ns0.guid, 0xAA, GUID_SIZE);
    memset(ns0.uuid, 0xBB, UUID_SIZE);
    UbseSsuDevInfo info;
    impl.ConvertDevInfo(devInfo, info);
    ASSERT_EQ(info.nameSpaces.size(), 1u);
    EXPECT_EQ(info.nameSpaces[0].namespaceId, 5u);
    EXPECT_EQ(info.nameSpaces[0].guid.size(), GUID_SIZE);
    EXPECT_EQ(info.nameSpaces[0].uuid.size(), UUID_SIZE);
    EXPECT_EQ(memcmp(info.nameSpaces[0].guid.c_str(), ns0.guid, GUID_SIZE), 0);
    EXPECT_EQ(memcmp(info.nameSpaces[0].uuid.c_str(), ns0.uuid, UUID_SIZE), 0);
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

TEST_F(TestUbseSsuAdapterImpl, GetSrcEid_WithEnvVar)
{
    // 设置 SSU_SRC_EID 时应将其内容拷贝到 srcEid.raw
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string srcEidStr = MakeEid('Z');
    setenv("SSU_SRC_EID", srcEidStr.c_str(), 1);
    DevEidT srcEid{};
    memset_s(srcEid.raw, EID_SIZE, 0, EID_SIZE);
    uint32_t ret = impl.GetSrcEid(srcEid);
    unsetenv("SSU_SRC_EID");
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(memcmp(srcEid.raw, srcEidStr.c_str(), EID_SIZE), 0);
}

TEST_F(TestUbseSsuAdapterImpl, GetSrcEid_TruncatedWhenTooLong)
{
    // SSU_SRC_EID 超过 EID_SIZE 时应按 EID_SIZE 截断拷贝
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string longEid(EID_SIZE + 10, 'Z');
    setenv("SSU_SRC_EID", longEid.c_str(), 1);
    DevEidT srcEid{};
    memset_s(srcEid.raw, EID_SIZE, 0xFF, EID_SIZE);
    uint32_t ret = impl.GetSrcEid(srcEid);
    unsetenv("SSU_SRC_EID");
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(memcmp(srcEid.raw, longEid.c_str(), EID_SIZE), 0);
}

// ==================== GetDevAddrByEid ====================

TEST_F(TestUbseSsuAdapterImpl, GetDevAddrByEid_Success)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('A');
    std::string entry = "192.168.1.10:8080/" + eid;
    setenv("SSU_NVME_SERVER_IP_LIST", entry.c_str(), 1);
    char devIp[DEV_IP_SIZE] = {0};
    uint32_t jettyId = 0;
    uint32_t ret = impl.GetDevAddrByEid(eid, devIp, jettyId);
    unsetenv("SSU_NVME_SERVER_IP_LIST");
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(std::string(devIp), "192.168.1.10");
    EXPECT_EQ(jettyId, 8080u);
}

TEST_F(TestUbseSsuAdapterImpl, GetDevAddrByEid_EnvNotSet)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    unsetenv("SSU_NVME_SERVER_IP_LIST");
    std::string eid = MakeEid('A');
    char devIp[DEV_IP_SIZE] = {0};
    uint32_t jettyId = 999;
    uint32_t ret = impl.GetDevAddrByEid(eid, devIp, jettyId);
    EXPECT_NE(ret, UBSE_OK);
    // 入口处 devIp/jettyId 被重置为 0
    EXPECT_EQ(devIp[0], 0);
    EXPECT_EQ(jettyId, 0u);
}

TEST_F(TestUbseSsuAdapterImpl, GetDevAddrByEid_EidNotFound)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid1 = MakeEid('A');
    std::string eid2 = MakeEid('B');
    std::string entry = "192.168.1.10:8080/" + eid1;
    setenv("SSU_NVME_SERVER_IP_LIST", entry.c_str(), 1);
    char devIp[DEV_IP_SIZE] = {0};
    uint32_t jettyId = 0;
    uint32_t ret = impl.GetDevAddrByEid(eid2, devIp, jettyId); // 查找 B，仅存在 A
    unsetenv("SSU_NVME_SERVER_IP_LIST");
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseSsuAdapterImpl, GetDevAddrByEid_MultipleEntriesMatchSecond)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid1 = MakeEid('A');
    std::string eid2 = MakeEid('B');
    std::string entry = "10.0.0.1:1/" + eid1 + ",10.0.0.2:2/" + eid2;
    setenv("SSU_NVME_SERVER_IP_LIST", entry.c_str(), 1);
    char devIp[DEV_IP_SIZE] = {0};
    uint32_t jettyId = 0;
    uint32_t ret = impl.GetDevAddrByEid(eid2, devIp, jettyId);
    unsetenv("SSU_NVME_SERVER_IP_LIST");
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(std::string(devIp), "10.0.0.2");
    EXPECT_EQ(jettyId, 2u);
}

TEST_F(TestUbseSsuAdapterImpl, GetDevAddrByEid_SkipsMalformedEntries)
{
    // 单条格式错误应被跳过继续查找：第一条畸形（无 /EID），第二条匹配目标 EID，应成功
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('A');
    std::string entry = "bad_entry,10.0.0.2:2/" + eid;
    setenv("SSU_NVME_SERVER_IP_LIST", entry.c_str(), 1);
    char devIp[DEV_IP_SIZE] = {0};
    uint32_t jettyId = 0;
    uint32_t ret = impl.GetDevAddrByEid(eid, devIp, jettyId);
    unsetenv("SSU_NVME_SERVER_IP_LIST");
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(std::string(devIp), "10.0.0.2");
    EXPECT_EQ(jettyId, 2u);
}

TEST_F(TestUbseSsuAdapterImpl, GetDevAddrByEid_InvalidEntryMissingSlash)
{
    // 条目缺少 /EID 分隔符，ParseSsuIpEntry 失败被跳过；列表中无匹配 EID，返回失败
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('A');
    setenv("SSU_NVME_SERVER_IP_LIST", "192.168.1.10:8080", 1);
    char devIp[DEV_IP_SIZE] = {0};
    uint32_t jettyId = 0;
    uint32_t ret = impl.GetDevAddrByEid(eid, devIp, jettyId);
    unsetenv("SSU_NVME_SERVER_IP_LIST");
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseSsuAdapterImpl, GetDevAddrByEid_Ipv6Address)
{
    // IPv6 地址用 rfind(':') 分离 IP 与 PORT，应正确解析
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('A');
    std::string entry = "[2001:db8::1]:443/" + eid;
    setenv("SSU_NVME_SERVER_IP_LIST", entry.c_str(), 1);
    char devIp[DEV_IP_SIZE] = {0};
    uint32_t jettyId = 0;
    uint32_t ret = impl.GetDevAddrByEid(eid, devIp, jettyId);
    unsetenv("SSU_NVME_SERVER_IP_LIST");
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(std::string(devIp), "[2001:db8::1]");
    EXPECT_EQ(jettyId, 443u);
}

TEST_F(TestUbseSsuAdapterImpl, GetDevAddrByEid_IpTooLong)
{
    // IP 长度 >= DEV_IP_SIZE 时应返回失败
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('A');
    std::string longIp(DEV_IP_SIZE + 5, 'x');
    std::string entry = longIp + ":8080/" + eid;
    setenv("SSU_NVME_SERVER_IP_LIST", entry.c_str(), 1);
    char devIp[DEV_IP_SIZE] = {0};
    uint32_t jettyId = 0;
    uint32_t ret = impl.GetDevAddrByEid(eid, devIp, jettyId);
    unsetenv("SSU_NVME_SERVER_IP_LIST");
    EXPECT_NE(ret, UBSE_OK);
}

// ==================== ValidatePersistentPaths ====================

TEST_F(TestUbseSsuAdapterImpl, ValidatePersistentPaths_ValidByPaths)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::vector<std::string> paths = {
        "/dev/disk/by-id/nvme-uuid.00112233-4455-6666-7777-888888999999",
        "/dev/disk/by-id/nvme-uuid.aaaabbbb-cccc-dddd-eeee-ffff00001111",
    };
    uint32_t ret = impl.ValidatePersistentPaths(paths);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseSsuAdapterImpl, ValidatePersistentPaths_SingleValidPath)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::vector<std::string> paths = {"/dev/disk/by-id/nvme-uuid.00112233-4455-6666-7777-888888999999"};
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
        "/dev/disk/by-id/nvme-uuid.00112233-4455-6666-7777-888888999999",
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

    MOCKER_CPP(&dlopen).stubs().will(returnValue(static_cast<void *>(nullptr)));
 	char dlerrorMsg[] = "mock dlopen failed";
 	MOCKER_CPP(dlerror).stubs().will(returnValue(static_cast<char *>(dlerrorMsg)));

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

TEST_F(TestUbseSsuAdapterImpl, AttachDevNameSpace_InvalidEid)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    auto ns = MakeNameSpaceForBasic("short", "nqn.attach", 1, "guid");
    uint32_t ret = impl.AttachDevNameSpace("hostNqn", ns);
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

TEST_F(TestUbseSsuAdapterImpl, DetachDevNameSpace_InvalidEid)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    auto ns = MakeNameSpaceForBasic("short", "nqn.detach", 1, "guid");
    uint32_t ret = impl.DetachDevNameSpace("hostNqn", ns);
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

TEST_F(TestUbseSsuAdapterImpl, AddNameSpaceAllowHost_InvalidEid)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    auto ns = MakeNameSpaceForBasic("short", "nqn.allow", 1, "guid");
    uint32_t ret = impl.AddNameSpaceAllowHost(ns, "hostNqn");
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

TEST_F(TestUbseSsuAdapterImpl, RemoveNameSpaceAllowHost_InvalidEid)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    auto ns = MakeNameSpaceForBasic("short", "nqn.allow", 1, "guid");
    uint32_t ret = impl.RemoveNameSpaceAllowHost(ns, "hostNqn");
    EXPECT_NE(ret, UBSE_OK);
}

// ==================== GetNameSpaceAllowHostList ====================

TEST_F(TestUbseSsuAdapterImpl, GetNameSpaceAllowHostList_InvalidEid)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    auto ns = MakeNameSpaceForBasic("short", "nqn.allow", 1, "guid");
    std::vector<std::string> allowHostList;
    uint32_t ret = impl.GetNameSpaceAllowHostList(ns, allowHostList);
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

TEST_F(TestUbseSsuAdapterImpl, CreateBlockDevice_LinearModeInvalidPath)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::vector<std::string> paths = {"/dev/nvme0n1"};
    UbseCreateBlockDeviceOptions opts;
    opts.addressingType = UbseSsuAddressingType::LINEAR;
    std::string devicePath;
    uint32_t ret = impl.CreateBlockDevice("testdev_linear", paths, opts, devicePath);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseSsuAdapterImpl, CreateBlockDevice_EmptyPathList)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::vector<std::string> paths;
    UbseCreateBlockDeviceOptions opts;
    std::string devicePath;
    uint32_t ret = impl.CreateBlockDevice("testdev", paths, opts, devicePath);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseSsuAdapterImpl, CreateBlockDevice_StripedModeInvalidPath)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::vector<std::string> paths = {"/dev/nvme0n1", "/dev/nvme0n2"};
    UbseCreateBlockDeviceOptions opts;
    opts.addressingType = UbseSsuAddressingType::STRIPED;
    opts.raidLevel = UbseSsuRaidLevel::RAID0;
    std::string devicePath;
    uint32_t ret = impl.CreateBlockDevice("testdev_striped", paths, opts, devicePath);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseSsuAdapterImpl, CreateBlockDevice_Raid5InvalidPath)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::vector<std::string> paths = {"/dev/nvme0n1", "/dev/nvme0n2", "/dev/nvme0n3"};
    UbseCreateBlockDeviceOptions opts;
    opts.addressingType = UbseSsuAddressingType::STRIPED;
    opts.raidLevel = UbseSsuRaidLevel::RAID5;
    std::string devicePath;
    uint32_t ret = impl.CreateBlockDevice("testdev_raid5", paths, opts, devicePath);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseSsuAdapterImpl, CreateBlockDevice_InvalidDeviceNameRejected)
{
    // deviceName 含 shell 元字符应被白名单校验拦截，防止命令注入
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::vector<std::string> paths = {"/dev/disk/by-id/nvme-uuid.00112233-4455-6666-7777-888888999999"};
    UbseCreateBlockDeviceOptions opts;
    std::string devicePath;
    uint32_t ret = impl.CreateBlockDevice("test;rm -rf /", paths, opts, devicePath);
    EXPECT_NE(ret, UBSE_OK);
}

// ==================== DeleteBlockDevice ====================

TEST_F(TestUbseSsuAdapterImpl, DeleteBlockDevice_NonExistentDevice)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    uint32_t ret = impl.DeleteBlockDevice("nonexistent_device_12345");
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseSsuAdapterImpl, DeleteBlockDevice_InvalidDeviceNameRejected)
{
    // deviceName 含 shell 元字符应被白名单校验拦截，防止命令注入
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    uint32_t ret = impl.DeleteBlockDevice("test;rm -rf /");
    EXPECT_NE(ret, UBSE_OK);
}

// ==================== VerifyNamespaceUuid ====================

TEST_F(TestUbseSsuAdapterImpl, VerifyNamespaceUuid_EmptyEid)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    UbseSsuDevNameSpace ns;
    ns.subSystem.eid = "";
    ns.uuid = "someuuid";
    uint32_t ret = impl.VerifyNamespaceUuid(ns);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseSsuAdapterImpl, VerifyNamespaceUuid_EmptyUuid)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    UbseSsuDevNameSpace ns;
    ns.subSystem.eid = MakeEid('V');
    ns.uuid = "";
    uint32_t ret = impl.VerifyNamespaceUuid(ns);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseSsuAdapterImpl, VerifyNamespaceUuid_BothEidAndUuidEmpty)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    UbseSsuDevNameSpace ns;
    ns.subSystem.eid = "";
    ns.uuid = "";
    uint32_t ret = impl.VerifyNamespaceUuid(ns);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
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

TEST_F(TestUbseSsuAdapterImpl, BuildNamespaceInfoForCreate_CustomDataCopied)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('C');
    auto ns = MakeNameSpaceForCreate(eid, "nqn.custom", 100, 50);
    memset(&ns.customData, 0xAB, sizeof(ns.customData));
    ns.customData.version = 1;
    DevNamespaceInfoT nsInfo{};
    uint32_t ret = impl.BuildNamespaceInfoForCreate(ns, nsInfo);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(nsInfo.userData[0], 1);
    EXPECT_EQ(nsInfo.userData[1], 0xAB);
}

TEST_F(TestUbseSsuAdapterImpl, BuildNamespaceInfoForCreate_SubNqnAtMaxSize)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('C');
    std::string maxSubNqn(SUBNQN_SIZE - 1, 'n');
    auto ns = MakeNameSpaceForCreate(eid, maxSubNqn, 100, 50);
    DevNamespaceInfoT nsInfo{};
    uint32_t ret = impl.BuildNamespaceInfoForCreate(ns, nsInfo);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(std::string(nsInfo.devAddr.subNqn), maxSubNqn);
}

TEST_F(TestUbseSsuAdapterImpl, BuildNamespaceInfoForCreate_NmicNonZero)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('C');
    auto ns = MakeNameSpaceForCreate(eid, "nqn.nmic", 100, 50);
    ns.nsOptions.nmic = 42;
    DevNamespaceInfoT nsInfo{};
    uint32_t ret = impl.BuildNamespaceInfoForCreate(ns, nsInfo);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_TRUE(nsInfo.baseAttr.nmic);
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

TEST_F(TestUbseSsuAdapterImpl, BuildNamespaceInfoForBasic_GuidLongerThanMaxSize)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('D');
    std::string longGuid(GUID_SIZE + 10, 'G');
    auto ns = MakeNameSpaceForBasic(eid, "nqn.basic", 5, longGuid);
    DevNamespaceInfoT nsInfo{};
    uint32_t ret = impl.BuildNamespaceInfoForBasic(ns, nsInfo);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(memcmp(nsInfo.guid, longGuid.c_str(), GUID_SIZE), 0);
}

TEST_F(TestUbseSsuAdapterImpl, BuildNamespaceInfoForBasic_EmptySubNqn)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('D');
    auto ns = MakeNameSpaceForBasic(eid, "", 1, "guid");
    DevNamespaceInfoT nsInfo{};
    uint32_t ret = impl.BuildNamespaceInfoForBasic(ns, nsInfo);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseSsuAdapterImpl, BuildNamespaceInfoForBasic_SubNqnAtMaxSize)
{
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::string eid = MakeEid('D');
    std::string maxSubNqn(SUBNQN_SIZE - 1, 'n');
    auto ns = MakeNameSpaceForBasic(eid, maxSubNqn, 5, "guid");
    DevNamespaceInfoT nsInfo{};
    uint32_t ret = impl.BuildNamespaceInfoForBasic(ns, nsInfo);
    EXPECT_EQ(ret, UBSE_OK);
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

TEST_F(TestUbseSsuAdapterImpl, ValidatePersistentPaths_UnsafeCharsRejected)
{
    // 前缀合法但含 shell 元字符（;）应被拒绝，防止命令注入
    auto &impl = UbseSsuAdapterImpl::GetInstance();
    std::vector<std::string> paths = {"/dev/disk/by-id/nvme-eui.0011223344556677;rm -rf /"};
    uint32_t ret = impl.ValidatePersistentPaths(paths);
    EXPECT_NE(ret, UBSE_OK);
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
