/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
*/


#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

#include "ubse_conf_module.h"

namespace ubse::nodeController {
std::string GetCmdLineResult(std::string_view cmd);
std::optional<ubse::config::ConfigItem> RegisterPmdMappingConfig();
std::optional<ubse::config::ConfigItem> RegisterAllocatorConfig();
}
namespace ubse::mem::mem_scheduler::ut {
using namespace ubse::nodeController;
TEST(RegisterCustomizedConfig, RegisterPmdMappingConfig_fail)
{
    MOCKER_CPP(GetCmdLineResult).reset();

    MOCKER_CPP(GetCmdLineResult).stubs().will(returnValue(std::string()));
    EXPECT_FALSE(RegisterPmdMappingConfig().has_value());

    MOCKER_CPP(GetCmdLineResult).reset();
    MOCKER_CPP(GetCmdLineResult).stubs().will(returnValue(std::string("lala")));
    EXPECT_FALSE(RegisterPmdMappingConfig().has_value());

    MOCKER_CPP(GetCmdLineResult).reset();
    MOCKER_CPP(GetCmdLineResult).stubs().will(returnValue(std::string("k_pmd_mapping=50%")));
    EXPECT_FALSE(RegisterPmdMappingConfig().has_value());

    MOCKER_CPP(GetCmdLineResult).reset();
    MOCKER_CPP(GetCmdLineResult).stubs().will(returnValue(std::string("pmd_mapping=50")));
    EXPECT_FALSE(RegisterPmdMappingConfig().has_value());

    MOCKER_CPP(GetCmdLineResult).reset();
    MOCKER_CPP(GetCmdLineResult).stubs().will(returnValue(std::string("pmd_mapping=50%fsdgf")));
    EXPECT_FALSE(RegisterPmdMappingConfig().has_value());

    MOCKER_CPP(GetCmdLineResult).reset();
    MOCKER_CPP(GetCmdLineResult).stubs().will(returnValue(std::string("pmd_mapping=120%")));
    EXPECT_FALSE(RegisterPmdMappingConfig().has_value());

    MOCKER_CPP(GetCmdLineResult).reset();
    MOCKER_CPP(GetCmdLineResult).stubs().will(returnValue(std::string("pmd_mapping=120%")));
    EXPECT_FALSE(RegisterPmdMappingConfig().has_value());
    MOCKER_CPP(GetCmdLineResult).reset();
}

TEST(RegisterCustomizedConfig, RegisterPmdMappingConfig_success)
{
    GTEST_SKIP();
    MOCKER_CPP(GetCmdLineResult).reset();

    MOCKER_CPP(GetCmdLineResult).stubs().will(returnValue(std::string("pmd_mapping=80%")));
    auto result = RegisterPmdMappingConfig();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(std::stoi(result->value), 80);

    MOCKER_CPP(GetCmdLineResult).reset();
    MOCKER_CPP(GetCmdLineResult).stubs().will(returnValue(std::string("dasd pmd_mapping=80% asfasgf")));
    result = RegisterPmdMappingConfig();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(std::stoi(result->value), 80);

    MOCKER_CPP(GetCmdLineResult).reset();
}

TEST(RegisterCustomizedConfig, RegisterAllocatorConfig_fail)
{
    MOCKER_CPP(GetCmdLineResult).reset();

    MOCKER_CPP(GetCmdLineResult).stubs().will(returnValue(std::string()));
    EXPECT_FALSE(RegisterAllocatorConfig().has_value());


    MOCKER_CPP(GetCmdLineResult).reset();
    MOCKER_CPP(GetCmdLineResult).stubs().will(returnValue(std::string("asdad")));
    EXPECT_FALSE(RegisterAllocatorConfig().has_value());

    MOCKER_CPP(GetCmdLineResult).reset();
}

TEST(RegisterCustomizedConfig, RegisterAllocatorConfig_success)
{
    MOCKER_CPP(GetCmdLineResult).reset();

    MOCKER_CPP(GetCmdLineResult).stubs().will(returnValue(std::string("hugetlb_pmd")));
    auto result = RegisterAllocatorConfig();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->value, "hugetlb_pmd");

    MOCKER_CPP(GetCmdLineResult).reset();
    MOCKER_CPP(GetCmdLineResult).stubs().will(returnValue(std::string("\t\n\r\f\v hugetlb_pmd")));
    result = RegisterAllocatorConfig();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->value, "hugetlb_pmd");

    MOCKER_CPP(GetCmdLineResult).reset();
    MOCKER_CPP(GetCmdLineResult).stubs().will(returnValue(std::string("hugetlb_pud")));
    result = RegisterAllocatorConfig();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->value, "hugetlb_pud");

    MOCKER_CPP(GetCmdLineResult).reset();
    MOCKER_CPP(GetCmdLineResult).stubs().will(returnValue(std::string("buddy_highmem")));
    result = RegisterAllocatorConfig();
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->value, "buddy_highmem");

    MOCKER_CPP(GetCmdLineResult).reset();
}
}
