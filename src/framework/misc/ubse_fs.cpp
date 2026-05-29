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

#include "ubse_fs.h"
#include <fcntl.h>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <regex>
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_pointer_process.h"

namespace ubse::misc::fs {
UBSE_DEFINE_THIS_MODULE("ubse");
constexpr size_t UBSE_DATA_MAX_SIZE = 500 * 1024; // 500k

static bool VerifyFileName(const std::string& fileName)
{
    std::regex pattern("[a-zA-Z0-9-_.]+");
    if (!std::regex_match(fileName, pattern)) {
        UBSE_LOG_ERROR << "Verify file=" << fileName << " fail";
        return false;
    }
    return true;
}

static bool RemoveFile(const std::string& path, const std::string& fileName)
{
    std::error_code ec;
    bool exists = std::filesystem::exists(path, ec);
    if (ec) {
        UBSE_LOG_ERROR << "Remove file=" << fileName << " fail, error=" << ec.message();
        return false;
    } else if (exists) {
        auto ret = std::filesystem::remove(path.c_str());
        if (!ret) {
            UBSE_LOG_ERROR << "Remove file=" << fileName << " fail, error=" << std::strerror(errno);
            return false;
        }
    }
    return true;
}

uint32_t UbseFs::DeleteFile(const std::string& fileName)
{
    if (!VerifyFileName(fileName)) {
        return UBSE_ERROR;
    }
    auto fileLock = GetLock(fileName);
    std::lock_guard<std::mutex> lock(*fileLock);
    const std::filesystem::path dataDir(rootPath_);
    const std::filesystem::path filePath = dataDir / fileName;
    const std::filesystem::path tmpPath = dataDir / (fileName + ".tmp");
    if (RemoveFile(filePath, fileName) && RemoveFile(tmpPath, fileName + ".tmp")) {
        RemoveLock(fileName);
        return UBSE_OK;
    }
    RemoveLock(fileName);
    return UBSE_ERROR;
}

uint32_t UbseFs::ReadFile(const std::string& fileName, uint8_t*& data, uint32_t& dataLen)
{
    if (!VerifyFileName(fileName)) {
        return UBSE_ERROR;
    }
    std::filesystem::path dir(rootPath_);
    std::filesystem::path filePath = dir / fileName;
    std::error_code ec;
    bool exists = std::filesystem::exists(filePath, ec);
    if (ec) {
        UBSE_LOG_ERROR << "Cannot determine if file=" << fileName << ", exists=" << ec.message();
        dataLen = 0;
        data = nullptr;
        return UBSE_ERROR;
    }
    if (!exists) {
        UBSE_LOG_WARN << "Read file=" << fileName << " not exist";
        dataLen = 0;
        data = nullptr;
        return UBSE_OK;
    }
    auto fileLock = GetLock(fileName);
    std::lock_guard<std::mutex> lock(*fileLock);
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file) {
        UBSE_LOG_ERROR << "Read file=" << fileName << " fail, error=" << std::strerror(errno);
        return UBSE_ERROR;
    }
    auto size = file.tellg();
    file.seekg(0);
    auto fileSize = static_cast<size_t>(size);
    if (fileSize > UBSE_DATA_MAX_SIZE) {
        UBSE_LOG_ERROR << "Read file=" << fileName << " fail, error=file size more than 500k";
        return UBSE_ERROR;
    }
    auto valueArr = new (std::nothrow) uint8_t[fileSize];
    if (valueArr == nullptr) {
        UBSE_LOG_ERROR << "Read file=" << fileName << " fail, error=can't alloc mem";
        return UBSE_ERROR;
    }
    if (!file.read(reinterpret_cast<char*>(valueArr), size)) { // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        SafeDeleteArray(valueArr);
        UBSE_LOG_ERROR << "Read file=" << fileName << " fail, error=" << std::strerror(errno);
        return UBSE_ERROR;
    }
    data = valueArr;
    dataLen = fileSize;
    return UBSE_OK;
}

uint32_t WriteTmpFile(const std::string& fileName, const uint8_t* data, const uint32_t dataLen)
{
    int fd = open(fileName.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd == -1) {
        UBSE_LOG_ERROR << "Write tmp file=" << fileName << " fail, error=" << std::strerror(errno);
        return UBSE_ERROR;
    }
    ssize_t bytesWritten = write(fd, data, dataLen);
    if (bytesWritten == -1) {
        close(fd);
        UBSE_LOG_ERROR << "Write tmp file=" << fileName << " fail, error=" << std::strerror(errno);
        return UBSE_ERROR;
    }
    int syncRet = fsync(fd);
    if (syncRet == -1) {
        close(fd);
        UBSE_LOG_ERROR << "Sync tmp file=" << fileName << " fail, error=" << std::strerror(errno);
        return UBSE_ERROR;
    }
    close(fd);
    return UBSE_OK;
}

uint32_t UbseFs::WriteFile(const std::string& fileName, const uint8_t* data, const uint32_t dataLen)
{
    if (!VerifyFileName(fileName)) {
        return UBSE_ERROR;
    }

    auto fileLock = GetLock(fileName);
    std::lock_guard<std::mutex> lock(*fileLock);

    if (dataLen > UBSE_DATA_MAX_SIZE) {
        UBSE_LOG_ERROR << "Write file=" << fileName << " fail, error=file size more than 500k";
        return UBSE_ERROR;
    }

    // 使用 std::filesystem 安全构建路径
    const std::filesystem::path dataDir(rootPath_);
    const std::filesystem::path filePath = dataDir / fileName;
    const std::filesystem::path tmpPath = dataDir / (fileName + ".tmp");

    std::error_code ec;
    if (WriteTmpFile(tmpPath, data, dataLen) != UBSE_OK) {
        std::filesystem::remove(tmpPath, ec);
        return UBSE_ERROR;
    }

    // 执行原子重命名
    std::filesystem::rename(tmpPath, filePath, ec);
    if (ec) {
        UBSE_LOG_ERROR << "Rename tmp file=" << fileName << " failed, error=" << ec.message();
        // 尝试清理残留临时文件
        std::filesystem::remove(tmpPath, ec); // 忽略清理失败
        return UBSE_ERROR;
    }

    return UBSE_OK;
}

std::shared_ptr<std::mutex> UbseFs::GetLock(const std::string& fileName)
{
    std::lock_guard<std::mutex> lock(mapLock_);
    if (shareMutexMap_.find(fileName) == shareMutexMap_.end()) {
        shareMutexMap_[fileName] = std::make_shared<std::mutex>();
    }
    return shareMutexMap_[fileName];
}

void UbseFs::RemoveLock(const std::string& fileName)
{
    std::lock_guard<std::mutex> lock(mapLock_);
    shareMutexMap_.erase(fileName);
}

} // namespace ubse::misc::fs