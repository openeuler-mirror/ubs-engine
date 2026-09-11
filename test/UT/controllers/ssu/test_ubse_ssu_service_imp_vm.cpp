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

// VM 模式下 UbseSsuServiceImp 分支 UT(查询/attach/detach/不支持接口/聚合 attach)

#include "test_ubse_ssu_service_imp_vm.h"

#include <vector>
#include "config/ubse_conf.h"
#include "ubse_error.h"

namespace ubse::ssu::service::ut {

using ubse::ssu::controller::UbseSsuVmCache;

// ============================================================================
// TestUbseSsuServiceImpVm 夹具实现
// ============================================================================

void TestUbseSsuServiceImpVm::SetUp()
{
    UbseSsuServiceImpTestBase::SetUp();
    SetupAdapterFuncs();
    ResetControllableMockState();

    // VM 模式:开启 vmMode_ 并注入 vmCache_
    service_.SetVmMode(true);
    service_.SetVmCache(&vmCache_);
}

void TestUbseSsuServiceImpVm::TearDown()
{
    // 恢复 host 模式状态,避免污染其他测试
    service_.SetVmCache(nullptr);
    service_.SetVmMode(false);
    vmCache_.Clear();
    // Reset dlManager handle_ to avoid ~UbseDlManager calling dlclose(0x1) and causing segfault
    UbseSsuAdapterImpl::GetInstance().dlManager_.handle_ = nullptr;
    UbseSsuServiceImpTestBase::TearDown();
}

UbseSsuAllocResult TestUbseSsuServiceImpVm::MakeVmAllocResult(const std::string &name, uint32_t nsCount)
{
    UbseSsuAllocResult r{};
    r.name = name;
    r.strategy = UbseSsuAllocStrategy::NORMAL;
    for (uint32_t i = 0; i < nsCount; ++i) {
        UbseSsuNameSpaceInfo ns{};
        // EID 必须为固定 16 字符,与适配器侧构造 namespace 信息的长度约束保持一致
        ns.tgtEid = std::string(16, 'A' + static_cast<char>(i));
        ns.tgtNqn = "nqn.target." + name + "." + std::to_string(i);
        ns.nsUuid = "uuid-" + name + "-" + std::to_string(i);
        ns.namespaceId = i + 1;
        ns.nsDevPath = "/dev/nvme0n" + std::to_string(i + 1);
        ns.nsSize = 1024 * 1024 * 1024;  // 1GiB
        ns.lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_512;
        r.nameSpaceList.push_back(std::move(ns));
    }
    return r;
}

UbseSsuConnectInfo TestUbseSsuServiceImpVm::MakeVmConnectInfo(const std::string &name, const std::string &tgtEid)
{
    UbseSsuConnectInfo ci{};
    ci.srcEid = "src_" + name;
    ci.tgtEid = tgtEid;
    ci.tgtNqn = "nqn.target." + name;
    ci.hostNqn = "nqn.host." + name;
    ci.nsUuid = "uuid-" + name;
    ci.nsId = 1;
    return ci;
}

// ============================================================================
// A. 查询接口(VM 模式走缓存 + identity 校验)
// ============================================================================

namespace {
// 构造"适配器可见"的 namespace 并注入 ControllableAcquireDevInfo mock 返回列表,
// 返回值可直接放入缓存条目(nsUuid 与适配器 ns.uuid 经 StrToUuid 转换后一致,
// 使 FindNsInDevice 能按 (eid, nsUuid) 命中,身份校验走本地适配器 customData)
UbseSsuNameSpaceInfo PushAdapterNs(const std::string &eid, const std::string &subNqn, uint32_t nsId,
                                   const std::string &uuidByte, uid_t uid = 100,
                                   const std::string &userName = "test_user")
{
    // guid/uuid 均为 16 字节二进制;uuid 用 uuidByte 区分不同 namespace
    auto ns = TestUbseSsuServiceImpVm::MakeNsForCache(eid, subNqn, nsId, 4096, std::string(16, '\xAB'), uuidByte,
                                                      uid, userName);
    g_acquireDevInfoNsList.push_back(ns);
    return TestUbseSsuServiceImpVm::MakeNameSpaceInfo(ns, 4096 * 512);
}

// 启用适配器设备查询 mock(RefreshDevCache → GetDevList → ControllableAcquireDevInfo)
void SetupAdapterDevQueryMock()
{
    UbseSsuAdapterImpl::GetInstance().acquireDevInfo_ = ControllableAcquireDevInfo;
    // GetDevList 内部调 GetAdminNqn → UbseGetStr,需 mock 配置(与 GetNsStats 用例同一约定)
    MOCKER_CPP(&ubse::config::UbseGetStr)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), mockcpp::any())
        .will(invoke(TestUbseSsuServiceImpVm::MockUbseGetStr));
}

// 构造"适配器可见"的缓存分配条目:每个 namespace 注册到 ControllableAcquireDevInfo mock,
// 返回值可直接 UpdateAllocInfo(nsUuid 与适配器 ns.uuid 经 StrToUuid 转换后一致)。
// attach/detach 身份校验需要本地适配器 customData 与 req.identity 匹配,
// 配合 MakeIdentity()(uid=100/"test_user",与 MakeNsForCache 默认 customData 一致)使用
UbseSsuAllocResult MakeAdapterBackedAllocResult(const std::string &name, uint32_t nsCount,
                                                UbseSsuAllocStrategy strategy = UbseSsuAllocStrategy::NORMAL)
{
    g_acquireDevInfoNsList.clear();
    SetupAdapterDevQueryMock();
    UbseSsuAllocResult r{};
    r.name = name;
    r.strategy = strategy;
    for (uint32_t i = 0; i < nsCount; ++i) {
        auto eid = std::string(16, 'A' + static_cast<char>(i));
        r.nameSpaceList.push_back(PushAdapterNs(eid, "nqn.target." + name + "." + std::to_string(i), i + 1,
                                                std::string(16, static_cast<char>('P' + i))));
    }
    return r;
}
} // namespace

/*
 * 用例描述:VM 模式下 ListAllocInfo 从本地缓存返回分配信息列表,并经 identity 校验过滤
 * 测试步骤:
 * 1、mock 适配器返回两条属于当前 identity 的 namespace(eid A/B 各一)
 * 2、向 vmCache_ 推送两条对应的分配信息
 * 3、调用 ListAllocInfo
 * 预期结果:
 * 1、返回 UBSE_OK
 * 2、result 包含两条分配信息
 */
TEST_F(TestUbseSsuServiceImpVm, ListAllocInfo_VmMode_ReturnsCacheData)
{
    SetupAdapterDevQueryMock();
    const std::string eidA(16, 'A');
    const std::string eidB(16, 'B');

    UbseSsuAllocResult r1{};
    r1.name = "vm_alloc_1";
    r1.strategy = UbseSsuAllocStrategy::NORMAL;
    r1.nameSpaceList.push_back(PushAdapterNs(eidA, "nqn.target.vm_alloc_1", 1, std::string(16, 'P')));
    UbseSsuAllocResult r2{};
    r2.name = "vm_alloc_2";
    r2.strategy = UbseSsuAllocStrategy::NORMAL;
    r2.nameSpaceList.push_back(PushAdapterNs(eidB, "nqn.target.vm_alloc_2", 1, std::string(16, 'Q')));
    vmCache_.UpdateAllocInfo(r1);
    vmCache_.UpdateAllocInfo(r2);

    std::vector<UbseSsuAllocResult> result;
    auto ret = service_.ListAllocInfo(result, MakeIdentity());

    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(result.size(), 2u);
}

/*
 * 用例描述:VM 模式下空缓存查询返回空列表(不报错)
 * 测试步骤:
 * 1、清空 vmCache_
 * 2、调用 ListAllocInfo
 * 预期结果:
 * 1、返回 UBSE_OK
 * 2、result 为空
 */
TEST_F(TestUbseSsuServiceImpVm, ListAllocInfo_VmMode_EmptyCache_ReturnsEmpty)
{
    vmCache_.Clear();
    std::vector<UbseSsuAllocResult> result;
    auto ret = service_.ListAllocInfo(result, MakeIdentity());

    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_TRUE(result.empty());
}

/*
 * 用例描述:VM 模式下 GetAllocInfoByName 从本地缓存返回指定分配信息(identity 匹配)
 * 测试步骤:
 * 1、mock 适配器返回属于当前 identity 的 namespace
 * 2、向 vmCache_ 推送对应的分配信息
 * 3、按 name 查询
 * 预期结果:
 * 1、返回 UBSE_OK
 * 2、result.name 与推送一致
 */
TEST_F(TestUbseSsuServiceImpVm, GetAllocInfoByName_VmMode_Exists_ReturnsResult)
{
    SetupAdapterDevQueryMock();
    const std::string eidA(16, 'A');

    UbseSsuAllocResult pushed{};
    pushed.name = "vm_query_ok";
    pushed.strategy = UbseSsuAllocStrategy::NORMAL;
    pushed.nameSpaceList.push_back(PushAdapterNs(eidA, "nqn.target.vm_query_ok", 1, std::string(16, 'P')));
    vmCache_.UpdateAllocInfo(pushed);

    UbseSsuAllocResult result;
    auto ret = service_.GetAllocInfoByName("vm_query_ok", result, MakeIdentity());

    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(result.name, "vm_query_ok");
}

/*
 * 用例描述:VM 模式下查询不存在的 name 返回 UBSE_SSU_ERROR_SPACE_NOT_FOUND
 * 测试步骤:
 * 1、不向缓存推送任何数据
 * 2、按不存在的 name 查询
 * 预期结果:
 * 1、返回 UBSE_SSU_ERROR_SPACE_NOT_FOUND
 */
TEST_F(TestUbseSsuServiceImpVm, GetAllocInfoByName_VmMode_NotExists_ReturnsError)
{
    UbseSsuAllocResult result;
    auto ret = service_.GetAllocInfoByName("not_in_cache", result, MakeIdentity());

    EXPECT_EQ(ret, UBSE_SSU_ERROR_SPACE_NOT_FOUND);
}

/*
 * 用例描述:VM 模式下 GetAllocInfoByName identity 不匹配返回 UBSE_ERR_ACCESS_DENIED
 * (与 master 路径 VerifyGetInfoIdentity 语义一致)
 * 测试步骤:
 * 1、mock 适配器返回属于其他用户(uid=101/other_user)的 namespace
 * 2、向 vmCache_ 推送对应的分配信息
 * 3、以 MakeIdentity()(uid=100)按 name 查询
 * 预期结果:
 * 1、返回 UBSE_ERR_ACCESS_DENIED
 */
TEST_F(TestUbseSsuServiceImpVm, GetAllocInfoByName_VmMode_IdentityMismatch_ReturnsDenied)
{
    SetupAdapterDevQueryMock();
    const std::string eidB(16, 'B');

    UbseSsuAllocResult pushed{};
    pushed.name = "other_alloc";
    pushed.strategy = UbseSsuAllocStrategy::NORMAL;
    pushed.nameSpaceList.push_back(
        PushAdapterNs(eidB, "nqn.target.other", 1, std::string(16, 'Q'), 101, "other_user"));
    vmCache_.UpdateAllocInfo(pushed);

    UbseSsuAllocResult result;
    auto ret = service_.GetAllocInfoByName("other_alloc", result, MakeIdentity());

    EXPECT_EQ(ret, UBSE_ERR_ACCESS_DENIED);
}

/*
 * 用例描述:VM 模式下 GetConnectInfo 从本地缓存返回连接信息(分配信息 identity 匹配)
 * 测试步骤:
 * 1、mock 适配器返回属于当前 identity 的 namespace
 * 2、向 vmCache_ 推送对应的分配信息与连接信息
 * 3、调用 GetConnectInfo
 * 预期结果:
 * 1、返回 UBSE_OK
 * 2、connectInfoList 非空
 */
TEST_F(TestUbseSsuServiceImpVm, GetConnectInfo_VmMode_ReturnsCacheData)
{
    SetupAdapterDevQueryMock();
    const std::string eidA(16, 'A');

    UbseSsuAllocResult pushed{};
    pushed.name = "vm_connect";
    pushed.strategy = UbseSsuAllocStrategy::NORMAL;
    pushed.nameSpaceList.push_back(PushAdapterNs(eidA, "nqn.target.vm_connect", 1, std::string(16, 'P')));
    vmCache_.UpdateAllocInfo(pushed);
    vmCache_.UpdateConnectInfo("vm_connect", {MakeVmConnectInfo("vm_connect", eidA)});

    std::vector<UbseSsuConnectInfo> connectInfoList;
    auto ret = service_.GetConnectInfo("vm_connect", nullptr, connectInfoList, MakeIdentity());

    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_FALSE(connectInfoList.empty());
}

/*
 * 用例描述:VM 模式下 GetConnectInfo identity 不匹配返回 UBSE_ERR_ACCESS_DENIED
 * 测试步骤:
 * 1、mock 适配器返回属于其他用户(uid=101/other_user)的 namespace
 * 2、向 vmCache_ 推送对应的分配信息与连接信息
 * 3、以 MakeIdentity()(uid=100)调用 GetConnectInfo
 * 预期结果:
 * 1、返回 UBSE_ERR_ACCESS_DENIED
 */
TEST_F(TestUbseSsuServiceImpVm, GetConnectInfo_VmMode_IdentityMismatch_ReturnsDenied)
{
    SetupAdapterDevQueryMock();
    const std::string eidB(16, 'B');

    UbseSsuAllocResult pushed{};
    pushed.name = "vm_connect_other";
    pushed.strategy = UbseSsuAllocStrategy::NORMAL;
    pushed.nameSpaceList.push_back(
        PushAdapterNs(eidB, "nqn.target.vm_connect_other", 1, std::string(16, 'Q'), 101, "other_user"));
    vmCache_.UpdateAllocInfo(pushed);
    vmCache_.UpdateConnectInfo("vm_connect_other", {MakeVmConnectInfo("vm_connect_other", eidB)});

    std::vector<UbseSsuConnectInfo> connectInfoList;
    auto ret = service_.GetConnectInfo("vm_connect_other", nullptr, connectInfoList, MakeIdentity());

    EXPECT_EQ(ret, UBSE_ERR_ACCESS_DENIED);
}

// ============================================================================
// B. Attach 接口(VM 模式走本地适配器)
// ============================================================================

/*
 * 用例描述:VM 模式下 attach 缓存中不存在的 namespace 返回 UBSE_SSU_ERROR_NS_NOT_FOUND
 * 测试步骤:
 * 1、不向缓存推送任何数据
 * 2、调用 AttachSpace
 * 预期结果:
 * 1、返回 UBSE_SSU_ERROR_NS_NOT_FOUND
 */
TEST_F(TestUbseSsuServiceImpVm, AttachSpace_VmMode_NotInCache_ReturnsNotFound)
{
    auto req = MakeSpaceReq("not_in_cache", "nqn.2024-01.org.nvmexpress:uuid:11111111-1111-1111-1111-111111111111");
    std::vector<std::string> nsDevPaths;
    auto ret = service_.AttachSpace(req, nsDevPaths);

    EXPECT_EQ(ret, UBSE_SSU_ERROR_NS_NOT_FOUND);
}

/*
 * 用例描述:VM 模式下 attach 成功返回设备路径并调用 MarkAttached
 * 测试步骤:
 * 1、向缓存推送一条分配信息(1 个 namespace,适配器可见且 identity 匹配)
 * 2、调用 AttachSpace
 * 预期结果:
 * 1、返回 UBSE_OK
 * 2、nsDevPaths 非空
 * 3、vmCache_.IsAttached 返回 true
 */
TEST_F(TestUbseSsuServiceImpVm, AttachSpace_VmMode_Success_ReturnsDevPath)
{
    vmCache_.UpdateAllocInfo(MakeAdapterBackedAllocResult("vm_attach_ok", 1));

    auto req = MakeSpaceReq("vm_attach_ok", "nqn.2024-01.org.nvmexpress:uuid:11111111-1111-1111-1111-111111111111",
                            MakeIdentity());
    std::vector<std::string> nsDevPaths;
    auto ret = service_.AttachSpace(req, nsDevPaths);

    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_FALSE(nsDevPaths.empty());
    EXPECT_TRUE(vmCache_.IsAttached("vm_attach_ok"));
}

/*
 * 用例描述:VM 模式下重复 attach 同一 namespace 返回成功(幂等)
 * 测试步骤:
 * 1、向缓存推送一条分配信息(适配器可见且 identity 匹配)
 * 2、第一次 attach 成功
 * 3、第二次再次 attach
 * 预期结果:
 * 1、两次都返回 UBSE_OK
 */
TEST_F(TestUbseSsuServiceImpVm, AttachSpace_VmMode_AlreadyAttached_Idempotent)
{
    vmCache_.UpdateAllocInfo(MakeAdapterBackedAllocResult("vm_attach_idem", 1));
    auto req = MakeSpaceReq("vm_attach_idem", "nqn.2024-01.org.nvmexpress:uuid:11111111-1111-1111-1111-111111111111",
                            MakeIdentity());

    std::vector<std::string> nsDevPaths1;
    auto ret1 = service_.AttachSpace(req, nsDevPaths1);
    EXPECT_EQ(ret1, UBSE_OK);

    std::vector<std::string> nsDevPaths2;
    auto ret2 = service_.AttachSpace(req, nsDevPaths2);
    EXPECT_EQ(ret2, UBSE_OK);
}

// ============================================================================
// C. Detach 接口(VM 模式走本地适配器)
// ============================================================================

/*
 * 用例描述:VM 模式下 detach 已 attach 的 namespace 返回成功
 * 测试步骤:
 * 1、向缓存推送分配信息并 attach 成功
 * 2、调用 DetachSpace
 * 预期结果:
 * 1、返回 UBSE_OK
 * 2、vmCache_.IsAttached 返回 false
 */
TEST_F(TestUbseSsuServiceImpVm, DetachSpace_VmMode_Attached_Success)
{
    vmCache_.UpdateAllocInfo(MakeAdapterBackedAllocResult("vm_detach_ok", 1));
    auto attachReq = MakeSpaceReq("vm_detach_ok", "nqn.2024-01.org.nvmexpress:uuid:11111111-1111-1111-1111-111111111111",
                                  MakeIdentity());
    std::vector<std::string> nsDevPaths;
    ASSERT_EQ(service_.AttachSpace(attachReq, nsDevPaths), UBSE_OK);

    auto detachReq = MakeSpaceReq("vm_detach_ok", "nqn.2024-01.org.nvmexpress:uuid:11111111-1111-1111-1111-111111111111",
                                  MakeIdentity());
    auto ret = service_.DetachSpace(detachReq);

    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_FALSE(vmCache_.IsAttached("vm_detach_ok"));
}

/*
 * 用例描述:VM 模式下 detach 未 attach 的 namespace 返回成功(幂等容错)
 * 测试步骤:
 * 1、向缓存推送分配信息(不 attach)
 * 2、直接调用 DetachSpace
 * 预期结果:
 * 1、返回 UBSE_OK
 */
TEST_F(TestUbseSsuServiceImpVm, DetachSpace_VmMode_NotAttached_Idempotent)
{
    vmCache_.UpdateAllocInfo(MakeVmAllocResult("vm_detach_idem", 1));

    auto req = MakeSpaceReq("vm_detach_idem", "nqn.2024-01.org.nvmexpress:uuid:11111111-1111-1111-1111-111111111111");
    auto ret = service_.DetachSpace(req);

    EXPECT_EQ(ret, UBSE_OK);
}

// ============================================================================
// D. 不支持接口(VM 模式拒绝)
// ============================================================================

/*
 * 用例描述:VM 模式下 AllocSpace 返回 UBSE_ERR_NOT_SUPPORTED
 * 测试步骤:
 * 1、调用 AllocSpace
 * 预期结果:
 * 1、返回 UBSE_ERR_NOT_SUPPORTED
 */
TEST_F(TestUbseSsuServiceImpVm, AllocSpace_VmMode_ReturnsNotSupported)
{
    auto req = MakeAllocReq("vm_alloc_reject");
    UbseSsuAllocResult result;
    auto ret = service_.AllocSpace(req, MakeIdentity(), result);
    EXPECT_EQ(ret, UBSE_ERR_NOT_SUPPORTED);
}

/*
 * 用例描述:VM 模式下 FreeSpace 返回 UBSE_ERR_NOT_SUPPORTED
 * 测试步骤:
 * 1、调用 FreeSpace
 * 预期结果:
 * 1、返回 UBSE_ERR_NOT_SUPPORTED
 */
TEST_F(TestUbseSsuServiceImpVm, FreeSpace_VmMode_ReturnsNotSupported)
{
    auto ret = service_.FreeSpace("vm_free_reject", MakeIdentity());
    EXPECT_EQ(ret, UBSE_ERR_NOT_SUPPORTED);
}

/*
 * 用例描述:VM 模式下 AddAccessPermission 返回 UBSE_ERR_NOT_SUPPORTED
 * 测试步骤:
 * 1、调用 AddAccessPermission
 * 预期结果:
 * 1、返回 UBSE_ERR_NOT_SUPPORTED
 */
TEST_F(TestUbseSsuServiceImpVm, AddAccessPermission_VmMode_ReturnsNotSupported)
{
    auto ret = service_.AddAccessPermission("vm_perm",
                                            "nqn.2024-01.org.nvmexpress:uuid:11111111-1111-1111-1111-111111111111",
                                            MakeIdentity());
    EXPECT_EQ(ret, UBSE_ERR_NOT_SUPPORTED);
}

/*
 * 用例描述:VM 模式下 RemoveAccessPermission 返回 UBSE_ERR_NOT_SUPPORTED
 * 测试步骤:
 * 1、调用 RemoveAccessPermission
 * 预期结果:
 * 1、返回 UBSE_ERR_NOT_SUPPORTED
 */
TEST_F(TestUbseSsuServiceImpVm, RemoveAccessPermission_VmMode_ReturnsNotSupported)
{
    auto ret = service_.RemoveAccessPermission("vm_perm",
                                               "nqn.2024-01.org.nvmexpress:uuid:11111111-1111-1111-1111-111111111111",
                                               MakeIdentity());
    EXPECT_EQ(ret, UBSE_ERR_NOT_SUPPORTED);
}

/*
 * 用例描述:VM 模式下 GetFeDeviceList 返回 UBSE_ERR_NOT_SUPPORTED
 * 测试步骤:
 * 1、调用 GetFeDeviceList
 * 预期结果:
 * 1、返回 UBSE_ERR_NOT_SUPPORTED
 */
TEST_F(TestUbseSsuServiceImpVm, GetFeDeviceList_VmMode_ReturnsNotSupported)
{
    std::vector<UbseSsuFe> feList;
    auto ret = service_.GetFeDeviceList(feList);
    EXPECT_EQ(ret, UBSE_ERR_NOT_SUPPORTED);
}

/*
 * 用例描述:VM 模式下 FeDeviceAlloc 返回 UBSE_ERR_NOT_SUPPORTED
 * 测试步骤:
 * 1、调用 FeDeviceAlloc
 * 预期结果:
 * 1、返回 UBSE_ERR_NOT_SUPPORTED
 */
TEST_F(TestUbseSsuServiceImpVm, FeDeviceAlloc_VmMode_ReturnsNotSupported)
{
    UbseSsuVfe vfe{};
    std::string guid;
    auto ret = service_.FeDeviceAlloc(1, vfe, guid);
    EXPECT_EQ(ret, UBSE_ERR_NOT_SUPPORTED);
}

/*
 * 用例描述:VM 模式下 GetNsStats 走本地适配器查询 namespace 统计,以适配器结果填充 statsList
 * 测试步骤:
 * 1、构造 namespace(uid/userName 与 MakeIdentity() 默认值匹配),设置 nuse
 * 2、注入 acquireDevInfo_ mock(ControllableAcquireDevInfo)在 RefreshDevCache 时返回该 namespace
 * 3、向 vmCache_ 推送分配信息(nsUuid 与 ns.uuid 经 StrToUuid 转换后一致)
 * 4、调用 GetNsStats
 * 预期结果:
 * 1、返回 UBSE_OK
 * 2、stats[0].usedSize 等于 namespace 的 nuse
 * 3、stats[0].totalSize 等于 NameSpaceInfo.nsSize
 */
TEST_F(TestUbseSsuServiceImpVm, GetNsStats_VmMode_ReturnsAdapterResult)
{
    const std::string eid = std::string(16, 'A');
    auto ns1 = MakeNsForCache(eid, "nqn.target.vm_stats", 1, 4096);
    ns1.nuse = 1024; // 已用 1024 LBAs

    // VM 内无 collector 缓存,通过 acquireDevInfo_ mock 在 RefreshDevCache 时返回 namespace
    g_acquireDevInfoNsList.push_back(ns1);
    UbseSsuAdapterImpl::GetInstance().acquireDevInfo_ = ControllableAcquireDevInfo;
    // GetDevList 内部调 GetAdminNqn → UbseGetStr,需 mock 配置(与 Perm 夹具同一约定)
    MOCKER_CPP(&ubse::config::UbseGetStr)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), mockcpp::any())
        .will(invoke(MockUbseGetStr));

    // 构造分配信息推送缓存,nsUuid 与 ns1.uuid 经 StrToUuid 转换后一致(与 master 路径同一约定)
    UbseSsuAllocResult allocResult{};
    allocResult.name = "vm_stats";
    allocResult.strategy = UbseSsuAllocStrategy::NORMAL;
    allocResult.nameSpaceList.push_back(MakeNameSpaceInfo(ns1, 4096 * 512));
    vmCache_.UpdateAllocInfo(allocResult);

    std::vector<UbseSsuNsStats> stats;
    auto ret = service_.GetNsStats("vm_stats", stats, MakeIdentity());

    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(stats.size(), 1u);
    // MakeNsForCache 默认 flbas=0(512B LBA):1024 LBA × 512B = 524288 字节
    EXPECT_EQ(stats[0].usedSize, 1024u * 512u);
    EXPECT_EQ(stats[0].totalSize, 4096u * 512);
}

/*
 * 用例描述:VM 模式下 GetNsStats 查询不在缓存中的 name 返回 UBSE_SSU_ERROR_SPACE_NOT_FOUND
 * 测试步骤:
 * 1、不向 vmCache_ 推送任何数据
 * 2、调用 GetNsStats
 * 预期结果:
 * 1、返回 UBSE_SSU_ERROR_SPACE_NOT_FOUND(与 GetAllocInfoByName VM 分支一致)
 */
TEST_F(TestUbseSsuServiceImpVm, GetNsStats_VmMode_NotInCache_ReturnsNotFound)
{
    std::vector<UbseSsuNsStats> stats;
    auto ret = service_.GetNsStats("not_in_cache", stats, MakeIdentity());
    EXPECT_EQ(ret, UBSE_SSU_ERROR_SPACE_NOT_FOUND);
}

// ============================================================================
// F. 聚合 attach 接口(AttachStripedSpace/AttachLinearSpace VM 分支)
// ============================================================================

// CreateBlockDevice 捕获状态:记录调用次数与最近一次 options(验证参数透传)
static std::atomic<int> g_createBlockDeviceCallCount{0};
static UbseCreateBlockDeviceOptions g_lastCreateBlockDeviceOptions{};

// CreateBlockDevice 捕获 mock:记录透传的 options,回填 devPath 并返回成功
static uint32_t MockCreateBlockDeviceCaptureOptions(UbseSsuAdapterImpl *_mockClass, const std::string &deviceName,
                                                    const std::vector<std::string> &devicePathList,
                                                    const UbseCreateBlockDeviceOptions &options, std::string &devPath)
{
    (void)_mockClass;
    (void)devicePathList;
    g_lastCreateBlockDeviceOptions = options;
    devPath = "/dev/ssu/" + deviceName;
    g_createBlockDeviceCallCount.fetch_add(1);
    return UBSE_OK;
}

static void SetupCreateBlockDeviceCaptureMock()
{
    g_createBlockDeviceCallCount.store(0);
    g_lastCreateBlockDeviceOptions = UbseCreateBlockDeviceOptions{};
    MOCKER_CPP_VIRTUAL(UbseSsuAdapterImpl::GetInstance(), &UbseSsuAdapterImpl::CreateBlockDevice)
        .stubs()
        .will(invoke(MockCreateBlockDeviceCaptureOptions));
}

static UbseSsuStripedSpaceReq MakeStripedVmReq(const std::string &name, UbseSsuAggregationRaidLevel level,
                                               UbseSsuChunkSize chunkSize)
{
    UbseSsuStripedSpaceReq req{};
    req.name = name;
    req.devName = "md_striped_vm";
    req.level = level;
    req.chunkSize = chunkSize;
    req.nqn = "nqn.2024-01.org.nvmexpress:uuid:11111111-1111-1111-1111-111111111111";
    // attach/detach 身份校验需要 identity 与适配器 customData 匹配
    req.identity = TestUbseSsuServiceImpVm::MakeIdentity();
    return req;
}

static UbseSsuLinearSpaceReq MakeLinearVmReq(const std::string &name)
{
    UbseSsuLinearSpaceReq req{};
    req.name = name;
    req.devName = "md_linear_vm";
    req.nqn = "nqn.2024-01.org.nvmexpress:uuid:11111111-1111-1111-1111-111111111111";
    // attach/detach 身份校验需要 identity 与适配器 customData 匹配
    req.identity = TestUbseSsuServiceImpVm::MakeIdentity();
    return req;
}

/*
 * 用例描述:VM 模式下 AttachStripedSpace 将 level/chunkSize 完整透传给 adapter CreateBlockDevice
 * 测试步骤:
 * 1、向缓存推送一条分配信息,安装 options 捕获 mock
 * 2、以 RAID5 + CHUNK_SIZE_128K(非默认值)调用 AttachStripedSpace
 * 预期结果:
 * 1、返回 UBSE_OK,devPath 为 adapter 回填值
 * 2、CreateBlockDevice 恰好调用一次,且 options 为 STRIPED + RAID5 + 128KB
 *    (回归:修复前 level/chunkSize 被对象切片丢失,固定使用默认 RAID0/512KB)
 */
TEST_F(TestUbseSsuServiceImpVm, AttachStripedSpace_VmMode_Success_PassesOptions)
{
    vmCache_.UpdateAllocInfo(
        MakeAdapterBackedAllocResult("vm_striped_opts", 1, UbseSsuAllocStrategy::STRIPED));
    SetupCreateBlockDeviceCaptureMock();

    auto req = MakeStripedVmReq("vm_striped_opts", UbseSsuAggregationRaidLevel::RAID5,
                                UbseSsuChunkSize::CHUNK_SIZE_128K);
    std::vector<std::string> nsDevPaths;
    std::string devPath;
    auto ret = service_.AttachStripedSpace(req, nsDevPaths, devPath);

    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(nsDevPaths.size(), 1u);
    EXPECT_EQ(nsDevPaths[0], "/dev/nvme0n1");
    EXPECT_EQ(devPath, "/dev/ssu/md_striped_vm");
    EXPECT_TRUE(vmCache_.IsAttached("vm_striped_opts"));

    EXPECT_EQ(g_createBlockDeviceCallCount.load(), 1);
    EXPECT_EQ(g_lastCreateBlockDeviceOptions.addressingType, UbseSsuAddressingType::STRIPED);
    EXPECT_EQ(g_lastCreateBlockDeviceOptions.raidLevel, UbseSsuRaidLevel::RAID5);
    EXPECT_EQ(g_lastCreateBlockDeviceOptions.chunkSize,
              static_cast<uint32_t>(UbseSsuChunkSize::CHUNK_SIZE_128K));
}

/*
 * 用例描述:VM 模式下 RAID0 请求同样透传 raidLevel 与 chunkSize
 * 测试步骤:
 * 1、向缓存推送一条分配信息,安装 options 捕获 mock
 * 2、以 RAID0 + CHUNK_SIZE_16K 调用 AttachStripedSpace
 * 预期结果:
 * 1、返回 UBSE_OK
 * 2、options 为 STRIPED + RAID0 + 16KB
 */
TEST_F(TestUbseSsuServiceImpVm, AttachStripedSpace_VmMode_RAID0_PassesChunkSize)
{
    vmCache_.UpdateAllocInfo(
        MakeAdapterBackedAllocResult("vm_striped_raid0", 1, UbseSsuAllocStrategy::STRIPED));
    SetupCreateBlockDeviceCaptureMock();

    auto req = MakeStripedVmReq("vm_striped_raid0", UbseSsuAggregationRaidLevel::RAID0,
                                UbseSsuChunkSize::CHUNK_SIZE_16K);
    std::vector<std::string> nsDevPaths;
    std::string devPath;
    auto ret = service_.AttachStripedSpace(req, nsDevPaths, devPath);

    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(g_createBlockDeviceCallCount.load(), 1);
    EXPECT_EQ(g_lastCreateBlockDeviceOptions.addressingType, UbseSsuAddressingType::STRIPED);
    EXPECT_EQ(g_lastCreateBlockDeviceOptions.raidLevel, UbseSsuRaidLevel::RAID0);
    EXPECT_EQ(g_lastCreateBlockDeviceOptions.chunkSize,
              static_cast<uint32_t>(UbseSsuChunkSize::CHUNK_SIZE_16K));
}

/*
 * 用例描述:VM 模式下 AttachLinearSpace 成功,透传 LINEAR options 并返回聚合设备路径
 * 测试步骤:
 * 1、向缓存推送一条分配信息,安装 options 捕获 mock
 * 2、调用 AttachLinearSpace
 * 预期结果:
 * 1、返回 UBSE_OK,nsDevPaths 为缓存中的设备路径,devPath 为 adapter 回填值
 * 2、CreateBlockDevice 恰好调用一次,且 options.addressingType 为 LINEAR
 * 3、vmCache_.IsAttached 返回 true
 */
TEST_F(TestUbseSsuServiceImpVm, AttachLinearSpace_VmMode_Success_PassesOptions)
{
    vmCache_.UpdateAllocInfo(MakeAdapterBackedAllocResult("vm_linear_ok", 1, UbseSsuAllocStrategy::LINEAR));
    SetupCreateBlockDeviceCaptureMock();

    auto req = MakeLinearVmReq("vm_linear_ok");
    std::vector<std::string> nsDevPaths;
    std::string devPath;
    auto ret = service_.AttachLinearSpace(req, nsDevPaths, devPath);

    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(nsDevPaths.size(), 1u);
    EXPECT_EQ(nsDevPaths[0], "/dev/nvme0n1");
    EXPECT_EQ(devPath, "/dev/ssu/md_linear_vm");
    EXPECT_TRUE(vmCache_.IsAttached("vm_linear_ok"));

    EXPECT_EQ(g_createBlockDeviceCallCount.load(), 1);
    EXPECT_EQ(g_lastCreateBlockDeviceOptions.addressingType, UbseSsuAddressingType::LINEAR);
}

/*
 * 用例描述:VM 模式下重复聚合 attach 幂等返回,不重复调用 CreateBlockDevice
 * 测试步骤:
 * 1、向缓存推送一条分配信息,安装 options 捕获 mock
 * 2、第一次 attach 成功(CreateBlockDevice 调用 1 次)
 * 3、第二次以相同 name 再次 attach
 * 预期结果:
 * 1、两次都返回 UBSE_OK(幂等语义扩展到聚合设备)
 * 2、CreateBlockDevice 调用次数仍为 1(回归:修复前幂等返回后仍重复创建设备)
 * 3、第二次返回已挂载的 nsDevPath 列表与聚合设备路径 /dev/ssu/{devName}
 */
TEST_F(TestUbseSsuServiceImpVm, AttachStripedSpace_VmMode_AlreadyAttached_Idempotent_NoRecreate)
{
    vmCache_.UpdateAllocInfo(
        MakeAdapterBackedAllocResult("vm_striped_idem", 1, UbseSsuAllocStrategy::STRIPED));
    SetupCreateBlockDeviceCaptureMock();

    auto req = MakeStripedVmReq("vm_striped_idem", UbseSsuAggregationRaidLevel::RAID5,
                                UbseSsuChunkSize::CHUNK_SIZE_128K);

    std::vector<std::string> nsDevPaths1;
    std::string devPath1;
    auto ret1 = service_.AttachStripedSpace(req, nsDevPaths1, devPath1);
    EXPECT_EQ(ret1, UBSE_OK);
    ASSERT_EQ(g_createBlockDeviceCallCount.load(), 1);

    std::vector<std::string> nsDevPaths2;
    std::string devPath2;
    auto ret2 = service_.AttachStripedSpace(req, nsDevPaths2, devPath2);

    EXPECT_EQ(ret2, UBSE_OK);
    EXPECT_EQ(g_createBlockDeviceCallCount.load(), 1);
    ASSERT_EQ(nsDevPaths2.size(), 1u);
    EXPECT_EQ(nsDevPaths2[0], "/dev/nvme0n1");
    EXPECT_EQ(devPath2, "/dev/ssu/md_striped_vm");
}

// ============================================================================
// D2. 失败路径回滚(回归:attach 中途失败需逆序回滚;聚合设备删除失败不得继续 detach)
// ============================================================================

/*
 * 用例描述:VM 模式下多 namespace attach 中途失败时,逆序回滚已 attach 的 namespace
 * 测试步骤:
 * 1、向缓存推送一条分配信息(含 3 个 namespace)
 * 2、设置 g_attachFailAfter=2,使第 3 个 namespace attach 失败
 * 3、调用 AttachSpace
 * 预期结果:
 * 1、返回 UBSE_SSU_ERROR_ATTACH_FAILED
 * 2、前 2 个已 attach 的 namespace 被回滚 detach(g_detachCallCount == 2)
 * 3、vmCache_.IsAttached 为 false(未标记整体 attach 成功)
 */
TEST_F(TestUbseSsuServiceImpVm, AttachSpace_VmMode_MidwayFail_RollsBackAttachedNs)
{
    vmCache_.UpdateAllocInfo(MakeAdapterBackedAllocResult("vm_attach_rollback", 3));

    // 第 3 次 attach(索引 2)失败
    g_attachFailAfter.store(2);

    auto req = MakeSpaceReq("vm_attach_rollback", "nqn.2024-01.org.nvmexpress:uuid:11111111-1111-1111-1111-111111111111",
                            MakeIdentity());
    std::vector<std::string> nsDevPaths;
    auto ret = service_.AttachSpace(req, nsDevPaths);

    EXPECT_EQ(ret, UBSE_SSU_ERROR_ATTACH_FAILED);
    EXPECT_EQ(g_attachCallCount.load(), 3);
    // 前 2 个已成功 attach 的 namespace 被逆序回滚 detach
    EXPECT_EQ(g_detachCallCount.load(), 2);
    EXPECT_FALSE(vmCache_.IsAttached("vm_attach_rollback"));
}

/*
 * 用例描述:VM 模式下聚合 detach 时 DeleteBlockDevice 失败,不得继续 detach namespace,整体返回失败
 * 测试步骤:
 * 1、向缓存推送 Linear 分配信息并通过 AttachLinearSpace 标记 attach 成功
 * 2、mock DeleteBlockDevice 返回失败
 * 3、调用 DetachLinearSpace
 * 预期结果:
 * 1、返回 UBSE_SSU_ERROR_BLOCK_DEVICE_DELETE_FAILED(回归:修复前忽略删除失败并返回成功)
 * 2、未调用 detach namespace(g_detachCallCount == 0,避免破坏聚合设备依赖)
 * 3、vmCache_.IsAttached 仍为 true(整体 detach 未完成)
 */
TEST_F(TestUbseSsuServiceImpVm, DetachLinearSpace_VmMode_DeleteBlockDeviceFail_NoNsDetach)
{
    vmCache_.UpdateAllocInfo(
        MakeAdapterBackedAllocResult("vm_detach_del_fail", 1, UbseSsuAllocStrategy::LINEAR));
    SetupCreateBlockDeviceCaptureMock();
    auto attachReq = MakeLinearVmReq("vm_detach_del_fail");
    std::vector<std::string> nsDevPaths;
    std::string devPath;
    ASSERT_EQ(service_.AttachLinearSpace(attachReq, nsDevPaths, devPath), UBSE_OK);

    MOCKER_CPP_VIRTUAL(UbseSsuAdapterImpl::GetInstance(), &UbseSsuAdapterImpl::DeleteBlockDevice)
        .stubs()
        .will(returnValue(UBSE_SSU_ERROR_BLOCK_DEVICE_DELETE_FAILED));

    UbseSsuLinearSpaceReq req{};
    req.name = "vm_detach_del_fail";
    req.devName = "md_vm_del_fail";
    req.nqn = "nqn.2024-01.org.nvmexpress:uuid:11111111-1111-1111-1111-111111111111";
    auto ret = service_.DetachLinearSpace(req);

    EXPECT_EQ(ret, UBSE_SSU_ERROR_BLOCK_DEVICE_DELETE_FAILED);
    // 聚合设备仍依赖各 namespace,删除失败时不得继续 detach
    EXPECT_EQ(g_detachCallCount.load(), 0);
    EXPECT_TRUE(vmCache_.IsAttached("vm_detach_del_fail"));
}

/*
 * 用例描述:VM 模式下 DetachLinearSpace 成功:删除聚合设备后 detach 所有 namespace
 * 测试步骤:
 * 1、向缓存推送 Linear 分配信息并通过 AttachLinearSpace 标记 attach 成功
 * 2、mock DeleteBlockDevice 返回成功
 * 3、调用 DetachLinearSpace
 * 预期结果:
 * 1、返回 UBSE_OK
 * 2、每个 namespace 均被 detach(g_detachCallCount == 1)
 * 3、vmCache_.IsAttached 返回 false
 */
TEST_F(TestUbseSsuServiceImpVm, DetachLinearSpace_VmMode_Success)
{
    vmCache_.UpdateAllocInfo(
        MakeAdapterBackedAllocResult("vm_detach_linear_ok", 1, UbseSsuAllocStrategy::LINEAR));
    SetupCreateBlockDeviceCaptureMock();
    auto attachReq = MakeLinearVmReq("vm_detach_linear_ok");
    std::vector<std::string> nsDevPaths;
    std::string devPath;
    ASSERT_EQ(service_.AttachLinearSpace(attachReq, nsDevPaths, devPath), UBSE_OK);

    MOCKER_CPP_VIRTUAL(UbseSsuAdapterImpl::GetInstance(), &UbseSsuAdapterImpl::DeleteBlockDevice)
        .stubs()
        .will(returnValue(UBSE_OK));

    auto detachReq = MakeLinearVmReq("vm_detach_linear_ok");
    auto ret = service_.DetachLinearSpace(detachReq);

    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(g_detachCallCount.load(), 1);
    EXPECT_FALSE(vmCache_.IsAttached("vm_detach_linear_ok"));
}

/*
 * 用例描述:VM 模式下 DetachStripedSpace 成功:删除聚合设备后 detach 所有 namespace
 * 测试步骤:
 * 1、向缓存推送分配信息并通过 AttachStripedSpace 标记 attach 成功
 * 2、mock DeleteBlockDevice 返回成功
 * 3、调用 DetachStripedSpace
 * 预期结果:
 * 1、返回 UBSE_OK
 * 2、每个 namespace 均被 detach(g_detachCallCount == 1)
 * 3、vmCache_.IsAttached 返回 false
 */
TEST_F(TestUbseSsuServiceImpVm, DetachStripedSpace_VmMode_Success)
{
    vmCache_.UpdateAllocInfo(
        MakeAdapterBackedAllocResult("vm_detach_striped_ok", 1, UbseSsuAllocStrategy::STRIPED));
    SetupCreateBlockDeviceCaptureMock();
    auto attachReq = MakeStripedVmReq("vm_detach_striped_ok", UbseSsuAggregationRaidLevel::RAID0,
                                      UbseSsuChunkSize::CHUNK_SIZE_128K);
    std::vector<std::string> nsDevPaths;
    std::string devPath;
    ASSERT_EQ(service_.AttachStripedSpace(attachReq, nsDevPaths, devPath), UBSE_OK);

    MOCKER_CPP_VIRTUAL(UbseSsuAdapterImpl::GetInstance(), &UbseSsuAdapterImpl::DeleteBlockDevice)
        .stubs()
        .will(returnValue(UBSE_OK));

    auto detachReq = MakeStripedVmReq("vm_detach_striped_ok", UbseSsuAggregationRaidLevel::RAID0,
                                      UbseSsuChunkSize::CHUNK_SIZE_128K);
    auto ret = service_.DetachStripedSpace(detachReq);

    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(g_detachCallCount.load(), 1);
    EXPECT_FALSE(vmCache_.IsAttached("vm_detach_striped_ok"));
}

// ============================================================================
// E. host 模式行为不变
// ============================================================================

/*
 * 用例描述:切换回 host 模式后 ListAllocInfo 不走 VM 缓存分支
 * 测试步骤:
 * 1、TearDown 恢复 host 模式
 * 2、SetUp 重新进入 host 模式(本用例不调用 SetVmMode(true))
 * 3、调用 ListAllocInfo
 * 预期结果:
 * 1、返回 UBSE_OK 且 result 为空(账本为空,vmCache_ 数据不泄漏到 host 分支)
 */
TEST_F(TestUbseSsuServiceImpVm, ListAllocInfo_HostMode_NotAffected)
{
    // 本用例夹具 SetUp 默认进入 VM 模式,这里手动切回 host 模式验证
    service_.SetVmCache(nullptr);
    service_.SetVmMode(false);

    // host 模式下 vmCache_ 不被使用;vmCache_ 中的数据不应影响 host 模式查询
    vmCache_.UpdateAllocInfo(MakeVmAllocResult("should_not_appear_in_host_mode", 1));

    std::vector<UbseSsuAllocResult> result;
    auto ret = service_.ListAllocInfo(result, MakeIdentity());
    // host 模式 master 路径(夹具默认 MockGetRole_Master):账本为空时
    // VerifyGetInfoIdentity 返回 UBSE_OK 且 verifiedNames 为空 → 确定性返回空列表
    EXPECT_EQ(ret, UBSE_OK);
    // 关键断言:host 模式不返回 vmCache_ 中推送的 "should_not_appear_in_host_mode"
    EXPECT_TRUE(result.empty());
}

} // namespace ubse::ssu::service::ut
