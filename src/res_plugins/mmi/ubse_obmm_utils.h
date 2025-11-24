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

#ifndef UBSE_MANAGER_RM_OBMM_UTILS_H
#define UBSE_MANAGER_RM_OBMM_UTILS_H

#include <ostream>
#include "ubse_common_def.h"
#include "ubse_mem_types.h"
#include "ubse_mem_obj.h"
#include "ubse_mem_obmm_def.h"
#include "ubse_mem_constants.h"

namespace ubse::mmi {
using namespace ubse::mem::obj;
using namespace ubse::common::def;

class RmObmmUtils {
public:
    static void CopyObmmMemDescValue(const ubse_mem_obmm_mem_desc &src, obmm_mem_desc *des, uint64_t hpa);
    static void CopyObmmMemDescValue(const obmm_mem_desc *src, ubse_mem_obmm_mem_desc &des);
    static bool ParsePreOnlineEidStr(const std::string &eid, uint32_t &value);
    static void ConstructUbMemPrivData(UbMemPrivData &ubPrivData, uint16_t marId);
    // 构造，导出时需要的结构,用完后必须生动free释放
    static obmm_mem_desc *ConstructExportMemDesc(const UbseMemLocalObmmCustomMeta &customMeta);
    static obmm_mem_desc *ConstructImportMemDesc(const UbseMemLocalObmmCustomMeta &customMeta,
                                                 const ubse_mem_obmm_mem_desc &desc);

    static UbseResult GetCustomMetaFromNumaExportObj(const UbseMemNumaBorrowExportObj &exportObj,
                                                     UbseMemLocalObmmCustomMeta &customMeta);

    static UbseResult GetCustomMetaFromFdExportObj(const UbseMemFdBorrowExportObj &exportObj,
                                                   UbseMemLocalObmmCustomMeta &customMeta);

    static UbseResult GetCustomMetaFromNumaImportObj(const UbseMemNumaBorrowImportObj &importObj,
                                                     UbseMemLocalObmmCustomMeta &customMeta);

    static UbseResult GetCustomMetaFromFdImportObj(const UbseMemFdBorrowImportObj &importObj,
                                                   UbseMemLocalObmmCustomMeta &customMeta);

    static UbseResult CopyUbseMemAlgoResult(const UbseMemAlgoResult &algoResult, const std::string &name,
                                            UbseMemLocalObmmCustomMeta &customMeta);

    static bool SplitObmmExportSize(const size_t size[MAX_NUMA_NODES], size_t blockSize,
                                    std::vector<std::array<size_t, MAX_NUMA_NODES>> &result);

    static uint32_t GetBlockSize(uint64_t &blockSize);

    static RmObmmUtils &GetInstance()
    {
        static RmObmmUtils instance;
        return instance;
    }
    RmObmmUtils(const RmObmmUtils &other) = delete;
    RmObmmUtils(RmObmmUtils &&other) = delete;
    RmObmmUtils &operator=(const RmObmmUtils &other) = delete;
    RmObmmUtils &operator=(RmObmmUtils &&other) noexcept = delete;

private:
    RmObmmUtils() = default;
};
} // namespace ubse::mmi

#endif // UBSE_MANAGER_RM_OBMM_UTILS_H
