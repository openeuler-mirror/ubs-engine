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

#include "ubse_cli_res_builder.h"
#include <sys/ioctl.h>
#include <unistd.h>
#include <iomanip>

namespace ubse::cli::framework {
UbseCliResBuilder::UbseCliResBuilder(size_t cols, size_t max_width)
{
    this->variableCellInfo_.rows = 0;
    if (cols <= 12 && max_width <= 80) { // The maximum number of columns is 12, and each column is a maximum of 80
                                         // characters. If it exceeds, it is not allowed.
        this->variableCellInfo_.cols = cols;
        this->variableCellInfo_.maxWidth = max_width;
        try {
            this->variableCellInfo_.columnWidths.resize(cols, 0);
        } catch (const std::bad_alloc& e) {
            std::cerr << "Memory allocation failed during resize: " << e.what() << std::endl;
        }
    } else {
        this->variableCellInfo_.cols = 0;
        this->variableCellInfo_.maxWidth = 0;
        try {
            this->variableCellInfo_.columnWidths.resize(0, 0);
        } catch (const std::bad_alloc& e) {
            std::cerr << "Memory allocation failed during resize: " << e.what() << std::endl;
        }
    }
}

size_t UbseCliResBuilder::UbseCliAddRow()
{
    this->variableCellInfo_.rows++;
    this->variableCellInfo_.cellDatas.emplace_back(variableCellInfo_.cols, "");
    return this->variableCellInfo_.rows;
}

bool UbseCliResBuilder::UbseCliSetMinColWidth(const size_t& col, const size_t& width)
{
    if (col > 0 && col <= this->variableCellInfo_.cols && width > 0 && width <= this->variableCellInfo_.maxWidth) {
        this->variableCellInfo_.columnWidths[col - UBSE_CLI_NUM_1] = width;
        return true;
    }
    return false;
}

bool UbseCliResBuilder::UbseCliSetMinWidth(const size_t& width)
{
    if (width > 0 && width <= this->variableCellInfo_.maxWidth) {
        for (size_t i = 0; i < this->variableCellInfo_.cols; i++) {
            this->variableCellInfo_.columnWidths[i] = width;
        }
        return true;
    }
    return false;
}

bool UbseCliResBuilder::UbseCliAddMergeRow(std::map<size_t, std::string>& merge_cell_data)
{
    for (auto it = merge_cell_data.begin(); it != merge_cell_data.end();) {
        if (it->first < UBSE_CLI_NUM_1 || it->first > this->variableCellInfo_.cols) {
            it = merge_cell_data.erase(it);
        } else {
            ++it;
        }
    }
    if (!merge_cell_data.empty() && this->variableCellInfo_.cols > 0) {
        this->variableCellInfo_.mergeCellData[this->variableCellInfo_.rows] = merge_cell_data;
        this->variableCellInfo_.rows++;
        this->variableCellInfo_.cellDatas.emplace_back(this->variableCellInfo_.cols, "");
        return true;
    }
    return false;
}

bool UbseCliResBuilder::UbseCliSetCellData(size_t row, size_t col, const std::string& value)
{
    --row;
    --col;
    if (row < this->variableCellInfo_.rows && col < this->variableCellInfo_.cols) {
        size_t length = value.size();
        if (length > this->variableCellInfo_.columnWidths[col]) {
            this->variableCellInfo_.columnWidths[col] =
                length < this->variableCellInfo_.maxWidth ? length : this->variableCellInfo_.maxWidth;
        }
        this->variableCellInfo_.cellDatas[row][col] = value;
        return true;
    }
    return false;
}

void UbseCliResBuilder::UbseCliAddlineSeparate(size_t line_separate_index_val)
{
    this->variableCellInfo_.lineSeparateIndex.insert(line_separate_index_val);
}

void UbseCliResBuilder::UbseCliAddBottomlineSeparate()
{
    UbseCliAddlineSeparate(UbseCliGetRows() + UBSE_CLI_NUM_1);
}

void UbseCliResBuilder::UbseCliAddNolineSeparate(size_t no_line_separate_index_val)
{
    this->variableCellInfo_.noSeparateIndex.insert(no_line_separate_index_val);
}

size_t UbseCliResBuilder::UbseCliGetRows() const
{
    return this->variableCellInfo_.rows;
}

size_t UbseCliResBuilder::UbseCliGetCols() const
{
    return this->variableCellInfo_.cols;
}

void UbseCliResBuilder::UbseCliAddBottomNolineSeparate()
{
    UbseCliAddNolineSeparate(UbseCliGetRows() + UBSE_CLI_NUM_1);
}

UbseCliVariableCellInfo UbseCliResBuilder::UbseCliVariableCellBuild()
{
    return this->variableCellInfo_;
}

void UbseCliStringEcho::UbseCliDisplayResult()
{
    UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation(this->ubseStringResult_);
}

void UbseCliVariableCellEcho::UbseCliDisplayResult()
{
    UbseCliDisplayOnScreen::UbseCliSingleTablePresentation(this->ubseCliVariableCellInfo_);
}

void UbseCliVariableCellsEcho::UbseCliDisplayResult()
{
    UbseCliDisplayOnScreen::UbseCliMultipleTablePresentation(this->ubseCliVariableCellsInfo_);
}

void UbseCliDisplayOnScreen::UbseCliDisplayWordsWithoutSeparation(const std::string& input)
{
    struct winsize window {
    };
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) < 0) {
        window.ws_col = UBSE_DEFAULT_SCREEN_WIDTH;
    }
    size_t line_width_limit = window.ws_col;

    std::string each_line_output{};
    std::string each_word{};

    for (size_t index = 0; index < input.length(); ++index) {
        char ch = input[index];

        if (ch == '\n') {
            std::cout << each_line_output << std::endl;
            each_line_output.clear();
        } else if (ch == '\t') {
            each_line_output += UbseCliHandleTab(each_line_output.length());
        } else if (ch == ' ') {
            if (!each_word.empty()) {
                UbseCliAddWordToLine(each_line_output, each_word, line_width_limit);
                each_word.clear();
            }
        } else {
            each_word += ch;
        }

        // Check the last word.
        if (index == input.length() - 1 || input[index + 1] == ' ' || input[index + 1] == '\n' ||
            input[index + 1] == '\t') {
            if (!each_word.empty()) {
                UbseCliAddWordToLine(each_line_output, each_word, line_width_limit);
                each_word.clear();
            }
        }
    }

    if (!each_line_output.empty()) {
        std::cout << each_line_output << std::endl;
    }
}

void UbseCliDisplayOnScreen::UbseCliAddWordToLine(std::string& line_output, const std::string& each_word,
                                                  size_t line_limit)
{
    if (!line_output.empty() && line_output.length() + each_word.length() + 1 > line_limit) {
        std::cout << line_output << std::endl;
        line_output = each_word;
    } else {
        if (!line_output.empty()) {
            line_output += ' ';
        }
        line_output += each_word;
    }
}

std::string UbseCliDisplayOnScreen::UbseCliHandleTab(size_t line_output_length)
{
    size_t tab_width = 4; // The width of a tab is 4.
    size_t spaces_to_add = tab_width - (line_output_length % tab_width);
    return std::move(std::string(spaces_to_add, ' '));
}

std::string UbseCliDisplayOnScreen::UbseCliTrimSpacesAfterLength(const std::string& str, size_t max_length)
{
    if (str.length() <= max_length) {
        return str;
    }
    std::string result = str.substr(0, max_length);
    size_t index = max_length;
    while (index < str.length() && str[index] == ' ') {
        index++;
    }
    if (index < str.length()) {
        result += str.substr(index);
    }
    return result;
}

void UbseCliDisplayOnScreen::UbseCliPrintMergeData(const size_t& col, const std::string& str, const size_t& width,
                                                   std::map<size_t, std::string>& cell_data_map)
{
    size_t length = 0;
    std::string next_line;
    if (str.size() > width) {
        size_t pos = str.rfind(' ', width);
        if (pos != std::string::npos) {
            length = pos;
            next_line = UbseCliTrimSpacesAfterLength(str, length + 1).substr(length + 1);
        } else {
            length = width;
            next_line = UbseCliTrimSpacesAfterLength(str, length).substr(length);
        }
        std::cout << std::left << std::setw(static_cast<int>(width)) << str.substr(0, length);
    } else {
        std::cout << std::left << std::setw(static_cast<int>(width)) << str;
    }
    cell_data_map.emplace(col, next_line);
}

bool UbseCliDisplayOnScreen::UbseCliCheckMapValues(std::map<size_t, std::string>& cell_data_map)
{
    for (const auto& pair : cell_data_map) {
        if (!pair.second.empty()) {
            return false;
        }
    }
    return true;
}

void UbseCliDisplayOnScreen::UbseCliTableDisplayMergeLine(const std::map<size_t, std::string>& cell_data_map,
                                                          const std::vector<size_t>& column_widths)
{
    const size_t CELL_GAP_WIDTH = 3;
    // Change "  " to "| " in the table development.
    std::cout << "  ";
    size_t last_data_index = 0;
    std::map<size_t, std::string> newCellDataMap;
    for (auto& data : cell_data_map) {
        size_t width = 0;
        for (size_t col = last_data_index; col < data.first; col++) {
            if (col != last_data_index) {
                // The gap between the two tables is 3, and when merging cells, it needs to be included in the merge.
                width = width + column_widths[col] +
                        CELL_GAP_WIDTH; // The builder class ensures that the data stored will not cause
                                        // out-of-bounds errors.
            } else {
                width += column_widths[col]; // The builder class ensures that the data stored will not cause
                                             // out-of-bounds errors.
            }
        }
        UbseCliPrintMergeData(data.first, data.second, width, newCellDataMap);
        // In the table development, change "   " to " | ".
        std::cout << "   ";
        last_data_index = data.first;
    }
    for (size_t col = last_data_index; col < column_widths.size(); col++) {
        std::cout << (std::string(column_widths[col], ' ') + std::string("   "));
    }
    std::cout << std::endl;
    if (!UbseCliCheckMapValues(newCellDataMap)) {
        UbseCliTableDisplayMergeLine(newCellDataMap, column_widths);
    }
}

void UbseCliDisplayOnScreen::UbseCliPrintSeparator(const UbseCliVariableCellInfo& variable_cell_info, char ch)
{
    const size_t CELL_PADDING = 2;
    std::cout << ch;
    for (size_t col = 0; col < variable_cell_info.cols; ++col) {
        // The width of the cell will have two spaces on either side, so we use 2 to let the horizontal line cover the
        // cell.
        std::cout << std::string(variable_cell_info.columnWidths[col] + CELL_PADDING, ch) << ch;
    }
    std::cout << std::endl;
}

void UbseCliDisplayOnScreen::UbseCliPrintTableData(const size_t& col, const std::string& str, const size_t& width,
                                                   std::map<size_t, std::string>& cell_data_map)
{
    size_t length;
    if (str.size() > width) {
        size_t pos = str.rfind(' ', width);
        std::string next_line;
        if (pos != std::string::npos) {
            length = pos;
            next_line = UbseCliTrimSpacesAfterLength(str, length + 1).substr(length + 1);
        } else {
            length = width;
            next_line = UbseCliTrimSpacesAfterLength(str, length).substr(length);
        }
        if (!next_line.empty()) {
            cell_data_map.emplace(col, next_line);
        }
        std::cout << std::left << std::setw(static_cast<int>(width)) << str.substr(0, length);
    } else {
        std::cout << std::left << std::setw(static_cast<int>(width)) << str;
    }
}

void UbseCliDisplayOnScreen::UbseCliTableDisplayRegularLine(std::map<size_t, std::string>& cell_data_map,
                                                            const std::vector<size_t>& column_widths)
{
    // Change "  " to "| " in table development.
    std::cout << "  ";
    size_t last_data_index = 0;
    std::map<size_t, std::string> new_cell_data_map;
    for (auto& data : cell_data_map) {
        for (size_t col = last_data_index; col < data.first; col++) {
            // Change "   " to " | " in table development.
            std::cout << ((std::string(column_widths[col], ' ') + std::string("   ")));
        }
        UbseCliPrintTableData(data.first, data.second, column_widths[data.first], new_cell_data_map);
        // Change "   " to " | " in table development.
        std::cout << "   ";
        last_data_index = data.first + 1;
    }
    for (size_t col = last_data_index; col < column_widths.size(); col++) {
        std::cout << (std::string(column_widths[col], ' ') + std::string("   "));
    }
    std::cout << std::endl;
    if (!new_cell_data_map.empty()) {
        UbseCliTableDisplayRegularLine(new_cell_data_map, column_widths);
    }
}

void UbseCliDisplayOnScreen::UbseCliSingleTablePresentation(const UbseCliVariableCellInfo& variable_cell_info)
{
    for (size_t row = 0; row < variable_cell_info.rows; ++row) {
        if (variable_cell_info.noSeparateIndex.find(row + 1) != variable_cell_info.noSeparateIndex.end()) {
        } else if (variable_cell_info.lineSeparateIndex.find(row + 1) != variable_cell_info.lineSeparateIndex.end()) {
            UbseCliPrintSeparator(variable_cell_info, '-');
        } else {
            UbseCliPrintSeparator(variable_cell_info, ' ');
        }
        if (variable_cell_info.mergeCellData.find(row) != variable_cell_info.mergeCellData.end()) {
            UbseCliTableDisplayMergeLine(variable_cell_info.mergeCellData.at(row), variable_cell_info.columnWidths);
        } else {
            // Change "  " to "| " in table development.
            std::cout << "  ";
            std::map<size_t, std::string> cell_data_map;
            for (size_t col = 0; col < variable_cell_info.cols; ++col) {
                UbseCliPrintTableData(col, variable_cell_info.cellDatas[row][col], variable_cell_info.columnWidths[col],
                                      cell_data_map);
                // Change "   " to " | " in table development.
                std::cout << "   ";
            }
            std::cout << std::endl;
            if (!cell_data_map.empty()) {
                UbseCliTableDisplayRegularLine(cell_data_map, variable_cell_info.columnWidths);
            }
        }
    }
    if (variable_cell_info.noSeparateIndex.find(variable_cell_info.rows + 1) !=
        variable_cell_info.noSeparateIndex.end()) {
    } else if (variable_cell_info.lineSeparateIndex.find(variable_cell_info.rows + 1) !=
               variable_cell_info.lineSeparateIndex.end()) {
        UbseCliPrintSeparator(variable_cell_info, '-');
    }
}

void UbseCliDisplayOnScreen::UbseCliMultipleTablePresentation(
    const std::vector<UbseCliVariableCellInfo>& variable_cells_info)
{
    for (const auto& cell_info : variable_cells_info) {
        UbseCliSingleTablePresentation(cell_info);
        std::cout << std::endl;
    }
}
} // namespace ubse::cli::framework