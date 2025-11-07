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

#include "ubse_os_util.h"

#include <pwd.h>
#include <memory>
#include <array>
#include <securec.h>

#include "ubse_error.h"

namespace ubse::utils {
using namespace ubse::common::def;

UbseResult UbseOsUtil::Exec(const std::string &cmd, std::string &res)
{
    std::array<char, NO_128> buffer{};
    res.clear();
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        return UBSE_ERROR;
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        res += buffer.data();
    }
    int status = pclose(pipe.release());
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        // 命令执行失败，处理错误
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseOsUtil::GetUidByName(const std::string &username, uid_t &uid)
{
    struct passwd *pwd = getpwnam(username.c_str());
    if (pwd == nullptr) {
        return UBSE_ERROR;
    }
    uid = pwd->pw_uid;
    // 清空密码
    if (pwd->pw_passwd != nullptr) {
        memset_s(pwd->pw_passwd, strlen(pwd->pw_passwd), 0, strlen(pwd->pw_passwd));
    }
    return UBSE_OK;
}
} // namespace ubse::utils