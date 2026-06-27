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

/**
 * @file urma_uvs_stub.cpp
 * @brief Stub implementation of URMA UVS library for IT testing.
 *
 * This shared library replaces the real URMA UVS library when running IT tests.
 * All operations return success (0) by default.
 */

#include <cstdint>
#include <cstring>

extern "C" {

uint32_t uvs_set_topo_info(void* topo, uint32_t topo_size, uint32_t topNum)
{
    return 0;
}

uint32_t uvs_get_device_name_by_eid(char* urmaEid, char* buf, size_t len)
{
    if (buf != nullptr && len > 0) {
        strncpy(buf, "stub_dev", len);
        buf[len - 1] = '\0';
    }
    return 0;
}

uint32_t uvs_create_agg_dev(char* aggrDevEid, const char* aggrDevName)
{
    return 0;
}

uint32_t uvs_delete_agg_dev(char* aggrDevEid)
{
    return 0;
}
}