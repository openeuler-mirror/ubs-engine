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
#include "ubse_npu_source_def.h"
#include <array>
#include "ubse_logger.h"
namespace ubse::npu::controller {
UBSE_DEFINE_THIS_MODULE("ubse");

std::array<uint8_t, UBSE_UB_DEVICE_GUID_SIZE> StringToArrayForGuid(const std::string &str)
{
    std::string newStr{};
    if (str.size() > UBSE_UB_DEVICE_GUID_SIZE) {
        UBSE_LOG_WARN << "There is truncate when str to guid";
        newStr = str.substr(0, UBSE_UB_DEVICE_GUID_SIZE);
    }
    std::array<uint8_t, UBSE_UB_DEVICE_GUID_SIZE> arr{};
    std::copy(newStr.begin(), newStr.end(), arr.begin());
    return arr;
}

ResourceType NpuResource::GetType() const
{
    return type_;
}
size_t NpuResource::CalculateSize() const
{
    size_t fixSize = sizeof(unsigned char) + sizeof(uint8_t) + sizeof(uint8_t) * DEVICE_ID_SIZE +
                     sizeof(uint8_t) * UBSE_UB_DEVICE_GUID_SIZE + sizeof(uint8_t) * UBSE_UB_DEVICE_GUID_SIZE;
    size_t flexibleArraySize = affinityDevices_.size() * (sizeof(unsigned char) + sizeof(uint8_t) * DEVICE_ID_SIZE);
    size_t head = sizeof(unsigned char);
    return fixSize + flexibleArraySize + head;
}
UbseResult NpuResource::Pack(UbsePackUtil &packUtil)
{
    if (!packUtil.UbsePackUChar(static_cast<unsigned char>(type_)))
        return UBSE_ERROR;
    if (!packUtil.UbsePackUChar(static_cast<unsigned char>(type_)))
        return UBSE_ERROR;
    if (!packUtil.UbsePackUint8(affinityDevices_.size()))
        return UBSE_ERROR;
    if (!packUtil.UbsePackUint8(slotId_))
        return UBSE_ERROR;
    if (!packUtil.UbsePackUint8(chipId_))
        return UBSE_ERROR;
    if (!packUtil.UbsePackUint8(index_))
        return UBSE_ERROR;
    for (auto id : StringToArrayForGuid(guid_)) {
        if (!packUtil.UbsePackUint8(id))
            return UBSE_ERROR;
    }
    for (auto id : StringToArrayForGuid(busInstanceGuid_)) {
        if (!packUtil.UbsePackUint8(id))
            return UBSE_ERROR;
    }
    for (auto &device : affinityDevices_) {
        if (!packUtil.UbsePackUChar(static_cast<unsigned char>(device.type)))
            return UBSE_ERROR;
        if (!packUtil.UbsePackUint8(device.slotId))
            return UBSE_ERROR;
        if (!packUtil.UbsePackUint8(device.chipId))
            return UBSE_ERROR;
        if (!packUtil.UbsePackUint8(device.index))
            return UBSE_ERROR;
    }
    return UBSE_OK;
}
UbseResult NpuResource::Unpack(UbsePackUtil &packUtil)
{
    return UBSE_OK;
}

void NpuResource::SetLoc(uint8_t slotId, uint8_t chipId, uint8_t index)
{
    slotId_ = slotId;
    chipId_ = chipId;
    index_ = index;
}

void NpuResource::SetGuid(const std::string &npuGuid)
{
    guid_ = npuGuid;
}
void NpuResource::SetBusInstanceGuid(const std::string &busInstanceGuid)
{
    busInstanceGuid_ = busInstanceGuid;
}

void NpuResource::AddAffinityDevice(const UbDevice &device)
{
    affinityDevices_.push_back(device);
}

ResourceType BusiResource::GetType() const
{
    return type_;
}
size_t BusiResource::CalculateSize() const
{
    size_t fixSize = sizeof(unsigned char) + sizeof(uint8_t) + sizeof(uint8_t) * UBSE_UB_DEVICE_GUID_SIZE;
    size_t flexibleArraySize = subDevices_.size() * (sizeof(unsigned char) + sizeof(uint8_t) * DEVICE_ID_SIZE);
    size_t head = sizeof(unsigned char);
    return fixSize + flexibleArraySize + head;
}
UbseResult BusiResource::Pack(UbsePackUtil &packUtil)
{
    if (!packUtil.UbsePackUChar(static_cast<unsigned char>(type_)))
        return UBSE_ERROR;
    if (!packUtil.UbsePackUChar(static_cast<unsigned char>(type_)))
        return UBSE_ERROR;
    if (!packUtil.UbsePackUint8(subDevices_.size()))
        return UBSE_ERROR;
    for (auto id : StringToArrayForGuid(guid_)) {
        if (!packUtil.UbsePackUint8(id))
            return UBSE_ERROR;
    }
    for (auto &device : subDevices_) {
        if (!packUtil.UbsePackUChar(static_cast<unsigned char>(device.type)))
            return UBSE_ERROR;
        if (!packUtil.UbsePackUint8(device.slotId))
            return UBSE_ERROR;
        if (!packUtil.UbsePackUint8(device.chipId))
            return UBSE_ERROR;
        if (!packUtil.UbsePackUint8(device.index))
            return UBSE_ERROR;
    }
    return UBSE_OK;
}
UbseResult BusiResource::Unpack(UbsePackUtil &packUtil)
{
    return UBSE_OK;
}
void BusiResource::SetGuid(const std::string &npuGuid)
{
    guid_ = std::move(npuGuid);
}
void BusiResource::AddSubDevice(const UbDevice &device)
{
    subDevices_.push_back(device);
}

NicResource::NicResource(const std::string &guid, const uint8_t slotId, const uint8_t chipId, const uint8_t index,
                         const std::vector<UbDevice> &affinityDevices)
    : slotId_(slotId),
      chipId_(chipId),
      index_(index),
      guid_(guid),
      affinityDevices_(affinityDevices)
{
}
ResourceType NicResource::GetType() const
{
    return type_;
}
size_t NicResource::CalculateSize() const
{
    size_t fixSize = sizeof(unsigned char) + sizeof(uint8_t) + sizeof(uint8_t) * DEVICE_ID_SIZE +
                     sizeof(uint8_t) * UBSE_UB_DEVICE_GUID_SIZE + sizeof(uint8_t) * UBSE_UB_DEVICE_GUID_SIZE;
    size_t flexibleArraySize = affinityDevices_.size() * (sizeof(unsigned char) + sizeof(uint8_t) * DEVICE_ID_SIZE);
    size_t head = sizeof(unsigned char);
    return fixSize + flexibleArraySize + head;
}
UbseResult NicResource::Pack(UbsePackUtil &packUtil)
{
    UBSE_LOG_INFO << "[NPU] slotId: " << slotId_ << ",affnityDevices slotId: " << affinityDevices_[0].slotId;
    if (!packUtil.UbsePackUChar(static_cast<unsigned char>(type_)))
        return UBSE_ERROR;
    if (!packUtil.UbsePackUChar(static_cast<unsigned char>(type_)))
        return UBSE_ERROR;
    if (!packUtil.UbsePackUint8(affinityDevices_.size()))
        return UBSE_ERROR;
    if (!packUtil.UbsePackUint8(slotId_))
        return UBSE_ERROR;
    if (!packUtil.UbsePackUint8(chipId_))
        return UBSE_ERROR;
    if (!packUtil.UbsePackUint8(index_))
        return UBSE_ERROR;
    for (auto id : StringToArrayForGuid(guid_)) {
        if (!packUtil.UbsePackUint8(id))
            return UBSE_ERROR;
    }
    for (auto id : StringToArrayForGuid(busInstanceGuid_)) {
        if (!packUtil.UbsePackUint8(id))
            return UBSE_ERROR;
    }
    for (auto &device : affinityDevices_) {
        if (!packUtil.UbsePackUChar(static_cast<unsigned char>(device.type)))
            return UBSE_ERROR;
        if (!packUtil.UbsePackUint8(device.slotId))
            return UBSE_ERROR;
        if (!packUtil.UbsePackUint8(device.chipId))
            return UBSE_ERROR;
        if (!packUtil.UbsePackUint8(device.index))
            return UBSE_ERROR;
    }
    return UBSE_OK;
}
UbseResult NicResource::Unpack(UbsePackUtil &packUtil)
{
    return UBSE_OK;
}
void NicResource::SetLoc(const uint8_t slotId, const uint8_t chipId, const uint8_t index)
{
    slotId_ = slotId;
    chipId_ = chipId;
    index_ = index;
}
void NicResource::SetGuid(const std::string &npuGuid)
{
    guid_ = npuGuid;
}
void NicResource::SetBusInstanceGuid(const std::string &busInstanceGuid)
{
    busInstanceGuid_ = busInstanceGuid;
}
void NicResource::AddAffinityDevice(const UbDevice &device)
{
    affinityDevices_.push_back(device);
}

ResourceType UbCtrlResource::GetType() const
{
    return type_;
}
size_t UbCtrlResource::CalculateSize() const
{
    size_t fixSize =
        sizeof(unsigned char) + sizeof(uint8_t) * UBSE_UB_DEVICE_GUID_SIZE + sizeof(uint8_t) * DEVICE_ID_SIZE;
    size_t headSize = sizeof(unsigned char);
    return fixSize + headSize;
}
UbseResult UbCtrlResource::Pack(UbsePackUtil &packUtil)
{
    UBSE_LOG_INFO << "[UbCtrl] pack size = " << CalculateSize()
                  << ", actually size: " << CalculateSize() - sizeof(size_t) << ", slotId: " << slotId_;
    if (!packUtil.UbsePackUChar(static_cast<unsigned char>(type_)))
        return UBSE_ERROR;
    if (!packUtil.UbsePackUChar(static_cast<unsigned char>(type_)))
        return UBSE_ERROR;
    for (auto id : StringToArrayForGuid(guid_)) {
        if (!packUtil.UbsePackUint8(id))
            return UBSE_ERROR;
    }
    if (!packUtil.UbsePackUint8(slotId_))
        return UBSE_ERROR;
    if (!packUtil.UbsePackUint8(chipId_))
        return UBSE_ERROR;
    if (!packUtil.UbsePackUint8(index_))
        return UBSE_ERROR;
    return UBSE_OK;
}
UbseResult UbCtrlResource::Unpack(UbsePackUtil &packUtil)
{
    return UBSE_OK;
}
} // namespace ubse::npu::controller