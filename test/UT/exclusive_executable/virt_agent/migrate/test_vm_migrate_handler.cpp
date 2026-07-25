/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_vm_migrate_handler.h"

#include <gmock/gmock-function-mocker.h>
#include <migrate_state_storage.h>
#include <mockcpp/GlobalMockObject.h>
#include <mockcpp/mokc.h>
#include <vm_http_util.h>
#include "vm_configuration.h"
#include "vm_migrate_handler.h"

using namespace vm;

namespace ubse::vm::ut {
static const int WAIT_TIME = 300;
TestVmMigrateHandler::TestVmMigrateHandler() = default;

void TestVmMigrateHandler::SetUp()
{
    Test::SetUp();
}

void TestVmMigrateHandler::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

uint32_t UbseGetRole(std::string& role)
{
    role = "master";
    return VM_OK;
}

VmResult GetMigrateStates(NumaVMInfoMap& numaVmInfoMap)
{
    VMBasicInfo basicInfo;
    basicInfo.vmMigrateStatus = VmMigrateStatus::MIGRATING;
    basicInfo.vmMigrateInTime = 1;
    VMNodeLocInfo locInfo;
    locInfo.hostId = "Node0";
    locInfo.socketId = 1;
    locInfo.numaId = 1;
    numaVmInfoMap[locInfo]["uuid"] = basicInfo;

    VMBasicInfo vmBasic1;
    vmBasic1.vmMigrateStatus = VmMigrateStatus::MIGRATEABLE;
    numaVmInfoMap[locInfo]["uuid1"] = vmBasic1;

    VMBasicInfo vmBasic2;
    vmBasic2.vmMigrateStatus = VmMigrateStatus::MIGRATING;
    vmBasic2.vmMigrateInTime = 0;
    numaVmInfoMap[locInfo]["uuid2"] = vmBasic2;

    VMBasicInfo vmBasic3;
    vmBasic3.vmMigrateStatus = VmMigrateStatus::MIGRATING;
    vmBasic3.vmMigrateInTime = std::time(nullptr);
    numaVmInfoMap[locInfo]["uuid3"] = vmBasic3;

    VMBasicInfo vmBasic4;
    vmBasic4.vmMigrateStatus = VmMigrateStatus::MIGRATING;
    vmBasic4.vmMigrateInTime = 1;
    numaVmInfoMap[locInfo]["uuid4"] = vmBasic4;

    return VM_OK;
}

} // namespace ubse::vm::ut
