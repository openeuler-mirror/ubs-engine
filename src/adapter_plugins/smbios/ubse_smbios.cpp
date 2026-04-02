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

#include "ubse_smbios.h"
#include "ubse_smbios_def.h"
#include "ubse_logger.h"
#include "ubse_smbios_impl.h"

namespace ubse::adapter_plugins::smbios {
using namespace ubse::common::def;
using namespace ubse::log;

UBSE_DEFINE_THIS_MODULE("ubse");

UbseResult UbseSmbios::GetMeshType(UbseMeshType &meshType)
{
    auto basicInfo = impl::UbseSmbiosImpl::GetInstance().GetSmbiosTypeInfo<UbseSmbiosType::SUPER_POD_BASIC_INFO_T>();
    if (basicInfo == nullptr) {
        UBSE_LOG_ERROR << "Get super pod basic info failed";
        return UBSE_ERROR;
    }
    meshType = static_cast<UbseMeshType>(basicInfo->meshType);
    return UBSE_OK;
}
} // namespace ubse::adapter_plugins::smbios
