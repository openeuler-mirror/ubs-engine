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

#ifndef TEST_UBSE_NODE_API_H
#define TEST_UBSE_NODE_API_H
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <mockcpp/mockcpp.hpp>
#include "ubse_node_api.h"
#include "ubse_serial_util.h"
#include "ubse_context.h"
#include "ubse_node_topology.h"
#include "ubse_api_server_module.h"
#include "ubse_node_controller_query_api.h"
#include "ubse_node_api_convert.h"
#include "ubse_election.h"
#include "ubse_election_module.h"

namespace ubse::node::api {
extern UbseResult UbseSplitStringByHyphen(const std::string &ubse_node_socket_id,
                                          ubse::serial::UbseSerialization &ubse_serial);
extern void UbseClusterList(std::vector<ubse::nodeController::UbseNodeInfo> &nodeList);
}
namespace ubse::node::api::ut {
using namespace ubse::node::api;
using namespace ubse::serial;
using namespace ubse::context;
using namespace ubse::nodeController;
using namespace ubse::election;
class TestUbseNodeApi : public testing::Test {
public:
    TestUbseNodeApi() = default;

    void SetUp() override;

    void TearDown() override;
};
}
#endif  // TEST_UBSE_NODE_API_H
