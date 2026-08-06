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
#include "lock/ubse_lock.h"

namespace ubse::urma {
using namespace ubse::log;
using namespace ubse::common::def;
using namespace ubse::context;

extern utils::ReadWriteLock g_invokeUrmaMutex;

CONDITION_DYNAMIC_CREATE(GetSceneType() == SceneType::COMMON, UbseUrmaUvsModule);

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

    uvsSetShareTopoInfo = (UvsSetShareTopoInfo)dlsym(handle, "uvs_set_share_topo_info");
    if (uvsSetShareTopoInfo == nullptr) {
        UBSE_LOG_WARN << "Failed to find symbol 'uvs_set_share_topo_info'";
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

    uvsGetEidSharing = (UvsGetEidSharing)dlsym(handle, "uvs_get_eid_sharing");
    if (uvsGetEidSharing == nullptr) {
        UBSE_LOG_WARN << "Failed to find symbol 'uvs_get_eid_sharing', set eidSharingModeEnabled=false";
    } else {
        // 共享模式在模块初始化时确定，运行期间保持不变，避免北向视图切换。
        bool enabled = false;
        const auto ret = uvsGetEidSharing(&enabled);
        if (ret != 0) {
            UBSE_LOG_WARN << "Failed to query URMA EID sharing mode, ret=" << ret
                          << ", set eidSharingModeEnabled=false";
        } else {
            eidSharingModeEnabled = enabled;
            UBSE_LOG_INFO << "URMA EID sharing mode enabled=" << static_cast<int>(eidSharingModeEnabled);
        }
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

bool UbseUrmaUvsModule::IsEidSharingModeEnabled() const
{
    return eidSharingModeEnabled;
}

void UbseUrmaUvsModule::Cleanup()
{
    ubse::utils::WriteLocker<utils::ReadWriteLock> writeLock(&g_invokeUrmaMutex);
    if (handle != nullptr) {
        dlclose(handle);
        handle = nullptr;
        uvsSetTopoInfo = nullptr;
        uvsSetShareTopoInfo = nullptr;
        uvsGetDeviceNameByUrmaEid = nullptr;
        uvsCreateAggrDev = nullptr;
        uvsDeleteAggrDev = nullptr;
    }
    uvsGetEidSharing = nullptr;
}
} // namespace ubse::urma
