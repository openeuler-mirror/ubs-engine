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

#include "test_ubse_cli_file_utils.h"
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <fstream>
#include <string>
#include "ubse_cert_file_utils.h"
namespace ubse::ut::cli {
using namespace ubse::cli::cert;

void TestUbseCliFileUtils::SetUp() {}
void TestUbseCliFileUtils::TearDown() {}

std::string CreateTempFile(const std::string &content = "")
{
    char tmpnam[256];
    std::tmpnam(tmpnam);
    std::string tempPath(tmpnam);
    std::ofstream file(tempPath);
    if (!content.empty()) {
        file << content;
    }
    file.close();
    return tempPath;
}

std::string CreateTempDir()
{
    char tmpnam[256];
    std::tmpnam(tmpnam);
    std::string tempDir(tmpnam);
    mkdir(tempDir.c_str(), 0755);
    return tempDir;
}

std::string CreateSymlink(const std::string &target)
{
    char tmpnam[256];
    std::tmpnam(tmpnam);
    std::string linkPath(tmpnam);
    symlink(target.c_str(), linkPath.c_str());
    return linkPath;
}

TEST(TestUbseCliFileUtils, CheckFileExists)
{
    std::string tempFile = CreateTempFile();
    EXPECT_TRUE(FileUtils::CheckFileExists(tempFile));
    remove(tempFile.c_str());
    EXPECT_FALSE(FileUtils::CheckFileExists(tempFile));
}

TEST(TestUbseCliFileUtils, CheckDirectoryExists)
{
    std::string tempDir = CreateTempDir();
    EXPECT_TRUE(FileUtils::CheckDirectoryExists(tempDir));
    remove(tempDir.c_str());
    EXPECT_FALSE(FileUtils::CheckDirectoryExists(tempDir));
}

TEST(TestUbseCliFileUtils, IsSymlink)
{
    std::string tempFile = CreateTempFile();
    std::string symlink = CreateSymlink(tempFile);
    EXPECT_TRUE(FileUtils::IsSymlink(symlink));
    EXPECT_FALSE(FileUtils::IsSymlink(tempFile));
    remove(tempFile.c_str());
    remove(symlink.c_str());
}

TEST(TestUbseCliFileUtils, RegularFilePathWithBaseDir)
{
    std::string tempFile = CreateTempFile();
    std::string baseDir = "/tmp"; // 假设 baseDir 是 /tmp
    std::string errMsg;

    // 正常路径
    EXPECT_TRUE(FileUtils::RegularFilePath(tempFile, baseDir, errMsg));
    EXPECT_TRUE(errMsg.empty());

    // 路径不在 baseDir 中
    std::string outsideFile = "/etc/passwd";
    EXPECT_FALSE(FileUtils::RegularFilePath(outsideFile, baseDir, errMsg));
    EXPECT_FALSE(errMsg.empty());

    remove(tempFile.c_str());
}

TEST(TestUbseCliFileUtils, RegularFilePathNoBaseDir)
{
    std::string tempFile = CreateTempFile();
    std::string errMsg;

    // 正常路径
    EXPECT_TRUE(FileUtils::RegularFilePath(tempFile, errMsg));
    EXPECT_TRUE(errMsg.empty());

    // 不存在的文件
    std::string nonExistent = tempFile + "_invalid";
    EXPECT_FALSE(FileUtils::RegularFilePath(nonExistent, errMsg));
    EXPECT_FALSE(errMsg.empty());

    remove(tempFile.c_str());
}

TEST(TestUbseCliFileUtils, IsFileValid)
{
    std::string tempFile = CreateTempFile("test content");
    std::string errMsg;

    // 正常文件
    EXPECT_TRUE(FileUtils::IsFileValid(tempFile, errMsg));
    EXPECT_TRUE(errMsg.empty());

    // 空文件
    std::string emptyFile = CreateTempFile();
    EXPECT_TRUE(FileUtils::IsFileValid(emptyFile, errMsg));
    EXPECT_FALSE(errMsg.empty());

    // 文件过大（g_defaultMaxDataSize = 4294967296）
    std::string bigFile = CreateTempFile(std::string(4294967297, 'a'));
    EXPECT_FALSE(FileUtils::IsFileValid(bigFile, errMsg));
    EXPECT_FALSE(errMsg.empty());

    remove(tempFile.c_str());
    remove(emptyFile.c_str());
    remove(bigFile.c_str());
}

TEST(TestUbseCliFileUtils, IsFile)
{
    std::string tempFile = CreateTempFile();
    std::string tempDir = CreateTempDir();

    EXPECT_TRUE(FileUtils::IsFile(tempFile));
    EXPECT_FALSE(FileUtils::IsFile(tempDir));

    remove(tempFile.c_str());
    remove(tempDir.c_str());
}

TEST(TestUbseCliFileUtils, GetFileSizeFileNotExist)
{
    std::string nonExistentFile = "/tmp/nonexistent";
    std::string errMsg;
    size_t fileSize = FileUtils::GetFileSize(nonExistentFile);
    EXPECT_EQ(fileSize, 0);
}

TEST(TestUbseCliFileUtils, GetFileSizeFileIsSymlink)
{
    std::string tempFile = CreateTempFile("test");
    std::string symlink = CreateSymlink(tempFile);
    std::string errMsg;
    size_t fileSize = FileUtils::GetFileSize(symlink);
    EXPECT_EQ(fileSize, 0);
    remove(tempFile.c_str());
    remove(symlink.c_str());
}

TEST(TestUbseCliFileUtils, RegularFilePathExceedsPathMax)
{
    std::string longPath(PATH_MAX + 1, 'a');
    std::string baseDir = "/tmp";
    std::string errMsg;
    EXPECT_FALSE(FileUtils::RegularFilePath(longPath, baseDir, errMsg));
    EXPECT_FALSE(errMsg.empty());
}

TEST(TestUbseCliFileUtils, RegularFilePathIsSymlink)
{
    std::string tempFile = CreateTempFile("test");
    std::string symlink = CreateSymlink(tempFile);
    std::string baseDir = "/tmp";
    std::string errMsg;
    EXPECT_FALSE(FileUtils::RegularFilePath(symlink, baseDir, errMsg));
    EXPECT_FALSE(errMsg.empty());
    remove(tempFile.c_str());
    remove(symlink.c_str());
}

TEST(TestUbseCliFileUtils, RegularFilePathInvalidBaseDir)
{
    std::string tempFile = CreateTempFile("test");
    std::string baseDir = "";
    std::string errMsg;
    EXPECT_FALSE(FileUtils::RegularFilePath(tempFile, baseDir, errMsg));
    EXPECT_FALSE(errMsg.empty());
    remove(tempFile.c_str());
}

TEST(TestUbseCliFileUtils, CheckPermissionWriteOrExecute)
{
    std::string tempFile = CreateTempFile("test");
    chmod(tempFile.c_str(), 0755); // 有执行权限
    std::string errMsg;
    EXPECT_FALSE(FileUtils::CheckPermission(tempFile, 0400, true, errMsg));
    EXPECT_FALSE(errMsg.empty());
    chmod(tempFile.c_str(), 0644); // 恢复权限
    remove(tempFile.c_str());
}

TEST(TestUbseCliFileUtils, CheckFilePathExistFileNotExist)
{
    std::string nonExistentFile = "/tmp/nonexistent";
    std::string errMsg;
    EXPECT_FALSE(FileUtils::CheckFilePathExist(nonExistentFile, true));
}

TEST(TestUbseCliFileUtils, CheckFilePathExistIsDir)
{
    std::string tempDir = CreateTempDir();
    std::string errMsg;
    EXPECT_FALSE(FileUtils::CheckFilePathExist(tempDir, true));
    remove(tempDir.c_str());
}

TEST(TestUbseCliFileUtils, GetPathBeforeLastPartRoot)
{
    std::string path = "/";
    std::string result = FileUtils::GetPathBeforeLastPart(path);
    EXPECT_TRUE(result.empty());
}

TEST(TestUbseCliFileUtils, GetPathBeforeLastPartSingleFile)
{
    std::string path = "file.txt";
    std::string result = FileUtils::GetPathBeforeLastPart(path);
    EXPECT_TRUE(result.empty());
}

TEST(TestUbseCliFileUtils, GetPathBeforeLastPartMultiLevel)
{
    std::string path = "/tmp/test/dir/file.txt";
    std::string result = FileUtils::GetPathBeforeLastPart(path);
    EXPECT_EQ(result, "/tmp/test/dir");
}

TEST(TestUbseCliFileUtils, IsEndWith)
{
    std::string errMsg;
    EXPECT_TRUE(FileUtils::IsEndWith("example.txt", ".txt"));
    EXPECT_FALSE(FileUtils::IsEndWith("example.txt", ".md"));
    EXPECT_FALSE(FileUtils::IsEndWith("abc", "abcdef"));
    EXPECT_FALSE(FileUtils::IsEndWith("", ".txt"));
    EXPECT_FALSE(FileUtils::IsEndWith("example.txt", ""));
    EXPECT_TRUE(FileUtils::IsEndWith("hello", "hello"));
    EXPECT_FALSE(FileUtils::IsEndWith("hello.txt", "txt."));
}
} // namespace ubse::ut::cli