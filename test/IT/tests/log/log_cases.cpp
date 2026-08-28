/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "log_cases.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <regex>
#include <string>
#include <thread>
#include <vector>

#include "ubse_ipc_log.h"
#include "it_assertion.h"
#include "it_console_log.h"
#include "it_wait_helper.h"
#include "ubs_engine_log.h"

namespace ubse::it::tests::log {

namespace {

// 修改配置文件中的 key=value（仅替换未注释的行，保留其他内容不变）
bool SetConfigValue(const std::string& cfgPath, const std::string& key, const std::string& value)
{
    std::ifstream ifs(cfgPath);
    if (!ifs.is_open()) {
        return false;
    }
    std::vector<std::string> lines;
    std::string line;
    const std::string prefix = key + "=";
    while (std::getline(ifs, line)) {
        if (line.rfind(prefix, 0) == 0) {
            line = prefix + value;
        }
        lines.push_back(line);
    }
    ifs.close();

    std::ofstream ofs(cfgPath, std::ios::trunc);
    if (!ofs.is_open()) {
        return false;
    }
    for (const auto& l : lines) {
        ofs << l << "\n";
    }
    ofs.close();
    return true;
}

// 向文件追加指定字节数的数据（模拟日志量增长，触发超过文件大小上限）
void AppendBytes(const std::string& path, size_t bytes)
{
    std::ofstream ofs(path, std::ios::app | std::ios::binary);
    const std::string chunk(1024, 'R');
    size_t written = 0;
    while (written < bytes) {
        size_t toWrite = std::min(chunk.size(), bytes - written);
        ofs.write(chunk.data(), static_cast<std::streamsize>(toWrite));
        written += toWrite;
    }
    ofs.flush();
    ofs.close();
}

// 从日志文件路径提取模块名（如 /path/log/ubse.log -> ubse）
std::string ModuleNameOf(const std::string& logFilePath)
{
    return std::filesystem::path(logFilePath).stem().string();
}

// 判断是否为指定模块的日志绕接压缩文件 <module>_YYYYMMDD_HHMMSS_XXX.tar.gz。
// 注意：daemon 中多个模块（ubse_mem_scheduler/ubse_fault/ubse_cli 等）共用同一日志绕接配置，
// 其绕接文件同样以 ubse_ 开头、.tar.gz 结尾，必须严格匹配"模块名+数字时间戳"避免误统计。
bool IsRotationFileOf(const std::string& name, const std::string& module)
{
    const std::string prefix = module + "_";
    if (name.rfind(prefix, 0) != 0) {
        return false;
    }
    static const std::regex suffixPattern(R"(\d{8}_\d{6}_\d{3}\.tar\.gz)");
    return std::regex_match(name.substr(prefix.size()), suffixPattern);
}

// 统计日志目录下指定模块绕接压缩文件的数量
size_t CountRotationFiles(const std::string& dir, const std::string& module)
{
    size_t count = 0;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        if (IsRotationFileOf(entry.path().filename().string(), module)) {
            ++count;
        }
    }
    return count;
}

// 返回日志目录下指定模块所有绕接压缩文件名（按名称排序，最早的在前）
std::vector<std::string> ListRotationFiles(const std::string& dir, const std::string& module)
{
    std::vector<std::string> files;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        const std::string name = entry.path().filename().string();
        if (IsRotationFileOf(name, module)) {
            files.push_back(name);
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

// 删除日志目录下指定模块所有绕接压缩文件（保证绕接数量统计准确）
void ClearRotationFiles(const std::string& dir, const std::string& module)
{
    for (const auto& name : ListRotationFiles(dir, module)) {
        std::error_code ec;
        std::filesystem::remove(dir + "/" + name, ec);
    }
}

// 解析日志行开头的 "YYYY-MM-DD HH:MM:SS.mmm" 时间戳，解析失败返回 false
bool TryParseLogTime(const std::string& line, std::time_t& out)
{
    int year = 0;
    int mon = 0;
    int day = 0;
    int hour = 0;
    int min = 0;
    int sec = 0;
    // 日志格式为 "[YYYY-MM-DD HH:MM:SS.mmm+HH:MM][LEVEL]..."，时间戳以 '[' 开头
    if (sscanf(line.c_str(), " [%d-%d-%d %d:%d:%d", &year, &mon, &day, &hour, &min, &sec) != 6) {
        return false;
    }
    struct tm t {
    };
    t.tm_year = year - 1900;
    t.tm_mon = mon - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = min;
    t.tm_sec = sec;
    t.tm_isdst = -1;
    out = mktime(&t);
    return out != static_cast<std::time_t>(-1);
}

// 日志文件中最后一条有效日志的时间戳，无有效日志返回 0
std::time_t LastLogTime(const std::string& path)
{
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        return 0;
    }
    std::time_t last = 0;
    std::string line;
    while (std::getline(ifs, line)) {
        std::time_t t = 0;
        if (TryParseLogTime(line, t) && t > last) {
            last = t;
        }
    }
    return last;
}

// 日志文件中是否存在时间戳晚于指定时间的日志（证明绕接后日志正常打印）
bool HasLogEntryAfter(const std::string& path, std::time_t after)
{
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        return false;
    }
    std::string line;
    while (std::getline(ifs, line)) {
        std::time_t t = 0;
        if (TryParseLogTime(line, t) && t > after) {
            return true;
        }
    }
    return false;
}

// 检查文件权限是否为 640（owner rw / group r / others 无任何权限）
bool IsPerm640(const std::string& path)
{
    std::error_code ec;
    const std::filesystem::file_status st = std::filesystem::status(path, ec);
    if (ec) {
        return false;
    }
    using P = std::filesystem::perms;
    const P p = st.permissions();
    return ((p & P::owner_read) != P::none) && ((p & P::owner_write) != P::none) && ((p & P::owner_exec) == P::none) &&
           ((p & P::group_read) != P::none) && ((p & P::group_write) == P::none) && ((p & P::group_exec) == P::none) &&
           ((p & P::others_read) == P::none) && ((p & P::others_write) == P::none) && ((p & P::others_exec) == P::none);
}

// ==================== 日志回调测试辅助 ====================

// 自定义日志处理函数的输出文件路径（回调为普通函数指针，不可带捕获，经文件级变量传递）
std::string g_simpleLogPath;

// 自定义日志处理函数：将 SDK 日志按自定义格式写入文件
// 自定义格式：[simple_log_handler] {log_level.name}：{message}
void SimpleLogHandler(uint32_t level, const char* message)
{
    static constexpr std::array<const char*, 5> levelNames = {"DEBUG", "INFO", "WARN", "ERROR", "CRIT"};
    if (level >= levelNames.size()) {
        return;
    }
    static std::mutex fileMutex;
    std::lock_guard<std::mutex> lock(fileMutex);
    std::ofstream ofs(g_simpleLogPath, std::ios::app);
    if (!ofs.is_open()) {
        return;
    }
    ofs << "[simple_log_handler] " << levelNames[level] << "：" << (message != nullptr ? message : "") << "\n";
}

} // namespace

void RunLogRotationTest(ubse::it::infra::ItCluster& cluster)
{
    auto& node = cluster.GetNode("1");
    auto& cliInvoker = cluster.GetCliInvoker("1");
    const std::string cfgPath = node.GetConfigFilePath();
    const std::string logPath = node.GetLogFilePath();
    const std::string logDir = node.GetSpec().LogDir();
    const std::string module = ModuleNameOf(logPath);

    // 1. 修改配置 log.max.fileSize=2 (2MB)，重启节点使配置生效
    EXPECT_TRUE(SetConfigValue(cfgPath, "log.max.fileSize", "2")) << "修改 log.max.fileSize 配置应成功";
    ASSERT_IT_OK(cluster.RestartNode("1", true, 30000)) << "重启节点使日志绕接配置生效应成功";

    // 2. 记录绕接前日志打印时间
    const std::time_t preRotateTime = LastLogTime(logPath);
    IT_LOG_INFO << "Log rotation: pre-rotation last log time=" << preRotateTime << ", log=" << logPath;

    // 3. 向日志追加 >2MB 数据，使 ubse.log 超过 log.max.fileSize
    constexpr size_t mB = 1024 * 1024;
    AppendBytes(logPath, 3 * mB);
    IT_LOG_INFO << "Log rotation: appended 3MB to " << logPath;

    // 通过 CLI 触发 daemon 写入日志（每次 UDS 连接都会打印 INFO 日志），从而触发绕接
    cliInvoker.ExecCli("display node");
    cliInvoker.ExecCli("display node");

    // 4. 等待绕接：有新的绕接压缩文件生成则绕接成功（仅统计本模块，排除其他模块的绕接文件）
    auto ret = ubse::it::infra::ItWaitHelper::WaitForCondition(
        [&logDir, &module]() -> bool { return CountRotationFiles(logDir, module) >= 1; },
        ubse::it::infra::ItWaitHelper::DEFAULT_ELECTION_TIMEOUT_MS);
    ASSERT_IT_OK(ret) << "超过 log.max.fileSize 后应生成新的绕接压缩文件";

    const size_t rotatedCount = CountRotationFiles(logDir, module);
    IT_LOG_INFO << "Log rotation: generated " << rotatedCount << " rotation file(s) in " << logDir;

    // 5. 验证绕接后日志正常打印：再次触发 daemon 写日志，
    //    新 ubse.log 中应出现时间戳晚于绕接前最后一条日志的新日志
    cliInvoker.ExecCli("display node");
    ret = ubse::it::infra::ItWaitHelper::WaitForCondition(
        [&logPath, &preRotateTime]() -> bool { return HasLogEntryAfter(logPath, preRotateTime); },
        ubse::it::infra::ItWaitHelper::DEFAULT_ELECTION_TIMEOUT_MS);
    ASSERT_IT_OK(ret) << "绕接后日志应继续正常打印";

    const std::time_t postRotateTime = LastLogTime(logPath);
    IT_LOG_INFO << "Log rotation: post-rotation last log time=" << postRotateTime << ", pre-rotation=" << preRotateTime;
    EXPECT_GE(postRotateTime, preRotateTime) << "绕接后日志打印时间应不早于绕接前";

    // 6. 恢复配置 log.max.fileSize=20 并重启进程
    EXPECT_TRUE(SetConfigValue(cfgPath, "log.max.fileSize", "20")) << "恢复 log.max.fileSize 配置应成功";
    ASSERT_IT_OK(cluster.RestartNode("1", true, 30000)) << "恢复配置并重启应成功";

    IT_LOG_INFO << "Log rotation test passed";
}

// 日志绕接文件数量上限测试（单节点场景）：
//   1) 修改配置 log.max.fileSize=2 (2MB)、log.fileNums=3，重启节点使配置生效；
//   2) 连续触发 4 次日志绕接；
//   3) 验证绕接文件数量始终不超过 log.fileNums(3)，且绕接 4 次后最早生成的日志包已被删除替换；
//   4) 恢复配置 log.max.fileSize=20、log.fileNums=20，重启节点。
void RunLogRotationFileNumTest(ubse::it::infra::ItCluster& cluster)
{
    auto& node = cluster.GetNode("1");
    auto& cliInvoker = cluster.GetCliInvoker("1");
    const std::string cfgPath = node.GetConfigFilePath();
    const std::string logPath = node.GetLogFilePath();
    const std::string logDir = node.GetSpec().LogDir();
    const std::string module = ModuleNameOf(logPath);

    // 1. 修改配置 log.max.fileSize=2 (2MB)、log.fileNums=3，重启节点使配置生效
    EXPECT_TRUE(SetConfigValue(cfgPath, "log.max.fileSize", "2")) << "修改 log.max.fileSize 配置应成功";
    EXPECT_TRUE(SetConfigValue(cfgPath, "log.fileNums", "3")) << "修改 log.fileNums 配置应成功";
    ASSERT_IT_OK(cluster.RestartNode("1", true, 30000)) << "重启节点使日志绕接配置生效应成功";

    // 清理历史绕接文件，保证绕接文件数量统计准确
    ClearRotationFiles(logDir, module);

    constexpr size_t MB = 1024 * 1024;

    // 2. 连续触发 3 次绕接，日志目录应累计 3 个绕接文件。
    //    绕接压缩文件名形如 <module>_YYYYMMDD_HHMMSS_XXX.tar.gz（秒级时间戳）。绕接删除最早
    //    文件后，剩余文件会重命名补位（序号前移），若多次绕接落在同一秒内，文件名会被完全
    //    复用，导致无法通过文件名判断最早文件是否被删除。因此每轮注入前等待 >1s，确保各次
    //    绕接的时间戳互不相同，文件名即可唯一标识物理文件。
    for (int i = 1; i <= 3; ++i) {
        if (i > 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1200));
        }
        AppendBytes(logPath, 3 * MB);
        IT_LOG_INFO << "Log rotation fileNums: round " << i << " appended 3MB to " << logPath;
        cliInvoker.ExecCli("display node");
        auto ret = ubse::it::infra::ItWaitHelper::WaitForCondition(
            [&logDir, &module, i]() -> bool { return CountRotationFiles(logDir, module) >= static_cast<size_t>(i); },
            ubse::it::infra::ItWaitHelper::DEFAULT_ELECTION_TIMEOUT_MS);
        ASSERT_IT_OK(ret) << "第 " << i << " 次绕接后应累计 " << i << " 个绕接文件";
    }

    // 3. 记录第 3 次绕接后最早的绕接文件（即第 1 次绕接生成的日志包）。
    //    由于各轮绕接时间戳不同，文件名可唯一标识物理文件。
    auto files = ListRotationFiles(logDir, module);
    ASSERT_FALSE(files.empty()) << "第 3 次绕接后应存在绕接文件";
    const std::string oldestFile = logDir + "/" + files.front();
    IT_LOG_INFO << "Log rotation fileNums: oldest rotation file after round 3 = " << oldestFile;

    // 4. 触发第 4 次绕接：超过 log.fileNums(3) 后最早生成的日志包应被删除，仅保留 3 个。
    //    同样等待 >1s 保证第 4 次绕接的时间戳与之前不同，文件名不被复用。
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    AppendBytes(logPath, 3 * MB);
    IT_LOG_INFO << "Log rotation fileNums: round 4 appended 3MB to " << logPath;
    cliInvoker.ExecCli("display node");
    auto ret = ubse::it::infra::ItWaitHelper::WaitForCondition(
        [&logDir, &module, &oldestFile]() -> bool {
            std::error_code ec;
            return CountRotationFiles(logDir, module) == 3 && !std::filesystem::exists(oldestFile, ec);
        },
        ubse::it::infra::ItWaitHelper::DEFAULT_ELECTION_TIMEOUT_MS);
    // 输出第 4 次绕接后的实际状态，便于定位问题
    IT_LOG_INFO << "Log rotation fileNums: after round-4 wait, count=" << CountRotationFiles(logDir, module)
                << ", oldestExists=" << std::filesystem::exists(oldestFile) << ", oldest=" << oldestFile;
    for (const auto& f : ListRotationFiles(logDir, module)) {
        IT_LOG_INFO << "Log rotation fileNums:   rotation file: " << f;
    }
    ASSERT_IT_OK(ret) << "绕接超过 log.fileNums 后最早生成的日志包应被删除，绕接文件数量应保持为 3";

    // 5. 验证最终绕接文件数量不超过 log.fileNums(3)
    const size_t rotatedCount = CountRotationFiles(logDir, module);
    IT_LOG_INFO << "Log rotation fileNums: final rotation file count = " << rotatedCount;
    EXPECT_EQ(rotatedCount, 3u) << "绕接四次后应仅保留 3 个绕接文件";
    EXPECT_LE(rotatedCount, 3u) << "绕接文件数量不应超过 log.fileNums(3)";

    // 6. 恢复配置 log.max.fileSize=20、log.fileNums=20 并重启进程
    EXPECT_TRUE(SetConfigValue(cfgPath, "log.max.fileSize", "20")) << "恢复 log.max.fileSize 配置应成功";
    EXPECT_TRUE(SetConfigValue(cfgPath, "log.fileNums", "20")) << "恢复 log.fileNums 配置应成功";
    ASSERT_IT_OK(cluster.RestartNode("1", true, 30000)) << "恢复配置并重启应成功";

    IT_LOG_INFO << "Log rotation fileNums test passed";
}

// 日志文件格式校验测试（单节点场景）：
//   1) 验证默认日志目录存在日志文件（ubse.log 非空）；
//   2) 验证日志目录下所有 .log 文件读写权限为 640（owner rw / group r / others 无）；
//   3) 获取最近 50h 的日志，逐条目校验日志格式：
//      - 标准格式（com_pattern）：[时间戳][等级][进程][线程][traceid][文件:函数:行号] 消息；
//      - 第三方组件格式（third_pattern）：[时间戳][等级][进程][线程][traceid][文件:行号] 消息
//        （如 HCOM 组件，无函数名字段）；
//      - 日志等级（level_pattern）必须在合法列表 WARN/DEBUG/INFO/ERROR/CRIT 内。
void RunLogFormatTest(ubse::it::infra::ItCluster& cluster)
{
    auto& node = cluster.GetNode("1");
    const std::string logDir = node.GetSpec().LogDir();
    const std::string logPath = node.GetLogFilePath();

    // 1. 验证默认日志目录存在且主日志文件存在非空
    ASSERT_TRUE(std::filesystem::exists(logDir)) << "默认日志目录应存在: " << logDir;
    ASSERT_TRUE(std::filesystem::exists(logPath)) << "主日志文件应存在: " << logPath;
    const auto logSize = std::filesystem::file_size(logPath);
    IT_LOG_INFO << "Log format: log file " << logPath << ", size=" << logSize;
    ASSERT_GT(logSize, 0u) << "主日志文件应非空";

    // 2. 验证日志目录下所有 .log 文件权限为 640
    std::vector<std::string> logFiles;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(logDir, ec)) {
        if (ec) {
            break;
        }
        if (entry.is_regular_file() && entry.path().extension() == ".log") {
            logFiles.push_back(entry.path().string());
        }
    }
    ASSERT_FALSE(logFiles.empty()) << "日志目录下应存在 .log 日志文件";
    for (const auto& f : logFiles) {
        IT_LOG_INFO << "Log format: checking permission of " << f;
        EXPECT_TRUE(IsPerm640(f)) << "日志文件权限应为 640: " << f;
    }

    // 3. 定义正则模式
    // 日期时间戳模式：[YYYY-MM-DD HH:MM:SS.mmm+HH:MM]
    const std::regex datePattern(R"(^\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}[+-]\d{2}:\d{2}\])");
    // 合法日志等级列表
    const std::regex levelPattern(R"(^(DEBUG|INFO|WARN|ERROR|CRIT)$)");
    // 标准日志格式：[时间戳][等级][进程][线程][traceid][文件:函数:行号] 消息。
    // traceid 为 UUID 或备用 hex 格式，也可能为空（UUID 库不可用时）。
    const std::regex comPattern(R"(^\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}[+-]\d{2}:\d{2}\])"
                                R"(\[(\w+)\]\[(\d+)\]\[(\d+)\]\[([0-9a-fA-F-]*)\])"
                                R"(\[([^\[\]:]+):([^\[\]:]+):(\d+)\] )");
    // 第三方组件日志格式：[时间戳][等级][进程][线程][traceid][文件:行号] 消息（无函数名，如 HCOM 组件）
    const std::regex thirdPattern(R"(^\[\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}[+-]\d{2}:\d{2}\])"
                                  R"(\[(\w+)\]\[(\d+)\]\[(\d+)\]\[([0-9a-fA-F-]*)\])"
                                  R"(\[([^\[\]:]+):(\d+)\] )");

    // 4. 获取最近 50h 的日志并校验格式。
    //    日志条目以时间戳行开始，多行消息的续行跟随所属条目，仅对条目首行做格式校验。
    constexpr int64_t logWindowSeconds = 50 * 3600;
    const std::time_t cutoff = std::time(nullptr) - logWindowSeconds;

    std::ifstream ifs(logPath);
    ASSERT_TRUE(ifs.is_open()) << "应能打开日志文件: " << logPath;

    size_t totalEntries = 0;                  // 50h 内的日志条目总数
    size_t comEntries = 0;                    // 标准格式条目数
    size_t thirdEntries = 0;                  // 第三方组件格式条目数
    size_t levelBad = 0;                      // 等级非法的条目数
    size_t formatBad = 0;                     // 格式错误的条目数
    std::map<std::string, size_t> levelCount; // 各等级条目数统计
    std::string line;
    while (std::getline(ifs, line)) {
        if (std::regex_search(line, datePattern)) {
            // 新日志条目开始：解析时间戳判断是否在 50h 窗口内
            std::time_t t = 0;
            if (!(TryParseLogTime(line, t) && t >= cutoff)) {
                continue;
            }
            ++totalEntries;
            // 注意用 regex_search 而非 regex_match：模式仅锚定行首头部格式，消息部分任意
            std::smatch m;
            if (std::regex_search(line, m, comPattern)) {
                ++comEntries;
            } else if (std::regex_search(line, m, thirdPattern)) {
                ++thirdEntries;
            } else {
                ++formatBad;
                IT_LOG_INFO << "Log format: bad format line: " << line.substr(0, 200);
                continue;
            }
            // 提取日志等级并校验合法性（com/third 模式第 1 组均为等级）
            const std::string level = m[1].str();
            if (std::regex_match(level, levelPattern)) {
                ++levelCount[level];
            } else {
                ++levelBad;
                IT_LOG_INFO << "Log format: invalid level '" << level << "': " << line.substr(0, 200);
            }
        }
        // 续行（多行消息）跟随所属条目，无需格式校验
    }
    ifs.close();

    // 5. 输出校验统计并断言
    IT_LOG_INFO << "Log format: total=" << totalEntries << ", com=" << comEntries << ", third=" << thirdEntries
                << ", badFormat=" << formatBad << ", badLevel=" << levelBad;
    for (const auto& [lv, cnt] : levelCount) {
        IT_LOG_INFO << "Log format: level " << lv << " count=" << cnt;
    }

    ASSERT_GT(totalEntries, 0u) << "最近 50h 内应存在日志条目";
    EXPECT_EQ(formatBad, 0u) << "所有日志条目格式应正确（标准或第三方组件格式）";
    EXPECT_EQ(levelBad, 0u) << "所有日志条目等级应合法（WARN/DEBUG/INFO/ERROR/CRIT）";
    EXPECT_GT(comEntries, 0u) << "应存在标准格式（含进程/线程/traceid/函数信息）的日志条目";

    IT_LOG_INFO << "Log format test passed";
}

// 日志回调注册测试（单节点场景）：
//   1) 通过 ubs_engine_log_callback_register 注册自定义日志处理函数 SimpleLogHandler；
//   2) 注册成功后输出日志 "Custom log handler registered successfully"（经回调输出）；
//   3) 触发多等级日志（INFO/WARN/ERROR）及真实 SDK 调用日志（非法参数调用产生 ERROR 级日志），
//      验证均经自定义处理函数按自定义格式输出；
//   4) 校验自定义格式：[simple_log_handler] {log_level.name}：{message}，
//      其中 log_level.name 必须在合法列表 INFO|WARN|ERROR|CRIT 内；
//   5) 恢复默认输出（注册 nullptr），清理临时文件。
void RunLogCallbackTest(ubse::it::infra::ItCluster& cluster)
{
    auto& node = cluster.GetNode("1");
    auto& sdk = cluster.GetSdkClient("1");

    // 1. 准备自定义日志文件（放在节点工作目录下，用例结束清理）
    g_simpleLogPath = node.GetWorkDir() + "/simple_log_handler.log";
    std::error_code ec;
    std::filesystem::remove(g_simpleLogPath, ec);

    // 2. 注册自定义日志处理函数，此后本进程内 SDK 日志均经该回调输出
    ubs_engine_log_callback_register(SimpleLogHandler);
    // 注册成功日志：经自定义处理函数按自定义格式输出
    IPC_LOG_INFO << "Custom log handler registered successfully";
    // 触发多等级日志，验证均按自定义格式输出
    IPC_LOG_WARN << "Log callback test: warn level message";
    IPC_LOG_ERROR << "Log callback test: error level message";
    // 真实 SDK 调用：非法参数调用在 SDK 内部产生 ERROR 级日志，同样经回调输出
    uint32_t cnt = 0;
    EXPECT_EQ(sdk.TopoNodeList(nullptr, &cnt), UBS_ERR_NULL_POINTER) << "非法参数调用应返回 NULL_POINTER";

    // 3. 恢复默认输出（stdout），避免影响其他用例；后续仅校验已写文件
    ubs_engine_log_callback_register(nullptr);

    // 4. 读取自定义日志文件校验格式
    const std::string customLogPath = g_simpleLogPath;
    g_simpleLogPath.clear();
    ASSERT_TRUE(std::filesystem::exists(customLogPath)) << "自定义日志文件应存在: " << customLogPath;

    // 自定义格式：[simple_log_handler] {log_level.name}：{message}（"："为全角冒号）
    // 合法日志等级列表：INFO|WARN|ERROR|CRIT
    const std::regex customPattern(R"(^\[simple_log_handler\] (INFO|WARN|ERROR|CRIT)：)");

    std::ifstream ifs(customLogPath);
    ASSERT_TRUE(ifs.is_open()) << "应能打开自定义日志文件: " << customLogPath;

    size_t totalLines = 0;       // 总行数
    size_t matchedLines = 0;     // 符合自定义格式的行数
    bool hasRegisterMsg = false; // 是否包含注册成功日志
    bool hasSdkLog = false;      // 是否包含 SDK 内部日志（非法参数调用触发）
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) {
            continue;
        }
        ++totalLines;
        if (std::regex_search(line, customPattern)) {
            ++matchedLines;
        } else {
            IT_LOG_INFO << "Log callback: bad format line: " << line.substr(0, 200);
        }
        if (line.find("Custom log handler registered successfully") != std::string::npos) {
            hasRegisterMsg = true;
        }
        if (line.find("Invalid parameters") != std::string::npos) {
            hasSdkLog = true;
        }
    }
    ifs.close();
    IT_LOG_INFO << "Log callback: total=" << totalLines << ", matched=" << matchedLines;

    EXPECT_GT(totalLines, 0u) << "自定义日志文件应有日志输出";
    EXPECT_TRUE(hasRegisterMsg) << "自定义日志应包含注册成功日志 Custom log handler registered successfully";
    EXPECT_TRUE(hasSdkLog) << "自定义日志应包含 SDK 内部日志（验证 SDK 日志经回调输出）";
    EXPECT_EQ(matchedLines, totalLines) << "所有日志行都应符合自定义格式 [simple_log_handler] LEVEL：message";

    // 5. 清理临时文件
    std::filesystem::remove(customLogPath, ec);

    IT_LOG_INFO << "Log callback test passed";
}

} // namespace ubse::it::tests::log