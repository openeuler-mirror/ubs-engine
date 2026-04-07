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
    NIC_PFE = 3,
    NIC_VFE = 4,
    UBCONTROLLER = 5,
    P_IDEV = 6,
    V_IDEV = 7,
    COLLECTION_DEVICE_TYPE_COUNT = 8
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
class CollectionDeviceNicPfe;
class CollectionDeviceNicVfe;

// Bus instance
class CollectionDeviceBusi : public CollectionDevice {
public:
    CollectionDeviceBusi(const CollectionDevId &guid, const mti::bus_instance::UbseMtiEid &eid,
                         const CollectionUpi &upi, CollectionDeviceType devType);
    CollectionDevId GetIdStr() override; // guid
    CollectionUpi GetUpiStr();           // upi
    void SetUpiStr(const CollectionUpi &upi);
    const std::vector<std::shared_ptr<CollectionDeviceNicPfe>> &GetSubDevNicPfe() const;
    const std::vector<std::shared_ptr<CollectionDeviceNicVfe>> &GetSubDevNicVfe() const;
    const std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> &GetSubDevIdev() const;

    void SetSubDevNicPfe(const std::shared_ptr<CollectionDeviceNicPfe> &devNic);
    void SetSubDevNicVfe(const std::shared_ptr<CollectionDeviceNicVfe> &devNic);
    void SetSubDevIdev(const std::shared_ptr<CollectionDeviceIdevVfe> &devIdev);

    void RemoveSubDevNicPfe(const std::shared_ptr<CollectionDeviceNicPfe> &devNic);
    void RemoveSubDevNicVfe(const std::shared_ptr<CollectionDeviceNicVfe> &devNic);
    void RemoveSubDevIdev(const std::shared_ptr<CollectionDeviceIdevVfe> &devIdev);

private:
    std::vector<std::shared_ptr<CollectionDeviceNicPfe>> subDevNicPfe;
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> subDevNicVfe;
    std::vector<std::shared_ptr<CollectionDeviceIdevVfe>> subDevIdev;
};

// Ub controller
class CollectionDeviceUbCtrl : public CollectionDevice {
public:
    explicit CollectionDeviceUbCtrl(const CollectDeviceLoc &devLoc);

    CollectionDevId GetIdStr() override; // chipId-dieId
    const std::vector<std::shared_ptr<CollectionDeviceIdevPfe>> &GetSubDevIdev();

    void SetSubDevIdev(const std::shared_ptr<CollectionDeviceIdevPfe> &idevPfe);

private:
    std::vector<std::shared_ptr<CollectionDeviceIdevPfe>> subDevIdev;
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
    std::shared_ptr<CollectionDeviceNicPfe> GetAffinityDevNicPfe();
    void SetAffinityDevNicPfe(const std::shared_ptr<CollectionDeviceNicPfe> &nicPfe);
    std::shared_ptr<CollectionDeviceNicVfe> GetAffinityDevNicVfe();
    void SetAffinityDevNicVfe(const std::shared_ptr<CollectionDeviceNicVfe> &nicVfe);

private:
    std::shared_ptr<CollectionDevice> bondingIdev; // pfe or vfe
    std::shared_ptr<CollectionDeviceNicPfe> affinityDevNicPfe;
    std::shared_ptr<CollectionDeviceNicVfe> affinityDevNicVfe;
};

// 1825
class CollectionDeviceNic : public CollectionDevice {
public:
    explicit CollectionDeviceNic(const CollectDeviceLoc &devLoc, CollectionDeviceType type);
    std::shared_ptr<CollectionDeviceBusi> GetBondingDevBusi();
    void SetBondingDevBusi(const std::shared_ptr<CollectionDeviceBusi> &devBusi);
    void RemoveBondingDevBusi();
    std::shared_ptr<CollectionDeviceDavid> GetAffinityDevDavid();
    void SetAffinityDevDavid(const std::shared_ptr<CollectionDeviceDavid> &devDavid);

private:
    std::weak_ptr<CollectionDeviceBusi> bondingDevBusi;
    std::weak_ptr<CollectionDeviceDavid> affinityDevDavid;
};

class CollectionDeviceNicPfe : public CollectionDeviceNic {
public:
    explicit CollectionDeviceNicPfe(const CollectDeviceLoc &devLoc);
    CollectionDevId GetIdStr() override; // slotId-chipId-pfeId
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> GetSubNicVfe();
    void SetSubNicVfe(const std::shared_ptr<CollectionDeviceNicVfe> &nicVfe);
private:
    std::vector<std::shared_ptr<CollectionDeviceNicVfe>> subNicVfe;
};

class CollectionDeviceNicVfe : public CollectionDeviceNic {
public:
    explicit CollectionDeviceNicVfe(const CollectDeviceLoc &devLoc);
    CollectionDevId GetIdStr() override; // slotId-chipId-pfeId-vfeId
    std::shared_ptr<CollectionDeviceNicPfe> GetParentNicPfe() const;
    void SetParentNicPfe(const std::shared_ptr<CollectionDeviceNicPfe> &nicPfe);
private:
    std::weak_ptr<CollectionDeviceNicPfe> parentNicPfe;
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

inline CollectionDevId GetDevIdByDevLocNicPfe(const CollectDeviceLoc &devLoc)
{
    return CollectionStringUtil::CollectionJoinStr(DeviceTypeToUint8(CollectionDeviceType::NIC_PFE), devLoc.slotId,
                                                   devLoc.chipId, devLoc.pfeId);
}

inline CollectionDevId GetDevIdByDevLocNicVfe(const CollectDeviceLoc &devLoc)
{
    return CollectionStringUtil::CollectionJoinStr(DeviceTypeToUint8(CollectionDeviceType::NIC_VFE), devLoc.slotId,
                                                   devLoc.chipId, devLoc.pfeId, devLoc.vfeId);
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
const size_t GET_DEVID_FUNCTION_NUM = static_cast<size_t>(CollectionDeviceType::COLLECTION_DEVICE_TYPE_COUNT);
const std::array<CollectionGetDevIdByDevLoc, GET_DEVID_FUNCTION_NUM> GET_DEVID_FUNCTION_TABLE{
    GetDevIdByDevLocBusi, GetDevIdByDevLocBusi, GetDevIdByDevLocNpu, GetDevIdByDevLocNicPfe, GetDevIdByDevLocNicVfe,
    GetDevIdByDevLocUbCtrl, GetDevIdByDevLocIdevPfe, GetDevIdByDevLocIdevVfe};

enum class ProductType {
    SERVER = 0,
    POD_16_1825 = 1,
    POD_32_1825 = 2
};

// server环境 David和1825亲和关系
const std::vector<std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> > SERVER_DAVID_NIC_MAPPING = {
    // davidChipId, nicSlotId, nicChipId, nicPfeId
    {1, 1, 11, 2},
    {2, 1, 11, 3},
    {3, 1, 12, 2},
    {4, 1, 12, 3},
    {5, 1, 13, 2},
    {6, 1, 13, 3},
    {7, 1, 14, 2},
    {8, 1, 14, 3}
};
// pod 16 1825环境 David和1825亲和关系
const std::vector<std::tuple<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t> > POD_16_1825_DAVID_NIC_MAPPING = {
    // davidSlotId, davidChipId, nicSlotId, nicChipId, nicPfeId
    {23, 1, 27, 3, 2},
    {23, 2, 27, 4, 2},
    {23, 3, 27, 3, 3},
    {23, 4, 27, 4, 3},
    {23, 5, 27, 3, 4},
    {23, 6, 27, 4, 4},
    {23, 7, 27, 3, 5},
    {23, 8, 27, 4, 5},
    {24, 1, 27, 5, 2},
    {24, 2, 27, 6, 2},
    {24, 3, 27, 5, 3},
    {24, 4, 27, 6, 3},
    {24, 5, 27, 5, 4},
    {24, 6, 27, 6, 4},
    {24, 7, 27, 5, 5},
    {24, 8, 27, 6, 5},
    {25, 1, 27, 7, 2},
    {25, 2, 27, 8, 2},
    {25, 3, 27, 7, 3},
    {25, 4, 27, 8, 3},
    {25, 5, 27, 7, 4},
    {25, 6, 27, 8, 4},
    {25, 7, 27, 7, 5},
    {25, 8, 27, 8, 5},
    {26, 1, 27, 9, 2},
    {26, 2, 27, 10, 2},
    {26, 3, 27, 9, 3},
    {26, 4, 27, 10, 3},
    {26, 5, 27, 9, 4},
    {26, 6, 27, 10, 4},
    {26, 7, 27, 9, 5},
    {26, 8, 27, 10, 5},
    {18, 1, 13, 3, 2},
    {18, 2, 13, 4, 2},
    {18, 3, 13, 3, 3},
    {18, 4, 13, 4, 3},
    {18, 5, 13, 3, 4},
    {18, 6, 13, 4, 4},
    {18, 7, 13, 3, 5},
    {18, 8, 13, 4, 5},
    {19, 1, 13, 5, 2},
    {19, 2, 13, 6, 2},
    {19, 3, 13, 5, 3},
    {19, 4, 13, 6, 3},
    {19, 5, 13, 5, 4},
    {19, 6, 13, 6, 4},
    {19, 7, 13, 5, 5},
    {19, 8, 13, 6, 5},
    {20, 1, 13, 7, 2},
    {20, 2, 13, 8, 2},
    {20, 3, 13, 7, 3},
    {20, 4, 13, 8, 3},
    {20, 5, 13, 7, 4},
    {20, 6, 13, 8, 4},
    {20, 7, 13, 7, 5},
    {20, 8, 13, 8, 5},
    {21, 1, 13, 9, 2},
    {21, 2, 13, 10, 2},
    {21, 3, 13, 9, 3},
    {21, 4, 13, 10, 3},
    {21, 5, 13, 9, 4},
    {21, 6, 13, 10, 4},
    {21, 7, 13, 9, 5},
    {21, 8, 13, 10, 5}
};
// pod 32 1825环境 David和1825亲和关系
const std::vector<std::tuple<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t> > POD_32_1825_DAVID_NIC_MAPPING = {
    // davidSlotId, davidChipId, nicSlotId, nicChipId, nicPfeId
    {23, 1, 27, 3, 2},
    {23, 2, 27, 17, 2},
    {23, 3, 27, 3, 3},
    {23, 4, 27, 17, 3},
    {23, 5, 27, 4, 4},
    {23, 6, 27, 18, 4},
    {23, 7, 27, 4, 5},
    {23, 8, 27, 18, 5},
    {24, 1, 27, 5, 2},
    {24, 2, 27, 15, 2},
    {24, 3, 27, 5, 3},
    {24, 4, 27, 15, 3},
    {24, 5, 27, 6, 4},
    {24, 6, 27, 16, 4},
    {24, 7, 27, 6, 5},
    {24, 8, 27, 16, 5},
    {25, 1, 27, 7, 2},
    {25, 2, 27, 13, 2},
    {25, 3, 27, 7, 3},
    {25, 4, 27, 13, 3},
    {25, 5, 27, 8, 4},
    {25, 6, 27, 14, 4},
    {25, 7, 27, 8, 5},
    {25, 8, 27, 14, 5},
    {26, 1, 27, 9, 2},
    {26, 2, 27, 11, 2},
    {26, 3, 27, 9, 3},
    {26, 4, 27, 11, 3},
    {26, 5, 27, 10, 4},
    {26, 6, 27, 12, 4},
    {26, 7, 27, 10, 5},
    {26, 8, 27, 12, 5},
    {18, 1, 13, 3, 2},
    {18, 2, 13, 17, 2},
    {18, 3, 13, 3, 3},
    {18, 4, 13, 17, 3},
    {18, 5, 13, 4, 4},
    {18, 6, 13, 18, 4},
    {18, 7, 13, 4, 5},
    {18, 8, 13, 18, 5},
    {19, 1, 13, 5, 2},
    {19, 2, 13, 15, 2},
    {19, 3, 13, 5, 3},
    {19, 4, 13, 15, 3},
    {19, 5, 13, 6, 4},
    {19, 6, 13, 16, 4},
    {19, 7, 13, 6, 5},
    {19, 8, 13, 16, 5},
    {20, 1, 13, 7, 2},
    {20, 2, 13, 13, 2},
    {20, 3, 13, 7, 3},
    {20, 4, 13, 13, 3},
    {20, 5, 13, 8, 4},
    {20, 6, 13, 14, 4},
    {20, 7, 13, 8, 5},
    {20, 8, 13, 14, 5},
    {21, 1, 13, 9, 2},
    {21, 2, 13, 11, 2},
    {21, 3, 13, 9, 3},
    {21, 4, 13, 11, 3},
    {21, 5, 13, 10, 4},
    {21, 6, 13, 12, 4},
    {21, 7, 13, 10, 5},
    {21, 8, 13, 12, 5}
};
} // namespace ubse::npu::controller

#endif // UBSE_NPU_RESOURCE_COLLECTION_DEF_H
