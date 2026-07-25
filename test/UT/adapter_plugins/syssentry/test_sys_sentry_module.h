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

#ifndef TEST_SYS_SENTRY_MODULE_H
#define TEST_SYS_SENTRY_MODULE_H

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <functional>
#include "ubse_conf.h"
#include "ubse_context.h"
#include "ubse_lcne_module.h"
#include "ubse_os_util.h"
#include "ubse_pointer_process.h"
#include "ubse_ras_handler.h"
#include "ubse_timer.h"
#include "adapter_plugins/mti/ubse_mti_def.h"
#include "adapter_plugins/mti/ubse_mti_interface.h"
#include "dlfcn.h"
#include "mockcpp/mockcpp.hpp"
#include "securec.h"
#include "sentry_observer.h"
#include "src/adapter_plugins/mti/ubse_mti_interface_default.h"
#include "sys_sentry_module.h"

namespace syssentry {
extern std::vector<std::string> SplitString(const std::string& str, char delimiter);
extern UbseResult GetEids(std::string& clientEid, std::string& serverEids);
extern UbseResult GetCurNodeCna(std::vector<std::string>& busNodeCnas);
extern UbseResult SetSysSentryFaultReporter();
extern void LinkStrings(std::string& result, const std::string linkSymbol, const std::vector<std::string> strings);
extern std::string ShellEscape(const std::string& str);
extern UbseResult ProcessEids(const std::map<ubse::adapter_plugins::mti::UbseMtiIouInfo,
                                             ubse::adapter_plugins::mti::UbseMtiEidGroup>& allSocketComEid,
                              const std::string& nodeId,
                              std::unordered_map<std::string, std::vector<std::string>>& eids,
                              std::vector<std::string>& eidGroup);
} // namespace syssentry
namespace syssentry::ut {
using namespace ubse::context;
using namespace syssentry;

class TestSysSentryModule : public testing::Test {
public:
    TestSysSentryModule() = default;

    void SetUp() override;

    void TearDown() override;
};
} // namespace syssentry::ut
#endif // TEST_SYS_SENTRY_MODULE_H
