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

#ifndef UBSE_CLI_WHITELIST
#define UBSE_CLI_WHITELIST

#include <string>

namespace ubse::cli::framework {
constexpr size_t UBSE_CLI_ASCII_NUMS = 256;
constexpr unsigned char UBSE_CLI_CONTROL_CHARS_LOWER = 0;
constexpr unsigned char UBSE_CLI_CONTROL_CHARS_UPPER = 31;
constexpr unsigned char UBSE_CLI_DELETE_CHAR = 127;
class UbseCliWhitelist {
public:
    explicit UbseCliWhitelist()
    {
        UbseCliSetDefaultWhitelist();
    }

    void UbseCliAddChar(char c)
    {
        whitelist_[static_cast<unsigned char>(c)] = true;
    }

    void UbseCliAddChars(const std::string& chars)
    {
        for (char c : chars) {
            UbseCliAddChar(c);
        }
    }

    [[nodiscard]] bool UbseCliIsAllowed(const std::string& input) const
    {
        for (unsigned char c : input) {
            if (c >= whitelist_.size() || !whitelist_[c]) {
                return false;
            }
        }
        return true;
    }

    // Add numbers
    void UbseCliSetDigitsOnly()
    {
        UbseCliAddChars("0123456789");
    }

    // Add letters
    void UbseCliSetLettersOnly()
    {
        UbseCliAddChars("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
    }

    // Add lowercase letters
    void UbseCliSetLowercaseOnly()
    {
        UbseCliAddChars("abcdefghijklmnopqrstuvwxyz");
    }

    // Add uppercase letters
    void UbseCliSetUppercaseOnly()
    {
        UbseCliAddChars("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    }

    // Clear the whitelist
    void UbseCliClearWhitelist()
    {
        whitelist_ = std::vector<bool>(UBSE_CLI_ASCII_NUMS, false);
    }

    void UbseCliSetNoControlChars()
    {
        UbseCliClearWhitelist();

        for (size_t i = UBSE_CLI_CONTROL_CHARS_LOWER; i < UBSE_CLI_DELETE_CHAR; ++i) {
            if (i <= UBSE_CLI_CONTROL_CHARS_UPPER) {
                continue;
            }
            whitelist_[i] = true;
        }
    }

private:
    void UbseCliSetDefaultWhitelist()
    {
        UbseCliAddChars("abcdefghijklmnopqrstuvwxyz"
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                        "0123456789"
                        "-._/");
    }

private:
    std::vector<bool> whitelist_ = std::vector<bool>(UBSE_CLI_ASCII_NUMS, false);
};
} // namespace ubse::cli::framework
#endif
