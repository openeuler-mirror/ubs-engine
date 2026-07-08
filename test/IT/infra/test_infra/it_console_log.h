/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef IT_CONSOLE_LOG_H
#define IT_CONSOLE_LOG_H

#include <iostream>
#include <mutex>
#include <sstream>

namespace ubse::it::infra {

class ItConsoleLogLine {
public:
    explicit ItConsoleLogLine(const char* level) : level_(level) {}

    ~ItConsoleLogLine()
    {
        static std::mutex mutex;
        std::lock_guard<std::mutex> lock(mutex);
        std::cerr << "[" << level_ << "] " << buffer_.str() << std::endl;
    }

    template <typename T>
    ItConsoleLogLine& operator<<(const T& value)
    {
        buffer_ << value;
        return *this;
    }

    ItConsoleLogLine& operator<<(std::ostream& (*manipulator)(std::ostream&))
    {
        buffer_ << manipulator;
        return *this;
    }

private:
    const char* level_;
    std::ostringstream buffer_;
};

} // namespace ubse::it::infra

#define IT_LOG_ERROR ::ubse::it::infra::ItConsoleLogLine("ERROR")
#define IT_LOG_WARN ::ubse::it::infra::ItConsoleLogLine("WARN")
#define IT_LOG_INFO ::ubse::it::infra::ItConsoleLogLine("INFO")

#endif // IT_CONSOLE_LOG_H
