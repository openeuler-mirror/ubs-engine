/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#ifndef TEST_UBSE_SSU_HANDLER_MESSAGE_H
#define TEST_UBSE_SSU_HANDLER_MESSAGE_H

#include <gtest/gtest.h>

#include "ubse_api_server_def.h"
#include "ipc/message/ubse_ssu_handler_message.h"

namespace ubse::ssu::ipc::ut {

// IPC 层 SsuXxxPack / SsuXxxUnpack 测试套件
class TestUbseSsuHandlerMessage : public testing::Test {
public:
    TestUbseSsuHandlerMessage() = default;
    void SetUp() override {}
    void TearDown() override {}

    struct IpcMessageGuard {
        api::server::UbseIpcMessage msg{nullptr, 0};
        ~IpcMessageGuard()
        {
            if (msg.buffer != nullptr) {
                delete[] msg.buffer;
                msg.buffer = nullptr;
                msg.length = 0;
            }
        }
    };

    static ubse::plugin::service::ssu::UbseSsuVfe MakeVfe();
    static ubse::plugin::service::ssu::UbseSsuFe MakeFe();
    static ubse::plugin::service::ssu::UbseSsuNameSpaceInfo MakeNsInfo();
    static ubse::plugin::service::ssu::UbseSsuAllocResult MakeAllocResult();
    static ubse::plugin::service::ssu::UbseSsuNsStats MakeNsStats();
    static ubse::plugin::service::ssu::UbseSsuConnectInfo MakeConnectInfo();
};

} // namespace ubse::ssu::ipc::ut

#endif // TEST_UBSE_SSU_HANDLER_MESSAGE_H
