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

void TestUbseSsuDebtLedger::SetUp()
{
    Test::SetUp();
    // 每个用例开始前清空账本，确保隔离
    UbseSsuDebtLedger::GetInstance().Remove("test-entry");
    UbseSsuDebtLedger::GetInstance().Remove("test-entry-2");
    UbseSsuDebtLedger::GetInstance().Remove("test-entry-3");
    UbseSsuDebtLedger::GetInstance().Remove("rebuild-entry");
    UbseSsuDebtLedger::GetInstance().Remove("rebuild-entry-2");
}

void TestUbseSsuDebtLedger::TearDown()
{
    // 清理所有用例可能遗留的条目
    UbseSsuDebtLedger::GetInstance().Remove("test-entry");
    UbseSsuDebtLedger::GetInstance().Remove("test-entry-2");
    UbseSsuDebtLedger::GetInstance().Remove("test-entry-3");
    UbseSsuDebtLedger::GetInstance().Remove("rebuild-entry");
    UbseSsuDebtLedger::GetInstance().Remove("rebuild-entry-2");
    Test::TearDown();
}

UbseSsuDevNameSpace TestUbseSsuDebtLedger::MakeNs(const std::string& name, const std::string& eid, uint32_t nsId,
                                                  uint64_t nsze)
{
    UbseSsuDevNameSpace ns;
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
    return ns;
}

UbseSsuDevInfo TestUbseSsuDebtLedger::MakeDev(const std::string& eid, const std::vector<UbseSsuDevNameSpace>& nsList,
                                              const std::vector<UbseSsuDevNameSpaceAttachInfo>& attachList,
                                              uint64_t totalBytes, uint64_t usedBytes)
{
    UbseSsuDevInfo dev;
    dev.subSystem.eid = eid;
    dev.subSystem.subNqn = "nqn.test." + eid;
    dev.totalBytes = totalBytes;
    dev.usedBytes = usedBytes;
    dev.state = UbseSsuState::ONLINE;
    dev.nameSpaces = nsList;
    dev.attachInfoList = attachList;
    return dev;
}

// 创建一个测试条目辅助函数（仅用于 Put/Get/Modify/Remove 等基础操作测试，不设置 nameSpaceList）
static std::shared_ptr<const UbseSsuLedgerEntry> MakeEntry(const std::string& name, UbseSsuNsState state)
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
    entry->allocResult.strategy = UbseSsuAllocStrategy::LINEAR;
    // nameSpaceList 为空 vector，基础操作测试不访问该字段，涉及 nameSpaceList 的测试使用 Rebuild
    return entry;
}

/*
 * 用例描述：Put 正常插入条目
 * 测试步骤：
 * 1、创建测试条目，调用 Put 插入账本
 * 预期结果：
 * 1、返回 true
 */
TEST_F(TestUbseSsuDebtLedger, PutSuccess)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    auto entry = MakeEntry("test-entry", UbseSsuNsState::CREATED);

    bool ok = ledger.Put("test-entry", entry);

    EXPECT_TRUE(ok);
    auto got = ledger.Get("test-entry");
    ASSERT_NE(got, nullptr);
    EXPECT_EQ(got->name, "test-entry");
    EXPECT_EQ(got->state, UbseSsuNsState::CREATED);
}

/*
 * 用例描述：Put 重复 name 时应拒绝，不覆盖已有条目
 * 测试步骤：
 * 1、Put 插入条目
 * 2、再次 Put 相同 name 的条目
 * 预期结果：
 * 1、第一次返回 true
 * 2、第二次返回 false
 * 3、条目内容为第一次插入的值
 */
TEST_F(TestUbseSsuDebtLedger, PutDuplicateReject)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    auto entry1 = MakeEntry("test-entry", UbseSsuNsState::CREATED);
    auto entry2 = MakeEntry("test-entry", UbseSsuNsState::ATTACHED);

    bool first = ledger.Put("test-entry", entry1);
    bool second = ledger.Put("test-entry", entry2);

    EXPECT_TRUE(first);
    EXPECT_FALSE(second);
    auto got = ledger.Get("test-entry");
    ASSERT_NE(got, nullptr);
    EXPECT_EQ(got->state, UbseSsuNsState::CREATED);
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
    bool ok = ledger.Put("test-entry", nullptr);
    EXPECT_FALSE(ok);
}

/*
 * 用例描述：Get 已存在的条目应返回正确条目
 * 测试步骤：
 * 1、Put 条目
 * 2、Get 同一 name
 * 预期结果：
 * 1、返回的条目非空
 * 2、属性与 Put 时一致
 */
TEST_F(TestUbseSsuDebtLedger, GetExisting)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    auto entry = MakeEntry("test-entry", UbseSsuNsState::CREATED);
    ledger.Put("test-entry", entry);

    auto got = ledger.Get("test-entry");

    ASSERT_NE(got, nullptr);
    EXPECT_EQ(got->name, "test-entry");
    EXPECT_EQ(got->state, UbseSsuNsState::CREATED);
}

/*
 * 用例描述：Get 不存在的 name 应返回 nullptr
 * 测试步骤：
 * 1、Get 一个从未 Put 的 name
 * 预期结果：
 * 1、返回 nullptr
 */
TEST_F(TestUbseSsuDebtLedger, GetNotExist)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();

    auto got = ledger.Get("non-existent");

    EXPECT_EQ(got, nullptr);
}

/*
 * 用例描述：Modify 已存在的条目应成功修改其属性
 * 测试步骤：
 * 1、Put 一个 CREATED 条目
 * 2、Modify 将其 state 改为 ATTACHED
 * 预期结果：
 * 1、Modify 返回 true
 * 2、Get 后 state 为 ATTACHED，其他属性不变
 */
TEST_F(TestUbseSsuDebtLedger, ModifySuccess)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    auto entry = MakeEntry("test-entry", UbseSsuNsState::CREATED);
    ledger.Put("test-entry", entry);

    bool ok = ledger.Modify("test-entry", [](UbseSsuLedgerEntry& e) { e.state = UbseSsuNsState::ATTACHED; });

    EXPECT_TRUE(ok);
    auto got = ledger.Get("test-entry");
    ASSERT_NE(got, nullptr);
    EXPECT_EQ(got->state, UbseSsuNsState::ATTACHED);
}

/*
 * 用例描述：Modify 不存在的条目应返回 false
 * 测试步骤：
 * 1、Modify 一个从未 Put 的 name
 * 预期结果：
 * 1、返回 false
 */
TEST_F(TestUbseSsuDebtLedger, ModifyNotExist)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    bool ok = ledger.Modify("non-existent", [](UbseSsuLedgerEntry& e) { e.state = UbseSsuNsState::ATTACHED; });
    EXPECT_FALSE(ok);
}

/*
 * 用例描述：Remove 已存在的条目应成功删除
 * 测试步骤：
 * 1、Put 条目
 * 2、Remove 该条目
 * 预期结果：
 * 1、Remove 返回 true
 * 2、Get 返回 nullptr
 */
TEST_F(TestUbseSsuDebtLedger, RemoveSuccess)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    auto entry = MakeEntry("test-entry", UbseSsuNsState::CREATED);
    ledger.Put("test-entry", entry);

    bool removed = ledger.Remove("test-entry");
    auto got = ledger.Get("test-entry");

    EXPECT_TRUE(removed);
    EXPECT_EQ(got, nullptr);
}

/*
 * 用例描述：Remove 不存在的条目应返回 false
 * 测试步骤：
 * 1、Remove 一个从未 Put 的 name
 * 预期结果：
 * 1、返回 false
 */
TEST_F(TestUbseSsuDebtLedger, RemoveNotExist)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    bool removed = ledger.Remove("non-existent");
    EXPECT_FALSE(removed);
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
    ledger.Put("test-entry", MakeEntry("test-entry", UbseSsuNsState::CREATED));
    ledger.Put("test-entry-2", MakeEntry("test-entry-2", UbseSsuNsState::ATTACHED));
    ledger.Put("test-entry-3", MakeEntry("test-entry-3", UbseSsuNsState::CREATING));

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
 * 用例描述：Rebuild 根据设备列表按 customData.name 分组恢复账本
 * 测试步骤：
 * 1、构造 2 个设备，各有命名空间且 customData.name 为 "rebuild-entry"
 * 2、调用 Rebuild
 * 预期结果：
 * 1、账本中存在 "rebuild-entry" 条目
 * 2、state 为 CREATED (namespaceId > 0)
 * 3、allocResult.nameSpaceList 包含 2 个 NS
 * 4、nsNum、totalBytes 等字段正确
 */
TEST_F(TestUbseSsuDebtLedger, RebuildFromDevices)
{
    auto& ledger = UbseSsuDebtLedger::GetInstance();
    auto ns1 = MakeNs("rebuild-entry", "eid-1", 1, 1024);
    ns1.customData.nsNum = 2;
    ns1.customData.totalBytes = 2048;
    auto ns2 = MakeNs("rebuild-entry", "eid-2", 2, 1024);
    ns2.customData.nsNum = 2;
    ns2.customData.totalBytes = 2048;
    auto dev1 = MakeDev("eid-1", {ns1});
    auto dev2 = MakeDev("eid-2", {ns2});

    ledger.Rebuild({dev1, dev2});

    auto entry = ledger.Get("rebuild-entry");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, UbseSsuNsState::CREATED);
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
    auto ns1 = MakeNs("rebuild-entry", "eid-1", 1, 1024);
    auto ns2 = MakeNs("rebuild-entry-2", "eid-2", 2, 2048);
    auto dev = MakeDev("eid-1", {ns1, ns2});

    ledger.Rebuild({dev});

    auto entry1 = ledger.Get("rebuild-entry");
    ASSERT_NE(entry1, nullptr);
    EXPECT_EQ(entry1->allocResult.nameSpaceList.size(), 1u);
    EXPECT_EQ(entry1->allocResult.nameSpaceList[0].namespaceId, 1u);

    auto entry2 = ledger.Get("rebuild-entry-2");
    ASSERT_NE(entry2, nullptr);
    EXPECT_EQ(entry2->allocResult.nameSpaceList.size(), 1u);
    EXPECT_EQ(entry2->allocResult.nameSpaceList[0].namespaceId, 2u);
}

/*
 * 用例描述：多线程并发 Put/Get/Modify/Remove，不应有数据竞争
 * 测试步骤：
 * 1、4 个线程同时操作 10 个不同的 key
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
                auto entry = MakeEntry(key, UbseSsuNsState::CREATED);
                ledger.Put(key, entry);
                ledger.Get(key);
                ledger.Modify(key, [](UbseSsuLedgerEntry& e) { e.state = UbseSsuNsState::ATTACHED; });
                ledger.Remove(key);
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
