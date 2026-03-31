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
#include "ubse_npu_msg_execute.h"

#include <array>

#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_context.h"
#include "ubse_npu_controller_module.h"
#include "ubse_npu_manager_api.h"
#include "ubse_pack_util.h"
#include "ubse_str_util.h"

namespace ubse::npu::controller {
using namespace ubse::utils;
using namespace ubse::context;
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");
// 4个类型的数量大小+总数量大小
constexpr size_t HEAD_SIZE = 5 * sizeof(uint8_t);
constexpr uint8_t HEX_RADIX = 16;

// list接口
uint32_t QueryDeviceRespPack(const std::vector<std::shared_ptr<IResource>> &devList, TransRespMsg &buffer);
// alloc接口
uint32_t UbseAllocRequestUnpack(const TransReqMsg &buffer, UbseAllocRequest &requestInfo);
uint32_t AllocDevResponsePack(const std::array<uint8_t, UBSE_UB_DEVICE_GUID_SIZE> &newBusInstanceGuid,
                              const std::vector<std::shared_ptr<IResource>> &devList, TransRespMsg &buffer);
uint32_t UbseQueryTidUbaRequestUnpack(const TransReqMsg &buffer, std::string &requestInfo);
uint32_t QueryTidUbaResponsePack(uint32_t &tid, uint64_t &uba, uint64_t &size, TransRespMsg &buffer);

uint32_t QueryDeviceExecute(TransReqMsg req, TransRespMsg &resp)
{
    std::vector<std::shared_ptr<IResource>> devList;
    auto npuCtrlModule = UbseContext::GetInstance().GetModule<UbseNpuControllerModule>();
    if (npuCtrlModule == nullptr) {
        UBSE_LOG_ERROR << "Get npu controller module failed";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = npuCtrlModule->QueryAllDevices(devList);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "QueryLocalUbDevices failed, " << FormatRetCode(ret);
        return ret;
    }
    // 封装回复数据
    ret = QueryDeviceRespPack(devList, resp);
    UBSE_LOG_INFO << "[NPU] pack query dev request";
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseNode pack failed, " << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

uint32_t AllocDeviceExecute(TransReqMsg req, TransRespMsg &resp)
{
    // 解析接收数据
    UbseAllocRequest requestInfo;
    auto ret = UbseAllocRequestUnpack(req, requestInfo);
    if (ret != UBSE_OK) {
        return ret;
    }
    // 业务处理
    std::array<uint8_t, UBSE_UB_DEVICE_GUID_SIZE> newBusInstanceGuid;
    std::string newBusInstanceGuidStr(reinterpret_cast<const char *>(newBusInstanceGuid.data()),
        UBSE_UB_DEVICE_GUID_SIZE);
    std::vector<std::shared_ptr<IResource>> devList;
    ret = AllocDevicesImpl(requestInfo, newBusInstanceGuidStr, devList);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "AllocDevices failed," << FormatRetCode(ret);
        return ret;
    }

    std::copy(newBusInstanceGuidStr.begin(), newBusInstanceGuidStr.end(), newBusInstanceGuid.begin());
    // 封装回复数据
    ret = AllocDevResponsePack(newBusInstanceGuid, devList, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseNode pack failed, " << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

uint32_t FreeDeviceExecute(TransReqMsg req, TransRespMsg &resp)
{
    UbseAllocRequest requestInfo{};
    auto ret = UbseAllocRequestUnpack(req, requestInfo);
    if (ret != UBSE_OK) {
        return ret;
    }

    ret = FreeUbDevicesImpl(requestInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "FreeUbDevice failed, " << FormatRetCode(ret);
        return ret;
    }
    // 构造一个空的返回消息
    resp.length = NO_8;
    resp.buffer = new (std::nothrow) uint8_t[resp.length];
    if (resp.buffer == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    return UBSE_OK;
}

uint32_t QueryTidUbaSizeExecute(TransReqMsg req, TransRespMsg &resp)
{
    std::string requestGuid;
    auto ret = UbseQueryTidUbaRequestUnpack(req, requestGuid);
    if (ret != UBSE_OK) {
        return ret;
    }
    UbaTidSize ubaTidSizeInfo;
    ret = QueryUbaTidSizeImpl(requestGuid, ubaTidSizeInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "QueryTidUbaSize failed, " << FormatRetCode(ret);
        return ret;
    }
    ret = QueryTidUbaResponsePack(ubaTidSizeInfo.tid, ubaTidSizeInfo.uba, ubaTidSizeInfo.size, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "response pack failed, " << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

struct DeviceCnt {
    uint8_t npuCnt{};
    uint8_t ubctrlCnt{};
    uint8_t busiCnt{};
    uint8_t nicPfeCnt{};
    uint8_t nicVfeCnt{};
};

void CountDevicesByType(const std::vector<std::shared_ptr<IResource>> &devList, DeviceCnt &devCnt)
{
    for (auto &dev : devList) {
        if (dev->GetType() == ResourceType::NIC_PFE) {
            devCnt.nicPfeCnt++;
        }
        if (dev->GetType() == ResourceType::NIC_VFE) {
            devCnt.nicVfeCnt++;
        }
        if (dev->GetType() == ResourceType::NPU) {
            devCnt.npuCnt++;
        }
        if (dev->GetType() == ResourceType::UBCONTROLLER) {
            devCnt.ubctrlCnt++;
        }
        if (dev->GetType() == ResourceType::BUSINSTANCE) {
            devCnt.busiCnt++;
        }
    }
}

uint32_t QueryDeviceRespBufferAlloc(const std::vector<std::shared_ptr<IResource>> &devList, TransRespMsg &buffer)
{
    size_t size = 0;
    for (auto &dev : devList) {
        size += dev->CalculateSize();
    }
    buffer.buffer = new (std::nothrow) uint8_t[size + HEAD_SIZE];
    if (buffer.buffer == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    buffer.length = size + HEAD_SIZE;
    UBSE_LOG_INFO << "[NPU] buffer before pack size = " << buffer.length;
    return UBSE_OK;
}

uint32_t PackDevList(const std::vector<std::shared_ptr<IResource> > &devList, UbsePackUtil &packUtil)
{
    DeviceCnt devCnt{};
    CountDevicesByType(devList, devCnt);
    if (!packUtil.UbsePackUint8(devList.size()) || !packUtil.UbsePackUint8(devCnt.nicPfeCnt) || !packUtil.
        UbsePackUint8(devCnt.nicVfeCnt) || !packUtil.UbsePackUint8(devCnt.npuCnt) ||
        !packUtil.UbsePackUint8(devCnt.ubctrlCnt) || !packUtil.UbsePackUint8(devCnt.busiCnt)) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    for (auto &dev : devList) {
        auto ret = dev->Pack(packUtil);
        if (ret != UBSE_OK) {
            return ret;
        }
    }
    return UBSE_OK;
}
uint32_t QueryDeviceRespPack(const std::vector<std::shared_ptr<IResource>> &devList, TransRespMsg &buffer)
{
    auto ret = QueryDeviceRespBufferAlloc(devList, buffer);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "allocate buffer failed, " << FormatRetCode(ret);
        return ret;
    }
    // 打包
    UbsePackUtil packUtil(buffer.buffer, buffer.length);
    ret = PackDevList(devList, packUtil);
    if (ret != UBSE_OK) {
        delete[] buffer.buffer;
        buffer.buffer = nullptr;
        buffer.length = 0;
        return ret;
    }
    return UBSE_OK;
}

void PrintInfo(const UbseAllocRequest &requestInfo)
{
    std::ostringstream oss;
    // 拼接 UPIs
    oss << "[NPU DATA] upis: [";
    for (auto upi : requestInfo.upis) {
        oss << static_cast<uint32_t>(upi) << ", ";
    }
    oss << "]; ";
    // 拼接 GUIDs
    oss << "busInstanceGuid: [" << requestInfo.busInstanceGuid << "] ";
    // 拼接子设备数量
    oss << "sub dev size: " << static_cast<uint32_t>(requestInfo.ubDevList.size());

    UBSE_LOG_INFO << oss.str();
}

uint32_t UnpackDeviceList(UbseUnpackUtil unpackUtil, std::vector<UbDevice> &devList)
{
    uint8_t devListSize;
    if (!unpackUtil.UnpackUint8(devListSize)) {
        UBSE_LOG_ERROR << "Failed to unpack device list size";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    for (size_t i = 0; i < devListSize; i++) {
        UbDevice tmpDev;
        unsigned char devType;
        if (!unpackUtil.UnpackUChar(devType)) {
            UBSE_LOG_ERROR << "Failed to unpack devType";
            return UBSE_ERROR_DESERIALIZE_FAILED;
        }
        tmpDev.type = static_cast<ResourceType>(devType);
        if (!unpackUtil.UnpackUint8(tmpDev.slotId)) {
            UBSE_LOG_ERROR << "Failed to unpack slot_id";
            return UBSE_ERROR_DESERIALIZE_FAILED;
        }
        if (!unpackUtil.UnpackUint8(tmpDev.chipId)) {
            UBSE_LOG_ERROR << "Failed to unpack chip_id";
            return UBSE_ERROR_DESERIALIZE_FAILED;
        }
        if (!unpackUtil.UnpackUint8(tmpDev.dieId)) {
            UBSE_LOG_ERROR << "Failed to unpack dieId";
            return UBSE_ERROR_DESERIALIZE_FAILED;
        }
        if (!unpackUtil.UnpackUint8(tmpDev.pfId)) {
            UBSE_LOG_ERROR << "Failed to unpack pfId";
            return UBSE_ERROR_DESERIALIZE_FAILED;
        }
        if (!unpackUtil.UnpackUint8(tmpDev.vfId)) {
            UBSE_LOG_ERROR << "Failed to unpack vfId";
            return UBSE_ERROR_DESERIALIZE_FAILED;
        }
        devList.emplace_back(tmpDev);
    }
    return UBSE_OK;
}

uint32_t UbseAllocRequestUnpack(const TransReqMsg &buffer, UbseAllocRequest &requestInfo)
{
    UbseUnpackUtil unpackUtil{buffer.buffer, buffer.length};
    for (size_t i = 0; i < UBSE_UB_UPI_STR_SIZE; i++) {
        if (!unpackUtil.UnpackUint8(requestInfo.upis[i])) {
            UBSE_LOG_ERROR << "Failed to unpack upi str";
            return UBSE_ERROR_DESERIALIZE_FAILED;
        }
    }
    std::string upiStr(requestInfo.upis, requestInfo.upis + UBSE_UB_UPI_STR_SIZE);
    if (ConvertStrToUint16(upiStr, requestInfo.upiStr, HEX_RADIX) != UBSE_OK) {
        UBSE_LOG_ERROR << "Invalid upi:" << upiStr;
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    uint8_t tmpGuid[UBSE_UB_DEVICE_GUID_SIZE];
    for (size_t i = 0; i < UBSE_UB_DEVICE_GUID_SIZE; i++) {
        if (!unpackUtil.UnpackUint8(tmpGuid[i])) {
            UBSE_LOG_ERROR << "Failed to unpack bus instance guid";
            return UBSE_ERROR_DESERIALIZE_FAILED;
        }
    }
    requestInfo.busInstanceGuid.assign(tmpGuid, tmpGuid + UBSE_UB_DEVICE_GUID_SIZE);
    auto validGuidLen = strlen(requestInfo.busInstanceGuid.c_str());
    if (validGuidLen < requestInfo.busInstanceGuid.size()) {
        requestInfo.busInstanceGuid.resize(validGuidLen);
    }
    if (auto ret = UnpackDeviceList(unpackUtil, requestInfo.ubDevList); ret != UBSE_OK) {
        return ret;
    }
    PrintInfo(requestInfo);
    return UBSE_OK;
}

uint32_t UbseQueryTidUbaRequestUnpack(const TransReqMsg &buffer, std::string &requestInfo)
{
    UbseUnpackUtil unpackUtil{buffer.buffer, buffer.length};
    for (size_t i = 0; i < UBSE_UB_DEVICE_GUID_SIZE; i++) {
        uint8_t tmp;
        if (!unpackUtil.UnpackUint8(tmp)) {
            UBSE_LOG_ERROR << "Failed to unpack bus instance guid";
            return UBSE_ERROR_DESERIALIZE_FAILED;
        }
        requestInfo += tmp;
    }
    return UBSE_OK;
}
uint32_t AllocDevRespBufferAlloc(const std::vector<std::shared_ptr<IResource>> &devList, TransRespMsg &buffer)
{
    uint32_t size = 0;
    size += sizeof(uint8_t) * UBSE_UB_DEVICE_GUID_SIZE;
    size += HEAD_SIZE;
    for (auto &dev : devList) {
        size += dev->CalculateSize();
    }
    buffer.buffer = new (std::nothrow) uint8_t[size];
    if (buffer.buffer == nullptr) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    buffer.length = size;
    return UBSE_OK;
}
uint32_t PackAllocResp(const std::array<uint8_t, UBSE_UB_DEVICE_GUID_SIZE> &newBusInstanceGuid,
                       const std::vector<std::shared_ptr<IResource>> &devList, UbsePackUtil &packUtil)
{
    for (auto guid : newBusInstanceGuid) {
        if (!packUtil.UbsePackUint8(guid)) {
            return UBSE_ERROR_SERIALIZE_FAILED;
        }
    }
    return PackDevList(devList, packUtil);
}

uint32_t AllocDevResponsePack(const std::array<uint8_t, UBSE_UB_DEVICE_GUID_SIZE> &newBusInstanceGuid,
                              const std::vector<std::shared_ptr<IResource>> &devList, TransRespMsg &buffer)
{
    auto ret = AllocDevRespBufferAlloc(devList, buffer);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to calculate buffer size, ret: " << FormatRetCode(ret);
        return ret;
    }

    // 打包
    UbsePackUtil packUtil(buffer.buffer, buffer.length);
    ret = PackAllocResp(newBusInstanceGuid, devList, packUtil);
    if (ret != UBSE_OK) {
        delete[] buffer.buffer;
        buffer.buffer = nullptr;
        buffer.length = 0;
        return ret;
    }
    return ret;
}

uint32_t QueryTidUbaResponsePack(uint32_t &tid, uint64_t &uba, uint64_t &size, TransRespMsg &buffer)
{
    uint32_t msgSize = sizeof(tid) + sizeof(uba) + sizeof(size);
    buffer.buffer = new (std::nothrow) uint8_t[msgSize];
    if (buffer.buffer == nullptr) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    buffer.length = msgSize;

    auto cleanBuffer = [&buffer]() {
        delete[] buffer.buffer;
        buffer.buffer = nullptr;
        buffer.length = 0;
    };

    UbsePackUtil packUtil(buffer.buffer, msgSize);
    if (!packUtil.UbsePackUint32(tid) || !packUtil.UbsePackUint64(uba) || !packUtil.UbsePackUint64(size)) {
        cleanBuffer();
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    return UBSE_OK;
}
} // namespace ubse::npu::controller
