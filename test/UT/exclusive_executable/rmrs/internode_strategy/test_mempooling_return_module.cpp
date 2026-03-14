/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <gtest/gtest.h>
#include "mockcpp/mockcpp.hpp"

#define private public
#include "mem_manager.h"
#include "mempooling_return_module.h"
#include "ubse_mem_controller.h"
#undef private
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)
namespace mempooling {
class TestMemReturnScanner : public ::testing::Test {
protected:
    TestMemReturnScanner() {}
    virtual void SetUp() {}
    virtual void TearDown()
    {
        GlobalMockObject::reset();
    }
};

TEST_F(TestMemReturnScanner, start_TestSuccess)
{
    MemReturnScanner::Instance().running.store(true);
    auto ret = MemReturnScanner::Instance().start();
    EXPECT_EQ(ret, true);
}

TEST_F(TestMemReturnScanner, start_TestSuccess1)
{
    MemReturnScanner::Instance().running.store(false);
    auto ret = MemReturnScanner::Instance().start();
    EXPECT_EQ(ret, true);
}

TEST_F(TestMemReturnScanner, run_finish)
{
    MemReturnManager::Instance().borrowCache.clear();
    MemReturnScanner::Instance().run();
}

uint32_t MockUbseQueryResult(const std::string &name, UbseMemResult &result, UbseMemBorrowType borrowType)
{
    result.stage == ubse::mem::controller::UbseMemStage::UBSE_DELETING;
    return 0;
}

uint32_t MockUbseQueryResult1(const std::string &name, UbseMemResult &result, UbseMemBorrowType borrowType)
{
    result.stage == ubse::mem::controller::UbseMemStage::UBSE_NOT_EXIST;
    return 0;
}

MpResult MockUpdate(MemReturnManager *obj, const std::string &borrowId, BorrowItem &value)
{
    obj->borrowCache.clear();
    return 0;
}

MpResult MockUpdate1(MemReturnManager *obj, const std::string &borrowId, BorrowItem &value)
{
    obj->borrowCache.clear();
    return 1;
}

TEST_F(TestMemReturnScanner, run_add)
{
    MOCKER_CPP(&ubse::mem::controller::UbseQueryResult,
               uint32_t(*)(const std::string &name, UbseMemResult &result, UbseMemBorrowType borrowType))
        .stubs()
        .will(invoke(MockUbseQueryResult));
    MemReturnManager::Instance().borrowCache = {{"borrow_0", {"borrow_0", "srcNid_4", 5, "dstNid_2", 3, 125}}};
    MOCKER_CPP(&MemReturnManager::Update,
               MpResult(*)(MemReturnManager *This, const std::string &borrowId, BorrowItem &value))
        .stubs()
        .will(invoke(MockUpdate));
    MemReturnScanner::Instance().run();
}

TEST_F(TestMemReturnScanner, run_add1)
{
    MOCKER_CPP(&ubse::mem::controller::UbseQueryResult,
               uint32_t(*)(const std::string &name, UbseMemResult &result, UbseMemBorrowType borrowType))
        .stubs()
        .will(invoke(MockUbseQueryResult));
    MemReturnManager::Instance().borrowCache = {{"borrow_0", {"borrow_0", "srcNid_4", 5, "dstNid_2", 3, 125}}};
    MOCKER_CPP(&MemReturnManager::Update,
               MpResult(*)(MemReturnManager *This, const std::string &borrowId, BorrowItem &value))
        .stubs()
        .will(invoke(MockUpdate1));
    MemReturnScanner::Instance().run();
}

MpResult MockRemove(MemReturnManager *obj, const std::string &borrowId)
{
    obj->borrowCache.clear();
    return 0;
}

MpResult MockRemove1(MemReturnManager *obj, const std::string &borrowId)
{
    obj->borrowCache.clear();
    return 1;
}

TEST_F(TestMemReturnScanner, run_sub)
{
    MOCKER_CPP(&ubse::mem::controller::UbseQueryResult,
               uint32_t(*)(const std::string &name, UbseMemResult &result, UbseMemBorrowType borrowType))
        .stubs()
        .will(invoke(MockUbseQueryResult1));
    MemReturnManager::Instance().borrowCache = {{"borrow_0", {"borrow_0", "srcNid_4", 5, "dstNid_2", 3, 125}}};
    MOCKER_CPP(&MemReturnManager::Remove, MpResult(*)(MemReturnManager * This, const std::string &borrowId))
        .stubs()
        .will(invoke(MockRemove));
    MOCKER_CPP(&MemReturnManager::IsAllReturned, bool (*)(const std::string &srcNid, const uint16_t &srcRemoteNumaId))
        .stubs()
        .will(returnValue(true));
    MemReturnScanner::Instance().run();
}

TEST_F(TestMemReturnScanner, run_sub1)
{
    MOCKER_CPP(&ubse::mem::controller::UbseQueryResult,
               uint32_t(*)(const std::string &name, UbseMemResult &result, UbseMemBorrowType borrowType))
        .stubs()
        .will(invoke(MockUbseQueryResult1));
    MemReturnManager::Instance().borrowCache = {{"borrow_0", {"borrow_0", "srcNid_4", 5, "dstNid_2", 3, 125}}};
    MOCKER_CPP(&MemReturnManager::Remove, MpResult(*)(MemReturnManager * This, const std::string &borrowId))
        .stubs()
        .will(invoke(MockRemove1));
    MemReturnScanner::Instance().run();
}

} // namespace mempooling
