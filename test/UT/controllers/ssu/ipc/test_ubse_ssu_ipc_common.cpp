/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include "test_ubse_ssu_ipc_common.h"
#include "ubse_ipc_common.h"
namespace ubse::ssu::ipc::ut {

using ubse::utils::UbseOsUtil;
using ubse::service::UbseServiceRegistry;

void IpcTestFixture::SetUp()
{
    Test::SetUp();
    mockService = std::make_shared<MockSsuService>();
    UbseServiceRegistry::GetInstance().RegisterService(mockService);
    // mock 静态函数 GetUserNameById，使 Init 阶段返回成功。
    MOCKER_CPP(&UbseOsUtil::GetUserNameById).stubs().will(returnValue((uint32_t)UBSE_OK));
}

void IpcTestFixture::TearDown()
{
    Test::TearDown();
    if (mockService != nullptr) {
        UbseServiceRegistry::GetInstance().UnRegisterService(mockService);
        mockService.reset();
    }
    GlobalMockObject::verify();
}

UbseRequestContext IpcTestFixture::MakeContext(uint16_t opCode)
{
    UbseRequestContext ctx{};
    ctx.clientInfo.uid = 1000;
    ctx.clientInfo.gid = 1000;
    ctx.clientInfo.pid = 1234;
    ctx.requestId = 0xABCD;
    ctx.timestamp = 0;
    ctx.moduleCode = static_cast<uint16_t>(UBSE_SSU);
    ctx.opCode = opCode;
    return ctx;
}

} // namespace ubse::ssu::ipc::ut
