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
    std::array<uint8_t, UBSE_UB_DEVICE_GUID_SIZE> arr{};
    size_t copySize = std::min(str.size(), arr.size());
    std::copy(str.begin(), str.begin() + copySize, arr.begin());
    return arr;
}

ResourceType NpuResource::GetType() const
{
    return type_;
}
size_t NpuResource::CalculateSize() const
{
    size_t fixSize = sizeof(unsigned char) + sizeof(uint8_t) + sizeof(uint8_t) * NPU_DEVICE_ID_SIZE +
                     sizeof(uint8_t) * UBSE_UB_DEVICE_GUID_SIZE + sizeof(uint8_t) * UBSE_UB_DEVICE_GUID_SIZE;
    size_t flexibleArraySize = affinityDevices_.size() * (sizeof(unsigned char) + sizeof(uint8_t) * UB_DEVICE_ID_SIZE);
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
        if (!packUtil.UbsePackUint8(device.dieId))
            return UBSE_ERROR;
        if (!packUtil.UbsePackUint8(device.pfId))
            return UBSE_ERROR;
        if (!packUtil.UbsePackUint8(device.vfId))
            return UBSE_ERROR;
    }
    return UBSE_OK;
}
UbseResult NpuResource::Unpack(UbsePackUtil &packUtil)
{
    return UBSE_OK;
}

void NpuResource::SetLoc(uint8_t slotId, uint8_t chipId)
{
    slotId_ = slotId;
    chipId_ = chipId;
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
    size_t flexibleArraySize = subDevices_.size() * (sizeof(unsigned char) + sizeof(uint8_t) * UB_DEVICE_ID_SIZE);
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
        if (!packUtil.UbsePackUint8(device.dieId))
            return UBSE_ERROR;
        if (!packUtil.UbsePackUint8(device.pfId))
            return UBSE_ERROR;
        if (!packUtil.UbsePackUint8(device.vfId))
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
    guid_ = npuGuid;
}
void BusiResource::AddSubDevice(const UbDevice &device)
{
    subDevices_.push_back(device);
}

ResourceType NicPfeResource::GetType() const
{
    return type_;
}
size_t NicPfeResource::CalculateSize() const
{
    size_t fixSize = sizeof(unsigned char) + sizeof(uint8_t) + sizeof(uint8_t) * NIC_PF_DEVICE_ID_SIZE +
                     sizeof(uint8_t) * UBSE_UB_DEVICE_GUID_SIZE + sizeof(uint8_t) * UBSE_UB_DEVICE_GUID_SIZE;
    size_t flexibleArraySize = affinityDevices_.size() * (sizeof(unsigned char) + sizeof(uint8_t) * UB_DEVICE_ID_SIZE);
    size_t head = sizeof(unsigned char);
    return fixSize + flexibleArraySize + head;
}
UbseResult NicPfeResource::Pack(UbsePackUtil &packUtil)
{
    UBSE_LOG_INFO << "[NIC_PFE] slotId: " << slotId_ << ",affnityDevices slotId: " << affinityDevices_[0].slotId;
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
    if (!packUtil.UbsePackUint8(pfId_))
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
        if (!packUtil.UbsePackUint8(device.dieId))
            return UBSE_ERROR;
        if (!packUtil.UbsePackUint8(device.pfId))
            return UBSE_ERROR;
        if (!packUtil.UbsePackUint8(device.vfId))
            return UBSE_ERROR;
    }
    return UBSE_OK;
}
UbseResult NicPfeResource::Unpack(UbsePackUtil &packUtil)
{
    return UBSE_OK;
}
void NicPfeResource::SetLoc(const uint8_t slotId, const uint8_t chipId, const uint8_t pfId)
{
    slotId_ = slotId;
    chipId_ = chipId;
    pfId_ = pfId;
}
void NicPfeResource::SetGuid(const std::string &npuGuid)
{
    guid_ = npuGuid;
}
void NicPfeResource::SetBusInstanceGuid(const std::string &busInstanceGuid)
{
    busInstanceGuid_ = busInstanceGuid;
}
void NicPfeResource::AddAffinityDevice(const UbDevice &device)
{
    affinityDevices_.push_back(device);
}

ResourceType NicVfeResource::GetType() const
{
    return type_;
}
size_t NicVfeResource::CalculateSize() const
{
    size_t fixSize = sizeof(unsigned char) + sizeof(uint8_t) + sizeof(uint8_t) * NIC_VFE_DEVICE_ID_SIZE +
                     sizeof(uint8_t) * UBSE_UB_DEVICE_GUID_SIZE + sizeof(uint8_t) * UBSE_UB_DEVICE_GUID_SIZE;
    size_t flexibleArraySize = affinityDevices_.size() * (sizeof(unsigned char) + sizeof(uint8_t) * UB_DEVICE_ID_SIZE);
    size_t head = sizeof(unsigned char);
    return fixSize + flexibleArraySize + head;
}
UbseResult NicVfeResource::Pack(UbsePackUtil &packUtil)
{
    UBSE_LOG_INFO << "[NIC_PFE] slotId: " << slotId_ << ",affnityDevices slotId: " << affinityDevices_[0].slotId;
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
    if (!packUtil.UbsePackUint8(pfId_))
        return UBSE_ERROR;
    if (!packUtil.UbsePackUint8(vfId_))
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
        if (!packUtil.UbsePackUint8(device.dieId))
            return UBSE_ERROR;
        if (!packUtil.UbsePackUint8(device.pfId))
            return UBSE_ERROR;
        if (!packUtil.UbsePackUint8(device.vfId))
            return UBSE_ERROR;
    }
    return UBSE_OK;
}
UbseResult NicVfeResource::Unpack(UbsePackUtil &packUtil)
{
    return UBSE_OK;
}
void NicVfeResource::SetLoc(const uint8_t slotId, const uint8_t chipId, const uint8_t pfId, const uint8_t vfId)
{
    slotId_ = slotId;
    chipId_ = chipId;
    pfId_ = pfId;
    vfId_ = vfId;
}
void NicVfeResource::SetGuid(const std::string &guid)
{
    guid_ = guid;
}
void NicVfeResource::SetBusInstanceGuid(const std::string &busInstanceGuid)
{
    busInstanceGuid_ = busInstanceGuid;
}
void NicVfeResource::AddAffinityDevice(const UbDevice &device)
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
        sizeof(unsigned char) + sizeof(uint8_t) * UBSE_UB_DEVICE_GUID_SIZE + sizeof(uint8_t) * UB_CTRL_DEVICE_ID_SIZE;
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
    if (!packUtil.UbsePackUint8(dieId_))
        return UBSE_ERROR;
    return UBSE_OK;
}
UbseResult UbCtrlResource::Unpack(UbsePackUtil &packUtil)
{
    return UBSE_OK;
}
} // namespace ubse::npu::controller