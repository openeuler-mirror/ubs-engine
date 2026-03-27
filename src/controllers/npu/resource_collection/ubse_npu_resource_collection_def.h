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
#ifndef UBSE_NPU_RESOURCE_COLLECTION_DEF_H
#define UBSE_NPU_RESOURCE_COLLECTION_DEF_H

#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <type_traits>
#include <vector>

#include "adapter_plugins/mti/ubse_mti_bus_instance.h"
#include "src/framework/misc/ubse_str_util.h"
#include "ubse_common_def.h"
#include "ubse_logger.h"
namespace ubse::npu::controller {
using namespace ubse::mti::bus_instance;
using namespace ubse::common::def;
using CollectionDevId = std::string;
using CollectionGuid = std::string;
using CollectionUpi = std::string;
using namespace ubse::log;

constexpr uint8_t MAX_DEVICE_NUM = 0xff;
constexpr uint8_t SPECIAL_FE_VAL = 0xff;
enum class CollectionDeviceType : uint8_t {
    HOST_BUSINSTANCE = 0,
    VM_BUSINSTANCE = 1,
    NPU = 2,
    NIC = 3,
    UBCONTROLLER = 4,
    P_IDEV = 5,
    V_IDEV = 6,
    COLLECTION_DEVICE_TYPE_COUNT = 7
};
enum class CollectionState {
    WAIT_INIT, // 未初始化
    RUNNING,   // 正在采集资源
    FINISH     // 完成资源采集，可访问设备
};

// 基础device数据结构
struct CollectDeviceLoc {
    uint8_t slotId;
    uint8_t chipId;
    uint8_t dieId;
    uint8_t pfeId;
    uint8_t vfeId;
    UbseMtiEid eid;
    CollectionGuid guid;
    CollectionUpi upi;
    CollectDeviceLoc();
    CollectDeviceLoc(const UbseMtiEid &, const CollectionGuid &, const CollectionUpi &);
};

class CollectionStringUtil {
public:
    template <typename... Args>
    static CollectionDevId CollectionJoinStr(Args... args)
    {
        static_assert(sizeof...(Args) > 0, "At least one argument is required");
        if constexpr (all_same_v<uint8_t, Args...>()) {
            std::ostringstream oss;
            ((oss << (oss.tellp() == 0 ? "" : "-") << std::to_string(args)), ...);
            return oss.str();
        } else {
            static_assert(all_same_v<uint8_t, Args...>(), "All arguments must be either uint8_t or CollectionDevId");
            return "";
        }
    }

    static CollectionDevId GuidToStr(const UbseMtiGuid &guid);

private:
    static CollectionDevId CollectionJoinStr()
    {
        return "";
    }

    template <typename T, typename... Ts>
    static constexpr bool all_same_v()
    {
        return (std::is_same_v<T, Ts> && ...);
    }
};

class CollectionDevice : std::enable_shared_from_this<CollectionDevice> {
public:
    CollectionDevice(CollectDeviceLoc devLoc, CollectionDeviceType devType);
    CollectionDevice(const CollectionDevId &guid, const UbseMtiEid &eid, const CollectionUpi &upi,
                     CollectionDeviceType devType);
    virtual ~CollectionDevice() = default;
    virtual CollectionDevId GetIdStr() = 0;

    void SetEid(const UbseMtiEid &eid);
    void SetGuid(const CollectionDevId &guid);
    void SetType(const CollectionDeviceType devType);
    CollectionDevId GetGuid() const;
    UbseMtiEid GetEid() const;
    CollectionDeviceType GetType() const;
    static CollectionDevId GetDevIdByDevLoc(const CollectDeviceLoc &devLoc, const CollectionDeviceType type);

    CollectDeviceLoc GetDeviceLoc();
    template <class T, template <class> class PtrT>
    static std::shared_ptr<T> CollectionToDerived(PtrT<CollectionDevice> baseDev)
    {
        static_assert(std::is_base_of_v<CollectionDevice, T>, "T must be derived from Base");
        static_assert(std::is_same_v<PtrT<CollectionDevice>, std::shared_ptr<CollectionDevice>>,
                      "Param must be shared_ptr<Base>");
        return std::dynamic_pointer_cast<T>(baseDev);
    }
    template <class T>
    static std::shared_ptr<CollectionDevice> CollectionToBase(std::shared_ptr<T> derivedDev)
    {
        static_assert(std::is_base_of_v<CollectionDevice, T>, "T must be derived from Base");
        return std::static_pointer_cast<CollectionDevice>(derivedDev);
    }

protected:
    CollectDeviceLoc dev;
    CollectionDeviceType type;
};

class CollectionDeviceUbCtrl;
class CollectionDeviceBusi;
class CollectionDeviceIdevPfe;
class CollectionDeviceIdevVfe;
class CollectionDeviceDavid;
class CollectionDeviceNic;

// Bus instance
class CollectionDeviceBusi : public CollectionDevice {
public:
    CollectionDeviceBusi(const CollectionDevId &guid, const mti::bus_instance::UbseMtiEid &eid,
                         const CollectionUpi &upi, CollectionDeviceType devType);
    CollectionDevId GetIdStr() override; // guid
    CollectionUpi GetUpiStr();           // upi
    void SetUpiStr(const CollectionUpi &upi);
    const std::vector<std::shared_ptr<CollectionDeviceNic>> &GetSubDevNic();
    const std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> &GetSubDevIdev();

    void SetSubDevNic(const std::shared_ptr<CollectionDeviceNic> &devNic);
    void SetSubDevIdev(const std::shared_ptr<CollectionDeviceIdevVfe> &devIdev);

    void RemoveSubDevNic(const std::shared_ptr<CollectionDeviceNic> &devNic);
    void RemoveSubDevIdev(const std::shared_ptr<CollectionDeviceIdevVfe> &devIdev);

private:
    std::vector<std::shared_ptr<CollectionDeviceNic>> subDevNic;
    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> subDevIdev;
};

// Ub controller
class CollectionDeviceUbCtrl : public CollectionDevice {
public:
    explicit CollectionDeviceUbCtrl(const CollectDeviceLoc &devLoc);

    CollectionDevId GetIdStr() override; // chipId-dieId
    const std::vector<std::shared_ptr<CollectionDeviceIdevPfe>> &GetSubDevIdev();
    const std::vector<std::shared_ptr<CollectionDeviceNic>> &GetAffiDevNic();

    void SetSubDevIdev(const std::shared_ptr<CollectionDeviceIdevPfe> &idevPfe);
    void SetAffinityDevNic(const std::shared_ptr<CollectionDeviceNic> &devNic);

private:
    std::vector<std::shared_ptr<CollectionDeviceIdevPfe>> subDevIdev;
    std::vector<std::shared_ptr<CollectionDeviceNic>> affinityDevNic;
};

// Idev: pfe vfe父类
class CollectionDeviceIdev : public CollectionDevice {
public:
    CollectionDeviceIdev(const CollectDeviceLoc &devLoc, CollectionDeviceType type);
    std::shared_ptr<CollectionDeviceDavid> GetBondingDevDavid() const;
    void SetBondingDevDavid(const std::shared_ptr<CollectionDeviceDavid> &devDavid);
    void RemoveBondingDevDavid();
    void SetIsComSharedFe(const bool isComSharedFe);
    bool GetIsComSharedFe() const;

private:
    std::weak_ptr<CollectionDeviceDavid> bondingDevDavid;
    bool isComSharedFe;
};

// Idev Pfe
class CollectionDeviceIdevPfe : public CollectionDeviceIdev {
public:
    explicit CollectionDeviceIdevPfe(const CollectDeviceLoc &devLoc);
    CollectionDevId GetIdStr() override; // chipId-dieId-pfeId
    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> GetSubDevVfe();
    void SetParentUbCtl(const std::shared_ptr<CollectionDeviceUbCtrl> &devUbCtrl);
    std::shared_ptr<CollectionDeviceUbCtrl> GetParentUbCtl() const;
    void SetSubDevIdev(const std::shared_ptr<CollectionDeviceIdevVfe> &idevVfe);

private:
    std::weak_ptr<CollectionDeviceUbCtrl> parentUbCtl;
    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> subDevIdev; // vfe
};

// Idev Vfe
class CollectionDeviceIdevVfe : public CollectionDeviceIdev {
public:
    explicit CollectionDeviceIdevVfe(const CollectDeviceLoc &devLoc);
    CollectionDevId GetIdStr() override; // chipId-dieId-pfeId-vfeId
    std::shared_ptr<CollectionDeviceIdevPfe> GetParentPfe() const;
    std::vector<std::shared_ptr<CollectionDeviceBusi>> GetBondingDevBusi() const;
    void SetParentPfe(const std::shared_ptr<CollectionDeviceIdevPfe> &idevPfe);
    void SetBondingDevBusi(const std::shared_ptr<CollectionDeviceBusi> &devBusi);
    void RemoveBondingDevBusi(const std::shared_ptr<CollectionDeviceBusi> &devBusi);

private:
    std::weak_ptr<CollectionDeviceIdevPfe> parentPfe;
    std::vector<std::weak_ptr<CollectionDeviceBusi>> bondingDevBusi;
};

// npu
class CollectionDeviceDavid : public CollectionDevice {
public:
    explicit CollectionDeviceDavid(const CollectDeviceLoc &devLoc);
    CollectionDevId GetIdStr() override; // slotId-chipId
    std::shared_ptr<CollectionDevice> GetBondingIdev();
    void SetBondingIdev(const std::shared_ptr<CollectionDevice> &idev);
    void RemoveBondingIdev();

private:
    std::shared_ptr<CollectionDevice> bondingIdev; // pfe or vfe
};

// 1825
class CollectionDeviceNic : public CollectionDevice {
public:
    explicit CollectionDeviceNic(const CollectDeviceLoc &devLoc);
    CollectionDevId GetIdStr() override; // slotId-chipId-pfeId
    std::shared_ptr<CollectionDeviceUbCtrl> GetAffinityDevUbCtrl();
    std::shared_ptr<CollectionDeviceBusi> GetBondingDevBusi();
    void SetAffinityDevUbCtrl(const std::shared_ptr<CollectionDeviceUbCtrl> &devUbCtrl);
    void SetBondingDevBusi(const std::shared_ptr<CollectionDeviceBusi> &devBusi);
    void RemoveBondingDevBusi();

private:
    std::weak_ptr<CollectionDeviceUbCtrl> affinityDevUbCtrl;
    std::weak_ptr<CollectionDeviceBusi> bondingDevBusi;
};

inline bool IsValidHexString(const std::string &str, bool allowDashes = false)
{
    return std::all_of(str.begin(), str.end(),
                       [allowDashes](char c) { return (allowDashes && c == '-') || std::isxdigit(c); });
}
inline uint8_t Uint32ToUint8(uint32_t val)
{
    return static_cast<uint8_t>(val);
}
inline uint8_t DeviceTypeToUint8(CollectionDeviceType type)
{
    return static_cast<uint8_t>(type);
}
inline CollectionDevId GetDevIdByDevLocBusi(const CollectDeviceLoc &devLoc)
{
    return devLoc.guid;
}

inline CollectionDevId GetDevIdByDevLocNpu(const CollectDeviceLoc &devLoc)
{
    return CollectionStringUtil::CollectionJoinStr(DeviceTypeToUint8(CollectionDeviceType::NPU), devLoc.slotId,
                                                   devLoc.chipId);
}
inline CollectionDevId GetDevIdByDevLocNic(const CollectDeviceLoc &devLoc)
{
    return CollectionStringUtil::CollectionJoinStr(DeviceTypeToUint8(CollectionDeviceType::NIC), devLoc.slotId,
                                                   devLoc.chipId, devLoc.pfeId);
}
inline CollectionDevId GetDevIdByDevLocUbCtrl(const CollectDeviceLoc &devLoc)
{
    return CollectionStringUtil::CollectionJoinStr(DeviceTypeToUint8(CollectionDeviceType::UBCONTROLLER), devLoc.chipId,
                                                   devLoc.dieId);
}

inline CollectionDevId GetDevIdByDevLocIdevPfe(const CollectDeviceLoc &devLoc)
{
    return CollectionStringUtil::CollectionJoinStr(DeviceTypeToUint8(CollectionDeviceType::P_IDEV), devLoc.chipId,
                                                   devLoc.dieId, devLoc.pfeId);
}
inline CollectionDevId GetDevIdByDevLocIdevVfe(const CollectDeviceLoc &devLoc)
{
    return CollectionStringUtil::CollectionJoinStr(DeviceTypeToUint8(CollectionDeviceType::V_IDEV), devLoc.chipId,
                                                   devLoc.dieId, devLoc.pfeId, devLoc.vfeId);
}
using CollectionGetDevIdByDevLoc = std::function<CollectionDevId(const CollectDeviceLoc &)>;
const size_t GET_DEVID_FUNCTION_NUM = 7;
const std::array<CollectionGetDevIdByDevLoc, GET_DEVID_FUNCTION_NUM> GET_DEVID_FUNCTION_TABLE{
    GetDevIdByDevLocBusi,   GetDevIdByDevLocBusi,    GetDevIdByDevLocNpu,    GetDevIdByDevLocNic,
    GetDevIdByDevLocUbCtrl, GetDevIdByDevLocIdevPfe, GetDevIdByDevLocIdevVfe};
} // namespace ubse::npu::controller

#endif // UBSE_NPU_RESOURCE_COLLECTION_DEF_H
