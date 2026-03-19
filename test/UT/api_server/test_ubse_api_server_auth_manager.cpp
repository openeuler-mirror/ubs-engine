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

#include "test_ubse_api_server_auth_manager.h"

#include <mockcpp/mockcpp.hpp>

#include "ubse_context.h"

namespace ubse::ut::api::server {
TestUbseApiServerAuthManager::TestUbseApiServerAuthManager() = default;
void TestUbseApiServerAuthManager::SetUp()
{
    authManager = std::make_unique<UbseApiServerAuthManager>();
    Test::SetUp();
}

void TestUbseApiServerAuthManager::TearDown()
{
    authManager.reset();
    GlobalMockObject::verify();
    Test::TearDown();
}

// 测试正常配置解析
TEST_F(TestUbseApiServerAuthManager, ShouldParseValidRoleConfigSuccessfully)
{
    // 准备测试数据
    const std::string configSection = "role_permissions";
    std::map<std::string, std::map<std::string, std::string>> configData{
        {configSection, {{"manager", "object1,object2,object3"}, {"user", "object4,object5"}, {"guest", "object6"}}}};

    // 设置mock行为
    MOCKER_CPP(&UbseConfModule::GetAllConfigWithPrefix)
        .stubs()
        .with(_, outBound(configData))
        .will(returnValue(UBSE_OK));
    // 执行测试
    auto confModule = std::make_shared<UbseConfModule>();
    auto result = authManager->ParseRoleConfig(confModule, configSection);

    // 验证结果
    EXPECT_EQ(result, UBSE_OK);

    // 验证角色配置是否正确解析
    auto roleToObjects = authManager->roleToObjects_;
    EXPECT_EQ(roleToObjects.size(), 4); // 3个角色+admin角色, 4个角色

    // 验证admin角色
    auto adminIt = roleToObjects.find("admin");
    ASSERT_NE(adminIt, roleToObjects.end());
    EXPECT_EQ(adminIt->second.size(), 1);                          // admin角色配置了1个对象
    EXPECT_NE(adminIt->second.find("all"), adminIt->second.end()); // admin角色配置all对象
                                                                   // 验证manager角色
    auto managerIt = roleToObjects.find("manager");
    ASSERT_NE(managerIt, roleToObjects.end());
    EXPECT_EQ(managerIt->second.size(), 3); // manager角色配置了3个对象
    EXPECT_NE(managerIt->second.find("object1"), managerIt->second.end());
    EXPECT_NE(managerIt->second.find("object2"), managerIt->second.end());
    EXPECT_NE(managerIt->second.find("object3"), managerIt->second.end());

    // 验证user角色
    auto userIt = roleToObjects.find("user");
    ASSERT_NE(userIt, roleToObjects.end());
    EXPECT_EQ(userIt->second.size(), 2); // user配置了2个对象

    // 验证guest角色
    auto guestIt = roleToObjects.find("guest");
    ASSERT_NE(guestIt, roleToObjects.end());
    EXPECT_EQ(guestIt->second.size(), 1); // guest配置了1个对象
}

// 配置模块返回错误
TEST_F(TestUbseApiServerAuthManager, ShouldReturnErrorWhenConfigModuleFails)
{
    const std::string configSection = "role_permissions";

    // 设置mock返回错误
    MOCKER_CPP(&UbseConfModule::GetAllConfigWithPrefix).stubs().will(returnValue(UBSE_ERROR));

    auto confModule = std::make_shared<UbseConfModule>();
    auto result = authManager->ParseRoleConfig(confModule, configSection);

    EXPECT_EQ(result, UBSE_ERROR);
    EXPECT_EQ(authManager->roleToObjects_.size(), 1); // 只有admin角色
}

// 配置节不存在
TEST_F(TestUbseApiServerAuthManager, ShouldReturnErrorWhenConfigSectionNotFound)
{
    const std::string configSection = "nonexistent_section";
    std::map<std::string, std::map<std::string, std::string>> configData{
        {"other_section", {{"role1", "obj1"}}} // 不包含目标section
    };

    // 设置mock行为
    MOCKER_CPP(&UbseConfModule::GetAllConfigWithPrefix)
        .stubs()
        .with(_, outBound(configData))
        .will(returnValue(UBSE_OK));

    // 执行测试
    auto confModule = std::make_shared<UbseConfModule>();
    auto result = authManager->ParseRoleConfig(confModule, configSection);

    EXPECT_EQ(result, UBSE_ERROR_CONF_INVALID);
    EXPECT_EQ(authManager->roleToObjects_.size(), 1); // 只有admin角色
}

// 配置节为空
TEST_F(TestUbseApiServerAuthManager, ShouldReturnErrorWhenConfigSectionIsEmpty)
{
    const std::string configSection = "role_permissions";
    std::map<std::string, std::map<std::string, std::string>> configData{
        {configSection, {}} // 空配置
    };

    // 设置mock行为
    MOCKER_CPP(&UbseConfModule::GetAllConfigWithPrefix)
        .stubs()
        .with(_, outBound(configData))
        .will(returnValue(UBSE_OK));

    // 执行测试
    auto confModule = std::make_shared<UbseConfModule>();
    auto result = authManager->ParseRoleConfig(confModule, configSection);

    EXPECT_EQ(result, UBSE_ERROR_CONF_INVALID);
    EXPECT_EQ(authManager->roleToObjects_.size(), 1); // 只有admin角色
}

// 部分角色配置无效
TEST_F(TestUbseApiServerAuthManager, ShouldHandlePartiallyInvalidRoleConfigurations)
{
    const std::string configSection = "role_permissions";
    std::map<std::string, std::map<std::string, std::string>> configData{
        {configSection,
         {{"admin", "object1,object2"},      // 无效：配置admin角色
          {"", "object3,object4"},           // 无效：空角色名
          {"user", ""},                      // 无效：空对象列表
          {"all", "all"},                    // 无效：配置all对象
          {"manager", "object6,object7"}}}}; // 有效配置

    // 设置mock行为
    MOCKER_CPP(&UbseConfModule::GetAllConfigWithPrefix)
        .stubs()
        .with(_, outBound(configData))
        .will(returnValue(UBSE_OK));

    // 执行测试
    auto confModule = std::make_shared<UbseConfModule>();
    auto result = authManager->ParseRoleConfig(confModule, configSection);

    // 应该成功解析有效配置，忽略无效配置
    EXPECT_EQ(result, UBSE_OK);

    const auto &roleToObjects = authManager->roleToObjects_;
    EXPECT_EQ(roleToObjects.size(), 2); // 只有admin和manager2个有效角色

    EXPECT_NE(roleToObjects.find("manager"), roleToObjects.end());
    EXPECT_EQ(roleToObjects.find(""), roleToObjects.end());     // 空角色名不应该被添加
    EXPECT_EQ(roleToObjects.find("user"), roleToObjects.end()); // 空对象列表的角色不应该被添加
    EXPECT_EQ(roleToObjects.find("all"), roleToObjects.end());  // 空对象列表的角色不应该被添加
    auto adminIt = roleToObjects.find("admin");
    EXPECT_NE(adminIt, roleToObjects.end());
    EXPECT_NE(adminIt->second.find("all"), adminIt->second.end()); // admin角色应该被配置为all
}

//  对象列表解析异常情况
TEST_F(TestUbseApiServerAuthManager, ShouldHandleMalformedObjectStrings)
{
    const std::string configSection = "role_permissions";
    std::map<std::string, std::map<std::string, std::string>> configData{{configSection,
                                                                          {{"role1", "obj1, obj2,  obj3"}, // 包含空格
                                                                           {"role2", "obj1,,obj3"}, // 包含空元素
                                                                           {"role3", "obj1;obj2;obj3"}, // 错误分隔符
                                                                           {"role4", "obj1,obj2,obj3"}}}}; // 正常格式

    // 设置mock行为
    MOCKER_CPP(&UbseConfModule::GetAllConfigWithPrefix)
        .stubs()
        .with(_, outBound(configData))
        .will(returnValue(UBSE_OK));

    // 执行测试
    auto confModule = std::make_shared<UbseConfModule>();
    auto result = authManager->ParseRoleConfig(confModule, configSection);

    // 应该成功解析有效配置，忽略无效配置
    EXPECT_EQ(result, UBSE_OK);

    const auto &roleToObjects = authManager->roleToObjects_;
    EXPECT_EQ(roleToObjects.size(), 5); // 5个有效角色

    EXPECT_NE(roleToObjects.find("role1"), roleToObjects.end());
    EXPECT_EQ(roleToObjects.at("role1").size(), 3); // role1有3个元素
    EXPECT_NE(roleToObjects.find("role2"), roleToObjects.end());
    EXPECT_EQ(roleToObjects.at("role2").size(), 2); // role2有2个元素
    EXPECT_NE(roleToObjects.find("role3"), roleToObjects.end());
    EXPECT_EQ(roleToObjects.at("role3").size(), 1); // role3有1个元素
    EXPECT_NE(roleToObjects.find("role4"), roleToObjects.end());
    EXPECT_EQ(roleToObjects.at("role4").size(), 3); // role4有3个元素
}

// 正常配置解析
TEST_F(TestUbseApiServerAuthManager, ParseValidUserConfig_Success)
{
    // 准备测试数据
    std::map<std::string, std::map<std::string, std::string>> configData = {
        {UbseApiServerAuthManager::AUTH_USER, {{"user1", "manager"}, {"user2", "operator"}, {"user3", "viewer"}}}};

    // 设置mock行为
    MOCKER_CPP(&UbseConfModule::GetAllConfigWithPrefix)
        .stubs()
        .with(_, outBound(configData))
        .will(returnValue(UBSE_OK));

    // 执行测试
    auto confModule = std::make_shared<UbseConfModule>();
    auto result = authManager->ParseUserConfig(confModule, UbseApiServerAuthManager::AUTH_USER);

    // 验证结果
    EXPECT_EQ(result, UBSE_OK);

    // 验证userToRole映射
    const auto &userToRoles = authManager->userToRole_;
    EXPECT_EQ(userToRoles.size(), 5); // 5个有效角色

    EXPECT_NE(userToRoles.find("user1"), userToRoles.end());
    EXPECT_EQ(userToRoles.at("user1"), "manager");
    EXPECT_NE(userToRoles.find("root"), userToRoles.end());
    EXPECT_EQ(userToRoles.at("root"), "admin");
    EXPECT_NE(userToRoles.find("ubse"), userToRoles.end());
    EXPECT_EQ(userToRoles.at("ubse"), "admin");
}

// 配置读取失败
TEST_F(TestUbseApiServerAuthManager, ConfigReadFailure_ReturnsError)
{
    // 设置mock行为
    MOCKER_CPP(&UbseConfModule::GetAllConfigWithPrefix).stubs().with().will(returnValue(UBSE_ERROR));

    // 执行测试
    auto confModule = std::make_shared<UbseConfModule>();
    auto result = authManager->ParseUserConfig(confModule, UbseApiServerAuthManager::AUTH_USER);

    // 验证结果
    EXPECT_EQ(result, UBSE_ERROR);
}

// 配置节不存在
TEST_F(TestUbseApiServerAuthManager, ConfigSectionNotFound_ReturnsError)
{
    // 准备空配置数据
    std::map<std::string, std::map<std::string, std::string>> emptyConfig;

    // 设置mock行为
    MOCKER_CPP(&UbseConfModule::GetAllConfigWithPrefix)
        .stubs()
        .with(_, outBound(emptyConfig))
        .will(returnValue(UBSE_OK));

    // 执行测试
    auto confModule = std::make_shared<UbseConfModule>();
    auto result = authManager->ParseUserConfig(confModule, UbseApiServerAuthManager::AUTH_USER);

    // 验证结果
    EXPECT_EQ(result, UBSE_ERROR_CONF_INVALID);
}

// 配置节为空
TEST_F(TestUbseApiServerAuthManager, EmptyConfigSection_ReturnsError)
{
    // 准备空节数据
    std::map<std::string, std::map<std::string, std::string>> configWithEmptySection = {
        {UbseApiServerAuthManager::AUTH_USER, {}}};

    // 设置mock行为
    MOCKER_CPP(&UbseConfModule::GetAllConfigWithPrefix)
        .stubs()
        .with(_, outBound(configWithEmptySection))
        .will(returnValue(UBSE_OK));

    // 执行测试
    auto confModule = std::make_shared<UbseConfModule>();
    auto result = authManager->ParseUserConfig(confModule, UbseApiServerAuthManager::AUTH_USER);

    // 验证结果
    EXPECT_EQ(result, UBSE_ERROR_CONF_INVALID);
}

// 包含无效用户配置
TEST_F(TestUbseApiServerAuthManager, MixedValidAndInvalidConfig_SkipsInvalidEntries)
{
    // 准备混合数据（包含有效和无效配置）
    std::map<std::string, std::map<std::string, std::string>> mixedConfig = {
        {UbseApiServerAuthManager::AUTH_USER,
         {
             {"validUser1", "admin"}, // 无效：配置admin角色
             {"", "operator"},        // 无效：空用户名
             {"validUser2", ""},      // 无效：空角色
             {"root", "manager"},     // 无效：配置root和ubse用户
             {"validUser3", "viewer"} // 有效
         }}};

    // 设置mock行为
    MOCKER_CPP(&UbseConfModule::GetAllConfigWithPrefix)
        .stubs()
        .with(_, outBound(mixedConfig))
        .will(returnValue(UBSE_OK));

    // 执行测试
    auto confModule = std::make_shared<UbseConfModule>();
    auto result = authManager->ParseUserConfig(confModule, UbseApiServerAuthManager::AUTH_USER);

    // 验证结果 - 应该成功但跳过无效条目
    EXPECT_EQ(result, UBSE_OK);

    const auto &userToRoles = authManager->userToRole_;
    EXPECT_EQ(userToRoles.size(), 3); // 3个有效角色
    EXPECT_EQ(userToRoles.find("validUser1"), userToRoles.end());
    EXPECT_EQ(userToRoles.find("validUser2"), userToRoles.end());
    EXPECT_NE(userToRoles.find("root"), userToRoles.end());
    EXPECT_NE(userToRoles.find("ubse"), userToRoles.end());
    EXPECT_NE(userToRoles.find("validUser3"), userToRoles.end());
    EXPECT_EQ(userToRoles.at("validUser3"), "viewer");
    EXPECT_EQ(userToRoles.at("root"), "admin");
    EXPECT_EQ(userToRoles.at("ubse"), "admin");
}

//  特殊字符和边界情况
TEST_F(TestUbseApiServerAuthManager, SpecialCharactersAndEdgeCases_HandlesCorrectly)
{
    std::map<std::string, std::map<std::string, std::string>> edgeCaseConfig = {
        {UbseApiServerAuthManager::AUTH_USER,
         {{"user-with-dash", "role-with-dash"},
          {"user_with_underscore", "role_with_underscore"},
          {"user.with.dots", "role.with.dots"},
          {"123numeric", "456numeric"},
          {"UserWithMixedCase", "RoleWithMixedCase"},
          {"very_long_user_name_that_exceeds_normal_length_but_should_still_work", "role"}}}};

    // 设置mock行为
    MOCKER_CPP(&UbseConfModule::GetAllConfigWithPrefix)
        .stubs()
        .with(_, outBound(edgeCaseConfig))
        .will(returnValue(UBSE_OK));

    // 执行测试
    auto confModule = std::make_shared<UbseConfModule>();
    auto result = authManager->ParseUserConfig(confModule, UbseApiServerAuthManager::AUTH_USER);

    // 验证结果
    EXPECT_EQ(result, UBSE_OK);
    const auto &userToRoles = authManager->userToRole_;
    EXPECT_EQ(userToRoles.size(), 8); // 8个有效用户
}

void MockConfInference()
{
    // 准备默认配置数据
    std::map<std::string, std::map<std::string, std::string>> config = {
        {UbseApiServerAuthManager::AUTH_ROLE_DEFAULT,
         {{"default_role", "read,write,execute"}, {"operator", "read,write"}, {"viewer", "read"}}},
        {UbseApiServerAuthManager::AUTH_USER_DEFAULT,
         {{"user1", "default_role"}, {"user2", "operator"}, {"user3", "viewer"}}},
        {UbseApiServerAuthManager::AUTH_ROLE,
         {{"operator", "read,write,execute,delete"}, // 修改operator角色
          {"mem_role", "mem.fd"}}},
        {UbseApiServerAuthManager::AUTH_USER,
         {{"user1", "operator"}, // 只修改user1的角色
          {"user4", "default_role"},
          {"mem_user", "mem_role"},
          {"user5", "nonexistent_role"}}}};

    // 设置mock行为
    std::shared_ptr<UbseConfModule> confModule = std::make_shared<UbseConfModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(confModule));
    MOCKER_CPP(&UbseConfModule::GetAllConfigWithPrefix).stubs().with(_, outBound(config)).will(returnValue(UBSE_OK));
}

// 用户配置覆盖默认配置
TEST_F(TestUbseApiServerAuthManager, UserConfigPartiallyOverridesDefaultConfig)
{
    MockConfInference();
    // 执行测试
    auto result = authManager->LoadAuthConfig();
    EXPECT_EQ(result, UBSE_OK);
    const auto &roleToObjects = authManager->roleToObjects_;
    const auto &userToRoles = authManager->userToRole_;
    EXPECT_EQ(roleToObjects.size(), 5); // 5个有效角色
    EXPECT_EQ(userToRoles.size(), 8);   // 8个有效用户
    EXPECT_NE(roleToObjects.find("operator"), roleToObjects.end());
    EXPECT_EQ(roleToObjects.at("operator").size(), 4); // 用户配置的角色有4个对象
    EXPECT_NE(roleToObjects.find("mem_role"), roleToObjects.end());
    EXPECT_NE(userToRoles.find("user1"), userToRoles.end());
    EXPECT_EQ(userToRoles.at("user1"), "operator");
    EXPECT_NE(userToRoles.find("user4"), userToRoles.end());
    EXPECT_EQ(userToRoles.at("user4"), "default_role");
    EXPECT_NE(userToRoles.find("mem_user"), userToRoles.end());
    EXPECT_EQ(userToRoles.at("mem_user"), "mem_role");
}

// 所有配置加载都失败
TEST_F(TestUbseApiServerAuthManager, AllConfigLoadsFail_StillReturnsSuccess)
{
    // 设置mock行为
    std::shared_ptr<UbseConfModule> confModule = std::make_shared<UbseConfModule>();
    MOCKER_CPP(&context::UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(confModule));
    MOCKER_CPP(&UbseConfModule::GetAllConfigWithPrefix).stubs().will(returnValue(UBSE_ERROR));
    // 执行测试
    auto result = authManager->LoadAuthConfig();
    EXPECT_EQ(result, UBSE_OK);
    const auto &roleToObjects = authManager->roleToObjects_;
    const auto &userToRoles = authManager->userToRole_;
    EXPECT_EQ(roleToObjects.size(), 1); // 1个有效角色
    EXPECT_EQ(userToRoles.size(), 2);   // 2个有效用户
}

// 内置用户拥有所有权限
TEST_F(TestUbseApiServerAuthManager, BuiltinUsersHaveAllPermissions)
{
    MockConfInference();
    // 读取配置
    auto result = authManager->LoadAuthConfig();
    EXPECT_EQ(result, UBSE_OK);

    // 对每个内置用户测试不同的目标对象
    for (const auto &builtinUser : UbseApiServerAuthManager::BUILTIN_USERS) {
        EXPECT_TRUE(authManager->CheckPermission(builtinUser, "any_object"));
        EXPECT_TRUE(authManager->CheckPermission(builtinUser, "nonexistent_object"));
        EXPECT_TRUE(authManager->CheckPermission(builtinUser, ""));
        EXPECT_TRUE(authManager->CheckPermission(builtinUser, "special.object.with.dots"));
    }
}

// 用户有具体对象权限
TEST_F(TestUbseApiServerAuthManager, UserWithSpecificObjectPermission)
{
    MockConfInference();
    // 读取配置
    auto result = authManager->LoadAuthConfig();
    EXPECT_EQ(result, UBSE_OK);

    // user1是operator角色，有read,write,execute,delete权限
    EXPECT_TRUE(authManager->CheckPermission("user1", "read"));
    EXPECT_TRUE(authManager->CheckPermission("user1", "write"));
    EXPECT_TRUE(authManager->CheckPermission("user1", "execute"));
    EXPECT_TRUE(authManager->CheckPermission("user1", "delete"));

    // 没有权限的对象
    EXPECT_FALSE(authManager->CheckPermission("user1", "object1"));
    EXPECT_FALSE(authManager->CheckPermission("user1", "object2"));
}

// 用户存在但角色不存在
TEST_F(TestUbseApiServerAuthManager, UserExistsButRoleNotFound)
{
    MockConfInference();
    // 读取配置
    auto result = authManager->LoadAuthConfig();
    EXPECT_EQ(result, UBSE_OK);

    EXPECT_FALSE(authManager->CheckPermission("user5", "object1"));
    EXPECT_FALSE(authManager->CheckPermission("user5", "any_object"));
}

// 用户不存在
TEST_F(TestUbseApiServerAuthManager, UserNotFound)
{
    MockConfInference();
    // 读取配置
    auto result = authManager->LoadAuthConfig();
    EXPECT_EQ(result, UBSE_OK);
    EXPECT_FALSE(authManager->CheckPermission("nonexistent_user", "object1"));
    EXPECT_FALSE(authManager->CheckPermission("unknown", "any_object"));
    EXPECT_FALSE(authManager->CheckPermission("", "object1")); // 空用户名
}

// 目标对象为空或特殊字符
TEST_F(TestUbseApiServerAuthManager, EmptyOrSpecialCharacterTargetObject)
{
    MockConfInference();
    // 读取配置
    auto result = authManager->LoadAuthConfig();
    EXPECT_EQ(result, UBSE_OK);
    // 空目标对象
    EXPECT_FALSE(authManager->CheckPermission("user1", ""));

    // 特殊字符目标对象
    EXPECT_FALSE(authManager->CheckPermission("user1", "object with spaces"));
    EXPECT_FALSE(authManager->CheckPermission("user1", "object/with/slashes"));
    EXPECT_FALSE(authManager->CheckPermission("user1", "object.with.dots"));

    // 特殊字符用户名
    EXPECT_FALSE(authManager->CheckPermission("user@domain", "object1"));
    EXPECT_FALSE(authManager->CheckPermission("user-name", "object1"));
}

// 大小写敏感性测试
TEST_F(TestUbseApiServerAuthManager, CaseSensitivity)
{
    MockConfInference();
    // 读取配置
    auto result = authManager->LoadAuthConfig();
    EXPECT_EQ(result, UBSE_OK);
    // 权限检查是大小写敏感的
    EXPECT_FALSE(authManager->CheckPermission("user1", "READ")); // 大写
    EXPECT_FALSE(authManager->CheckPermission("user1", "Read")); // 混合大小写
    EXPECT_TRUE(authManager->CheckPermission("user1", "read"));  // 正确大小写
}
} // namespace ubse::ut::api::server