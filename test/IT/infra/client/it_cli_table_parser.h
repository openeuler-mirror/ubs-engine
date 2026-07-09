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

#ifndef IT_CLI_TABLE_PARSER_H
#define IT_CLI_TABLE_PARSER_H

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ubse::it::infra {

/**
 * @brief Parser for CLI table-formatted output (e.g. "ubsectl display memory -t borrow_detail").
 *
 * Recognizes tables with a header row, optional dash-separator rows, and multi-line
 * records delimited by blank lines.  Columns are detected from the header token
 * positions and used to slice data rows.
 *
 * CLI output often intermixes log lines (stderr merged via 2>&1) with table content.
 * This parser automatically skips log/diagnostic lines and extracts only the table.
 */
class UbseCliTableParser {
public:
    using Record = std::map<std::string, std::string>;

    UbseCliTableParser() = default;

    explicit UbseCliTableParser(std::string sourceText) : sourceText_(std::move(sourceText)) {}

    void SetSourceText(std::string sourceText)
    {
        sourceText_ = std::move(sourceText);
    }

    [[nodiscard]] std::vector<Record> Parse()
    {
        columnNames_.clear();
        if (sourceText_.empty()) {
            return {};
        }
        auto lines = SplitIntoLines(sourceText_);
        auto headerIndex = FindHeaderLineIndex(lines);
        if (!headerIndex.has_value()) {
            return {}; // no table found — not an error, just empty
        }
        return ParseAllRows(lines, headerIndex.value());
    }

    [[nodiscard]] const std::vector<std::string>& ColumnNames() const noexcept
    {
        return columnNames_;
    }

private:
    struct ColumnSpan {
        std::string name;
        size_t charStart;
        std::optional<size_t> charEnd;
    };

    struct Token {
        std::string_view text;
        size_t charOffset;
    };

    std::string sourceText_;
    std::vector<std::string> columnNames_;

    // True when `c` is any ASCII control character or space (byte ≤ 0x20)
    static bool IsBlank(char c) noexcept
    {
        return static_cast<unsigned char>(c) <= ' ';
    }

    static std::string_view TrimEdges(std::string_view s) noexcept
    {
        while (!s.empty() && IsBlank(s.front()))
            s.remove_prefix(1);
        while (!s.empty() && IsBlank(s.back()))
            s.remove_suffix(1);
        return s;
    }

    // True when the line is a log/diagnostic line, NOT part of the table.
    // Log lines from IT framework start with "[INFO]", "[ERROR]", "[WARN]".
    // The CLI may also emit "ERROR: ..." messages.
    static bool IsNoiseLine(std::string_view line)
    {
        auto trimmed = TrimEdges(line);
        if (trimmed.empty())
            return true;

        // IT framework log lines: [INFO], [ERROR], [WARN]
        if (trimmed.front() == '[')
            return true;

        // CLI error messages: "ERROR: ..."
        if (trimmed.size() >= 6 && trimmed.substr(0, 6) == "ERROR:")
            return true;

        return false;
    }

    static std::vector<std::string> SplitIntoLines(std::string_view text)
    {
        std::vector<std::string> lines;
        for (size_t pos = 0; pos < text.size();) {
            if (auto newlinePos = text.find('\n', pos); newlinePos != std::string_view::npos) {
                lines.emplace_back(text.substr(pos, newlinePos - pos));
                pos = newlinePos + 1;
            } else {
                lines.emplace_back(text.substr(pos));
                break;
            }
        }
        return lines;
    }

    static std::vector<Token> ExtractTokens(std::string_view line)
    {
        std::vector<Token> tokens;
        for (size_t i = 0; i < line.size();) {
            // skip blanks
            while (i < line.size() && IsBlank(line[i]))
                ++i;
            if (i >= line.size())
                break;
            // scan a token
            auto tokenStart = i;
            while (i < line.size() && !IsBlank(line[i]))
                ++i;
            tokens.push_back({line.substr(tokenStart, i - tokenStart), tokenStart});
        }
        return tokens;
    }

    static bool IsDashSeparator(std::string_view line)
    {
        auto trimmed = TrimEdges(line);
        return !trimmed.empty() && std::all_of(trimmed.begin(), trimmed.end(), [](char c) { return c == '-'; });
    }

    static bool LooksLikeHeaderRow(std::string_view line)
    {
        auto tokens = ExtractTokens(line);
        return tokens.size() >= 2 && std::all_of(tokens.begin(), tokens.end(), [](const Token& t) {
                   // Column names never end with ':' — that's a section title
                   return t.text.size() <= 20 && !t.text.empty() && t.text.back() != ':';
               });
    }

    static std::optional<size_t> FindHeaderLineIndex(const std::vector<std::string>& lines)
    {
        for (size_t i = 0; i < lines.size(); ++i) {
            if (IsNoiseLine(lines[i]))
                continue;
            if (!IsDashSeparator(lines[i]) && LooksLikeHeaderRow(lines[i]))
                return i;
        }
        return std::nullopt;
    }

    static std::vector<ColumnSpan> BuildColumnSpans(std::string_view headerLine)
    {
        auto tokens = ExtractTokens(headerLine);
        std::vector<ColumnSpan> spans;
        spans.reserve(tokens.size());
        for (size_t i = 0; i < tokens.size(); ++i)
            spans.push_back({std::string(tokens[i].text), tokens[i].charOffset,
                             (i + 1 < tokens.size()) ? std::optional{tokens[i + 1].charOffset} : std::nullopt});
        return spans;
    }

    std::vector<Record> ParseAllRows(const std::vector<std::string>& lines, size_t headerIndex)
    {
        auto columnSpans = BuildColumnSpans(lines[headerIndex]);

        columnNames_.reserve(columnSpans.size());
        for (const auto& col : columnSpans)
            columnNames_.push_back(col.name);

        std::vector<Record> allRecords;
        Record currentRecord;
        bool recordInProgress = false;

        for (size_t lineIdx = headerIndex + 1; lineIdx < lines.size(); ++lineIdx) {
            const auto& line = lines[lineIdx];

            // Skip log/diagnostic lines
            if (IsNoiseLine(line)) {
                // Blank/noise line terminates the current record
                if (TrimEdges(line).empty() && recordInProgress) {
                    allRecords.push_back(std::move(currentRecord));
                    currentRecord = {};
                    recordInProgress = false;
                }
                continue;
            }

            // A dash-separator after data rows closes the table — stop parsing
            if (IsDashSeparator(line)) {
                if (recordInProgress) {
                    allRecords.push_back(std::move(currentRecord));
                    currentRecord = {};
                    recordInProgress = false;
                }
                // If we already have records, this closing separator ends the table
                if (!allRecords.empty())
                    break;
                continue;
            }

            if (!recordInProgress) {
                for (const auto& col : columnSpans)
                    currentRecord[col.name];
                recordInProgress = true;
            }

            for (const auto& col : columnSpans) {
                if (col.charStart >= line.size())
                    continue;

                auto boundary = std::min(col.charEnd.value_or(line.size()), line.size());
                auto rawSlice = line.substr(col.charStart, boundary - col.charStart);
                auto trimmedCell = TrimEdges(rawSlice);

                if (!trimmedCell.empty())
                    currentRecord[col.name] += std::string(trimmedCell);
            }
        }

        if (recordInProgress)
            allRecords.push_back(std::move(currentRecord));

        return allRecords;
    }
};

} // namespace ubse::it::infra

#endif // IT_CLI_TABLE_PARSER_H
