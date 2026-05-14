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

#include "ubse_logger_module.h"

#include <new>    // for nothrow
#include <string> // for basic_string

#include "ubse_conf_module.h"
#include "ubse_context.h"         // for UbseContext, ProcessMode
#include "ubse_error.h"           // for UBSE_OK, UBSE_ERROR_INVAL
#include "ubse_logger_config.h"   // for UbseLoggerConfig
#include "ubse_logger_filesink.h" // for UbseLoggerFilesink
#include "ubse_logger_manager.h"  // for UbseLoggerManager
#include "ubse_logger_writer.h"   // for LoggerOptions, UbseLoggerWriter
#include "trace_context.h"

namespace ubse::log {
BASE_DYNAMIC_CREATE(UbseLoggerModule, ubse::config::UbseConfModule);
UbseLoggerWriter* g_writer = nullptr;

UbseResult UbseLoggerModule::Initialize()
{
    TraceContext::InitUuid();
    UbseLoggerConfig config;
    auto ret = config.Initialize();
    if (ret != UBSE_OK) {
        return ret;
    }
    LoggerOptions options;
    options.minLogLevel = UbseLoggerManager::StringToLogLevel(config.GetLogCfgLevel());
    options.maxFileSizeInMB = config.GetLogCfgFileSize();
    options.rotationFileCount = config.GetLogCfgFileNums();
    options.bufferMaxItem = config.GetLogCfgQueueItems();
    options.syslogOpen = config.GetSyslogSwitch();
    options.syslogType = config.GetSyslogType();
    options.logPath = UBSE_LOG_PATH;
    auto log = UbseLoggerManager::Instance();
    if (log == nullptr) {
        return UBSE_ERROR_INVAL;
    }
    g_writer = new (std::nothrow)
        UbseLoggerFilesink(options.logPath, options.maxFileSizeInMB * NO_1024 * NO_1024, options.rotationFileCount);
    if (g_writer == nullptr) {
        return UBSE_ERROR_INVAL;
    }
    ret = log->Init(options, g_writer);
    if (ret != UBSE_OK) {
        delete g_writer;
        return ret;
    }
    return UBSE_OK;
}

void UbseLoggerModule::UnInitialize()
{
    if (g_writer == nullptr) {
        return;
    }
    delete g_writer;
    g_writer = nullptr;
}

UbseResult UbseLoggerModule::Start()
{
    return UBSE_OK;
}

void UbseLoggerModule::Stop()
{
    if (UbseLoggerManager::gInstance != nullptr) {
        ubse::log::UbseLoggerManager::Destroy();
    }
}
} // namespace ubse::log