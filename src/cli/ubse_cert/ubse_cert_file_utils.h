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

#include <string>

namespace ubse::cli::cert {
class FileUtils {
public:
    /** Check whether the destination path is a link
     * @param filePath raw file path
     * @return
     */
    static bool IsSymlink(const std::string& filePath);

    /** Check whether the destination path is a hard link
     * @param filePath raw file path
     * @return
     */
    static bool IsHardLink(const std::string& filePath);

    /** Check whether the destination path is canonical.
     * @param filePath raw file path
     * @param errMsg the err msg
     * @return
     */
    static bool IsCanonicalPath(const std::string& filePath, std::string& errMsg);
};
} // namespace ubse::cli::cert
#endif // UBSE_CERT_FILE_UTIL_H
