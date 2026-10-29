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

#include "test_ubse_ssu_debt_ledger.h"
#include <securec.h>
#include <algorithm>
#include <thread>
#include <vector>

namespace ubse::ssu::debt::ut {

using namespace ubse::adapter_plugins::ssu::def;

namespace {
// 测试涉及的 name 全集，SetUp/TearDown 统一按 name 维度清理（复合键下同 name 可能多条）
const std::vector<std::string> g_testEntryNames = {
    "test-entry", "test-entry-2", "test-entry-3", "rebuild-entry", "rebuild-entry-2",
};

constexpr const char *TEST_UUID_A = "uuid-aaaa";
constexpr const char *TEST_UUID_B = "uuid-bbbb";
} // namespace

void TestUbseSsuDebtLedger::SetUp()
{
    Test::SetUp();
    // 每个用例开始前清空账本，确保隔离
    for (const auto &name : g_testEntryNames) {
        UbseSsuDebtLedger::GetInstance().RemoveByName(name);
    }
}

void TestUbseSsuDebtLedger::TearDown()
{
    // 清理所有用例可能遗留的条目
    for (const auto &name : g_testEntryNames) {
        UbseSsuDebtLedger::GetInstance().RemoveByName(name);
    }
    Test::TearDown();
}

UbseSsuDevNameSpace TestUbseSsuDebtLedger::MakeNs(const std::string& name, const std::string& eid, uint32_t nsId,
                                                  uint64_t nsze, const std::string& resourceUuid)
{
    // 值初始化：char数组（含customData.resourceUuid）恒为零字节，空uuid用例
    // （遗留数据形态）不依赖栈布局，避免ReadCustomDataResourceUuid读出垃圾串
    UbseSsuDevNameSpace ns{};
    ns.namespaceId = nsId;
    ns.subSystem.eid = eid;
    ns.subSystem.subNqn = "nqn.test." + eid;
    ns.uuid = std::string(16, '\xAA'); // 16字节原始UUID，Rebuild时经StrToUuid转为标准格式
    ns.nsze = nsze;
    ns.ncap = nsze;
    ns.nsOptions.flbas = 0;
    ns.customData.allocStrategy = 1;
    ns.customData.nsNum = 1;
    ns.customData.totalBytes = 1024;
    ns.customData.uid = 1000;
    errno_t ret;
    ret = strncpy_s(ns.customData.name, sizeof(ns.customData.name), name.c_str(), name.size());
    EXPECT_EQ(ret, EOK);
    ret = strncpy_s(ns.customData.userName, sizeof(ns.customData.userName), "testuser", 8);
    EXPECT_EQ(ret, EOK);
    ret = strncpy_s(ns.customData.defaultNqn, sizeof(ns.customData.defaultNqn), "nqn.default", 10);
    EXPECT_EQ(ret, EOK);
    if (!resourceUuid.empty()) {
        ret = strncpy_s(ns.customData.resourceUuid, sizeof(ns.customData.resourceUuid), resourceUuid.c_str(),
                        resourceUuid.size());
        EXPECT_EQ(ret, EOK);
    }
    return ns;
}

UbseSsuDevInfo TestUbseSsuDebtLedger::MakeDev(const std::string& eid, const std::vector<UbseSsuDevNameSpace>& nsList,
                                              uint64_t totalBytes, uint64_t usedBytes)
{
    UbseSsuDevInfo dev;
    dev.subSystem.eid = eid;
    dev.subSystem.subNqn = "nqn.test." + eid;
    dev.totalBytes = totalBytes;
    dev.usedBytes = usedBytes;
    dev.state = UbseSsuState::ONLINE;
    dev.nameSpaces = nsList;
    return dev;
}

// 创建一个测试条目辅助函数（仅用于 Put/Get/Modify/Remove 等基础操作测试，不设置 nameSpaceList）
static std::shared_ptr<const UbseSsuLedgerEntry> MakeEntry(const std::string& name, UbseSsuNsState state,
                                                           const std::string& resourceUuid = "")
{
    auto entry = std::make_shared<UbseSsuLedgerEntry>();
    entry->name = name;
    entry->state = state;
    entry->allocReq.name = name;
    entry->allocReq.nsSize = 1024;
    entry->allocReq.nsNum = 1;
    entry->allocReq.strategy = UbseSsuAllocStrategy::LINEAR;
    entry->allocReq.lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_512;
    entry->allocResult.name = name;
    entry->allocResult.resourceUuid = resourceUuid;
    entry->allocResult.strategy = UbseSsuAllocStrategy::LINEAR;
    // nameSpaceList 为空 vector，基础操作测试不访问该字段，涉及 nameSpaceList 的测试使用 Rebuild
    return entry;
}

/*
 * 用例描述：Put 正常插入条目（复合键 name + resourceUuid）
 * 测试步骤：
 * 1、创建测试条目，调用 Put 插入账本
 * 预期结果：
 * 1、返回 true
 * 2、按相同复合键 Get 可取回，resourceUuid 一致
 */
TEST_F(TestUbseSsuDebtLedger, PutSuccess)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    auto entry = MakeEntry("test-entry", UbseSsuNsState::CREATED, TEST_UUID_A);

    bool ok = ledger.Put("test-entry", TEST_UUID_A, entry);

    EXPECT_TRUE(ok);
    auto got = ledger.Get("test-entry", TEST_UUID_A);
    ASSERT_NE(got, nullptr);
    EXPECT_EQ(got->name, "test-entry");
    EXPECT_EQ(got->allocResult.resourceUuid, TEST_UUID_A);
    EXPECT_EQ(got->state, UbseSsuNsState::CREATED);
}

/*
 * 用例描述：Put 重复复合键时应拒绝，不覆盖已有条目
 * 测试步骤：
 * 1、Put 插入 (name, uuid) 条目
 * 2、再次 Put 相同 (name, uuid) 的条目
 * 预期结果：
 * 1、第一次返回 true
 * 2、第二次返回 false
 * 3、条目内容为第一次插入的值
 */
TEST_F(TestUbseSsuDebtLedger, PutDuplicateReject)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    auto entry1 = MakeEntry("test-entry", UbseSsuNsState::CREATED, TEST_UUID_A);
    auto entry2 = MakeEntry("test-entry", UbseSsuNsState::ATTACHED, TEST_UUID_A);

    bool first = ledger.Put("test-entry", TEST_UUID_A, entry1);
    bool second = ledger.Put("test-entry", TEST_UUID_A, entry2);

    EXPECT_TRUE(first);
    EXPECT_FALSE(second);
    auto got = ledger.Get("test-entry", TEST_UUID_A);
    ASSERT_NE(got, nullptr);
    EXPECT_EQ(got->state, UbseSsuNsState::CREATED);
}

/*
 * 用例描述：同 name 不同 resourceUuid 应各自成条目（脑裂合并场景的核心诉求）
 * 测试步骤：
 * 1、Put (name, uuidA) 与 (name, uuidB) 两条目
 * 2、分别按复合键 Get
 * 预期结果：
 * 1、两次 Put 均返回 true
 * 2、各自取回对应条目，互不串扰
 */
TEST_F(TestUbseSsuDebtLedger, PutSameNameDifferentUuidBothSucceed)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    auto entryA = MakeEntry("test-entry", UbseSsuNsState::CREATED, TEST_UUID_A);
    auto entryB = MakeEntry("test-entry", UbseSsuNsState::CREATED, TEST_UUID_B);

    EXPECT_TRUE(ledger.Put("test-entry", TEST_UUID_A, entryA));
    EXPECT_TRUE(ledger.Put("test-entry", TEST_UUID_B, entryB));

    auto gotA = ledger.Get("test-entry", TEST_UUID_A);
    ASSERT_NE(gotA, nullptr);
    EXPECT_EQ(gotA->allocResult.resourceUuid, TEST_UUID_A);
    auto gotB = ledger.Get("test-entry", TEST_UUID_B);
    ASSERT_NE(gotB, nullptr);
    EXPECT_EQ(gotB->allocResult.resourceUuid, TEST_UUID_B);
}

/*
 * 用例描述：Put 空条目应拒绝
 * 测试步骤：
 * 1、调用 Put 传入空指针
 * 预期结果：
 * 1、返回 false
 */
TEST_F(TestUbseSsuDebtLedger, PutNullEntry)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    bool ok = ledger.Put("test-entry", TEST_UUID_A, nullptr);
    EXPECT_FALSE(ok);
}

/*
 * 用例描述：Get 已存在的条目应返回正确条目
 * 测试步骤：
 * 1、Put 条目
 * 2、Get 同一复合键
 * 预期结果：
 * 1、返回的条目非空
 * 2、属性与 Put 时一致
 */
TEST_F(TestUbseSsuDebtLedger, GetExisting)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    auto entry = MakeEntry("test-entry", UbseSsuNsState::CREATED, TEST_UUID_A);
    ledger.Put("test-entry", TEST_UUID_A, entry);

    auto got = ledger.Get("test-entry", TEST_UUID_A);

    ASSERT_NE(got, nullptr);
    EXPECT_EQ(got->name, "test-entry");
    EXPECT_EQ(got->state, UbseSsuNsState::CREATED);
}

/*
 * 用例描述：Get 不存在的复合键应返回 nullptr
 * 测试步骤：
 * 1、Get 一个从未 Put 的复合键
 * 2、Get 同 name 但错误 resourceUuid 的复合键
 * 预期结果：
 * 1、均返回 nullptr
 */
TEST_F(TestUbseSsuDebtLedger, GetNotExist)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    ledger.Put("test-entry", TEST_UUID_A, MakeEntry("test-entry", UbseSsuNsState::CREATED, TEST_UUID_A));

    EXPECT_EQ(ledger.Get("non-existent", TEST_UUID_A), nullptr);
    // 同 name 但 uuid 不匹配时不得误命中
    EXPECT_EQ(ledger.Get("test-entry", TEST_UUID_B), nullptr);
}

/*
 * 用例描述：GetByName 应返回该 name 下全部条目（含不同 resourceUuid）
 * 测试步骤：
 * 1、Put 同 name 两个 uuid 条目与其他 name 条目
 * 2、GetByName 查询
 * 预期结果：
 * 1、仅返回该 name 的两条目，resourceUuid 覆盖 uuidA/uuidB
 */
TEST_F(TestUbseSsuDebtLedger, GetByNameReturnsAllUuidEntries)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    ledger.Put("test-entry", TEST_UUID_A, MakeEntry("test-entry", UbseSsuNsState::CREATED, TEST_UUID_A));
    ledger.Put("test-entry", TEST_UUID_B, MakeEntry("test-entry", UbseSsuNsState::CREATED, TEST_UUID_B));
    ledger.Put("test-entry-2", TEST_UUID_A, MakeEntry("test-entry-2", UbseSsuNsState::CREATED, TEST_UUID_A));

    auto entries = ledger.GetByName("test-entry");

    ASSERT_EQ(entries.size(), 2u);
    std::vector<std::string> uuids;
    for (const auto& e : entries) {
        ASSERT_NE(e, nullptr);
        EXPECT_EQ(e->name, "test-entry");
        uuids.push_back(e->allocResult.resourceUuid);
    }
    EXPECT_NE(std::find(uuids.begin(), uuids.end(), TEST_UUID_A), uuids.end());
    EXPECT_NE(std::find(uuids.begin(), uuids.end(), TEST_UUID_B), uuids.end());
}

/*
 * 用例描述：GetByName 对不存在 name 应返回空列表
 * 测试步骤：
 * 1、GetByName 一个从未 Put 的 name
 * 预期结果：
 * 1、返回空 vector
 */
TEST_F(TestUbseSsuDebtLedger, GetByNameNotExistReturnsEmpty)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    EXPECT_TRUE(ledger.GetByName("non-existent").empty());
}

/*
 * 用例描述：Modify 仅修改指定复合键条目
 * 测试步骤：
 * 1、Put 同 name 两个 uuid 条目
 * 2、Modify (name, uuidA) 的 state
 * 预期结果：
 * 1、Modify 返回 true
 * 2、uuidA 条目 state 变更，uuidB 条目不变
 */
TEST_F(TestUbseSsuDebtLedger, ModifyOnlyAffectsSpecifiedUuid)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    ledger.Put("test-entry", TEST_UUID_A, MakeEntry("test-entry", UbseSsuNsState::CREATED, TEST_UUID_A));
    ledger.Put("test-entry", TEST_UUID_B, MakeEntry("test-entry", UbseSsuNsState::CREATED, TEST_UUID_B));

    bool ok = ledger.Modify("test-entry", TEST_UUID_A, [](UbseSsuLedgerEntry& e) { e.state = UbseSsuNsState::ATTACHED; });

    EXPECT_TRUE(ok);
    auto gotA = ledger.Get("test-entry", TEST_UUID_A);
    ASSERT_NE(gotA, nullptr);
    EXPECT_EQ(gotA->state, UbseSsuNsState::ATTACHED);
    auto gotB = ledger.Get("test-entry", TEST_UUID_B);
    ASSERT_NE(gotB, nullptr);
    EXPECT_EQ(gotB->state, UbseSsuNsState::CREATED);
}

/*
 * 用例描述：Modify 不存在的复合键应返回 false
 * 测试步骤：
 * 1、Modify 一个从未 Put 的复合键
 * 预期结果：
 * 1、返回 false
 */
TEST_F(TestUbseSsuDebtLedger, ModifyNotExist)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    bool ok = ledger.Modify("non-existent", TEST_UUID_A,
                            [](UbseSsuLedgerEntry& e) { e.state = UbseSsuNsState::ATTACHED; });
    EXPECT_FALSE(ok);
}

/*
 * 用例描述：Remove 仅删除指定复合键条目
 * 测试步骤：
 * 1、Put 同 name 两个 uuid 条目
 * 2、Remove (name, uuidA)
 * 预期结果：
 * 1、Remove 返回 true
 * 2、uuidA 条目删除，uuidB 条目保留
 */
TEST_F(TestUbseSsuDebtLedger, RemoveOnlyDeletesSpecifiedUuid)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    ledger.Put("test-entry", TEST_UUID_A, MakeEntry("test-entry", UbseSsuNsState::CREATED, TEST_UUID_A));
    ledger.Put("test-entry", TEST_UUID_B, MakeEntry("test-entry", UbseSsuNsState::CREATED, TEST_UUID_B));

    bool removed = ledger.Remove("test-entry", TEST_UUID_A);

    EXPECT_TRUE(removed);
    EXPECT_EQ(ledger.Get("test-entry", TEST_UUID_A), nullptr);
    EXPECT_NE(ledger.Get("test-entry", TEST_UUID_B), nullptr);
}

/*
 * 用例描述：Remove 不存在的复合键应返回 false
 * 测试步骤：
 * 1、Remove 一个从未 Put 的复合键
 * 预期结果：
 * 1、返回 false
 */
TEST_F(TestUbseSsuDebtLedger, RemoveNotExist)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    bool removed = ledger.Remove("non-existent", TEST_UUID_A);
    EXPECT_FALSE(removed);
}

/*
 * 用例描述：RemoveByName 应删除该 name 下全部复合键条目
 * 测试步骤：
 * 1、Put 同 name 两个 uuid 条目与其他 name 条目
 * 2、RemoveByName 该 name
 * 预期结果：
 * 1、RemoveByName 返回 true
 * 2、该 name 下条目全部删除，其他 name 条目保留
 */
TEST_F(TestUbseSsuDebtLedger, RemoveByNameDeletesAllUuidEntries)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    ledger.Put("test-entry", TEST_UUID_A, MakeEntry("test-entry", UbseSsuNsState::CREATED, TEST_UUID_A));
    ledger.Put("test-entry", TEST_UUID_B, MakeEntry("test-entry", UbseSsuNsState::CREATED, TEST_UUID_B));
    ledger.Put("test-entry-2", TEST_UUID_A, MakeEntry("test-entry-2", UbseSsuNsState::CREATED, TEST_UUID_A));

    EXPECT_TRUE(ledger.RemoveByName("test-entry"));

    EXPECT_TRUE(ledger.GetByName("test-entry").empty());
    EXPECT_NE(ledger.Get("test-entry-2", TEST_UUID_A), nullptr);
}

/*
 * 用例描述：GetAll 在空账本时应返回空列表
 * 测试步骤：
 * 1、清空账本后调用 GetAll
 * 预期结果：
 * 1、返回空 vector
 */
TEST_F(TestUbseSsuDebtLedger, GetAllEmpty)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    auto all = ledger.GetAll();
    EXPECT_TRUE(all.empty());
}

/*
 * 用例描述：GetAll 在有多条目时应返回全部条目
 * 测试步骤：
 * 1、Put 3 个条目
 * 2、调用 GetAll
 * 预期结果：
 * 1、返回 vector size 为 3
 * 2、包含所有预期的 name
 */
TEST_F(TestUbseSsuDebtLedger, GetAllMultiple)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    ledger.Put("test-entry", TEST_UUID_A, MakeEntry("test-entry", UbseSsuNsState::CREATED, TEST_UUID_A));
    ledger.Put("test-entry-2", TEST_UUID_A, MakeEntry("test-entry-2", UbseSsuNsState::ATTACHED, TEST_UUID_A));
    ledger.Put("test-entry-3", TEST_UUID_A, MakeEntry("test-entry-3", UbseSsuNsState::CREATING, TEST_UUID_A));

    auto all = ledger.GetAll();

    ASSERT_EQ(all.size(), 3u);
    // 验证包含所有 name
    std::vector<std::string> names;
    for (const auto& e : all) {
        names.push_back(e->name);
    }
    EXPECT_NE(std::find(names.begin(), names.end(), "test-entry"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "test-entry-2"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "test-entry-3"), names.end());
}

/*
 * 用例描述：Rebuild 根据设备列表按 (customData.name, customData.resourceUuid) 分组恢复账本
 * 测试步骤：
 * 1、构造 2 个设备，各有命名空间且 customData.name 为 "rebuild-entry"、resourceUuid 相同
 * 2、调用 Rebuild
 * 预期结果：
 * 1、账本中存在 "rebuild-entry" 条目
 * 2、state 为 CREATED (namespaceId > 0)
 * 3、allocResult.nameSpaceList 包含 2 个 NS
 * 4、nsNum、totalBytes 等字段正确，resourceUuid 与 customData 一致
 */
TEST_F(TestUbseSsuDebtLedger, RebuildFromDevices)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    auto ns1 = MakeNs("rebuild-entry", "eid-1", 1, 1024, TEST_UUID_A);
    ns1.customData.nsNum = 2;
    ns1.customData.totalBytes = 2048;
    auto ns2 = MakeNs("rebuild-entry", "eid-2", 2, 1024, TEST_UUID_A);
    ns2.customData.nsNum = 2;
    ns2.customData.totalBytes = 2048;
    auto dev1 = MakeDev("eid-1", {ns1});
    auto dev2 = MakeDev("eid-2", {ns2});

    ledger.Rebuild({dev1, dev2});

    auto entry = ledger.Get("rebuild-entry", TEST_UUID_A);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED);
    EXPECT_EQ(entry->allocResult.resourceUuid, TEST_UUID_A);
    ASSERT_EQ(entry->allocResult.nameSpaceList.size(), 2u);
    EXPECT_EQ(entry->allocResult.nameSpaceList[0].tgtEid, "eid-1");
    EXPECT_EQ(entry->allocResult.nameSpaceList[0].namespaceId, 1u);
    EXPECT_EQ(entry->allocResult.nameSpaceList[1].tgtEid, "eid-2");
    EXPECT_EQ(entry->allocResult.nameSpaceList[1].namespaceId, 2u);
    EXPECT_EQ(entry->allocReq.nsNum, 2u);
    EXPECT_EQ(entry->allocReq.nsSize, 2048u);
}

/*
 * 用例描述：Rebuild 多个 name 分组
 * 测试步骤：
 * 1、构造 2 个设备，各有 2 个命名空间，分别属于 "rebuild-entry" 和 "rebuild-entry-2"
 * 2、调用 Rebuild
 * 预期结果：
 * 1、账本中存在 2 个条目
 * 2、每个条目包含正确的 NS 列表
 */
TEST_F(TestUbseSsuDebtLedger, RebuildMultipleEntries)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    auto ns1 = MakeNs("rebuild-entry", "eid-1", 1, 1024, TEST_UUID_A);
    auto ns2 = MakeNs("rebuild-entry-2", "eid-2", 2, 2048, TEST_UUID_A);
    auto dev = MakeDev("eid-1", {ns1, ns2});

    ledger.Rebuild({dev});

    auto entry1 = ledger.Get("rebuild-entry", TEST_UUID_A);
    ASSERT_NE(entry1, nullptr);
    EXPECT_EQ(entry1->allocResult.nameSpaceList.size(), 1u);
    EXPECT_EQ(entry1->allocResult.nameSpaceList[0].namespaceId, 1u);

    auto entry2 = ledger.Get("rebuild-entry-2", TEST_UUID_A);
    ASSERT_NE(entry2, nullptr);
    EXPECT_EQ(entry2->allocResult.nameSpaceList.size(), 1u);
    EXPECT_EQ(entry2->allocResult.nameSpaceList[0].namespaceId, 2u);
}

/*
 * 用例描述：主备回切场景——账本已驻留上一任期陈旧条目（ATTACHED + attachNodes非空）时，
 * Rebuild应以当前硬件清单覆盖刷新（state=CREATED、attachNodes清空），
 * 而非因"条目已存在"跳过（跳过会让对账基于陈旧attachNodes漏发REMOUNT）
 * 测试步骤：
 * 1、Put 一个 ATTACHED、attachNodes 非空的陈旧条目（复合键 uuidA）
 * 2、用相同 name、相同 resourceUuid 的设备列表调用 Rebuild
 * 预期结果：
 * 1、条目被覆盖为重建视图：state=CREATED、attachNodes 为空、restoredFromHardware=true
 * 2、allocResult 以扫描到的 NS 为准
 */
TEST_F(TestUbseSsuDebtLedger, RebuildReplacesStaleEntryFromPreviousTenure)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    UbseSsuLedgerEntry stale;
    stale.name = "rebuild-entry";
    stale.state = UbseSsuNsState::ATTACHED;
    stale.attachNodes.insert("master_node_1");
    stale.allocReq.strategy = UbseSsuAllocStrategy::NORMAL;
    stale.allocResult.resourceUuid = TEST_UUID_A;
    ASSERT_TRUE(ledger.Put("rebuild-entry", std::make_shared<const UbseSsuLedgerEntry>(stale)));

    auto ns1 = MakeNs("rebuild-entry", "eid-1", 1, 1024, TEST_UUID_A);
    ns1.customData.nsNum = 1;
    ns1.customData.totalBytes = 1024;
    ledger.Rebuild({MakeDev("eid-1", {ns1})});

    auto entry = ledger.Get("rebuild-entry", TEST_UUID_A);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED);
    EXPECT_TRUE(entry->attachNodes.empty());
    EXPECT_TRUE(entry->restoredFromHardware);
    ASSERT_EQ(entry->allocResult.nameSpaceList.size(), 1u);
    EXPECT_EQ(entry->allocResult.nameSpaceList[0].namespaceId, 1u);
}

/*
 * 用例描述：Rebuild 同 name 不同 resourceUuid 应各自成条目，NS 不混排
 * 测试步骤：
 * 1、构造 2 个设备的命名空间同 name，resourceUuid 分别为 uuidA/uuidB
 * 2、调用 Rebuild
 * 预期结果：
 * 1、账本中存在 2 个条目
 * 2、各条目 NS 数量为 1，且分属各自 uuid 组，不混排
 */
TEST_F(TestUbseSsuDebtLedger, RebuildSameNameDifferentUuidsSeparateEntries)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    auto ns1 = MakeNs("rebuild-entry", "eid-1", 1, 1024, TEST_UUID_A);
    auto ns2 = MakeNs("rebuild-entry", "eid-2", 2, 1024, TEST_UUID_B);
    auto dev1 = MakeDev("eid-1", {ns1});
    auto dev2 = MakeDev("eid-2", {ns2});

    ledger.Rebuild({dev1, dev2});

    auto entryA = ledger.Get("rebuild-entry", TEST_UUID_A);
    ASSERT_NE(entryA, nullptr);
    ASSERT_EQ(entryA->allocResult.nameSpaceList.size(), 1u);
    EXPECT_EQ(entryA->allocResult.nameSpaceList[0].tgtEid, "eid-1");
    EXPECT_EQ(entryA->allocResult.resourceUuid, TEST_UUID_A);

    auto entryB = ledger.Get("rebuild-entry", TEST_UUID_B);
    ASSERT_NE(entryB, nullptr);
    ASSERT_EQ(entryB->allocResult.nameSpaceList.size(), 1u);
    EXPECT_EQ(entryB->allocResult.nameSpaceList[0].tgtEid, "eid-2");
    EXPECT_EQ(entryB->allocResult.resourceUuid, TEST_UUID_B);
}

/*
 * 用例描述：Rebuild 遗留数据（resourceUuid 为空）同 name 应归并为一条
 * 测试步骤：
 * 1、构造 2 个设备的命名空间同 name，customData.resourceUuid 均为空（旧版本分配）
 * 2、调用 Rebuild
 * 预期结果：
 * 1、账本中仅 1 条条目，resourceUuid 为空串
 * 2、该条目包含全部 2 个 NS
 */
TEST_F(TestUbseSsuDebtLedger, RebuildLegacyEmptyUuidSameNameMerged)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    auto ns1 = MakeNs("rebuild-entry", "eid-1", 1, 1024); // 不设置 resourceUuid，遗留数据
    auto ns2 = MakeNs("rebuild-entry", "eid-2", 2, 1024);
    auto dev1 = MakeDev("eid-1", {ns1});
    auto dev2 = MakeDev("eid-2", {ns2});

    ledger.Rebuild({dev1, dev2});

    auto entry = ledger.Get("rebuild-entry", "");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->allocResult.resourceUuid, "");
    ASSERT_EQ(entry->allocResult.nameSpaceList.size(), 2u);
    EXPECT_TRUE(ledger.GetByName("rebuild-entry").size() == 1u);
}

/*
 * 用例描述：多线程并发 Put/Get/Modify/Remove，不应有数据竞争
 * 测试步骤：
 * 1、4 个线程同时操作 10 个不同的复合键
 * 2、主线程等待所有线程完成
 * 预期结果：
 * 1、所有操作正常完成，无崩溃
 * 2、最终账本状态一致
 */
TEST_F(TestUbseSsuDebtLedger, ConcurrentSafety)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    std::vector<std::thread> threads;
    constexpr int KEY_COUNT = 10;
    constexpr int THREAD_COUNT = 4;

    for (int t = 0; t < THREAD_COUNT; ++t) {
        threads.emplace_back([&ledger, t]() {
            for (int i = 0; i < KEY_COUNT; ++i) {
                std::string key = "concurrent-" + std::to_string(i) + "-" + std::to_string(t);
                auto entry = MakeEntry(key, UbseSsuNsState::CREATED, TEST_UUID_A);
                ledger.Put(key, TEST_UUID_A, entry);
                ledger.Get(key, TEST_UUID_A);
                ledger.Modify(key, TEST_UUID_A, [](UbseSsuLedgerEntry& e) { e.state = UbseSsuNsState::ATTACHED; });
                ledger.Remove(key, TEST_UUID_A);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // 验证所有临时条目已被移除
    auto all = ledger.GetAll();
    EXPECT_TRUE(all.empty());
}

} // namespace ubse::ssu::debt::ut
