/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef PAGE_FILE_HELPER_H
#define PAGE_FILE_HELPER_H

#include <fstream>
#include <string>
#include "ubse_logger.h"

#include "mp_configuration.h"
#include "mp_error.h"
#include "over_commit_def.h"
#include "ubse_file_util.h"

namespace mempooling::over_commit {

class PageFileHelper {
public:
    static MpResult GetHugePageCanonicalPath(const std::string& remoteNumaId, std::string& filePath);

    static MpResult AllocateHugePages(const std::vector<MemBorrowInfoWithSrc> &memBorrowInfoWithSrcs);

    static MpResult GetOriginalHugePages(const std::string& filePath, uint64_t& originalHugePages);

    static MpResult RewriteHugePages(const std::string& realPath, uint64_t borrowSize);

    static MpResult RewriteHugePagesWithRetry(const std::string& filePath, uint64_t& originalHugePages,
                                  const uint16_t fst, const uint64_t snd, const int retryCount);

private:
    static std::string HUGEPAGES_PATH_HEAD;
    static std::string HUGEPAGES_PATH_TAIL;
};
}  // namespace mempooling::over_commit

#endif  // PAGE_FILE_HELPER_H
