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
#include <algorithm>
#include <cctype>
#include <cstdint>
#include "ubse_logger.h"
#include "ubse_smbios_def.h"
#include "ubse_smbios_impl.h"

namespace ubse::adapter_plugins::smbios {
using namespace ubse::common::def;
using namespace ubse::log;

// QEMU虚拟机（仿真环境）的系统制造商标识，小写形式用于大小写不敏感匹配
const std::string QEMU_MANUFACTURER = "qemu";

UBSE_DEFINE_THIS_MODULE("ubse");

UbseResult UbseSmbios::GetMeshType(UbseMeshType& meshType)
{
    auto basicInfo = impl::UbseSmbiosImpl::GetInstance().GetSmbiosTypeInfo<UbseSmbiosType::SUPER_POD_BASIC_INFO_T>();
    if (basicInfo == nullptr) {
        UBSE_LOG_ERROR << "Get super pod basic info failed";
        return UBSE_ERROR;
    }
    meshType = static_cast<UbseMeshType>(basicInfo->meshType);
    return UBSE_OK;
}

// 判断是否为CLOS组网，获取失败时默认为FULL_MESH组网，即默认返回false
bool UbseSmbios::IsClosType()
{
    auto basicInfo = impl::UbseSmbiosImpl::GetInstance().GetSmbiosTypeInfo<UbseSmbiosType::SUPER_POD_BASIC_INFO_T>();
    if (basicInfo == nullptr) {
        UBSE_LOG_ERROR << "Get super pod basic info failed";
        return false;
    }
    auto meshType = static_cast<UbseMeshType>(basicInfo->meshType);
    return meshType == UbseMeshType::CLOS;
}

UbseResult UbseSmbios::GetSystemManufacturer(std::string& manufacturer)
{
    auto sysInfo = impl::UbseSmbiosImpl::GetInstance().GetSmbiosTypeInfo<UbseSmbiosType::TYPE_1>();
    if (sysInfo == nullptr) {
        UBSE_LOG_ERROR << "Get system information failed";
        return UBSE_ERROR;
    }
    if (sysInfo->manufacturer.empty()) {
        UBSE_LOG_ERROR << "SMBIOS system manufacturer is empty";
        return UBSE_ERROR;
    }
    manufacturer = sysInfo->manufacturer;
    return UBSE_OK;
}

// 判断是否为QEMU虚拟机（仿真环境），获取失败时默认为非QEMU，即默认返回false
bool UbseSmbios::IsQemuVm()
{
    std::string manufacturer;
    if (GetSystemManufacturer(manufacturer) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get system manufacturer for QEMU check";
        return false;
    }
    std::transform(manufacturer.begin(), manufacturer.end(), manufacturer.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return manufacturer == QEMU_MANUFACTURER;
}

bool UbseSmbios::Is1650V100Cpu()
{
    auto cpuInfo = impl::UbseSmbiosImpl::GetInstance().GetSmbiosTypeInfo<UbseSmbiosType::TYPE_4>();
    if (cpuInfo == nullptr) {
        UBSE_LOG_ERROR << "Failed to get SMBIOS processor information";
        return false;
    }
    if (cpuInfo->version.empty()) {
        UBSE_LOG_ERROR << "SMBIOS processor version is empty";
        return false;
    }
    auto normalizedVersion = cpuInfo->version;
    std::transform(normalizedVersion.begin(), normalizedVersion.end(), normalizedVersion.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return normalizedVersion.find("kunpeng 950 7592c") != std::string::npos ||
           normalizedVersion.find("1650v100") != std::string::npos;
}

UbseResult UbseSmbios::GetSuperPodId(uint16_t& superPodId)
{
    auto basicInfo = impl::UbseSmbiosImpl::GetInstance().GetSmbiosTypeInfo<UbseSmbiosType::SUPER_POD_BASIC_INFO_T>();
    if (basicInfo == nullptr) {
        UBSE_LOG_ERROR << "Get super pod basic info failed";
        return UBSE_ERROR;
    }
    superPodId = static_cast<uint16_t>(basicInfo->superPodId);
    return UBSE_OK;
}

UbseResult UbseSmbios::GetPodId(uint16_t& podId)
{
    auto basicInfo = impl::UbseSmbiosImpl::GetInstance().GetSmbiosTypeInfo<UbseSmbiosType::SUPER_POD_BASIC_INFO_T>();
    if (basicInfo == nullptr) {
        UBSE_LOG_ERROR << "Get super pod basic info failed";
        return UBSE_ERROR;
    }
    podId = static_cast<uint16_t>(basicInfo->podId);
    return UBSE_OK;
}

UbseResult UbseSmbios::GetServerIdx(uint32_t& serverIdx)
{
    auto basicInfo = impl::UbseSmbiosImpl::GetInstance().GetSmbiosTypeInfo<UbseSmbiosType::SUPER_POD_BASIC_INFO_T>();
    if (basicInfo == nullptr) {
        UBSE_LOG_ERROR << "Get super pod basic info failed";
        return UBSE_ERROR;
    }
    // 计算serverIdx，serverIdx从0开始，slotId从1开始
    serverIdx = basicInfo->serverIdx;
    return UBSE_OK;
}
} // namespace ubse::adapter_plugins::smbios
