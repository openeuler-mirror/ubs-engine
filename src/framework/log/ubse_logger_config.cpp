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

#include <iostream>             // for basic_ostream, char_traits, operator<<
#include <unordered_set>        // for unordered_set
#include "syslog.h"

#include "ubse_conf_module.h"   // for UbseConfModule
#include "ubse_context.h"       // for UbseContext
#include "ubse_error.h"         // for UBSE_OK, UBSE_ERROR
#include "ubse_logger_config.h" // for UBSE_LOG_CONFIG

namespace ubse::log {
using namespace ubse::config;
using namespace ubse::context;

constexpr uint32_t MIN_LOG_FILESIZE = 2;
constexpr uint32_t MAX_LOG_FILESIZE = 20;
constexpr uint32_t DEFAULT_LOG_FILESIZE = 20;
constexpr uint32_t MIN_LOG_FILENUM = 1;
constexpr uint32_t MAX_LOG_FILENUM = 200;
constexpr uint32_t DEFAULT_LOG_FILENUM = 20;
constexpr uint32_t MIN_LOG_ITEM = 64;
constexpr uint32_t MAX_LOG_ITEM = 4096;
constexpr uint32_t DEFAULT_LOG_ITEM = 4096;
constexpr bool DEFAULT_SYSLOG_OPEN = false;
constexpr std::string_view DEFAULT_SYSLOG_TYPE = "user";

class UbseLoggerConfig::Impl {
public:
    std::shared_ptr<UbseConfModule> cfgRef;
};

UbseLoggerConfig::UbseLoggerConfig() : pImpl(std::make_unique<Impl>()) {}
UbseLoggerConfig::~UbseLoggerConfig() = default;

UbseResult UbseLoggerConfig::Initialize()
{
    // 调用配置管理接口
    auto &ctxRef = UbseContext::GetInstance();
    pImpl->cfgRef = ctxRef.GetModule<UbseConfModule>();
    if (pImpl->cfgRef == nullptr) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

// 获取配置信息的接口实现
std::string UbseLoggerConfig::GetLogCfgLevel()
{
    std::string logCfgLevel;
    const std::unordered_set<std::string> validLogLevels = {"DEBUG", "INFO", "WARN", "ERROR", "CRIT"};
    if (pImpl->cfgRef == nullptr) {
        std::cerr << "Please get ubse logger instance first" << std::endl;
        return "";
    }
    uint32_t code = pImpl->cfgRef->GetConf<std::string>("ubse.log", "log.level", logCfgLevel);
    if (code != UBSE_OK) {
        logCfgLevel = "INFO"; // INFO为manager.log.level的默认配置
        std::cerr << "Unable to read LogCfgLevel, code: " << code << ", will use default value:" << logCfgLevel
                  << std::endl;
    }
    if (validLogLevels.find(logCfgLevel) == validLogLevels.end()) {
        std::cerr << "Invalid logCfgLevel: " << logCfgLevel << ", will use default value: "
                  << "INFO" << std::endl;
        logCfgLevel = "INFO";
    }
    return logCfgLevel;
}

uint32_t UbseLoggerConfig::GetLogCfgFileSize()
{
    uint32_t logCfgFileSize;
    if (pImpl->cfgRef == nullptr) {
        std::cerr << "Please get ubse logger instance first" << std::endl;
        return UBSE_ERROR_NULLPTR;
    }
    uint32_t code = pImpl->cfgRef->GetConf<uint32_t>("ubse.log", "log.max.fileSize", logCfgFileSize);
    if (code != UBSE_OK) {
        logCfgFileSize = DEFAULT_LOG_FILESIZE;
        std::cerr << "Unable to read LogCfgFileSize, code: " << code << ", will use default value:" << logCfgFileSize
                  << std::endl;
    }
    if (logCfgFileSize < MIN_LOG_FILESIZE || logCfgFileSize > MAX_LOG_FILESIZE) {
        std::cerr << "Invalid logCfgFileSize: " << logCfgFileSize
                  << ", will use default value: " << DEFAULT_LOG_FILESIZE << std::endl;
        logCfgFileSize = DEFAULT_LOG_FILESIZE;
    }
    return logCfgFileSize;
}

uint32_t UbseLoggerConfig::GetLogCfgFileNums()
{
    uint32_t logCfgFileNums;
    if (pImpl->cfgRef == nullptr) {
        std::cerr << "Please get ubse logger instance first" << std::endl;
        return UBSE_ERROR_NULLPTR;
    }
    uint32_t code = pImpl->cfgRef->GetConf<uint32_t>("ubse.log", "log.fileNums", logCfgFileNums);
    if (code != UBSE_OK) {
        logCfgFileNums = DEFAULT_LOG_FILENUM;
        std::cerr << "Unable to read LogCfgFileNums, code: " << code << ", will use default value:" << logCfgFileNums
                  << std::endl;
    }
    if (logCfgFileNums < MIN_LOG_FILENUM || logCfgFileNums > MAX_LOG_FILENUM) {
        std::cerr << "Invalid logCfgFileNums: " << logCfgFileNums << ", will use default value: " << DEFAULT_LOG_FILENUM
                  << std::endl;
        logCfgFileNums = DEFAULT_LOG_FILENUM;
    }
    return logCfgFileNums;
}

uint32_t UbseLoggerConfig::GetLogCfgQueueItems()
{
    uint32_t logCfgQueueItems;
    if (pImpl->cfgRef == nullptr) {
        std::cerr << "Please get ubse logger instance first" << std::endl;
        return UBSE_ERROR_NULLPTR;
    }
    uint32_t code = pImpl->cfgRef->GetConf<uint32_t>("ubse.log", "log.queue.maxItem", logCfgQueueItems);
    if (code != UBSE_OK) {
        logCfgQueueItems = DEFAULT_LOG_ITEM;
        std::cerr << "Unable to read LogCfgQueueItems, code: " << code
                  << ", will use default value:" << logCfgQueueItems << std::endl;
    }
    if (logCfgQueueItems < MIN_LOG_ITEM || logCfgQueueItems > MAX_LOG_ITEM) {
        std::cerr << "Invalid logCfgQueueItems: " << logCfgQueueItems
                  << ", will use default value: " << DEFAULT_LOG_ITEM << std::endl;
        logCfgQueueItems = DEFAULT_LOG_ITEM;
    }
    return logCfgQueueItems;
}

bool UbseLoggerConfig::GetSyslogSwitch()
{
    bool syslogOpen;
    if (pImpl->cfgRef == nullptr) {
        std::cerr << "Please get ubse logger instance first" << std::endl;
        return DEFAULT_SYSLOG_OPEN;
    }
    uint32_t code = pImpl->cfgRef->GetConf<bool>("ubse.log", "log.sys.open", syslogOpen);
    if (code != UBSE_OK) {
        syslogOpen = DEFAULT_SYSLOG_OPEN;
        std::cerr << "Unable to read SyslogOpen, code: " << code << ", will use default value: false" << std::endl;
    }
    return syslogOpen;
}

uint32_t UbseLoggerConfig::GetSyslogType()
{
    std::string syslogTypeStr = "";
    static const std::unordered_map<std::string, uint32_t> typeMap = {
        {"kern", LOG_KERN},     {"user", LOG_USER},     {"mail", LOG_MAIL},         {"daemon", LOG_DAEMON},
        {"auth", LOG_AUTH},     {"syslog", LOG_SYSLOG}, {"lpr", LOG_LPR},           {"news", LOG_NEWS},
        {"uucp", LOG_UUCP},     {"cron", LOG_CRON},     {"authpriv", LOG_AUTHPRIV}, {"ftp", LOG_FTP},
        {"local0", LOG_LOCAL0}, {"local1", LOG_LOCAL1}, {"local2", LOG_LOCAL2},     {"local3", LOG_LOCAL3},
        {"local4", LOG_LOCAL4}, {"local5", LOG_LOCAL5}, {"local6", LOG_LOCAL6},     {"local7", LOG_LOCAL7}};
    if (pImpl->cfgRef == nullptr) {
        std::cerr << "Please get ubse logger instance first" << std::endl;
        return LOG_USER;
    }
    uint32_t code = pImpl->cfgRef->GetConf<std::string>("ubse.log", "log.sys.type", syslogTypeStr);
    if (code != UBSE_OK) {
        syslogTypeStr = DEFAULT_SYSLOG_TYPE;
        std::cerr << "Unable to read SyslogType, code: " << code << ", will use default value:" << DEFAULT_SYSLOG_TYPE
                  << std::endl;
    }
    if (typeMap.find(syslogTypeStr) == typeMap.end()) {
        std::cerr << "Invalid SyslogCfgLevel: " << syslogTypeStr << ", will use default value: " << DEFAULT_SYSLOG_TYPE
                  << std::endl;
        syslogTypeStr = DEFAULT_SYSLOG_TYPE;
    }
    auto it = typeMap.find(syslogTypeStr);
    return it->second;
}

} // namespace ubse::log