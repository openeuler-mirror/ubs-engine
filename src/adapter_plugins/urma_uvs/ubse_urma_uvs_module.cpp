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

#include "ubse_urma_uvs_module.h"

#include <dlfcn.h>
#include "ubse_common_def.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"

namespace ubse::urma {
using namespace ubse::log;
using namespace ubse::common::def;

DYNAMIC_CREATE(UbseUrmaUvsModule);

UBSE_DEFINE_THIS_MODULE("ubse");

UbseResult UbseUrmaUvsModule::Initialize()
{
    Cleanup();
    handle = dlopen("libtpsa.so", RTLD_LAZY);
    if (handle == nullptr) {
        UBSE_LOG_ERROR << "dlopen libtpsa.so failed";
        return UBSE_ERROR_FILE_NOT_EXIST;
    }

    uvsSetTopoInfo = (UvsSetTopoInfo)dlsym(handle, "uvs_set_topo_info");
    if (uvsSetTopoInfo == nullptr) {
        UBSE_LOG_WARN << "Failed to find symbol 'uvs_set_topo_info'";
    }

    uvsGetDeviceNameByUrmaEid = (UvsGetDeviceNameByUrmaEid)dlsym(handle, "uvs_get_device_name_by_eid");
    if (uvsGetDeviceNameByUrmaEid == nullptr) {
        UBSE_LOG_WARN << "Failed to find symbol 'uvs_get_device_name_by_eid'";
    }

    uvsCreateAggrDev = (UvsCreateAggrDev)dlsym(handle, "uvs_create_agg_dev");
    if (uvsCreateAggrDev == nullptr) {
        UBSE_LOG_WARN << "Failed to find symbol 'uvs_create_agg_dev'";
    }

    uvsDeleteAggrDev = (UvsDeleteAggrDev)dlsym(handle, "uvs_delete_agg_dev");
    if (uvsDeleteAggrDev == nullptr) {
        UBSE_LOG_WARN << "Failed to find symbol 'uvs_delete_agg_dev'";
    }

    if (uvsSetTopoInfo == nullptr || uvsGetDeviceNameByUrmaEid == nullptr || uvsCreateAggrDev == nullptr ||
        uvsDeleteAggrDev == nullptr) {
        UBSE_LOG_WARN << "Failed to find symbol in libtpsa.so";
    }
    return UBSE_OK;
}

void UbseUrmaUvsModule::UnInitialize()
{
    Cleanup();
}

UbseResult UbseUrmaUvsModule::Start()
{
    return UBSE_OK;
}

void UbseUrmaUvsModule::Stop() {}

void UbseUrmaUvsModule::Cleanup()
{
    if (handle != nullptr) {
        dlclose(handle);
        handle = nullptr;
        uvsSetTopoInfo = nullptr;
        uvsGetDeviceNameByUrmaEid = nullptr;
        uvsCreateAggrDev = nullptr;
        uvsDeleteAggrDev = nullptr;
    }
}
} // namespace ubse::urma