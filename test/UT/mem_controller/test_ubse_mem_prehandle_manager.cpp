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

#include "test_ubse_mem_prehandle_manager.h"

#include <mockcpp/mokc.h>

#include "src/controllers/mem/mem_decoder_utils/ubse_mem_prehandle_manager.h"
#include "src/adapter_plugins/mti/lcne/ubse_lcne_decoder_entry.h"
#include "ubse_error.h"
#include "ubse_logger.h"

#include "ubse_mem_configuration.h"

namespace ubse::ut::mem::decoder::utils {
using namespace ubse::mem::decoder::utils;
using namespace ubse::context;

void TestUbseMemPrehandleManager ::SetUp()
{
    Test::SetUp();
}

void TestUbseMemPrehandleManager ::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemPrehandleManager, InitPreHandleFailed)
{
    MOCKER_CPP(MemDecoderUtils::GetAllHandles).stubs().will(returnValue(UBSE_ERROR));
}

TEST_F(TestUbseMemPrehandleManager, InitPreHandleSuccess)
{
    MOCKER_CPP(MemDecoderUtils::GetAllHandles).stubs().will(returnValue(UBSE_OK));
}
}