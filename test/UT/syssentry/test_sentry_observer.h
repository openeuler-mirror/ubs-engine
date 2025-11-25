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

#ifndef TEST_SENTRY_OBSERVER_H
#define TEST_SENTRY_OBSERVER_H

#include <functional>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "mockcpp/mockcpp.hpp"
#include "ubse_context.h"
#include "ubse_conf.h"
#include "sentry_observer.h"
#include "dlfcn.h"
#include "ubse_pointer_process.h"
#include "securec.h"
#include "ubse_ras_handler.h"

namespace syssentry::ut {
using namespace ubse::context;
using namespace syssentry;

class TestSentryObserver : public testing::Test {
public:
    TestSentryObserver() = default;

    void SetUp() override;

    void TearDown() override;
};
}
#endif  // TEST_SENTRY_OBSERVER_H
