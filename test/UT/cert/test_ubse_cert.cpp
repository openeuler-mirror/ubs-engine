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

#include "test_ubse_cert.h"
#include <mockcpp/mockcpp.hpp>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "ubse_error.h"
#include "ubse_cert_cli_import.h"

namespace ubse::ut::cert {
using namespace ubse::cli::cert;
std::string get_current_path()
{
    return std::filesystem::current_path().string();
}
int crate_cert_demo()
{
    std::string certDir = get_current_path();
    std::string serverPath = certDir + "target/" + "server.pem";
    std::string trustPath = certDir + "target/" + "trust.pem";
    std::string serverKeyPath = certDir + "target/" + "server_key.pem";
    std::string passwordPath = certDir + "target/" + "key_pwd.txt";
    std::string serverCertPath = certDir + "target/" + "server_cert.pem";
    std::string trustCertPath = certDir + "target/" + "trust_cert.pem";
    int ret = UBSE_OK;

    // 确保目标目录存在，如果不存在则创建
    try {
        if (!std::filesystem::exists(certDir)) {
            std::filesystem::create_directories(certDir);
            chmod(certDir.c_str(), 0700);
        }
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Failed to create directory: " << e.what() << std::endl;
        return 1;
    }

    // 写入目标文件
    std::ofstream destServerCertFile(serverPath, std::ios::binary);
    std::ofstream destTrustCertFile(trustPath, std::ios::binary);
    std::ofstream destServerKeyFile(serverKeyPath, std::ios::binary);
    std::ofstream destPasswordFile(passwordPath, std::ios::binary);

    if (!destServerCertFile || !destTrustCertFile || !destServerKeyFile || !destPasswordFile) {
        std::cerr << "Failed to create destination certificate files" << std::endl;
        return 1;
    }
    std::string serverContent = "server cert demo";
    std::string trustContent = "trust cert demo";
    std::string serverKeyContent = "server key demo";
    std::string passwordContent = "password demo";

    std::ifstream serverCertFile(serverCertPath, std::ios::binary);
    std::ifstream trustCertFile(trustCertPath, std::ios::binary);
    std::ifstream serverKeyFile(serverKeyPath, std::ios::binary);
    destServerCertFile << serverCertFile.rdbuf();
    destTrustCertFile << trustCertFile.rdbuf();
    destServerKeyFile << serverKeyFile.rdbuf();
    std::string passphrase = GetInteractiveInput("请输入密码: ");
    destPasswordFile << passphrase;

    // 关闭文件
    destServerCertFile.close();
    destTrustCertFile.close();
    destServerKeyFile.close();
    return 0;
}

int clean_cert_file()
{
    std::string certDir = get_current_path();
    std::string serverPath = certDir + "target/" + "server.pem";
    std::string trustPath = certDir + "target/" + "trust.pem";
    std::string serverKeyPath = certDir + "target/" + "server_key.pem";
    if (std::filesystem::exists(serverPath)) {
        std::filesystem::remove(serverPath);
        std::cout << "Removed file: " << serverPath << std::endl;
    }
    if (std::filesystem::exists(trustPath)) {
        std::filesystem::remove(trustPath);
        std::cout << "Removed file: " << trustPath << std::endl;
    }

    if (std::filesystem::exists(serverKeyPath)) {
        std::filesystem::remove(serverKeyPath);
        std::cout << "Removed file: " << serverKeyPath << std::endl;
    }
}

void TestUbseCert::SetUp()
{
    crate_cert_demo();
    Test::SetUp();
}

void TestUbseCert::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
    clean_cert_file();
}

TEST_F(TestUbseCert, Import_cert_valid)
{
    std::string errMsg{};
    std::string certDir = get_current_path();
    std::string serverPath = certDir + "server.pem";
    std::string trustPath = certDir + "trust.pem";
    std::string serverKeyPath = certDir + "server_key.pem";
    bool ret = ImportCertSet(serverPath, trustPath, serverKeyPath, "", errMsg);
    EXPECT_FALSE(ret);
}
}
