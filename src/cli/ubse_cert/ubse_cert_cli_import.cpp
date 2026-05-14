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
#include "ubse_cert_cli_import.h"
#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include "ubse_cert_file_utils.h"
#include "ubse_error.h"
namespace ubse::cli::cert {
static const std::string CERT_DIR = "/var/lib/ubse/cert/";
static const std::string SERVER_CERT_FILENAME = CERT_DIR + "server.pem";
static const std::string TRUST_CERT_FILENAME = CERT_DIR + "trust.pem";
static const std::string CA_CRL_FILENAME = CERT_DIR + "ca.crl";
static const std::string SERVER_KEY_FILENAME = CERT_DIR + "server_key.pem";
static const std::string PASSWORD_FILENAME = CERT_DIR + "key_pwd.txt";
static const std::string TYPE_IN_PASS_TIPS = "Enter certificate password: ";
const mode_t OWNER_READ_WRITE = 0600;
constexpr uint32_t FILE_MAX_SIZE = 2 * 1024 * 1024; // 2 * 1024 * 1024: 文件支持2M文件

/**
 * @brief 导入证书集合，包括可选的证书吊销列表(CRL)
 *
 * 此函数执行以下操作：
 * 1. 如果提供了CRL路径，则首先导入证书吊销列表
 * 2. 导入服务器证书、信任证书和服务器密钥
 *
 * @param serverCertPath 身份证书路径
 * @param trustCertPath 信任证书路径
 * @param serverKeyPath 身份证书密钥路径
 * @param caCrlPath 证书吊销列表路径（可为nullptr）
 * @param errMsg 错误信息
 *
 * @return bool
 * @retval true 所有证书和CRL导入成功
 * @retval false 导入过程中出现任何错误
 *
 * @note
 * - 如果存在CRL并导入失败，整个操作将中止
 * - 依赖于 ImportCaCrl() 和 ImportCert() 两个函数
 */
bool ImportCertSet(const std::string& serverCertPath, const std::string& trustCertPath,
                   const std::string& serverKeyPath, const std::string& caCrlPath, std::string& errMsg)
{
    if (!caCrlPath.empty()) {
        if (!ImportCaCrl(caCrlPath, errMsg)) {
            return false;
        }
    }

    if (!ImportCert(serverCertPath, trustCertPath, serverKeyPath, errMsg)) {
        return false;
    }
    return true;
}

bool SetEgidAndEuid(uid_t& gid, uid_t& uid, std::string& errMsg)
{
    if (setegid(gid) != 0) {
        errMsg = "Set egid failed, gid=" + std::to_string(gid) + ", error_info=" + std::strerror(errno);
        return false;
    }

    if (seteuid(uid) != 0) {
        errMsg = "Set euid failed, uid=" + std::to_string(uid) + ", error_info=" + std::strerror(errno);
        return false;
    }
    return true;
}

bool CopyFileContents(const std::string& sourcePath, const std::string& destinationPath, std::string& errMsg)
{
    std::ifstream sourceFile(sourcePath, std::ios::binary);
    if (!sourceFile) {
        errMsg = "Failed to open source file: " + sourcePath;
        return false;
    }

    uid_t original_euid = geteuid();
    uid_t original_egid = getegid();

    struct passwd* pw = getpwnam("ubse");
    if (pw == nullptr) {
        errMsg = "user ubse does not exists.";
        sourceFile.close();
        return false;
    }

    if (!SetEgidAndEuid(pw->pw_gid, pw->pw_uid, errMsg)) {
        sourceFile.close();
        return false;
    }

    int destFd = open(destinationPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (destFd == -1) {
        errMsg = "Failed to create destination file: " + destinationPath;
        sourceFile.close();
        return false;
    }

    std::string buffer((std::istreambuf_iterator<char>(sourceFile)), std::istreambuf_iterator<char>());
    if (write(destFd, buffer.c_str(), buffer.size()) == -1) {
        errMsg = "Failed to write to destination file: " + destinationPath;
        close(destFd);
        sourceFile.close();
        return false;
    }

    close(destFd);
    sourceFile.close();
    return SetEgidAndEuid(original_egid, original_euid, errMsg);
}

bool EnterPassword(std::string& errMsg)
{
    uid_t original_euid = geteuid();
    uid_t original_egid = getegid();

    struct passwd* pw = getpwnam("ubse");
    if (pw == nullptr) {
        errMsg = "user ubse does not exists.";
        return false;
    }

    if (!SetEgidAndEuid(pw->pw_gid, pw->pw_uid, errMsg)) {
        return false;
    }

    int fd = open(PASSWORD_FILENAME.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd == -1) {
        errMsg = "Failed to create password file";
        return false;
    }

    if (!SetEgidAndEuid(original_egid, original_euid, errMsg)) {
        close(fd);
        return false;
    }

    std::string passphrase = GetInteractiveInput(TYPE_IN_PASS_TIPS);
    if (passphrase.empty()) {
        errMsg = "Password is invalid";
        close(fd);
        return false;
    }

    if (write(fd, passphrase.c_str(), passphrase.size()) == -1) {
        errMsg = "Failed to write to password file";
        close(fd);
        return false;
    }

    std::fill(passphrase.begin(), passphrase.end(), '\0');
    close(fd);
    return true;
}

bool CheckCertPath(std::string& errMsg)
{
    std::error_code ec;
    if (!std::filesystem::exists(CERT_DIR, ec)) {
        if (ec) {
            errMsg = "Failed to access cert directory, only the root and ubse users are supported.";
            return false;
        }
        errMsg = "Failed to get cert directory, please check whether ubs engine already installed.";
        return false;
    }
    return true;
}

/**
 * @brief 导入服务器证书、信任证书和服务器密钥
 *
 * 此函数执行以下主要操作：
 * 1. 检查证书目录是否存在
 * 2. 验证输入的文件路径是否有效
 * 3. 打开源证书和密钥文件
 * 4. 创建目标文件并复制源文件内容
 * 5. 交互式获取密码并写入密码文件
 * 6. 设置目标文件的权限为仅所有者可读写(0600)
 *
 * @param serverCertPath 身份证书源文件路径
 * @param trustCertPath 信任证书源文件路径
 * @param serverKeyPath 身份证书密钥源文件路径
 *
 * @return bool
 * @retval true 导入成功
 * @retval false 导入失败（文件操作错误、路径无效等）
 *
 * @note
 * - 使用二进制模式读写文件
 * - 所有目标文件权限均设置为0600
 * - 异常情况下会输出错误信息到标准错误流
 */
bool ImportCert(const std::string& serverCertPath, const std::string& trustCertPath, const std::string& serverKeyPath,
                std::string& errMsg)
{
    return CheckCertPath(errMsg) && CheckFilePathValid(serverCertPath, true, errMsg) &&
           CheckFilePathValid(trustCertPath, true, errMsg) && CheckFilePathValid(serverKeyPath, true, errMsg) &&
           CopyFileContents(serverCertPath, SERVER_CERT_FILENAME, errMsg) &&
           CopyFileContents(trustCertPath, TRUST_CERT_FILENAME, errMsg) &&
           CopyFileContents(serverKeyPath, SERVER_KEY_FILENAME, errMsg) && EnterPassword(errMsg);
}

/**
 * @brief 导入证书吊销列表(CRL)
 *
 * 此函数执行以下操作：
 * 1. 验证输入的CRL文件路径是否有效
 * 2. 打开源CRL文件
 * 3. 创建目标CRL文件并复制内容
 * 4. 设置目标文件权限为仅所有者可读写
 *
 * @param caCrlPath CA证书吊销列表源文件路径
 *
 * @return int8_t
 * @retval UBSE_OK CRL成功导入
 * @retval UBSE_ERROR 导入过程中出现错误（文件操作失败、路径无效等）
 *
 * @note
 * - 使用二进制模式读写文件
 * - 目标文件权限设置为0600
 * - 捕获并记录可能的文件操作异常
 */
bool ImportCaCrl(const std::string& caCrlPath, std::string& errMsg)
{
    return CheckCertPath(errMsg) && CheckFilePathValid(caCrlPath, true, errMsg) &&
           CopyFileContents(caCrlPath, CA_CRL_FILENAME, errMsg);
}

int DeleteCertificateFiles(std::string& errMsg)
{
    int deletedFiles = 0;
    std::error_code ec;

    for (const auto& entry : std::filesystem::directory_iterator(CERT_DIR, ec)) {
        if (ec) {
            errMsg = "Failed to iterate certificate directory, " + ec.message();
            return -1;
        }
        if (std::filesystem::is_regular_file(entry.path(), ec) && !ec) {
            if (std::filesystem::remove(entry.path(), ec) && !ec) {
                deletedFiles++;
                continue;
            }
            errMsg = "Failed to delete file, " + ec.message();
            return -1;
        }
        errMsg = "Failed to delete file, " + ec.message();
        return -1;
    }
    return deletedFiles;
}

/**
 * @brief 删除证书目录下的所有证书文件
 *
 * 此函数执行以下操作：
 * 1. 验证证书目录的有效性
 * 2. 尝试删除目录中的所有证书文件
 *
 * @return int8_t
 * - UBSE_OK: 成功删除证书文件
 * - UBSE_ERROR: 目录无效或删除过程中出现错误
 *
 * @note
 * - 使用 CheckFilePathValid() 检查目录有效性
 * - 调用 DeleteCertificateFiles() 执行实际的文件删除
 * - 输出删除的文件数量
 *
 * @warning
 * - 此操作会永久删除证书目录下的所有文件
 * - 请谨慎使用，确保不会意外删除重要证书
 */
UbseResult DeleteCertSet(std::string& errMsg)
{
    if (access(CERT_DIR.c_str(), F_OK) != 0) {
        return UBSE_ERROR_FILE_NOT_EXIST;
    }
    if (access(CERT_DIR.c_str(), R_OK | W_OK | X_OK) != 0) {
        return UBSE_ERROR;
    }
    int deletedFiles = DeleteCertificateFiles(errMsg);
    if (deletedFiles == 0) {
        return UBSE_ERROR_FILE_NOT_EXIST;
    }
    if (deletedFiles > 0) {
        return UBSE_OK;
    }
    return UBSE_ERROR;
}

/**
 * @brief 交互式获取用户输入，支持隐藏回显（如密码输入）
 *
 * 此函数提供安全的交互式输入方法，具有以下特性：
 * - 关闭终端回显，适合敏感信息输入
 * - 支持退格键删除输入
 * - 逐字符读取输入
 *
 * @param prompt 输入提示信息
 * @return std::string 用户输入的字符串
 *
 * @note
 * - 使用 termios 结构体修改终端属性
 * - 适用于 UNIX/Linux 系统
 * - 输入完成后自动恢复终端原始设置
 *
 * @warning
 * - 仅支持 POSIX 系统
 * - 不适用于 Windows 平台
 * - 需要包含 <termios.h> 头文件
 *
 * @example
 * std::string password = GetInteractiveInput("请输入密码: ");
 */
constexpr uint32_t MAX_KEY_LEN = 1000;

void CheckInput(int& ch, std::string& inputData)
{
    // 逐字符读取密码
    while ((ch = getchar()) != '\n' && ch != EOF) {
        if (ch == '\b') { // 处理退格键
            if (inputData.empty()) {
                continue;
            }
            inputData.pop_back();
            continue;
        }
        inputData += static_cast<char>(ch);
        if (inputData.size() > MAX_KEY_LEN) {
            std::cout << std::endl
                      << "The password length exceeds " << MAX_KEY_LEN << " characters, please try again." << std::endl;
            inputData.clear();
            break;
        }
    }
}

std::string GetInteractiveInput(const std::string& prompt)
{
    termios oldt{};
    termios newt{};
    std::string inputData{};
    int ch;

    // 获取当前终端属性
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;

    // 关闭回显
    newt.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    do {
        // 显示提示信息
        std::cout << prompt;
        CheckInput(ch, inputData);
    } while (ch != '\n' && ch != EOF);
    std::cout << std::endl;
    // 恢复终端属性
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    return inputData;
}

/**
 * @brief 检查文件或目录路径的有效性
 *
 * 对给定路径进行多重验证，确保路径的合法性和可访问性
 *
 * @param filePath 待检查的文件或目录路径
 * @param isFile 是否为文件（true）或目录（false）
 *
 * @return bool
 * - true: 路径有效且可访问
 * - false: 路径无效或不可访问
 *
 * @details 执行以下检查：
 * 1. 路径规范化检查
 * 2. 如果是文件，检查文件状态
 * 3. 检查路径是否存在且具有读取权限
 *
 * @note
 * - 使用 FileUtils 工具类进行路径验证
 * - 支持文件和目录路径检查
 */
bool CheckFilePathValid(const std::string& filePath, bool isFile, std::string& errMsg)
{
    if (access(filePath.c_str(), F_OK | R_OK) != 0) {
        errMsg = filePath + " not exist or unreadable.";
        return false;
    }

    if (!FileUtils::IsCanonicalPath(filePath, errMsg)) {
        return false;
    }

    struct stat st {};
    if (stat(filePath.c_str(), &st) != 0) {
        errMsg = filePath + " access failed.";
        return false;
    }

    if (isFile && !S_ISREG(st.st_mode)) {
        errMsg = filePath + " is not regular file.";
        return false;
    }

    if (st.st_size > FILE_MAX_SIZE) {
        errMsg = filePath + " exceeds the maximum size of " + std::to_string(FILE_MAX_SIZE) + " bytes";
        return false;
    }
    return true;
}
} // namespace ubse::cli::cert
