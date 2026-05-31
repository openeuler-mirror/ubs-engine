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
#include "ubse_mti_util.h"
#include <algorithm>
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_str_util.h"
#include "securec.h"

namespace ubse::mti {
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");

const size_t LSUB_EID_LEN = 5;
const size_t LSUB_GUID_LEN = 32;
const size_t LSUB_UPI_LEN = 4;

bool IsValidHexString(const std::string& str, bool allowDashes = false)
{
    return std::all_of(str.begin(), str.end(),
                       [allowDashes](char c) { return (allowDashes && c == '-') || std::isxdigit(c); });
}

bool EidStrToArray(const std::string& eidStr, std::array<uint8_t, 16>& eid)
{
    if (eidStr.size() != LSUB_EID_LEN || !IsValidHexString(eidStr)) {
        UBSE_LOG_ERROR << "Lsub output eid invalid, eid is : " << eidStr;
        return false;
    }
    try {
        // 解析16进制字符串为unsigned long
        auto eidValue = std::stoul(eidStr, nullptr, 16);
        // 检查值是否在32位范围内（0-0xFFFFFFFF）
        if (eidValue > UINT32_MAX) {
            UBSE_LOG_ERROR << "EID value exceeds 32-bit range: " << eidStr << " (" << eidValue << " > " << UINT32_MAX
                           << ")";
            return false;
        }
        // 将32位整数存储到EID数组的前4个字节
        // 使用memcpy_s确保安全的内存复制
        uint32_t safeEidValue = static_cast<uint32_t>(eidValue);
        auto ret = memcpy_s(eid.data(), eid.size(), &safeEidValue, sizeof(safeEidValue));
        if (ret != EOK) {
            UBSE_LOG_ERROR << "Failed to copy EID value to array, error code: " << ret;
            return false;
        }
    } catch (const std::exception& e) {
        UBSE_LOG_ERROR << "Lsub output eid invalid, eid is : " << eidStr << ", error : " << e.what();
        return false;
    }
    return true;
}

bool EidArrayToStr(const std::array<uint8_t, 16>& eid, std::string& eidStr)
{
    std::ostringstream oss;
    uint32_t eidValue = 0;
    // 从EID数组的前4个字节读取32位整数
    auto ret = memcpy_s(&eidValue, sizeof(eidValue), eid.data(), sizeof(eidValue));
    if (ret != EOK) {
        UBSE_LOG_ERROR << "Failed to copy EID value from array, error code: " << FormatRetCode(ret);
        return false;
    }
    oss << std::hex << std::setw(LSUB_EID_LEN) << std::setfill('0') << eidValue;
    eidStr = oss.str();
    return true;
}

bool GuidStrToArray(const std::string& guidStr, std::array<uint8_t, 16>& guid)
{
    if (guidStr.size() != LSUB_GUID_LEN || !IsValidHexString(guidStr)) {
        UBSE_LOG_ERROR << "Lsub output guid invalid, guid is : " << guidStr;
        return false;
    }
    // 高效转换：避免字符串拷贝和stringstream开销
    for (size_t i = 0; i < guid.size(); ++i) {
        uint8_t byteValue;
        // 每2个字符转换为一个字节
        if (!utils::HexPairToByte(guidStr[i * 2], guidStr[i * 2 + 1], byteValue)) {
            // 每2个字符转换为一个字节
            UBSE_LOG_ERROR << "Failed to convert hex pair at position " << (i * 2) << ": " << guidStr.substr(i * 2, 2);
            return false;
        }
        // Guid内存布局为小端序，与字符串表示相反，由最大idx 15 开始填充
        guid[15 - i] = byteValue;
    }
    return true;
}

std::string GuidArrayToStr(const std::array<uint8_t, 16>& guid)
{
    std::ostringstream oss;
    // 格式化输出：小端序GUID，每个字节2个字符，不足补0
    oss << std::hex << std::nouppercase << std::setfill('0');
    for (size_t i = guid.size() - 1; i >= 0; i--) {
        // 每个字节2个字符，不足补0
        oss << std::setw(2) << static_cast<int>(guid[i]);
    }
    return oss.str();
}

bool UpiStrToUint16(const std::string& upiStr, uint16_t& upi)
{
    if (upiStr.size() != LSUB_UPI_LEN || !IsValidHexString(upiStr)) {
        UBSE_LOG_ERROR << "Lsub output upi invalid, upi is : " << upiStr;
        return false;
    }
    try {
        unsigned long value = std::stoul(upiStr, nullptr, 16);
        if (value > UINT16_MAX) {
            UBSE_LOG_ERROR << "Lsub output upi invalid, upi is : " << upiStr;
            return false;
        }
        upi = static_cast<uint16_t>(value);
    } catch (const std::exception& e) {
        UBSE_LOG_ERROR << "Lsub output upi invalid, upi is : " << upiStr << ", error : " << e.what();
        return false;
    }
    return true;
}

std::string UpiUint16ToStr(uint16_t upi)
{
    std::ostringstream oss;
    // 格式化输出：4个字符，不足补0
    oss << std::hex << std::nouppercase << std::setw(4) << std::setfill('0') << upi;
    return oss.str();
}
} // namespace ubse::mti