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
#include "ubse_mmi_interface.h"
#include "ubse_mmi_obmm_def.h"
#include "ubse_mem_common_utils.h"
#include "ubse_mem_types.h"

namespace ubse::mmi {
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::common::def;

obmm_mem_desc *ConstructExportMemDesc(const UbseMemLocalObmmCustomMeta &customMeta,
    const UbMemPrivData &ubMemPrivData);
obmm_mem_desc *ConstructImportMemDesc(const ObmmOpParam &opParam, const ubse_mem_obmm_mem_desc &desc);

obmm_preimport_info *ConstructPreImportInfo(const BasicPreImportInfo &basicPreImportInfo);

UbseResult GetCustomMetaFromNumaExportObj(const UbseMemNumaBorrowExportObj &exportObj,
    UbseMemLocalObmmCustomMeta &customMeta);

UbseResult GetCustomMetaFromFdExportObj(const UbseMemFdBorrowExportObj &exportObj,
    UbseMemLocalObmmCustomMeta &customMeta);

UbseResult GetCustomMetaFromShmExportObj(const UbseMemShareBorrowExportObj &exportObj,
    UbseMemLocalObmmCustomMeta &customMeta);

UbseResult GetCustomMetaFromAddrExportObj(const UbseMemAddrBorrowExportObj &exportObj,
    UbseMemLocalObmmCustomMeta &customMeta);

UbseResult GetCustomMetaFromNumaImportObj(const UbseMemNumaBorrowImportObj &importObj,
    UbseMemLocalObmmCustomMeta &customMeta);

UbseResult GetCustomMetaFromFdImportObj(const UbseMemFdBorrowImportObj &importObj,
    UbseMemLocalObmmCustomMeta &customMeta);

UbseResult GetCustomMetaFromShmImportObj(const UbseMemShareBorrowImportObj &importObj,
    UbseMemLocalObmmCustomMeta &customMeta);

UbseResult GetCustomMetaFromAddrImportObj(const UbseMemAddrBorrowImportObj &importObj,
    UbseMemLocalObmmCustomMeta &customMeta, int index);

class RmObmmUtils {
public:
    static RmObmmUtils &GetInstance()
    {
        static RmObmmUtils instance;
        return instance;
    }

    static UbseResult GetPreOnlineInfo(std::vector<BasicPreImportInfo> &basicPreImportInfos);

    uint32_t GetPreOnlineSwitch(bool &preOnlineSwitch);

    RmObmmUtils(const RmObmmUtils &other) = delete;
    RmObmmUtils(RmObmmUtils &&other) = delete;
    RmObmmUtils &operator=(const RmObmmUtils &other) = delete;
    RmObmmUtils &operator=(RmObmmUtils &&other) noexcept = delete;

private:
    RmObmmUtils() = default;
    static UbseResult GetBasicPreImportInfos(std::vector<BasicPreImportInfo> &basicPreImportInfos, std::ifstream &file,
                                             const std::vector<std::string> &tokens);
};
} // namespace ubse::mmi

#endif // UBSE_MANAGER_RM_OBMM_UTILS_H
