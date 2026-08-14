/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#ifndef TEST_UBSE_SSU_OBJ_MESSAGE_H
#define TEST_UBSE_SSU_OBJ_MESSAGE_H

#include <gtest/gtest.h>

#include "ipc/message/ubse_ssu_obj_message.h"

namespace ubse::ssu::ipc::ut {

// 对象层 pack/unpack 测试套件
// 覆盖 String/Guid/Vfe/NameSpaceInfo/AllocResult/NsDevPaths/ConnectInfo/NsStats/Fe
// 以及 SpaceReq/LinearSpaceReq/StripedSpaceReq 的 Unpack 路径
class TestUbseSsuObjMessage : public testing::Test {
public:
    TestUbseSsuObjMessage() = default;
    void SetUp() override {}
    void TearDown() override {}

    static ubse::plugin::service::ssu::UbseSsuVfe MakeVfe();
    static ubse::plugin::service::ssu::UbseSsuNameSpaceInfo MakeNameSpaceInfo();
    static ubse::plugin::service::ssu::UbseSsuAllocResult MakeAllocResult();
    static ubse::plugin::service::ssu::UbseSsuConnectInfo MakeConnectInfo();
    static ubse::plugin::service::ssu::UbseSsuNsStats MakeNsStats();
    static ubse::plugin::service::ssu::UbseSsuFe MakeFe();
};

} // namespace ubse::ssu::ipc::ut

#endif // TEST_UBSE_SSU_OBJ_MESSAGE_H
