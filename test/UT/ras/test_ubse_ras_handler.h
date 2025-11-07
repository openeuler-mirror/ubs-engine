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

#ifndef TEST_UBSE_RAS_HANDLER_H
#define TEST_UBSE_RAS_HANDLER_H

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "ubse_context.h"
#include "mockcpp/mockcpp.hpp"
#include "ubse_ras_handler.h"
#include "dlfcn.h"
#include "ubse_ras_error.h"
#include "ubse_election.h"
#include "ubse_election_module.h"
#include "message/ubse_ras_message.h"
#include "ubse_ras.h"
#include "ubse_node_controller.h"
#include "ubse_str_util.h"
#include "ubse_lcne_module.h"
namespace ubse::ras {
extern void SwitchRoleWhenMasterFault(std::string &faultInfo);
}
namespace ubse::ras::ut {
using namespace ubse::context;
using namespace ubse::election;
using namespace ubse::nodeController;
class TestUbseRasHandler : public testing::Test {
public:
    TestUbseRasHandler() = default;

    void SetUp() override;

    void TearDown() override;
};
}
#endif  // TEST_UBSE_RAS_HANDLER_H
