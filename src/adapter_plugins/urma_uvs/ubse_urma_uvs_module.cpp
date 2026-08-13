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
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include "ubse_common_def.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_security.h"
#include "ubse_smbios.h"
#include "lock/ubse_lock.h"

namespace ubse::urma {
using namespace ubse::log;
using namespace ubse::common::def;
using namespace ubse::context;

extern utils::ReadWriteLock g_invokeUrmaMutex;

CONDITION_DYNAMIC_CREATE(GetSceneType() == SceneType::COMMON, UbseUrmaUvsModule);

UBSE_DEFINE_THIS_MODULE("ubse");

namespace {
constexpr auto EID_SHARING_FILE = "/sys/class/ubcore/ubcore/eid_sharing";
constexpr char EID_SHARING_ENABLED = '1';

UbseResult WriteEidSharingFile()
{
    const int fd = open(EID_SHARING_FILE, O_WRONLY);
    if (fd < 0) {
        UBSE_LOG_ERROR << "Failed to open URMA EID sharing file, file=" << EID_SHARING_FILE
                       << ", error=" << std::strerror(errno);
        return UBSE_ERROR_IO;
    }
    const auto written = write(fd, &EID_SHARING_ENABLED, sizeof(EID_SHARING_ENABLED));
    const int writeErrno = errno;
    (void)close(fd);
    if (written < 0) {
        UBSE_LOG_ERROR << "Failed to write URMA EID sharing file, file=" << EID_SHARING_FILE
                       << ", error=" << std::strerror(writeErrno);
        return UBSE_ERROR_IO;
    }
    if (written != static_cast<ssize_t>(sizeof(EID_SHARING_ENABLED))) {
        UBSE_LOG_ERROR << "Short write to URMA EID sharing file, file=" << EID_SHARING_FILE
                       << ", expected=" << sizeof(EID_SHARING_ENABLED) << ", actual=" << written;
        return UBSE_ERROR_IO;
    }
    return UBSE_OK;
}
} // namespace

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
    if (ubse::adapter_plugins::smbios::UbseSmbios::GetInstance().IsClosType()) {
        return UBSE_OK;
    }
    // 启动配置失败不阻塞 UBSE，后续北向请求会再次尝试配置。
    bool enabled = false;
    const auto ret = EnsureEidSharingConfigured(enabled);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to configure URMA EID sharing during startup, ret=" << ret;
    }
    return UBSE_OK;
}

void UbseUrmaUvsModule::Stop() {}

UbseResult UbseUrmaUvsModule::EnsureEidSharingConfigured(bool& enabled)
{
    enabled = false;
    // 只有1650v100支持EID共享，其他CPU保持原有一一对应模式。
    if (!ubse::adapter_plugins::smbios::UbseSmbios::GetInstance().Is1650V100Cpu()) {
        return UBSE_OK;
    }
    std::lock_guard<std::mutex> guard(eidSharingMutex);
    if (eidSharingConfigured) {
        enabled = true;
        return UBSE_OK;
    }
    const auto ret = ConfigureEidSharing();
    if (ret != UBSE_OK) {
        return ret;
    }
    // 配置状态只在本进程内保存，成功后不再重复写入sysfs。
    eidSharingConfigured = true;
    enabled = true;
    UBSE_LOG_INFO << "Configured URMA EID sharing successfully";
    return UBSE_OK;
}

UbseResult UbseUrmaUvsModule::ConfigureEidSharing()
{
    auto ret = ubse::security::ChangeOverrideCapability(true);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to add override capability before configuring URMA EID sharing, ret=" << ret;
        return ret;
    }
    ret = WriteEidSharingFile();
    const auto capabilityRet = ubse::security::ChangeOverrideCapability(false);
    if (capabilityRet != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to remove override capability after configuring URMA EID sharing, ret="
                       << capabilityRet;
    }
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to configure URMA EID sharing through sysfs, ret=" << ret;
    }
    return ret;
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
}
} // namespace ubse::urma
