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
#include <sys/stat.h>
#include <termios.h>
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
 *
 * @return int8_t
 * @retval UBSE_OK 所有证书和CRL导入成功
 * @retval UBSE_ERROR 导入过程中出现任何错误
 *
 * @note
 * - 如果存在CRL并导入失败，整个操作将中止
 * - 依赖于 ImportCaCrl() 和 ImportCert() 两个函数
 */
int8_t ImportCertSet(const char *serverCertPath, const char *trustCertPath, const char *serverKeyPath,
    const char *caCrlPath)
{
    if (caCrlPath != nullptr) {
        std::cerr << "CRL detected, importing Certificate Revocation List." << std::endl;
        if (ImportCaCrl(caCrlPath) != UBSE_OK) {
            return UBSE_ERROR;
        }
    }

    if (ImportCert(serverCertPath, trustCertPath, serverKeyPath) != UBSE_OK) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

int CopyFileContents(const std::string &sourcePath, const std::string &destinationPath)
{
    std::ifstream sourceFile(sourcePath, std::ios::binary);
    if (!sourceFile) {
        std::cerr << "Failed to open source file: " << sourcePath << std::endl;
        return UBSE_ERROR;
    }

    std::ofstream destFile(destinationPath, std::ios::binary);
    if (!destFile) {
        std::cerr << "Failed to create destination file: " << destinationPath << std::endl;
        sourceFile.close();
        return UBSE_ERROR;
    }

    destFile << sourceFile.rdbuf();
    destFile.close();
    sourceFile.close();
    return UBSE_OK;
}

/* *
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
 * @return int8_t
 * @retval UBSE_OK 导入成功
 * @retval UBSE_ERROR 导入失败（文件操作错误、路径无效等）
 *
 * @note
 * - 使用二进制模式读写文件
 * - 所有目标文件权限均设置为0600
 * - 异常情况下会输出错误信息到标准错误流
 */
int8_t ImportCert(const char *serverCertPath, const char *trustCertPath, const char *serverKeyPath)
{
    if (!std::filesystem::exists(CERT_DIR)) {
        std::cerr << "Failed to get cert directory, please check weather ubs engine already installed." << std::endl;
        return UBSE_ERROR;
    }

    if (!CheckFilePathValid(serverCertPath, true) || !CheckFilePathValid(trustCertPath, true) ||
        !CheckFilePathValid(serverKeyPath, true)) {
        std::cerr << "ERROR: Input parameters do not conform to the expected format or constraints." << std::endl;
        return UBSE_ERROR;
    }

    if (CopyFileContents(serverCertPath, SERVER_CERT_FILENAME) != UBSE_OK ||
        CopyFileContents(trustCertPath, TRUST_CERT_FILENAME) != UBSE_OK ||
        CopyFileContents(serverKeyPath, SERVER_KEY_FILENAME) != UBSE_OK) {
        return UBSE_ERROR;
    }

    std::ofstream destPasswordFile(PASSWORD_FILENAME, std::ios::binary);
    if (!destPasswordFile) {
        std::cerr << "Failed to create password file" << std::endl;
        return UBSE_ERROR;
    }
    std::string Passphrase = GetInteractiveInput(TYPE_IN_PASS_TIPS);
    destPasswordFile << Passphrase;
    std::fill(Passphrase.begin(), Passphrase.end(), '\0');
    destPasswordFile.close();

    chmod(SERVER_CERT_FILENAME.c_str(), 0600);
    chmod(TRUST_CERT_FILENAME.c_str(), 0600);
    chmod(SERVER_KEY_FILENAME.c_str(), 0600);
    chmod(PASSWORD_FILENAME.c_str(), 0600);

    return UBSE_OK;
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
int8_t ImportCaCrl(const char *caCrlPath)
{
    if (!CheckFilePathValid(caCrlPath, true)) {
        std::cerr << "ERROR: Input parameters do not conform to the expected format or constraints." << std::endl;
        return UBSE_ERROR;
    }

    try {
        std::ifstream caCrlFile(caCrlPath, std::ios::binary);

        if (!caCrlFile) {
            std::cerr << "Failed to open source certificate files" << std::endl;
            return UBSE_ERROR;
        }

        std::ofstream destCaCrlFile(CA_CRL_FILENAME, std::ios::binary);
        if (!destCaCrlFile) {
            std::cerr << "Failed to create destination Crl files" << std::endl;
            return UBSE_ERROR;
        }

        destCaCrlFile << caCrlFile.rdbuf();

        caCrlFile.close();
        destCaCrlFile.close();

        chmod(CA_CRL_FILENAME.c_str(), 0600); // CA证吊销列表书权限
        return UBSE_OK;
    } catch (const std::exception &e) {
        std::cerr << "ERROR: importing certificates: " << e.what() << std::endl;
        return UBSE_ERROR;
    }
}

bool DeleteSingleFile(const std::filesystem::path &filePath)
{
    try {
        if (std::filesystem::is_regular_file(filePath)) {
            std::filesystem::remove(filePath);
            return true;
        }
        return false;
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Error deleting file " << filePath << ": " << e.what() << std::endl;
        return false;
    }
}

int DeleteCertificateFiles()
{
    int deletedFiles = 0;
    try {
        for (const auto &entry : std::filesystem::directory_iterator(CERT_DIR)) {
            if (DeleteSingleFile(entry.path())) {
                deletedFiles++;
            }
        }
        return deletedFiles;
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Error iterating certificate directory: " << e.what() << std::endl;
        return -1;
    }
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
int8_t DeleteCertSet()
{
    if (!CheckFilePathValid(CERT_DIR, false)) {
        std::cerr << "ERROR: Input parameters do not conform to the expected format or constraints." << std::endl;
        return UBSE_ERROR;
    }
    int deletedFiles = DeleteCertificateFiles();
    if (deletedFiles >= 0) {
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
std::string GetInteractiveInput(const std::string &prompt)
{
    struct termios oldt;
    struct termios newt;
    std::string inputData;
    char ch;

    // 获取当前终端属性
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;

    // 关闭回显
    newt.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    // 显示提示信息
    std::cout << prompt;

    // 逐字符读取密码
    while ((ch = getchar()) != '\n' && ch != EOF) {
        if (ch == 127 || ch == '\b') { // 处理退格键
            if (!inputData.empty()) {
                std::cout << "\b \b";
                inputData.pop_back();
            }
        } else {
            inputData += ch;
        }
    }

    // 恢复终端属性
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    std::cout << std::endl;

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
 * - 使用 FileUtil 工具类进行路径验证
 * - 支持文件和目录路径检查
 */
bool CheckFilePathValid(std::string filePath, bool isFile)
{
    if (!FileUtil::CanonicalPath(filePath)) {
        return false;
    }
    if (isFile && !FileUtil::CheckFileStat(filePath)) {
        return false;
    }
    if (!FileUtil::Exist(filePath, F_OK | R_OK)) {
        return false;
    }
    return true;
}
}
