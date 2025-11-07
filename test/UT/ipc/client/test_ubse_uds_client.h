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

#ifndef RACK_MANAGER_UBSE_UDS_CLIENT_H
#define RACK_MANAGER_UBSE_UDS_CLIENT_H

#include <gtest/gtest.h>

#include "src/framework/ipc/client/ubse_uds_client.h"
#include "src/framework/ipc/ubse_ipc_message.h"
#include "ubse_ipc_client.h"

namespace ubse::ut::ipc {
using namespace ubse::ipc;
class TestUbseUdsClient : public testing::Test {
public:
    TestUbseUdsClient();

    virtual void SetUp();

    virtual void TearDown();

private:
    std::unique_ptr<UbseUDSClient> client;
};
} // namespace ubse::ut::ipc
#endif // RACK_MANAGER_UBSE_UDS_CLIENT_H
