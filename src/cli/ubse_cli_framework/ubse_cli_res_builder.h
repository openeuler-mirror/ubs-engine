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

#ifndef UBSE_CLI_RES_BUILDER_H
#define UBSE_CLI_RES_BUILDER_H

#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <unordered_set>
#include <map>
#include <unordered_map>

namespace ubse::cli::framework {
constexpr size_t UBSE_CLI_NUM_0 = 0;
constexpr size_t UBSE_CLI_NUM_1 = 1;
constexpr size_t UBSE_CLI_NUM_2 = 2;
constexpr size_t UBSE_CLI_NUM_3 = 3;
constexpr size_t UBSE_CLI_NUM_4 = 4;
constexpr size_t UBSE_CLI_NUM_5 = 5;
constexpr size_t UBSE_CLI_NUM_6 = 6;
constexpr size_t UBSE_CLI_NUM_7 = 7;
constexpr size_t UBSE_CLI_NUM_8 = 8;
constexpr size_t UBSE_CLI_NUM_9 = 9;
constexpr size_t UBSE_CLI_NUM_10 = 10;

// Data structure for variable-length cells
struct UbseCliVariableCellInfo {
    size_t rows{};
    size_t cols{};
    size_t maxWidth{}; // Maximum width of each column in the table
    std::vector<size_t> columnWidths{};
    std::vector<std::vector<std::string>> cellDatas{};
    std::unordered_map<size_t, std::map<size_t, std::string>> mergeCellData{};
    std::unordered_set<size_t> lineSeparateIndex{};
    std::unordered_set<size_t> noSeparateIndex{};
};

class UbseCliResBuilder {
public:
    // 'cols' specifies the fixed number of columns, and values over 12 are not allowed. 'max_width' specifies the
    // maximum width per column, and values over 80 are not allowed.
    explicit UbseCliResBuilder(size_t cols, size_t max_width = UBSE_CLI_NUM_10);

    // Add a new row of data, and the return value is the current number of rows.
    size_t UbseCliAddRow();

    // Set the minimum width for a specific column, where 'col' ranges from 0 to the maximum number of columns set, and
    // 'width' ranges from the 0 to the maximum width set.
    bool UbseCliSetMinColWidth(const size_t &col, const size_t &width);

    // Set the minimum width for all columns
    // 'width' ranges from the 0 to the maximum width set.
    bool UbseCliSetMinWidth(const size_t &width);

    // Set data that can occupy multiple cells, where key is the position of the cell and value occupies the cells
    // between nearby keys, default key is the first cell. If key exceeds the column count, it will be unavailable.
    bool UbseCliAddMergeRow(std::map<size_t, std::string> &merge_cell_data);

    // Set cell data, and exceed the set range will be unavailable.
    bool UbseCliSetCellData(size_t row, size_t col, const std::string &value);

    // Set whether to add a horizontal line above the current row, default is empty row.
    void UbseCliAddlineSeparate(size_t line_separate_index_val);

    // Set whether to add a horizontal line below the current row, default is empty row.
    void UbseCliAddBottomlineSeparate();

    // Set whether to add any empty row above the current row.
    void UbseCliAddNolineSeparate(size_t no_line_separate_index_val);

    // Set whether to add any empty row below the current row.
    void UbseCliAddBottomNolineSeparate();

    // Get the current row number
    size_t UbseCliGetRows() const;

    // Get the current col number
    size_t UbseCliGetCols() const;

    // Get a built-in table data obj
    UbseCliVariableCellInfo UbseCliVariableCellBuild();

private:
    UbseCliVariableCellInfo variableCellInfo;
};

// The virtual base class that needs to be used for the data to be displayed, below is the supported display class.
class UbseCliResultEcho {
public:
    virtual void UbseCliDisplayResult() = 0;

    virtual ~UbseCliResultEcho() = default;
};

// Output a string with words separated by line breaks without splitting them.
class UbseCliStringEcho : public UbseCliResultEcho {
public:
    explicit UbseCliStringEcho(std::string string_result) : ubseStringResult(std::move(string_result)) {}

    void UbseCliDisplayResult() final;

private:
    std::string ubseStringResult{};
};

// Output a structured table.
class UbseCliVariableCellEcho : public UbseCliResultEcho {
public:
    explicit UbseCliVariableCellEcho(UbseCliVariableCellInfo &variable_cell_info)
        : ubseCliVariableCellInfo(variable_cell_info)
    {}

    void UbseCliDisplayResult() final;

private:
    UbseCliVariableCellInfo ubseCliVariableCellInfo{};
};

// Output multiple structured tables.
class UbseCliVariableCellsEcho : public UbseCliResultEcho {
public:
    explicit UbseCliVariableCellsEcho(const std::vector<UbseCliVariableCellInfo> &variable_cells_info)
        : ubseCliVariableCellsInfo(variable_cells_info){};

    void UbseCliDisplayResult() final;

private:
    std::vector<UbseCliVariableCellInfo> ubseCliVariableCellsInfo{};
};

constexpr size_t UBSE_DEFAULT_SCREEN_WIDTH = 100;

class UbseCliDisplayOnScreen {
public:
    // Words should not be separated when wrapping to the next line.
    static void UbseCliDisplayWordsWithoutSeparation(const std::string &input);

    // Print the table data on the screen.
    static void UbseCliSingleTablePresentation(const UbseCliVariableCellInfo &variable_cell_info);

    // Print the table data on the screen.
    static void UbseCliMultipleTablePresentation(const std::vector<UbseCliVariableCellInfo> &variable_cells_info);

private:
    // Handle tab characters.
    static std::string UbseCliHandleTab(size_t line_output_length);

    // Add words to the current line.
    static void UbseCliAddWordToLine(std::string &line_output, const std::string &each_word, size_t line_limit);

    // Split the string but retain the trailing spaces until a non-space character is encountered.
    static std::string UbseCliTrimSpacesAfterLength(const std::string &str, size_t max_length);

    // Output the data from Merge rows.
    static void UbseCliPrintMergeData(const size_t &col, const std::string &str, const size_t &width,
        std::map<size_t, std::string> &cell_data_map);

    // Ensure that the second value of the map is not empty.
    static bool UbseCliCheckMapValues(std::map<size_t, std::string> &cell_data_map);

    // Read the data from each merge row.
    static void UbseCliTableDisplayMergeLine(const std::map<size_t, std::string> &cell_data_map,
        const std::vector<size_t> &column_widths);

    // Output the spacing between rows.
    static void UbseCliPrintSeparator(const UbseCliVariableCellInfo &variable_cell_info, char ch);

    // Output the data from regular rows.
    static void UbseCliPrintTableData(const size_t &col, const std::string &str, const size_t &width,
        std::map<size_t, std::string> &cell_data_map);

    // Read the data from each regular row.
    static void UbseCliTableDisplayRegularLine(std::map<size_t, std::string> &cell_data_map,
        const std::vector<size_t> &column_widths);
};
} // namespace ubse::cli::framework
#endif