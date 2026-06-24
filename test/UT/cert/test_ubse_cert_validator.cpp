/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_cert_validator.h"

#include <fstream>
#include <mockcpp/mockcpp.hpp>

#include "ubse_cert_def.h"
#include "ubse_cert_validator.h"
#include "ubse_file_util.h"

namespace ubse::ut::cert {
using namespace ubse::cert;
using namespace ubse::utils;

namespace {
const char* TEST_PWD_FILE = "/tmp/ut_cert_key_pwd.txt";

void CreateTempPasswordFile(const char* content)
{
    std::ofstream f(TEST_PWD_FILE);
    f << content;
    f.close();
}

void RemoveTempPasswordFile()
{
    std::remove(TEST_PWD_FILE);
}
} // namespace

void TestUbseCertValidator::SetUp()
{
    Test::SetUp();
}

void TestUbseCertValidator::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

// ============== LoadPasswordFromFile ==============
TEST_F(TestUbseCertValidator, LoadPasswordFromFile_FileNotExist)
{
    MOCKER_CPP(UbseFileUtil::CheckFileExists).stubs().with(eq(std::string(TEST_PWD_FILE))).will(returnValue(false));

    auto result = UbseSslValidator::LoadPasswordFromFile(TEST_PWD_FILE);
    EXPECT_EQ(result.size(), 0);
}

TEST_F(TestUbseCertValidator, LoadPasswordFromFile_Success)
{
    CreateTempPasswordFile("test_password");

    MOCKER_CPP(UbseFileUtil::CheckFileExists).stubs().with(eq(std::string(TEST_PWD_FILE))).will(returnValue(true));

    auto result = UbseSslValidator::LoadPasswordFromFile(TEST_PWD_FILE);
    EXPECT_EQ(result.size(), 13);
    EXPECT_EQ(result.c_str(), std::string("test_password"));

    RemoveTempPasswordFile();
}

TEST_F(TestUbseCertValidator, LoadPasswordFromFile_WithCarriageReturn)
{
    CreateTempPasswordFile("password\r");

    MOCKER_CPP(UbseFileUtil::CheckFileExists).stubs().with(eq(std::string(TEST_PWD_FILE))).will(returnValue(true));

    auto result = UbseSslValidator::LoadPasswordFromFile(TEST_PWD_FILE);
    EXPECT_EQ(result.size(), 8);
    EXPECT_EQ(result.c_str(), std::string("password"));

    RemoveTempPasswordFile();
}

TEST_F(TestUbseCertValidator, LoadPasswordFromFile_EmptyFile)
{
    CreateTempPasswordFile("");

    MOCKER_CPP(UbseFileUtil::CheckFileExists).stubs().with(eq(std::string(TEST_PWD_FILE))).will(returnValue(true));

    auto result = UbseSslValidator::LoadPasswordFromFile(TEST_PWD_FILE);
    EXPECT_EQ(result.size(), 0);

    RemoveTempPasswordFile();
}

// ============== CheckAllFileExist ==============
TEST_F(TestUbseCertValidator, CheckAllFileExist_AllExist)
{
    MOCKER_CPP(UbseFileUtil::CheckFileExists).stubs().will(returnValue(true));

    EXPECT_TRUE(UbseSslValidator::CheckAllFileExist());
}

TEST_F(TestUbseCertValidator, CheckAllFileExist_ServerCertMissing)
{
    MOCKER_CPP(UbseFileUtil::CheckFileExists)
        .stubs()
        .with(eq(std::string(UbseSSLConfig::ServerCertFile)))
        .will(returnValue(false));
    MOCKER_CPP(UbseFileUtil::CheckFileExists)
        .stubs()
        .with(eq(std::string(UbseSSLConfig::ServerKeyFile)))
        .will(returnValue(true));
    MOCKER_CPP(UbseFileUtil::CheckFileExists)
        .stubs()
        .with(eq(std::string(UbseSSLConfig::TrustCertFile)))
        .will(returnValue(true));
    MOCKER_CPP(UbseFileUtil::CheckFileExists)
        .stubs()
        .with(eq(std::string(UbseSSLConfig::PasswordFile)))
        .will(returnValue(true));

    EXPECT_FALSE(UbseSslValidator::CheckAllFileExist());
}

TEST_F(TestUbseCertValidator, CheckAllFileExist_ServerKeyMissing)
{
    MOCKER_CPP(UbseFileUtil::CheckFileExists)
        .stubs()
        .with(eq(std::string(UbseSSLConfig::ServerCertFile)))
        .will(returnValue(true));
    MOCKER_CPP(UbseFileUtil::CheckFileExists)
        .stubs()
        .with(eq(std::string(UbseSSLConfig::ServerKeyFile)))
        .will(returnValue(false));
    MOCKER_CPP(UbseFileUtil::CheckFileExists)
        .stubs()
        .with(eq(std::string(UbseSSLConfig::TrustCertFile)))
        .will(returnValue(true));
    MOCKER_CPP(UbseFileUtil::CheckFileExists)
        .stubs()
        .with(eq(std::string(UbseSSLConfig::PasswordFile)))
        .will(returnValue(true));

    EXPECT_FALSE(UbseSslValidator::CheckAllFileExist());
}

TEST_F(TestUbseCertValidator, CheckAllFileExist_TrustCertMissing)
{
    MOCKER_CPP(UbseFileUtil::CheckFileExists)
        .stubs()
        .with(eq(std::string(UbseSSLConfig::ServerCertFile)))
        .will(returnValue(true));
    MOCKER_CPP(UbseFileUtil::CheckFileExists)
        .stubs()
        .with(eq(std::string(UbseSSLConfig::ServerKeyFile)))
        .will(returnValue(true));
    MOCKER_CPP(UbseFileUtil::CheckFileExists)
        .stubs()
        .with(eq(std::string(UbseSSLConfig::TrustCertFile)))
        .will(returnValue(false));
    MOCKER_CPP(UbseFileUtil::CheckFileExists)
        .stubs()
        .with(eq(std::string(UbseSSLConfig::PasswordFile)))
        .will(returnValue(true));

    EXPECT_FALSE(UbseSslValidator::CheckAllFileExist());
}

TEST_F(TestUbseCertValidator, CheckAllFileExist_PasswordFileMissing)
{
    MOCKER_CPP(UbseFileUtil::CheckFileExists)
        .stubs()
        .with(eq(std::string(UbseSSLConfig::ServerCertFile)))
        .will(returnValue(true));
    MOCKER_CPP(UbseFileUtil::CheckFileExists)
        .stubs()
        .with(eq(std::string(UbseSSLConfig::ServerKeyFile)))
        .will(returnValue(true));
    MOCKER_CPP(UbseFileUtil::CheckFileExists)
        .stubs()
        .with(eq(std::string(UbseSSLConfig::TrustCertFile)))
        .will(returnValue(true));
    MOCKER_CPP(UbseFileUtil::CheckFileExists)
        .stubs()
        .with(eq(std::string(UbseSSLConfig::PasswordFile)))
        .will(returnValue(false));

    EXPECT_FALSE(UbseSslValidator::CheckAllFileExist());
}

// ============== VerifyCertAndKeyMatch ==============
TEST_F(TestUbseCertValidator, VerifyCertAndKeyMatch_NullCert)
{
    EVP_PKEY* pkey = reinterpret_cast<EVP_PKEY*>(0x1);
    EXPECT_FALSE(UbseSslValidator::VerifyCertAndKeyMatch(nullptr, pkey, "cert", "key"));
}

TEST_F(TestUbseCertValidator, VerifyCertAndKeyMatch_NullKey)
{
    X509* cert = reinterpret_cast<X509*>(0x1);
    EXPECT_FALSE(UbseSslValidator::VerifyCertAndKeyMatch(cert, nullptr, "cert", "key"));
}

TEST_F(TestUbseCertValidator, VerifyCertAndKeyMatch_NullBoth)
{
    EXPECT_FALSE(UbseSslValidator::VerifyCertAndKeyMatch(nullptr, nullptr, "cert", "key"));
}

// ============== LoadAndValidateCert ==============
TEST_F(TestUbseCertValidator, LoadAndValidateCert_FileNotExist)
{
    MOCKER_CPP(UbseFileUtil::CheckFileExists).stubs().will(returnValue(false));

    X509* cert = UbseSslValidator::LoadAndValidateCert("/nonexistent.pem", "test");
    EXPECT_EQ(cert, nullptr);
}

// ============== LoadAndValidatePrivateKey ==============
TEST_F(TestUbseCertValidator, LoadAndValidatePrivateKey_FileNotExist)
{
    MOCKER_CPP(UbseFileUtil::CheckFileExists).stubs().will(returnValue(false));

    EVP_PKEY* pkey = UbseSslValidator::LoadAndValidatePrivateKey("/nonexistent.pem", nullptr, "test");
    EXPECT_EQ(pkey, nullptr);
}

// ============== LoadAndValidateCaStore ==============
TEST_F(TestUbseCertValidator, LoadAndValidateCaStore_FileNotExist)
{
    MOCKER_CPP(UbseFileUtil::CheckFileExists).stubs().will(returnValue(false));

    X509_STORE* store = UbseSslValidator::LoadAndValidateCaStore("/nonexistent.pem");
    EXPECT_EQ(store, nullptr);
}

// ============== ValidateCRLIfExists ==============
TEST_F(TestUbseCertValidator, ValidateCRLIfExists_FileAbsent)
{
    MOCKER_CPP(UbseFileUtil::CheckFileExists).stubs().will(returnValue(false));

    EXPECT_TRUE(UbseSslValidator::ValidateCRLIfExists());
}

// ============== ValidateAll ==============
TEST_F(TestUbseCertValidator, ValidateAll_CheckAllFileExistFail)
{
    MOCKER_CPP(UbseFileUtil::CheckFileExists)
        .stubs()
        .with(eq(std::string(UbseSSLConfig::ServerCertFile)))
        .will(returnValue(false));
    MOCKER_CPP(UbseFileUtil::CheckFileExists)
        .stubs()
        .with(eq(std::string(UbseSSLConfig::ServerKeyFile)))
        .will(returnValue(true));
    MOCKER_CPP(UbseFileUtil::CheckFileExists)
        .stubs()
        .with(eq(std::string(UbseSSLConfig::TrustCertFile)))
        .will(returnValue(true));
    MOCKER_CPP(UbseFileUtil::CheckFileExists)
        .stubs()
        .with(eq(std::string(UbseSSLConfig::PasswordFile)))
        .will(returnValue(true));

    EXPECT_FALSE(UbseSslValidator::ValidateAll());
}

TEST_F(TestUbseCertValidator, ValidateAll_CheckAllFileExist_PasswordEmpty)
{
    CreateTempPasswordFile("");

    MOCKER_CPP(UbseFileUtil::CheckFileExists).stubs().will(returnValue(true));

    EXPECT_FALSE(UbseSslValidator::ValidateAll());

    RemoveTempPasswordFile();
}

// ============== ConfigureCrlValidation ==============
TEST_F(TestUbseCertValidator, ConfigureCrlValidation_CrlFileNotExist)
{
    MOCKER_CPP(UbseFileUtil::CheckFileExists).stubs().will(returnValue(false));

    EXPECT_TRUE(UbseSslValidator::ConfigureCrlValidation(nullptr));
}
} // namespace ubse::ut::cert
