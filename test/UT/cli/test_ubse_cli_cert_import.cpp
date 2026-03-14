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

#include "test_ubse_cli_cert_import.h"
#include <filesystem>
#include <fstream>
#include <securec.h>
#include <ubse_common_def.h>
#include <mockcpp/mockcpp.hpp>
#include <string>
#include "ubse_cert_cli_import.h"
#include "ubse_cert_file_utils.h"
#include "ubse_error.h"

namespace ubse::ut::cli {
using namespace ubse::cli::cert;
using namespace common::def;

void TestUbseCliCertImport::SetUp() {}

void TestUbseCliCertImport::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

bool MockCopyFileContentsFail(const std::string &source, const std::string &dest)
{
    return false;
}

bool MockImportCaCrlFail(std::string &caCrlPath)
{
    return false;
}

TEST(TestUbseCliCertImport, PathValidCanonicalPathFalse)
{
    std::string filePath = "/path";
    std::string errMsg{};
    bool isFile = true;
    bool result = CheckFilePathValid(filePath, isFile, errMsg);
    EXPECT_FALSE(result);
}

TEST(TestUbseCliCertImport, PathValidFileStatFalse)
{
    std::string filePath = "/path";
    std::string errMsg{};
    bool isFile = true;
    bool result = CheckFilePathValid(filePath, isFile, errMsg);
    EXPECT_FALSE(result);
}

TEST(TestUbseCliCertImport, FileExistFalse)
{
    std::string filePath = "/path";
    std::string errMsg{};
    bool isFile = true;
    bool result = CheckFilePathValid(filePath, isFile, errMsg);
    EXPECT_FALSE(result);
}

TEST(TestUbseCliCertImport, FileExistTrue)
{
    std::string filePath = "/tmp/mock_test";
    std::ofstream mock_file(filePath);
    std::string errMsg{};
    bool isFile = true;
    bool result = CheckFilePathValid(filePath, isFile, errMsg);
    EXPECT_TRUE(result);
    std::filesystem::remove(filePath);
}

TEST(TestUbseCliCertImport, ImportCertSetCrlImportFail)
{
    MOCKER(&ImportCaCrl).stubs().will(invoke(MockImportCaCrlFail));

    std::string errMsg{};
    std::string serverCertPath = "/path/to/server.pem";
    std::string trustCertPath = "/path/to/trust.pem";
    std::string serverKeyPath = "/path/to/server_key.pem";
    std::string caCrlPath = "/path/to/ca.crl";

    bool result = ImportCertSet(serverCertPath, trustCertPath, serverKeyPath, caCrlPath, errMsg);
    EXPECT_FALSE(result);

    MOCKER(&ImportCaCrl).reset();
}

TEST(TestUbseCliCertImport, ImportCertCopyFileFail)
{
    MOCKER(&CopyFileContents).stubs().will(invoke(MockCopyFileContentsFail));

    std::string errMsg{};
    std::string serverCertPath = "/path/to/server.pem";
    std::string trustCertPath = "/path/to/trust.pem";
    std::string serverKeyPath = "/path/to/server_key.pem";

    bool result = ImportCert(serverCertPath, trustCertPath, serverKeyPath, errMsg);
    EXPECT_FALSE(result);
}

TEST(TestUbseCliCertImport, ImportCaCrlFail)
{
    std::string errMsg{};
    std::string caCrlPath = "/path/to/ca.crl";

    bool result = ImportCaCrl(caCrlPath, errMsg);
    EXPECT_FALSE(result);
}


TEST(TestUbseCliCertImport, DeleteCertSetFail)
{
    std::string errMsg{};
    UbseResult result = DeleteCertSet(errMsg);
    EXPECT_EQ(result, UBSE_ERROR);
}

TEST(TestUbseCliCertImport, DeleteCertSetWhenFilePathIsInvalid)
{
    std::string errMsg{};
    bool falseVal = false;
    MOCKER(CheckFilePathValid).stubs().will(returnValue(falseVal));
    auto ret = DeleteCertSet(errMsg);
    ASSERT_EQ(ret, UBSE_ERROR);
}
} // namespace ubse::ut::cli
