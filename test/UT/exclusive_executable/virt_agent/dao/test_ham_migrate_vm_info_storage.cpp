/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#include "test_ham_migrate_vm_info_storage.h"
#include <mockcpp/GlobalMockObject.h>
#include "ham_migrate_vm_info_storage.h"

#include <ham_migrate_vm_info_message.h>
#include <mockcpp/mokc.h>

using namespace vm;

namespace ubse::ut::vm {
static const int PID = 123;
static const std::string NODE = "Node0";
static const std::string HAM_MIGRATE_KEY_PREFIX = "ubse_";
static const std::string HAM_MIGRATE_KEY = "ham_migrate_";
static const int OLD_INDEX = 5;
static const int NEW_INDEX = OLD_INDEX + 1;

// 设置测试环境
void TestHamMigrateVmInfoStorage::SetUp()
{
    Test::SetUp();
}

// 拆卸测试环境
void TestHamMigrateVmInfoStorage::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

uint32_t UbseStorageQueryData(const std::string &keyPrefix, const std::string &key, void *ctx,
                              UbseStorageDealDataFunc func)
{
    auto *hamMigrateVmInfos = static_cast<std::vector<HamMigrateVmInfo> *>(ctx);
    HamMigrateVmInfo hamMigrateVmInfo;
    hamMigrateVmInfos->emplace_back(hamMigrateVmInfo);
    return VM_OK;
}

TEST_F(TestHamMigrateVmInfoStorage, SetHamMigrateVmInfo)
{
    GTEST_SKIP();
    std::string keyPrefix;
    std::string key;
    std::string nodeId = NODE;
    int pid = PID;
    HamMigrateVmInfo hamMigrateVmInfo{};
    hamMigrateVmInfo.nodeId = nodeId;
    hamMigrateVmInfo.socketId = 1;
    hamMigrateVmInfo.numaId = 1;
    hamMigrateVmInfo.pid = pid;
    hamMigrateVmInfo.uuid = "test-uuid";
    hamMigrateVmInfo.borrowName = "borrowName";
    hamMigrateVmInfo.vmState = VmState::BORROWED_MIGRATED;
    hamMigrateVmInfo.vmOpState = VmOpState::BORROWED_ADDRESS;
    std::vector<HamMigrateVmInfo> hamMigrateVmInfo_;
    hamMigrateVmInfo_.emplace_back(hamMigrateVmInfo);
    HamMigrateVmInfoMessage hamMigrateVmInfoMessage{};
    hamMigrateVmInfoMessage.SetData(hamMigrateVmInfo_);
    hamMigrateVmInfoMessage.Serialize();
    UbseByteBuffer data{.data = hamMigrateVmInfoMessage.SerializedData(),
                        .len = hamMigrateVmInfoMessage.SerializedDataSize(),
                        .freeFunc = nullptr};
    MOCKER(ubse::storage::UbseStoragePutData).stubs().with(spy(keyPrefix), spy(key), any()).will(returnValue(VM_OK));
    VmResult ret = HamMigrateVmInfoStorage::SetHamMigrateVmInfo(hamMigrateVmInfo);
    EXPECT_EQ(ret, VM_OK);
    EXPECT_EQ(keyPrefix + key, HAM_MIGRATE_KEY_PREFIX + HAM_MIGRATE_KEY);
    std::vector<HamMigrateVmInfo> hamMigrateVmInfos;
    HamMigrateVmInfoStorage::QueryHandler(HAM_MIGRATE_KEY_PREFIX, key, data, &hamMigrateVmInfos);
    auto _hamMigrateVmInfo = hamMigrateVmInfos[0];
    bool isHamMigrateVmInfoEq = hamMigrateVmInfo.nodeId == _hamMigrateVmInfo.nodeId &&
                                hamMigrateVmInfo.socketId == _hamMigrateVmInfo.socketId &&
                                hamMigrateVmInfo.numaId == _hamMigrateVmInfo.numaId &&
                                hamMigrateVmInfo.pid == _hamMigrateVmInfo.pid &&
                                hamMigrateVmInfo.uuid == _hamMigrateVmInfo.uuid &&
                                hamMigrateVmInfo.borrowName == _hamMigrateVmInfo.borrowName &&
                                hamMigrateVmInfo.vmState == _hamMigrateVmInfo.vmState &&
                                hamMigrateVmInfo.vmOpState == _hamMigrateVmInfo.vmOpState;
    EXPECT_TRUE(isHamMigrateVmInfoEq);
    GlobalMockObject::verify();

    MOCKER(ubse::storage::UbseStoragePutData).stubs().will(returnValue(VM_ERROR));
    ret = HamMigrateVmInfoStorage::SetHamMigrateVmInfo(hamMigrateVmInfo);
    EXPECT_EQ(ret, VM_ERROR);
    GlobalMockObject::verify();
}

TEST_F(TestHamMigrateVmInfoStorage, GetHamMigrateVmInfo)
{
    GTEST_SKIP();
    std::string nodeId = NODE;
    int pid = PID;
    HamMigrateVmInfo vmInfo;
    VmResult ret = HamMigrateVmInfoStorage::GetHamMigrateVmInfo(nodeId, pid, vmInfo);
    EXPECT_EQ(ret, VM_OK);
    EXPECT_EQ(vmInfo.vmOpState, VmOpState::BORROWED_ADDRESS);

    ret = HamMigrateVmInfoStorage::GetHamMigrateVmInfo(nodeId, 0, vmInfo);
    EXPECT_EQ(ret, VM_ERROR);
}

TEST_F(TestHamMigrateVmInfoStorage, GetHamMigrateVmInfos)
{
    GTEST_SKIP();
    std::string nodeId = NODE;
    std::vector<HamMigrateVmInfo> hamMigrateVmInfos;
    HamMigrateVmInfoStorage::GetHamMigrateVmInfos(nodeId, hamMigrateVmInfos);
    EXPECT_EQ(hamMigrateVmInfos[0].vmOpState, VmOpState::BORROWED_ADDRESS);

    HamMigrateVmInfoStorage::GetAllHamMigrateVmInfos(hamMigrateVmInfos);
    EXPECT_EQ(hamMigrateVmInfos[0].vmOpState, VmOpState::BORROWED_ADDRESS);
}

TEST_F(TestHamMigrateVmInfoStorage, DelHamMigrateVmInfo)
{
    GTEST_SKIP();
    std::string keyPrefix;
    std::string key;
    std::string nodeId = NODE;
    int pid = PID;

    VmResult ret = HamMigrateVmInfoStorage::DelHamMigrateVmInfo(nodeId, pid);
    EXPECT_NE(ret, VM_OK);

    MOCKER(ubse::storage::UbseStoragePutData).stubs().will(returnValue(VM_OK));
    ret = HamMigrateVmInfoStorage::DelHamMigrateVmInfo(nodeId, pid);
    EXPECT_NE(ret, VM_OK);

    ret = HamMigrateVmInfoStorage::DelHamMigrateVmInfo(nodeId, pid);
    EXPECT_NE(ret, VM_OK);

    MOCKER(ubse::storage::UbseStoragePutData).reset();
    MOCKER(ubse::storage::UbseStoragePutData)
        .stubs()
        .with(spy(keyPrefix), spy(key), any(), any())
        .will(returnValue(VM_OK));
    HamMigrateVmInfo hamMigrateVmInfo;
    ret = HamMigrateVmInfoStorage::DelHamMigrateVmInfo(nodeId, pid);
    EXPECT_EQ(ret, VM_OK);
    EXPECT_EQ(keyPrefix + key, HAM_MIGRATE_KEY_PREFIX + HAM_MIGRATE_KEY);
    GlobalMockObject::verify();
}

TEST_F(TestHamMigrateVmInfoStorage, QueryHandler)
{
    UbseByteBuffer buffer;
    std::vector<HamMigrateVmInfo> hamMigrateVmInfos;
    HamMigrateVmInfoStorage::QueryHandler(HAM_MIGRATE_KEY_PREFIX, HAM_MIGRATE_KEY, buffer, &hamMigrateVmInfos);
    EXPECT_TRUE(hamMigrateVmInfos.empty());

    buffer.len = 0;
    HamMigrateVmInfoStorage::QueryHandler(HAM_MIGRATE_KEY_PREFIX, HAM_MIGRATE_KEY, buffer, &hamMigrateVmInfos);
    EXPECT_TRUE(hamMigrateVmInfos.empty());
}

TEST_F(TestHamMigrateVmInfoStorage, ToString)
{
    std::vector<HamMigrateVmInfo> hamMigrateVmInfos;
    HamMigrateVmInfo hamMigrateVmInfo;
    hamMigrateVmInfos.emplace_back(hamMigrateVmInfo);
    auto json = HamMigrateVmInfoStorage::ToString(hamMigrateVmInfos);
    EXPECT_FALSE(json.empty());
}
} // namespace ubse::ut::vm