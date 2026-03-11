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
#ifndef UBSE_NPU_SOURCE_DEF_H
#define UBSE_NPU_SOURCE_DEF_H
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>
#include "../../include/ubse_common_def.h"
#include "src/framework/misc/ubse_pack_util.h"
#include "ubse_common_def.h"
#include "ubse_error.h"
namespace ubse::npu::controller {
constexpr uint8_t UBSE_UB_DEVICE_GUID_SIZE = 32;
constexpr uint8_t UBSE_UB_UPI_STR_SIZE = 3;
constexpr uint8_t DEVICE_ID_SIZE = 3;

using namespace ubse::common::def;
using namespace ubse::utils;
// 资源类型枚举
enum class ResourceType {
    BUSINSTANCE = 1,
    NPU,
    NIC,
    UBCONTROLLER
};
struct UbDevice {
    ResourceType type;
    uint8_t slotId{};
    uint8_t chipId{};
    uint8_t index{};
};
struct UbaTidSize {
    uint64_t uba{};
    uint64_t size{};
    uint32_t tid{};
};

struct UbseAllocRequest {
    union {
        uint8_t upis[UBSE_UB_UPI_STR_SIZE]; // 租户对应的UPI 范围是[1, 0x7fff-1001]
        uint16_t upiStr;
    };
    std::string busInstanceGuid;
    std::vector<UbDevice> ubDevList;
};
// 抽象资源基类
class IResource {
public:
    virtual ~IResource() = default;

    virtual size_t CalculateSize() const = 0;
    virtual UbseResult Pack(UbsePackUtil &packUtil) = 0;
    virtual UbseResult Unpack(UbsePackUtil &packUtil) = 0;
    virtual ResourceType GetType() const = 0;
};

// Npu资源
class NpuResource : public IResource {
public:
    NpuResource() = default;
    size_t CalculateSize() const override;
    UbseResult Pack(UbsePackUtil &packUtil) override;
    UbseResult Unpack(UbsePackUtil &packUtil) override;
    ResourceType GetType() const override;

    void SetLoc(uint8_t slotId, uint8_t chipId, uint8_t index);
    void SetGuid(const std::string& npuGuid);
    void SetBusInstanceGuid(const std::string& busInstanceGuid);
    void AddAffinityDevice(const UbDevice& device);

private:
    ResourceType type_ = ResourceType::NPU;
    uint8_t slotId_{};
    uint8_t chipId_{};
    uint8_t index_{};
    std::string guid_;
    std::string busInstanceGuid_;
    std::vector<UbDevice> affinityDevices_;
};
// Businstance
class BusiResource : public IResource {
public:
    BusiResource() = default;
    ResourceType GetType() const override;
    size_t CalculateSize() const override;
    UbseResult Pack(UbsePackUtil &packUtil) override;
    UbseResult Unpack(UbsePackUtil &packUtil) override;
    void SetGuid(const std::string& npuGuid);
    void AddSubDevice(const UbDevice& device);

private:
    ResourceType type_ = ResourceType::BUSINSTANCE;
    std::string guid_;
    std::vector<UbDevice> subDevices_;
};
// Nic资源
class NicResource : public IResource {
public:
    NicResource() = default;
    NicResource(const std::string &guid, uint8_t slotId, uint8_t chipId, uint8_t index,
                const std::vector<UbDevice> &affinityDevices);
    ResourceType GetType() const override;
    size_t CalculateSize() const override;
    UbseResult Pack(UbsePackUtil &packUtil) override;
    UbseResult Unpack(UbsePackUtil &packUtil) override;
    void SetLoc(uint8_t slotId, uint8_t chipId, uint8_t index);
    void SetGuid(const std::string& npuGuid);
    void SetBusInstanceGuid(const std::string& busInstanceGuid);
    void AddAffinityDevice(const UbDevice& device);

private:
    ResourceType type_ = ResourceType::NIC;
    uint8_t slotId_{};
    uint8_t chipId_{};
    uint8_t index_{};
    std::string guid_;
    std::string busInstanceGuid_;
    std::vector<UbDevice> affinityDevices_;
};

// ubcontroller 资源
class UbCtrlResource : public IResource {
public:
    UbCtrlResource() = default;
    UbCtrlResource(const std::string &guid, const uint8_t slotId, const uint8_t chipId, const uint8_t index)
        : guid_(guid),
          slotId_(slotId),
          chipId_(chipId),
          index_(index)
    {
    }
    ResourceType GetType() const override;
    size_t CalculateSize() const override;
    UbseResult Pack(UbsePackUtil &packUtil) override;
    UbseResult Unpack(UbsePackUtil &packUtil) override;

private:
    ResourceType type_ = ResourceType::UBCONTROLLER;
    std::string guid_;
    uint8_t slotId_{};
    uint8_t chipId_{};
    uint8_t index_{};
};
} // namespace ubse::npu::controller
#endif // UBSE_NPU_SOURCE_DEF_H