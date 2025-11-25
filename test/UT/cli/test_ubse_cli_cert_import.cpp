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

void TestUbseCliCertImport::TearDown() {}

// Mock CanonicalPath
bool MockCanonicalPathFalse(std::string &path)
{
    return false;
}

bool MockCanonicalPathTrue(std::string &path)
{
    return true;
}

// Mock CheckFileStat
bool MockCheckFileStatFalse(const std::string &filePath)
{
    return false;
}

bool MockCheckFileStatTrue(const std::string &filePath)
{
    return true;
}

// Mock Exist
bool MockExistFalse(const std::string &path, const int &mode)
{
    return false;
}

bool MockExistTrue(const std::string &path, const int &mode)
{
    return true;
}

// Mock Remove
bool MockRemoveFalse(const std::filesystem::path &filePath)
{
    return false;
}

bool MockRemoveTrue(const std::filesystem::path &filePath)
{
    return true;
}

// Mock DirectoryIterator
std::vector<std::filesystem::path> MockDirectoryIteratorEmpty()
{
    return {};
}

std::vector<std::filesystem::path> MockDirectoryIteratorWithOneFile()
{
    return { "/var/lib/ubse/cert/server.pem" };
}

// Mock GetInteractiveInput
std::string MockGetInteractiveInput(const std::string &prompt)
{
    return "mock_password";
}

// Mock CopyFileContents
int8_t MockCopyFileContentsSuccess(const std::string &source, const std::string &dest)
{
    return UBSE_OK;
}

int8_t MockCopyFileContentsFail(const std::string &source, const std::string &dest)
{
    return UBSE_ERROR;
}

// Mock ImportCaCrl
int8_t MockImportCaCrlSuccess(const char *caCrlPath)
{
    return UBSE_OK;
}

int8_t MockImportCaCrlFail(const char *caCrlPath)
{
    return UBSE_ERROR;
}

// Mock chmod
int MockChmodSuccess(const char *path, mode_t mode)
{
    return 0;
}

int MockChmodFail(const char *path, mode_t mode)
{
    return -1;
}

TEST(TestUbseCliCertImport, PathValidCanonicalPathFalse)
{
    MOCKER(&FileUtil::CanonicalPath).stubs().will(invoke(MockCanonicalPathFalse));
    std::string filePath = "/path";
    bool isFile = true;
    bool result = CheckFilePathValid(filePath, isFile);
    EXPECT_FALSE(result);
    MOCKER(&FileUtil::CanonicalPath).reset();
}

TEST(TestUbseCliCertImport, PathValidFileStatFalse)
{
    MOCKER(&FileUtil::CanonicalPath).stubs().will(invoke(MockCanonicalPathTrue));
    MOCKER(&FileUtil::CheckFileStat).stubs().will(invoke(MockCheckFileStatFalse));
    std::string filePath = "/path";
    bool isFile = true;
    bool result = CheckFilePathValid(filePath, isFile);
    EXPECT_FALSE(result);
    MOCKER(&FileUtil::CanonicalPath).reset();
    MOCKER(&FileUtil::CheckFileStat).reset();
}

TEST(TestUbseCliCertImport, FileExistFalse)
{
    MOCKER(&FileUtil::CanonicalPath).stubs().will(invoke(MockCanonicalPathTrue));
    MOCKER(&FileUtil::CheckFileStat).stubs().will(invoke(MockCheckFileStatTrue));
    MOCKER(&FileUtil::Exist).stubs().will(invoke(MockExistFalse));
    std::string filePath = "/path";
    bool isFile = true;
    bool result = CheckFilePathValid(filePath, isFile);
    EXPECT_FALSE(result);
    MOCKER(&FileUtil::CanonicalPath).reset();
    MOCKER(&FileUtil::CheckFileStat).reset();
    MOCKER(&FileUtil::Exist).reset();
}

TEST(TestUbseCliCertImport, FileExistTrue)
{
    MOCKER(&FileUtil::CanonicalPath).stubs().will(invoke(MockCanonicalPathTrue));
    MOCKER(&FileUtil::CheckFileStat).stubs().will(invoke(MockCheckFileStatTrue));
    MOCKER(&FileUtil::Exist).stubs().will(invoke(MockExistTrue));
    std::string filePath = "/path";
    bool isFile = true;
    bool result = CheckFilePathValid(filePath, isFile);
    EXPECT_TRUE(result);
    MOCKER(&FileUtil::CanonicalPath).reset();
    MOCKER(&FileUtil::CheckFileStat).reset();
    MOCKER(&FileUtil::Exist).reset();
}

TEST(TestUbseCliCertImport, ImportCertSetCrlImportFail)
{
    MOCKER(&FileUtil::CanonicalPath).stubs().will(invoke(MockCanonicalPathTrue));
    MOCKER(&FileUtil::CheckFileStat).stubs().will(invoke(MockCheckFileStatTrue));
    MOCKER(&FileUtil::Exist).stubs().will(invoke(MockExistTrue));
    MOCKER(&ImportCaCrl).stubs().will(invoke(MockImportCaCrlFail));

    const char *serverCertPath = "/path/to/server.pem";
    const char *trustCertPath = "/path/to/trust.pem";
    const char *serverKeyPath = "/path/to/server_key.pem";
    const char *caCrlPath = "/path/to/ca.crl";

    UbseResult result = ImportCertSet(serverCertPath, trustCertPath, serverKeyPath, caCrlPath);
    EXPECT_EQ(result, UBSE_ERROR);

    MOCKER(&FileUtil::CanonicalPath).reset();
    MOCKER(&FileUtil::CheckFileStat).reset();
    MOCKER(&FileUtil::Exist).reset();
    MOCKER(&ImportCaCrl).reset();
}

TEST(TestUbseCliCertImport, ImportCertCopyFileFail)
{
    MOCKER(&FileUtil::CanonicalPath).stubs().will(invoke(MockCanonicalPathTrue));
    MOCKER(&FileUtil::CheckFileStat).stubs().will(invoke(MockCheckFileStatTrue));
    MOCKER(&FileUtil::Exist).stubs().will(invoke(MockExistTrue));
    MOCKER(&CopyFileContents).stubs().will(invoke(MockCopyFileContentsFail));

    const char *serverCertPath = "/path/to/server.pem";
    const char *trustCertPath = "/path/to/trust.pem";
    const char *serverKeyPath = "/path/to/server_key.pem";

    UbseResult result = ImportCert(serverCertPath, trustCertPath, serverKeyPath);
    EXPECT_EQ(result, UBSE_ERROR);

    MOCKER(&FileUtil::CanonicalPath).reset();
    MOCKER(&FileUtil::CheckFileStat).reset();
    MOCKER(&FileUtil::Exist).reset();
    MOCKER(&CopyFileContents).reset();
}

TEST(TestUbseCliCertImport, ImportCaCrlFail)
{
    MOCKER(&FileUtil::Exist).stubs().will(invoke(MockExistFalse));

    const char *caCrlPath = "/path/to/ca.crl";

    UbseResult result = ImportCaCrl(caCrlPath);
    EXPECT_EQ(result, UBSE_ERROR);

    MOCKER(&FileUtil::Exist).reset();
}


TEST(TestUbseCliCertImport, DeleteCertSetFail)
{
    MOCKER(&FileUtil::Exist).stubs().will(invoke(MockExistFalse));

    UbseResult result = DeleteCertSet();
    EXPECT_EQ(result, UBSE_ERROR);
    MOCKER(&FileUtil::Exist).reset();
}
} // namespace ubse::ut::cli
