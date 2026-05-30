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

#include <grp.h>
#include <pwd.h>
#include <securec.h>
#include <ubse_logger.h>
#include <ubse_str_util.h>
#include <array>
#include <csignal>
#include <fstream>
#include <memory>
#include <regex>

#include "ubse_error.h"
#include "ubse_logger.h"

namespace ubse::utils {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::common::def;

UbseResult UbseOsUtil::GetUserNameById(uid_t uid, std::string &userName)
{
    struct passwd pwd;
    struct passwd *result = nullptr;

    // 获取推荐的缓冲区大小
    long bufSize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (bufSize == -1) {
        bufSize = 16384; // 兜底值16384
    }
    std::vector<char> buffer(static_cast<size_t>(bufSize));

    auto ret = getpwuid_r(uid, &pwd, buffer.data(), buffer.size(), &result);
    if (ret != 0 || result == nullptr) {
        return UBSE_ERROR;
    }
    userName = std::string(pwd.pw_name);
    return UBSE_OK;
}

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
    // 参数校验
    if (username.empty()) {
        return UBSE_ERROR_INVAL;
    }

    struct passwd pwd;
    struct passwd *result = nullptr;

    // 获取推荐 buffer 大小
    long bufSize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (bufSize == -1) {
        bufSize = 16384; // 兜底值16384
    }
    std::vector<char> buffer(static_cast<size_t>(bufSize));

    int ret = getpwnam_r(username.c_str(), &pwd, buffer.data(), buffer.size(), &result);
    if (ret != 0 || result == nullptr) {
        return UBSE_ERROR;
    }
    uid = pwd.pw_uid;
    return UBSE_OK;
}

UbseResult UbseOsUtil::GetNumaIdByPid(const uint64_t &pid, uint32_t &numaId)
{
    auto path = "/proc/" + std::to_string(pid) + "/numa_maps";
    std::regex keywordRegexForMigration("(/qemu.*N(\\d+)=(\\d+))");
    std::regex keywordRegexForFastRR("/qemu.*ram-node.*N\\d+=\\d+");
    std::ifstream file(path);
    if (!file.is_open()) {
        UBSE_LOG_ERROR << "open " << path << " failed, " << std::strerror(errno);
        return UBSE_ERROR;
    }
    auto matchNumaId = [&file, &numaId, pid](const std::regex& keywordRegex) -> bool {
        std::string line;
        std::smatch n1Match;
        // 操作系统5.10.0-136.12.0.86.x1.eulerx_a2
        // 数据在同一行: N0=15232 N5=669 表示numa=0使用了15232个2M大页,numa=5使用了669个2M大页
        // 操作系统openEuler22版本 N0=15232和N5=669 数据不在同一行
        std::regex n1Regex("N(\\d+)=(\\d+)");
        while (std::getline(file, line)) {
            if (std::regex_search(line, keywordRegex) && std::regex_search(line, n1Match, n1Regex)) {
                UBSE_LOG_INFO << "get numaId from line=" << line << ", by pid=" << pid;
                auto ret = ConvertStrToUint32(n1Match[1], numaId);
                if (ret != UBSE_OK) {
                    UBSE_LOG_ERROR << "Convert numaId=" << n1Match[1] << " failed, by pid=" << pid;
                    return false;
                }
                UBSE_LOG_INFO << "get numaId=" << numaId << ", by pid=" << pid;
                return true;
            }
        }
        return false;
    };
    if (matchNumaId(keywordRegexForFastRR)) {
        return UBSE_OK;
    }
    // 重置文件指针到开头
    file.clear();
    file.seekg(0);
    if (matchNumaId(keywordRegexForMigration)) {
        return UBSE_OK;
    }
    UBSE_LOG_ERROR << "get numaId failed, by pid=" << pid;
    return UBSE_ERROR;
}
} // namespace ubse::utils