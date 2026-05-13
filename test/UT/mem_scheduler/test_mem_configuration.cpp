/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#include "test_mem_configuration.h"

#include "ubse_mem_configuration.h"
#include "ubse_node_controller.h"

namespace ubse::mem::mem_scheduler::ut {
using namespace ubse::nodeController;
using namespace ubse::mem::strategy;
void TestMemConfiguration::SetUp()
{
    Test::SetUp();
}

void TestMemConfiguration::TearDown()
{
    Test::TearDown();
}

namespace {
uint32_t MockGetConf(const std::string& section, const std::string& configKey, std::string& configVal)
{
    if (configKey == OCK_MEM_SYSTEM_POOL_MEMORY_RATIO) {
        configVal = "75";
    }

    if (configKey == OBMM_MEMORY_BLOCK_SIZE_CONFIG_KEY) {
        configVal = "1024";
    }

    return UBSE_OK;
}
} // namespace
/*
 * 用例描述：
 * 正常情况下获取所有的配置项
 * 测试步骤：
 * 1、正常初始化
 * 2、获取配置项正常
 * 预期结果：
 * 1、正常保存了所有配置项
 */
TEST_F(TestMemConfiguration, GetAllConfigSuccess)
{
    MOCKER(&ubse::context::UbseContext::GetModule<ubse::config::UbseConfModule>)
        .stubs()
        .will(returnValue(std::make_shared<ubse::config::UbseConfModule>()));
    auto borrowSize = UbseMemConfiguration::GetInstance().GetMaxBorrowSize();
    auto importSize = UbseMemConfiguration::GetInstance().GetMaxSocketImportSize();
    EXPECT_EQ(borrowSize, MAX_BORROW_MEM_PER_NODE);
    EXPECT_EQ(importSize, MAX_IMPORT_MEM_SIZE_PER_SOCKET * ONE_M);
}

} // namespace ubse::mem::mem_scheduler::ut