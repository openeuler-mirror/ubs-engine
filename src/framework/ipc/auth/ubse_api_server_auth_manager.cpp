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

#include "ubse_api_server_auth_manager.h"

#include <algorithm>
#include <iostream>
#include <sstream>

#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"

namespace api::server {
using namespace ubse::log;

UBSE_DEFINE_THIS_MODULE("ubse");

constexpr uint32_t MAX_OBJECT_STR = 1024;

// 配置section
const std::string UbseApiServerAuthManager::AUTH_USER_DEFAULT = "auth.user.default";
const std::string UbseApiServerAuthManager::AUTH_ROLE_DEFAULT = "auth.role.default";
const std::string UbseApiServerAuthManager::AUTH_USER = "auth.user";
const std::string UbseApiServerAuthManager::AUTH_ROLE = "auth.role";

// 内置常量定义
const std::string UbseApiServerAuthManager::ADMIN_ROLE = "admin";
const std::string UbseApiServerAuthManager::ALL_OBJECT = "all";
const std::vector<std::string> UbseApiServerAuthManager::BUILTIN_USERS = {"ubse", "root"};

UbseApiServerAuthManager& UbseApiServerAuthManager::GetInstance()
{
    static UbseApiServerAuthManager authManager;
    return authManager;
}

UbseApiServerAuthManager::UbseApiServerAuthManager()
{
    InitializeBuiltinAuth();
}

void UbseApiServerAuthManager::InitializeBuiltinAuth()
{
    // 设置内置用户到管理员角色的映射
    for (const auto& user : BUILTIN_USERS) {
        userToRole_[user] = ADMIN_ROLE;
    }

    // 设置管理员角色拥有所有对象权限
    roleToObjects_[ADMIN_ROLE] = {ALL_OBJECT};
}

UbseResult UbseApiServerAuthManager::LoadAuthConfig()
{
    auto confModule = ubse::context::UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (confModule == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    auto ret = ParseRoleConfig(confModule, AUTH_ROLE_DEFAULT);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to read the default role configuration, " << FormatRetCode(ret);
    }
    ret = ParseUserConfig(confModule, AUTH_USER_DEFAULT);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to read the default user configuration, " << FormatRetCode(ret);
    }
    ret = ParseRoleConfig(confModule, AUTH_ROLE);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to read the role configuration, " << FormatRetCode(ret);
    }
    ret = ParseUserConfig(confModule, AUTH_USER);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to read the user configuration, " << FormatRetCode(ret);
    }
    return UBSE_OK;
}

static inline std::string trim(const std::string& s)
{
    auto front = std::find_if_not(s.begin(), s.end(), [](int c) { return std::isspace(c); });
    auto back = std::find_if_not(s.rbegin(), s.rend(), [](int c) { return std::isspace(c); }).base();
    return (back <= front ? std::string() : std::string(front, back));
}

std::vector<std::string> UbseApiServerAuthManager::ParseObjects(const std::string& objectsStr)
{
    std::vector<std::string> objects{};
    std::istringstream iss(objectsStr);
    std::string object;
    while (std::getline(iss, object, ',')) {
        // 移除对象名的空白字符
        object = trim(object);
        if (!object.empty()) {
            objects.push_back(object);
        }
    }
    return objects;
}

uint32_t UbseApiServerAuthManager::ParseRoleConfig(const std::shared_ptr<UbseConfModule>& confModule,
                                                   const std::string& configSection)
{
    std::map<std::string, std::map<std::string, std::string>> roleConfigVals{};
    auto ret = confModule->GetAllConfigWithPrefix(configSection, roleConfigVals);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to read role configuration for section: " << configSection << ", "
                      << FormatRetCode(ret);
        return ret;
    }
    auto it = roleConfigVals.find(configSection);
    if (it == roleConfigVals.end() || it->second.empty()) {
        UBSE_LOG_WARN << "No configuration found or empty section: " << configSection;
        return UBSE_ERROR_CONF_INVALID;
    }
    const auto& configVals = it->second;

    for (const auto& [role, objectsStr] : configVals) {
        if (objectsStr.size() >= MAX_OBJECT_STR) {
            UBSE_LOG_ERROR << "objectsStr size exceeded MAX_OBJECT_STR. Current size: " << objectsStr.size()
                           << " , Max size: " << MAX_OBJECT_STR;
            continue;
        }
        std::vector<std::string> objects = ParseObjects(objectsStr);
        if (IsValidRoleConfig(role, objects)) {
            roleToObjects_[role] = std::unordered_set<std::string>(objects.begin(), objects.end());
        } else {
            UBSE_LOG_ERROR << "Invalid configuration detected for role: " << role;
        }
    }
    return UBSE_OK;
}

uint32_t UbseApiServerAuthManager::ParseUserConfig(const std::shared_ptr<UbseConfModule>& confModule,
                                                   const std::string& configSection)
{
    std::map<std::string, std::map<std::string, std::string>> userConfigVals{};
    auto ret = confModule->GetAllConfigWithPrefix(configSection, userConfigVals);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to read user configuration for section: " << configSection << ", "
                      << FormatRetCode(ret);
        return ret;
    }
    auto it = userConfigVals.find(configSection);
    if (it == userConfigVals.end() || it->second.empty()) {
        UBSE_LOG_WARN << "No configuration found or empty section: " << configSection;
        return UBSE_ERROR_CONF_INVALID;
    }
    const auto& configVals = it->second;
    for (const auto& [user, role] : configVals) {
        if (IsValidUserConfig(user, role)) {
            userToRole_[user] = role;
        } else {
            UBSE_LOG_ERROR << "Invalid configuration detected for user: " << user;
        }
    }
    return UBSE_OK;
}

bool UbseApiServerAuthManager::IsValidUserConfig(const std::string& user, const std::string& role)
{
    // 检查是否包含内置用户
    if (std::find(BUILTIN_USERS.begin(), BUILTIN_USERS.end(), user) != BUILTIN_USERS.end()) {
        return false;
    }

    // 检查是否包含管理员角色
    if (role == ADMIN_ROLE) {
        return false;
    }

    return !user.empty() && !role.empty();
}

bool UbseApiServerAuthManager::IsValidRoleConfig(const std::string& role, const std::vector<std::string>& objects)
{
    // 检查是否包含管理员角色
    if (role == ADMIN_ROLE) {
        return false;
    }

    // 检查自定义角色是否引用all对象
    if (std::find(objects.begin(), objects.end(), ALL_OBJECT) != objects.end()) {
        return false;
    }

    return !role.empty() && !objects.empty();
}

bool UbseApiServerAuthManager::CheckPermission(const std::string& username, const std::string& targetObject)
{
    UBSE_LOG_INFO << "Starting permission check, user=" << username << ", target object=" << targetObject;
    // 查找用户角色
    auto userIt = userToRole_.find(username);
    if (userIt == userToRole_.end()) {
        UBSE_LOG_ERROR << "User " << username << " not found";
        return false; // 用户未找到
    }

    const std::string& role = userIt->second;
    UBSE_LOG_DEBUG << "User " << username << " has role " << role;

    // 查找角色对应的对象集合
    auto roleIt = roleToObjects_.find(role);
    if (roleIt == roleToObjects_.end()) {
        UBSE_LOG_ERROR << "Role " << role << " is not configured with any object permissions";
        return false; // 角色未找到
    }

    const auto& objects = roleIt->second;

    // 检查是否拥有all权限或具体对象权限
    return objects.find(ALL_OBJECT) != objects.end() || objects.find(targetObject) != objects.end();
}

void UbseApiServerAuthManager::clear()
{
    // 清除非内置的配置
    userToRole_.clear();
    roleToObjects_.clear();

    // 重新初始化内置权限
    InitializeBuiltinAuth();
}

bool UbseApiServerAuthManager::CheckPermission(const std::string& username, uint16_t moduleCode, uint16_t opCode)
{
    UBSE_LOG_INFO << "Attempting to check permissions for user=" << username << ", moduleCode=" << moduleCode
                  << ", opCode=" << opCode;
    // 首先检查是否是内置用户
    if (std::find(BUILTIN_USERS.begin(), BUILTIN_USERS.end(), username) != BUILTIN_USERS.end()) {
        UBSE_LOG_DEBUG << "User " << username << " is built-in user, automatically granted all permissions";
        return true; // 内置用户拥有所有权限
    }

    auto object = objectManager_.GetObjectString(moduleCode, opCode);
    if (object.empty()) {
        UBSE_LOG_ERROR << "No object mapping exists for moduleCode=" << moduleCode << ", opCode=" << opCode;
        return false;
    }
    // 临时兼容处理, 外部接口默认配置为no_limit不限制权限
    if (object == "no_limit") {
        UBSE_LOG_DEBUG << "No restrictions on permissions";
        return true;
    }
    return CheckPermission(username, object);
}

void UbseApiServerAuthManager::AddObjectMapping(uint16_t moduleCode, uint16_t opCode, const std::string& object)
{
    objectManager_.AddObjectMapping(moduleCode, opCode, object);
}
} // namespace api::server
