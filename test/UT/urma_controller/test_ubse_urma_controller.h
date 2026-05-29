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

#ifndef TEST_UBSE_URMA_CONTROLLER_H
#define TEST_UBSE_URMA_CONTROLLER_H

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mockcpp/mokc.h>
#include "ubse_urma_controller_manager.h"
void ClearNodeInfosForTest()
{
    ubse::utils::WriteLocker<ubse::utils::ReadWriteLock> writeLock(
        &ubse::urmaController::UbseUrmaControllerManager::GetInstance().rwLock);
    ubse::urmaController::UbseUrmaControllerManager::GetInstance().nodeInfos.clear();
    ubse::urmaController::UbseUrmaControllerManager::GetInstance().globalFeId = 0;
    ubse::urmaController::UbseUrmaControllerManager::GetInstance().globalUrmaId = 0;
    ubse::urmaController::UbseUrmaControllerManager::GetInstance().feIdMap.clear();
}

namespace ubse::urmaController::ut {
class TestUbseUrmaController : public testing::Test {
protected:
    void SetUp() override
    {
        Test::SetUp();
        ClearNodeInfosForTest();
    }

    void TearDown() override
    {
        Test::TearDown();
        ClearNodeInfosForTest();
        GlobalMockObject::verify();
    }
};
} // namespace ubse::urmaController::ut

#endif // TEST_UBSE_URMA_CONTROLLER_H
