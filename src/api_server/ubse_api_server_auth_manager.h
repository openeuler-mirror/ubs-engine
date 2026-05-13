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

#ifndef UBSE_API_SERVER_AUTH_MANAGER_H
#define UBSE_API_SERVER_AUTH_MANAGER_H

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ubse_api_server_object_manager.h"
#include "ubse_common_def.h"
#include "ubse_conf_module.h"

namespace api::server {
using namespace ubse::config;
using namespace ubse::common::def;

class UbseApiServerAuthManager {
public:
    UbseApiServerAuthManager();

    ~UbseApiServerAuthManager() = default;

    // 加载权限配置文件
    UbseResult LoadAuthConfig();

    // 检查用户是否有权限访问对象
    bool CheckPermission(const std::string& username, uint16_t moduleCode, uint16_t opCode);

    void AddObjectMapping(uint16_t moduleCode, uint16_t opCode, const std::string& object);

    // 清理所有加载的配置
    void clear();

private:
    // 初始化内置权限
    void InitializeBuiltinAuth();

    // 验证配置项
    static bool IsValidUserConfig(const std::string& user, const std::string& role);
    static bool IsValidRoleConfig(const std::string& role, const std::vector<std::string>& objects);

    // 检查用户是否有权限访问对象
    bool CheckPermission(const std::string& username, const std::string& targetObject);

    uint32_t ParseRoleConfig(const std::shared_ptr<UbseConfModule>& confModule, const std::string& configSection);
    uint32_t ParseUserConfig(const std::shared_ptr<UbseConfModule>& confModule, const std::string& configSection);
    static std::vector<std::string> ParseObjects(const std::string& objectsStr);

    // 内置常量
    static const std::string ADMIN_ROLE;
    static const std::string ALL_OBJECT;
    static const std::vector<std::string> BUILTIN_USERS;

    // 配置section
    static const std::string AUTH_USER_DEFAULT;
    static const std::string AUTH_ROLE_DEFAULT;
    static const std::string AUTH_USER;
    static const std::string AUTH_ROLE;

    // 存储结构
    std::unordered_map<std::string, std::string> userToRole_;                        // 用户->角色映射
    std::unordered_map<std::string, std::unordered_set<std::string>> roleToObjects_; // 角色->对象集合映射

    UbseApiServerObjectManager objectManager_{};
};
} // namespace api::server

#endif // UBSE_API_SERVER_AUTH_MANAGER_H
