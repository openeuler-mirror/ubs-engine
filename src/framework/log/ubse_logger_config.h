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

#ifndef UBSE_LOG_CONFIG_H
#define UBSE_LOG_CONFIG_H

#include <referable/ubse_ref.h> // for Ref, Referable
#include <cstdint>              // for uint32_t
#include <memory>               // for shared_ptr
#include <string>               // for string

#include "ubse_common_def.h" // for UbseResult

namespace ubse::log {
using namespace ubse::utils;
using namespace ubse::common::def;

class UbseLoggerConfig : public Referable {
    using UbseLogConfigPtr = Ref<UbseLoggerConfig>;

public:
    UbseLoggerConfig();
    ~UbseLoggerConfig() override;
    UbseResult Initialize();
    // 获取配置接口声明
    std::string GetLogCfgLevel();
    uint32_t GetLogCfgFileSize();
    uint32_t GetLogCfgFileNums();
    uint32_t GetLogCfgQueueItems();
    bool GetSyslogSwitch();
    uint32_t GetSyslogType();

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};
} // namespace ubse::log
#endif // UBSE_LOG_CONFIG_H
