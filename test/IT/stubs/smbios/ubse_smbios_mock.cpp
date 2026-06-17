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

/*
 * SMBIOS mock for IT testing.
 *
 * Overrides UbseSmbios facade methods to return test data from environment
 * variables. This file is compiled into ubse_it_daemon (a separate IT test
 * daemon binary) via --whole-archive to override the real ubse_smbios symbols.
 *
 * Environment variables (read ONLY by this stub, never by business code):
 *   UBSE_IT_MESH_TYPE    - 组网类型 ("1"=FULL_MESH, "8"=CLOS)
 *   UBSE_IT_POD_ID       - 超节点ID (e.g. "1")
 *   UBSE_IT_SUPER_POD_ID - 二层超节点ID (e.g. "1")
 *   UBSE_IT_SERVER_IDX   - 服务器索引 (e.g. "0")
 *
 * BUSINESS CODE IS NEVER MODIFIED. All IT-specific behavior is in this file only.
 */

#include "ubse_error.h"
#include "ubse_smbios.h"

#include <cstdlib>

namespace ubse::adapter_plugins::smbios {

static int32_t GetEnvInt(const char* var, int32_t defaultVal)
{
    const char* v = getenv(var);
    return v ? std::atoi(v) : defaultVal;
}

uint32_t UbseSmbios::GetMeshType(UbseMeshType& meshType)
{
    meshType = static_cast<UbseMeshType>(GetEnvInt("UBSE_IT_MESH_TYPE", 1));
    return UBSE_OK;
}

bool UbseSmbios::IsClosType()
{
    return static_cast<UbseMeshType>(GetEnvInt("UBSE_IT_MESH_TYPE", 1)) == UbseMeshType::CLOS;
}

uint32_t UbseSmbios::GetSuperPodId(uint16_t& superPodId)
{
    superPodId = static_cast<uint16_t>(GetEnvInt("UBSE_IT_SUPER_POD_ID", 1));
    return UBSE_OK;
}

uint32_t UbseSmbios::GetPodId(uint16_t& podId)
{
    podId = static_cast<uint16_t>(GetEnvInt("UBSE_IT_POD_ID", 1));
    return UBSE_OK;
}

uint32_t UbseSmbios::GetServerIdx(uint32_t& serverIdx)
{
    serverIdx = static_cast<uint32_t>(GetEnvInt("UBSE_IT_SERVER_IDX", 0));
    return UBSE_OK;
}

} // namespace ubse::adapter_plugins::smbios
