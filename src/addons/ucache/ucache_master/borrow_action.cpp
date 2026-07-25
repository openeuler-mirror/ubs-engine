/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "borrow_action.h"

#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>

#include "ubse_error.h"
#include "ubse_logger.h"
#include "securec.h"
#include "ucache_config.h"
#include "ucache_error.h"
#include "ucache_string_util.h"

namespace ucache {
namespace borrow_action {
using namespace ubse::log;
using namespace ucache::mem_borrow;
using namespace ucache::deserialize;
std::string BorrowAction::ToString() const
{
    std::ostringstream oss;
    oss << "{";
    if (type == ActionType::BORROW) {
        oss << "Type:BORROW,";
        oss << "from:" << nodeMemBorrowInfo.srcNodeId << ",";
        oss << "to: " << nodeMemBorrowInfo.destNodeId << ",";
        oss << "size:" << nodeMemBorrowInfo.totalSize;
    } else if (type == ActionType::RETURN) {
        oss << "Type:RETURN,";
        oss << "from:" << nodeMemBorrowInfo.srcNodeId << ",";
        oss << "size:" << nodeMemBorrowInfo.totalSize;
    }
    oss << "}";
    return oss.str();
}

constexpr int UUID_BYTE_SIZE = 16;
constexpr int RANDOM_BYTE_MIN = 0;
constexpr int RANDOM_BYTE_MAX = 255;
constexpr int HEX_WIDTH = 2;

void GenerateUniqueId(std::string& str)
{
    std::random_device rd;
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < UUID_BYTE_SIZE; ++i) {
        ss << std::setw(HEX_WIDTH) << static_cast<int>(rd() & 0xFF);
    }
    str = ss.str();
}

constexpr char RESOURCE_KIND[] = "MEM";

uint32_t ExecuteBorrowMem(const std::string& from, const std::string& to)
{
    uint32_t ret = UCACHE_OK;
    std::string memName;
    GenerateUniqueId(memName);

    int fromSocketId = 0;
    int fromNumaId = 0;
    // 通过全局借用topo，查询出需要从哪个socket、哪个numa借出内存
    ret = MemBorrowTopo::globalMemBorrowTopo.GetBorrowableNumaInfo(from, fromSocketId, fromNumaId);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Get borrowable numa info failed";
        return ret;
    }
    UbseMemBorrower borrower{.nodeId = to};
    uint32_t fromslotId = 0;
    ret = SafeStoInt(from, fromslotId);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Get slot id failed ret is " << ret;
        return ret;
    }
    UbseMemNumaLender lender{.slotId = fromslotId,
                             .socketId = static_cast<uint32_t>(fromSocketId),
                             .numaId = static_cast<uint64_t>(fromNumaId),
                             .size = UcacheConfig::GetInstance().GetBorrowSize()};

    std::vector<UbseMemNumaLender> lenders{};
    lenders.emplace_back(std::move(lender));
    int16_t borrowLocalNuma = static_cast<int16_t>(fromNumaId);
    uint8_t usrInfo[ubse::mem::controller::UBSE_MAX_USR_INFO_LEN];
    errno_t err =
        memcpy_s(usrInfo, ubse::mem::controller::UBSE_MAX_USR_INFO_LEN, &borrowLocalNuma, sizeof(borrowLocalNuma));
    if (err != 0) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "memcpy_s for usrInfo failed.";
        return EXEC_MEM_BORROW_ERROR;
    }
    UbseMemNumaDesc desc{};
    UbseResult result = UbseMemNumaCreateWithLenderSafe(memName, borrower, lenders, usrInfo, desc);
    if (result != UBSE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Borrow mem failed: rack mem API error, result=" << static_cast<uint32_t>(result) << ".";
        return UBSE_API_ERROR;
    }

    ret = MemBorrowTopo::globalMemBorrowTopo.SetNumaNodeBorrowSize(memName, from, to, fromNumaId,
                                                                   static_cast<int>(desc.numaId));
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Update borrow topology after memory borrow failed.";
        return EXEC_MEM_BORROW_ERROR;
    }
    return ret;
}

uint32_t ExecuteReturnMem(const std::string& from, const std::string& to)
{
    std::string memName;
    int numaId;
    uint32_t ret = UCACHE_OK;

    ret = MemBorrowTopo::globalMemBorrowTopo.GetReturnableMem(from, to, memName, numaId);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Return memory failed: no returnable memory";
        return EXEC_MEM_RETURN_ERROR;
    }

    UbseMemBorrower borrower{.nodeId = to};
    UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Start to return memory, memName=" << memName << ".";
    UbseResult result = UbseMemNumaDelete(memName, borrower);
    if (result != UBSE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Return memory failed: rack memory API error, result=" << static_cast<uint32_t>(result) << ".";
        return EXEC_MEM_RETURN_ERROR;
    }
    ret = MemBorrowTopo::globalMemBorrowTopo.DelNumaNodeBorrowSize(memName, from, to, numaId);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Update borrow topology after memory borrow failed.";
        return EXEC_MEM_RETURN_ERROR;
    }
    return ret;
}

uint32_t ProcessOneBorrowAction(const BorrowAction& action)
{
    uint32_t ret = UCACHE_OK;
    if (action.type == ActionType::BORROW) {
        ret = ExecuteBorrowMem(action.nodeMemBorrowInfo.srcNodeId, action.nodeMemBorrowInfo.destNodeId);
        if (ret != UCACHE_OK) {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "Process borrow action failed: execute memory borrow error.";
            return ret;
        }
    } else if (action.type == ActionType::RETURN) {
        ret = ExecuteReturnMem(action.nodeMemBorrowInfo.srcNodeId, action.nodeMemBorrowInfo.destNodeId);
        if (ret != UCACHE_OK) {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "Process return action failed: execute memory return error.";
            return ret;
        }
    } else {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Process action failed: unknown action type.";
        return MEM_ACTION_TYPE_ERROR;
    }
    return ret;
}

uint32_t ExecuteBorrowActions(const std::vector<BorrowAction>& actionSet)
{
    uint32_t ret = UCACHE_OK;
    if (actionSet.empty()) {
        UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Execute action failed: empty action set.";
        return ret;
    }
    for (const auto& action : actionSet) {
        ret = ProcessOneBorrowAction(action);
        if (ret != UCACHE_OK) {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Execute action failed: " << action.ToString();
            return ret;
        } else {
            UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Execute action success: " << action.ToString();
        }
    }
    return ret;
}
} // namespace borrow_action
} // namespace ucache
