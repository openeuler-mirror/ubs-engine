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

#include "ubse_logger_filesink.h"
#include <sys/stat.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_os_util.h"
#include "ubse_security_module.h"

namespace ubse::log {
using namespace ubse::common::def;
using namespace ubse::security;
UbseLoggerFilesink::UbseLoggerFilesink(std::string basePath, uint32_t maxFileSize, uint32_t maxFileCount)
    : basePath_(std::move(basePath)),
      maxFileSize_(maxFileSize),
      maxFileCount_(maxFileCount)
{
    auto canonicalPath = realpath(this->basePath_.c_str(), nullptr);
    if (canonicalPath == nullptr) {
        std::cerr << "FilePath does not exist: " << this->basePath_ << std::endl;
        std::vector<__u32> caps{CAP_DAC_OVERRIDE};
        auto result = UbseSecurityModule::ModifyEffectiveCapabilities(caps, true);
        if (result != UBSE_OK) {
            std::cerr << "Add capabilities failed." << std::endl;
        }
        result = mkdir(this->basePath_.c_str(), 0750); // 权限0750
        UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
        if (result == -1) {
            perror("mkdir"); // Print error message
            std::cerr << "Failed to create directory: " << std::endl;
        }
    } else {
        this->basePath_ = canonicalPath;
    }
    free(canonicalPath);
}

UbseLoggerFilesink::~UbseLoggerFilesink()
{
    for (auto& it : fileMap_) {
        if (it.second.logFile.is_open()) {
            it.second.logFile.close();
        }
    }
}

bool UbseLoggerFilesink::Initialize() const
{
    if (maxFileSize_ == 0 || maxFileCount_ == 0) {
        return false;
    }

    fs::path dir = fs::path(basePath_);
    if (!fs::exists(dir) && !fs::create_directories(dir)) {
        std::cerr << "Failed to create directory: " << dir << std::endl;
        return false;
    }
    return true;
}

bool UbseLoggerFilesink::Write(UbseLoggerEntry& loggerEntry)
{
    std::string fileName = loggerEntry.GetModuleName();
    if (!fileMap_[fileName].isInitialized) {
        fileMap_[fileName].filePath = basePath_ + "/" + fileName + ".log";
        fileMap_[fileName].isInitialized = true;
    }

    bool needOpen = !fileMap_[fileName].logFile.is_open() || IsFileStatusChanged(fileName) ||
                    !fileMap_[fileName].logFile.good();
    if (needOpen) {
        fileMap_[fileName].logFile.close();
        if (!OpenFile(fileName)) {
            std::cerr << "Open file failed" << std::endl;
            return false;
        }
    }

    loggerEntry.OutPutLog(fileMap_[fileName].logFile);

    uintmax_t currentFileSize = 0;
    try {
        currentFileSize = fs::file_size(fileMap_[fileName].filePath);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    if (currentFileSize > maxFileSize_) {
        if (!RollFile(fileName)) {
            std::cerr << "Failed rolling file" << std::endl;
            return false;
        }
    }
    return true;
}

bool UbseLoggerFilesink::IsFileStatusChanged(const std::string& fileName)
{
    struct stat fileStat {};
    if (stat(fileMap_[fileName].filePath.c_str(), &fileStat) != 0) {
        return true; // 文件不存在或无法访问
    }
    bool isChanged = (fileMap_[fileName].inode != fileStat.st_ino);
    return isChanged;
}

bool UbseLoggerFilesink::RollFile(const std::string& fileName)
{
    ManageFileRotation(fileName);
    time_t currentTime = std::time(nullptr);
    std::string compressedFilename = GenerateCompressedFilename(fileName, fileMap_[fileName].fileIndex, currentTime);
    if (!CompressFile(fileName, fileMap_[fileName].filePath, compressedFilename)) {
        std::cerr << "Failed compressing file" << std::endl;
        return false;
    }
    return true;
}

std::string UbseLoggerFilesink::GenerateCompressedFilename(const std::string& fileName, uint32_t index,
                                                           time_t timeStamp)
{
    std::ostringstream oss;

    // 获取当前时间信息
    struct tm timeinfo {};
    localtime_r(&timeStamp, &timeinfo);
    struct tm* ptimeinfo = &timeinfo;

    // 生成文件名
    oss << basePath_ << "/" << fileName << "_" << std::setw(4) << std::setfill('0') << // 年份格式占4位
        (ptimeinfo->tm_year + 1900) << std::setw(2) << std::setfill('0') << // 月份格式占2位 起始年份1900
        (ptimeinfo->tm_mon + 1) << std::setw(2) << std::setfill('0') << ptimeinfo->tm_mday // 日期格式占2位
        << "_" << std::setw(2) << std::setfill('0') << ptimeinfo->tm_hour                  // 小时格式占2位
        << std::setw(2) << std::setfill('0') << ptimeinfo->tm_min                          // 分钟格式占2位
        << std::setw(2) << std::setfill('0') << ptimeinfo->tm_sec                          // 秒格式占2位
        << "_" << std::setw(3) << std::setfill('0') << index                               // 序号格式占3位
        << ".tar.gz";

    return oss.str();
}

bool UbseLoggerFilesink::OpenFile(const std::string& fileName)
{
    fs::path filePath = fileMap_[fileName].filePath;
    try {
        fileMap_[fileName].logFile.open(filePath, std::ios::out | std::ios::app);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }

    if (fileMap_[fileName].logFile.is_open()) {
        struct stat fileStat {};
        stat(fileMap_[fileName].filePath.c_str(), &fileStat);
        fileMap_[fileName].inode = fileStat.st_ino;
        try {
            // 设置权限为640
            fs::permissions(filePath, fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return false;
        }
        return true;
    } else {
        std::cerr << "Failed to open log file: " << filePath << std::endl;
        return false;
    }
}

std::string ShellEscape(const std::string& str)
{
    if (str.empty()) {
        return "''";
    }
    std::string result;
    result += '\'';
    for (char c : str) {
        if (c == '\'') {
            result += "'\\''";
        } else {
            result += c;
        }
    }
    result += '\'';
    return result;
}

bool UbseLoggerFilesink::CompressFile(const std::string& fileName, const std::string& sourceFilename,
                                      const std::string& destFilename)
{
    std::string command = "tar -czf " + ShellEscape(destFilename) + " -C " + ShellEscape(basePath_) + " " +
                          ShellEscape(fileName + ".log");
    std::string result;
    auto ret = ubse::utils::UbseOsUtil::Exec(command, result);
    if (ret != UBSE_OK) {
        std::cerr << "Failed to compress file using system command, " << result << std::endl;
        return false;
    }
    try {
        fs::permissions(destFilename, fs::perms::owner_read | fs::perms::group_read); // 设置权限为440
        fs::remove(sourceFilename);
        fileMap_[fileName].logFile.close();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return true;
}

uint32_t UbseLoggerFilesink::RenameCompressedFile(std::vector<std::string>& compressedFiles)
{
    uint32_t currentFilecount = compressedFiles.size();
    for (uint32_t i = 0; i < currentFilecount; i++) {
        std::smatch match;
        if (std::regex_search(compressedFiles[i], match, std::regex(R"(_(\d{3})\.tar\.gz)"))) {
            uint32_t newIndex = i + 1;
            std::ostringstream oss;
            oss << std::setw(3) << std::setfill('0') << newIndex; // 匹配压缩包名3位序号

            std::string newFilename = compressedFiles[i];
            newFilename.replace(match.position(1), match.length(1), oss.str());

            try {
                fs::rename(compressedFiles[i], newFilename);
                compressedFiles[i] = std::move(newFilename);
            } catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << std::endl;
            }
        }
    }
    return currentFilecount;
}

void UbseLoggerFilesink::ManageFileRotation(const std::string& fileName)
{
    // 存储匹配到的文件
    std::vector<std::string> compressedFiles;
    fs::path filePath = fs::path(basePath_ + "/" + fileName);
    fs::path dir = filePath.parent_path();

    std::regex filenamePattern(filePath.string() + filenameSuffixPattern_);

    for (const auto& entry : fs::directory_iterator(dir)) {
        if (fs::is_regular_file(entry) && std::regex_match(entry.path().string(), filenamePattern)) {
            compressedFiles.push_back(entry.path().string());
        }
    }

    // 按照文件名中的序号进行排序
    std::sort(compressedFiles.begin(), compressedFiles.end());

    fileMap_[fileName].fileIndex = RenameCompressedFile(compressedFiles) + 1;

    if (compressedFiles.size() < maxFileCount_) {
        return;
    }
    uint32_t currentFilecount = compressedFiles.size();
    for (uint32_t i = 0; i < currentFilecount - maxFileCount_ + 1; i++) {
        std::string oldestFile = compressedFiles.front();
        fs::remove(oldestFile);
        compressedFiles.erase(compressedFiles.begin());
    }

    // 重新命名剩余的文件
    RenameCompressedFile(compressedFiles);

    // 将新文件的索引设置为最大
    fileMap_[fileName].fileIndex = maxFileCount_;
}
} // namespace ubse::log