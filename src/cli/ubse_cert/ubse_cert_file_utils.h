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

#ifndef UBSE_CERT_FILE_UTIL_H
#define UBSE_CERT_FILE_UTIL_H

#include <sys/stat.h>
#include <string>
#include "common.h"

namespace ubse::cli::cert {
class FileUtils {
public:
    /* *
     * judge file exists
     * @param path: file full path
     * @param pattern: regex pattern
     */
    static bool CheckFileExists(const std::string &filePath);

    /* *
     * is directory exists.
     * @param dir directory
     * @return
     */
    static bool CheckDirectoryExists(const std::string &dirPath);

    /* * Check whether the destination path is a link
     * @param filePath raw file path
     * @return
     */
    static bool IsSymlink(const std::string &filePath);

    /* * Regular the file path using realPath.
     * @param filePath raw file path
     * @param baseDir file path must in base dir
     * @param errMsg the err msg
     * @return
     */
    static bool RegularFilePath(const std::string &filePath, const std::string &baseDir, std::string &errMsg);

    /* * Regular the file path using realPath.
     * @param filePath raw file path
     * @param errMsg the err msg
     * @return
     */
    static bool RegularFilePath(const std::string &filePath, std::string &errMsg);

    /* * Check the existence of the file and the size of the file.
     * @param configFile the input file path
     * @param errMsg the err msg
     * @return
     */
    static bool IsFileValid(const std::string &configFile, std::string &errMsg);

    static bool CheckPermission(const std::string &filePath, const mode_t &mode, bool onlyCurrentUserOp,
        std::string &errMsg);

    static bool CheckFilePathExist(std::string &filePath, bool isFile = true);

    static std::string GetPathBeforeLastPart(const std::string &path);

    static bool IsEndWith(const std::string &str, const std::string &suffix);

    static bool IsFile(const std::string &path);

    static size_t GetFileSize(const std::string &filePath);

    static bool CheckDataSize(long size);
};
}
#endif // UBSE_CERT_FILE_UTIL_H
