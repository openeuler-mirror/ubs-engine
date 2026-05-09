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

#ifndef UBS_ENGINE_VIRT_AGENT_MEM_MIGRATE_FUZZ_H
#define UBS_ENGINE_VIRT_AGENT_MEM_MIGRATE_FUZZ_H

#include <gtest/gtest.h>

namespace ubse::virt::migrate {
class VirtAgentMemMigrateFuzz : public testing::Test {
public:
    VirtAgentMemMigrateFuzz() = default;

    void SetUp() override;

    void TearDown() override;
};
} // namespace ubse::virt::migrate

#endif // UBS_ENGINE_VIRT_AGENT_MEM_MIGRATE_FUZZ_H
