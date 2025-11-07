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

#ifndef TEST_UBSE_IPC_SERVER_H
#define TEST_UBSE_IPC_SERVER_H

#include <gtest/gtest.h>

#include "src/framework/ipc/include/ubse_ipc_server.h"
#include "src/framework/ipc/ubse_ipc_message.h"

namespace ubse::ut::ipc {
using namespace ubse::ipc;
class TestUbseIpcServer : public testing::Test {
public:
    TestUbseIpcServer();

    virtual void SetUp();

    virtual void TearDown();

private:
    std::unique_ptr<UbseIpcServer> server;
};
} // namespace ubse::ut::ipc
#endif // TEST_UBSE_IPC_SERVER_H
