/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "mem_borrow_executor.h"

#include <atomic>
#include <cerrno>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_controller.h"
#include "mem_manager.h"
#include "mempooling_return_module.h"
#include "mp_configuration.h"
#include "mp_smap_controller.h"
#include "mp_string_util.h"

namespace mempooling {
using namespace ubse::log;
using namespace ubse::com;
using namespace rmrs::serialize;
using namespace mempooling::smap;
using namespace ubse::mem::controller;

constexpr int UUID_BYTE_SIZE = 16;
constexpr int RANDOM_BYTE_MIN = 0;
constexpr int RANDOM_BYTE_MAX = 255;
constexpr auto RESOURCE_KIND = "MEM";
constexpr int WIDTH = 2;

MpResult MemBorrowExecutor::PrepareMemNumaCreateParams(const std::string attachNode,
                                                       const RackCreateResourceWaterBorrowAttr& attr,
                                                       UbseMemBorrower& borrower,
                                                       std::vector<UbseMemNumaLender>& lenders,
                                                       uint8_t usrInfo[ubse::mem::controller::UBSE_MAX_USR_INFO_LEN])
{
    if (attr.waterMallocAttr.lenderLocs.size() != attr.waterMallocAttr.lenderSizes.size()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowExecute] lenderLocs and lenderSizes size mismatch: "
            << attr.waterMallocAttr.lenderLocs.size() << " vs " << attr.waterMallocAttr.lenderSizes.size();
        return MEM_POOLING_ERROR;
    }

    // ubse: 借入方
    borrower.nodeId = attachNode;
    borrower.affinitySocketId = attr.waterMallocAttr.srcSocket;
    borrower.uid = attr.waterMallocAttr.uid;
    borrower.username = attr.waterMallocAttr.username;
    // ubse: 借出方

    MEM_POOLING_RES ret = MEM_POOLING_OK;
    for (size_t i = 0; i < attr.waterMallocAttr.lenderLocs.size(); i++) {
        UbseMemNumaLender lender;
        lender.size = attr.waterMallocAttr.lenderSizes[i];
        lender.numaId = attr.waterMallocAttr.lenderLocs[i].numaId;
        ret = MpStringUtil::SafeStoul(attr.waterMallocAttr.lenderLocs[i].nodeId, lender.slotId);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][MemBorrowExecute] SafeStoul failed, slotId="
                << attr.waterMallocAttr.lenderLocs[i].nodeId << " , ret=" << ret << ".";
            return MEM_POOLING_ERROR;
        }
        lender.socketId = attr.waterMallocAttr.lenderLocs[i].socketId;
        (void)lenders.emplace_back(lender);
    }

    errno_t err = memcpy_s(usrInfo, ubse::mem::controller::UBSE_MAX_USR_INFO_LEN, &attr.waterMallocAttr.srcNuma,
                           sizeof(attr.waterMallocAttr.srcNuma));
    if (err != 0) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowExecute] Memcpy_s failed, error_code=" << err << ".";
        return MEM_POOLING_ERROR;
    }

    return MEM_POOLING_OK;
}

MpResult MemBorrowExecutor::MemBorrow(const std::string& attachNode, const RackCreateResourceWaterBorrowAttr& attr,
                                      std::string& name, int16_t& presentNumaId, bool isBorrowIdPersistence)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemBorrow][MemBorrowExecute] MemBorrowExecutor starts to borrow memory.";

    if (GenerateUniqueId(attachNode, name) != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowExecute] Generate unique name failed.";
        return MEM_POOLING_ERROR;
    }

    UbseMemBorrower borrower;
    std::vector<UbseMemNumaLender> lenders;
    uint8_t usrInfo[ubse::mem::controller::UBSE_MAX_USR_INFO_LEN];

    if (PrepareMemNumaCreateParams(attachNode, attr, borrower, lenders, usrInfo) != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowExecute] Prepare params of MemNumaCreate interface failed.";
        return MEM_POOLING_ERROR;
    }

    if (isBorrowIdPersistence) {
        MpResult retBorrowIdsCompleted = BorrowIdsCompleted::Instance().Update(name);
        if (retBorrowIdsCompleted != MEM_POOLING_OK) {
            // 可靠性保障，如果失败，不影响主功能
            UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][MemBorrowExecute] UpdateBorrowIdCompleted failed, borrow_id=" << name << ".";
        }
    }

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][MemBorrowExecute] Start calling ubse interface "
                                                        "for memory borrowing.";

    // 调用ubse借用接口
    UbseMemNumaDesc ubseMemNumaDesc;
    auto start = std::chrono::steady_clock::now();
    UbseResult errCode = UbseMemNumaCreateWithLender(name, borrower, lenders, usrInfo, ubseMemNumaDesc);
    auto end = std::chrono::steady_clock::now();
    if (errCode != UBSE_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][MemBorrowExecute]"
                                                             " Borrow memory by memfabric failed.";
        return MEM_POOLING_ERROR;
    }
    // 处理结果
    presentNumaId = ubseMemNumaDesc.numaId;

    if (presentNumaId <= 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowExecute] Borrow memory by memfabric success, but presentNumaId=" << presentNumaId
            << " is invalid, which is less than or equal to 0.";
        return MEM_POOLING_ERROR;
    }
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemBorrow][MemBorrowExecute] Execution time=" << duration.count()
        << " microseconds, borrowMemSize=" << attr.size << " Byte.";

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemBorrow][MemBorrowExecute] MemBorrowExecutor borrows memory success, borrow_id=" << name << ".";

    return MEM_POOLING_OK;
}

MpResult MemBorrowExecutor::MemFree(const std::string& name)
{
    return MemFreeWithOps(name, false, false);
}

MpResult MemBorrowExecutor::RemoveBorrowIdRedirectionRecursively(const std::string& name)
{
    MpResult retDirect;

    std::string redirectNameKey = name;
    std::string redirectNameVal = name;
    do {
        redirectNameKey = redirectNameVal;
        redirectNameVal.clear();
        retDirect = BorrowIdRedirection::Instance().Query(redirectNameKey, redirectNameVal);
        if (retDirect != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemFree][MemFreeExecute] Get redirection of borrow_id=" << name << " failed.";
            return retDirect;
        }
        if (redirectNameVal.empty()) {
            break; // 如果redirectNameVal为空，说明已经没有重定向关系，不需要再删除
        }
        retDirect = BorrowIdRedirection::Instance().Remove(redirectNameKey);
        if (retDirect != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemFree][MemFreeExecute] Remove redirection of borrow_id=" << name << " failed.";
            return retDirect;
        }
    } while (!redirectNameVal.empty());

    return MEM_POOLING_OK;
}

static __always_inline void GenerateSmapTaskId(uint64_t& taskId)
{
    struct timespec ts {};
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemFree][MemFreeExecute] Failed to get time errno=" << errno;
        return;
    }
    taskId = static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL + static_cast<uint64_t>(ts.tv_nsec);
}

MpResult MemBorrowExecutor::GenerateSmapParams(const std::string& name, MigrateBackMsg& migrateBackMsg,
                                               EnableNodeMsg& enableMsg, std::string& importNodeId, bool isFault)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemFree][MemFreeExecute] Begin to generate smap migrate back params, borrow_id=" << name << ".";
    bool isContainerScene = false;
    if (MpConfiguration::GetInstance().GetSceneType() == MpSceneType::CONTAINER_SCENE) {
        isContainerScene = true;
    }
    std::vector<BorrowRecord> borrowRecords;
    auto ret = BorrowRecordHelper::Instance().CollectBorrowRecordsAll(borrowRecords, isFault);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemFree][MemFreeExecute] CollectBorrowRecords failed.";
        return ret;
    }
    for (auto& record : borrowRecords) {
        if (record.name == name) {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemFree][MemFreeExecute] Find borrow_id=" << name
                << " in borrow records, start to generate smap migrate back params.";
            int destNid = -1;
            if (isContainerScene) {
                destNid = record.borrowLocalNuma;
            }

            if (record.borrowMemId.size() > MAX_NR_MIGBACK_MP) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[MemFree][MemFreeExecute] The borrow_id=" << name
                    << " has too many memid, size=" << record.borrowMemId.size() << ".";
                return MEM_POOLING_ERROR;
            }

            for (size_t i = 0; i < record.borrowMemId.size(); i++) {
                migrateBackMsg.payload[i].memid = record.borrowMemId[i];
                migrateBackMsg.payload[i].srcNid = record.borrowRemoteNuma;
                migrateBackMsg.payload[i].destNid = destNid;
            }
            migrateBackMsg.count = static_cast<int>(record.borrowMemId.size());
            uint64_t taskId{};
            GenerateSmapTaskId(taskId);
            migrateBackMsg.taskID = taskId;
            importNodeId = record.borrowNode;
            for (int i = 0; i < migrateBackMsg.count; i++) {
                UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[MemFree][MemFreeExecute] Index=" << i << ", srcNid=" << migrateBackMsg.payload[i].srcNid
                    << ", dstNid=" << migrateBackMsg.payload[i].destNid
                    << ", memid=" << migrateBackMsg.payload[i].memid;
            }
            enableMsg.nid = record.borrowRemoteNuma;
            enableMsg.enable = SMAP_ENABLE_NUMA;
            break;
        }
    }

    return MEM_POOLING_OK;
}

// 从相同节点，借用内存执行RPC函数，发送到从节点执行，需要传入执行的节点Nid
MpResult MemBorrowExecutor::SmapMigreatBackRpc(const std::string importNodeId, const MigrateBackMsg& migrateBackMsg)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemFree][MemFreeExecute] Master invoke the agent SmapMigreatBackRpc, importNodeId=" << importNodeId << ".";

    MpResult ret = SmapMigrateBackProcess(migrateBackMsg);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemFree][MemFreeExecute] Smap migrate back execute failed.";
        return ret;
    }

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemFree][MemFreeExecute] Smap migrate back execute success.";
    return MEM_POOLING_OK;
}

void PersistenceSmapEnable(const int16_t& numaId)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemReturn] Start to PersistenceSmapEnable numaId=" << numaId << ".";
    MpResult retSmapEnableCompleted = SmapEnableCompleted::Instance().Update(numaId);
    if (retSmapEnableCompleted != MEM_POOLING_OK) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemReturn] Update SmapEnableCompleted numaId failed. ret=" << retSmapEnableCompleted << ".";
    } else {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn] Update SmapEnableCompleted numaId success.";
    }
}

void DeletePersistenceSmapEnable(const int16_t& numaId)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemReturn] Delete PersistenceSmapEnable numaId=" << numaId << ".";
    MpResult retSmapEnableCompleted = SmapEnableCompleted::Instance().Remove(numaId);
    if (retSmapEnableCompleted != MEM_POOLING_OK) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemReturn] Delete PersistenceSmapEnable numaId failed. ret=" << retSmapEnableCompleted << ".";
    } else {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn] Delete PersistenceSmapEnable pids success.";
    }
}

MpResult MemBorrowExecutor::MemFreeWithOpsBySmap(const std::string& name, const std::string& deleteName, bool isFault)
{
    MigrateBackMsg migrateBackMsg;
    EnableNodeMsg enableMsg;
    std::string importNodeId;
    auto retSmap = GenerateSmapParams(deleteName, migrateBackMsg, enableMsg, importNodeId, isFault);
    if (retSmap != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemFree][MemFreeExecute] GenerateParams failed.";
        return retSmap;
    }

    // 持久化SmapEnable数据
    if (migrateBackMsg.count > 0) {
        PersistenceSmapEnable(static_cast<int16_t>(migrateBackMsg.payload[0].srcNid));
    } else {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemFree][MemFreeExecute] Smap migrate back count=" << migrateBackMsg.count << " invalid.";
        return MEM_POOLING_ERROR;
    }
    retSmap = SmapMigrateBackProcess(migrateBackMsg);
    if (retSmap != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemFree][MemFreeExecute] Smap migrate back execute failed.";
        retSmap = SmapEnableNumaProcess(enableMsg);
        // 迁回失败并对远端numa进行了enable，把持久化数据删掉
        DeletePersistenceSmapEnable(static_cast<int16_t>(migrateBackMsg.payload[0].srcNid));
        if (retSmap != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemFree][MemFreeExecute] SmapEnableNumaProcess failed.";
        }
        return MEM_POOLING_ERROR;
    }
    
    auto retMemFreeByUbse = MemFreeWithOpsByMemfabric(name, deleteName, isFault);
    if (retMemFreeByUbse != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemFree][MemFreeExecute] MemFreeWithOpsByMemfabric failed.";
        return MEM_POOLING_ERROR;
    }

    retSmap = SmapEnableNumaProcess(enableMsg);
    // 归还成功并对远端numa进行了enable，把持久化数据删掉
    DeletePersistenceSmapEnable(static_cast<int16_t>(migrateBackMsg.payload[0].srcNid));
    if (retSmap != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemFree][MemFreeExecute] SmapEnableNumaProcess failed.";
        return MEM_POOLING_ERROR;
    }

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemFree][MemBorrowExecute] MemBorrowExecutor frees memory success, borrow_id=" << name << ".";

    return MEM_POOLING_OK;
}

MpResult MemBorrowExecutor::MemFreeWithOpsByMemfabric(const std::string& name, const std::string& deleteName,
                                                      bool isFault)
{
    std::vector<BorrowRecord> borrowRecords;
    auto retCode = BorrowRecordHelper::Instance().CollectBorrowRecordsAll(borrowRecords, isFault);
    if (retCode != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemFree][MemFreeExecute] Collect all borrowRecords failed.";
        return retCode;
    }
    UbseMemBorrower borrower;
    borrower.nodeId = name.substr(0, name.find('-'));

    UbseResult ret = UbseMemNumaDelete(deleteName, borrower);
    if (isFault && ret == UBSE_ERR_UNIMPORT_SUCCESS) {
 	         UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
 	             << "[MemFree][MemFreeExecute][FaultHandle] Free memory of borrowId=" << name
 	             << " unimport success in fault handle. Ret_code=" << static_cast<int>(ret) << ".";
    } else if (ret != UBSE_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemFree][MemFreeExecute] Free memory of borrowId " << name
            << " by memfabric failed. Ret_code=" << static_cast<int>(ret) << ".";
        if (ret == UBSE_ERR_TIMEOUT) {
            return HandleTimeoutFree(name, borrowRecords);
        }
        return static_cast<uint32_t>(ret);
    }

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemFree][MemBorrowExecute] MemBorrowExecutor frees memory success, borrow_id=" << name << ".";

    return MEM_POOLING_OK;
}

MpResult MemBorrowExecutor::HandleTimeoutFree(const std::string& name, const std::vector<BorrowRecord>& borrowRecords)
{
    UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemFree][MemFreeExecute] RackManager return err code is unknown, code=RUNNING, change to TIMEOUT.";
    // 然后从中过滤出name
    for (auto &record : borrowRecords) {
        if (record.name == name) {
            BorrowItem item{name,
                            record.borrowNode,
                            static_cast<uint16_t>(record.borrowRemoteNuma),
                            record.lentNode,
                            record.lentSocketId,
                            0};
            auto retCode = MemReturnManager::Instance().Update(name, item);
            if (retCode != MEM_POOLING_OK) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[MemFree][MemFreeExecute] Update borrowId=" << name << " failed.";
                return retCode;
            }
            if (!MemReturnScanner::Instance().start()) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[MemFree][MemFreeExecute] Start borrow return scanner failed.";
                return MEM_POOLING_ERROR;
            }
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemFree][MemFreeExecute] Free memory of borrowId=" << name
                << " , remoteNumaId=" << static_cast<uint16_t>(record.borrowRemoteNuma)
                << " ,lentNode=" << record.lentNode << " ,lentSocketId=" << record.lentSocketId << " time out.";
            break;
        }
    }
    return static_cast<uint32_t>(UBSE_ERR_TIMEOUT);
}

MpResult MemBorrowExecutor::MemFreeWithOps(const std::string &name, bool isForceDelete, bool smapBack, bool isFault)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemFree][MemFreeExecute] MemBorrowExecutor starts to free memory, borrowI_id=" << name << ".";

    std::string deleteName = name;

    std::string redirectNameKey = name;
    std::string redirectNameVal = name;

    do {
        redirectNameKey = redirectNameVal;
        redirectNameVal.clear();
        MpResult retDirect = BorrowIdRedirection::Instance().Query(redirectNameKey, redirectNameVal);
        if (retDirect != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemFree][MemFreeExecute] Get redirection of borrow_id=" << name << " failed.";
            return retDirect;
        }
    } while (!redirectNameVal.empty());

    deleteName = redirectNameKey;
    if (deleteName != name) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemFree][MemFreeExecute] BorrowId=" << name << " rediects to borrow_id=" << deleteName << ".";
    }

    if (smapBack) {
        auto ret = MemFreeWithOpsBySmap(name, deleteName, isFault);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemFree][MemFreeExecute] MemFreeWithOpsBySmap failed.";
            return ret;
        }
    } else {
        auto ret = MemFreeWithOpsByMemfabric(name, deleteName, isFault);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemFree][MemFreeExecute] MemFreeWithOpsByMemfabric failed.";
            return ret;
        }
    }

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemFree][MemBorrowExecute] MemBorrowExecutor frees memory success, borrow_id=" << name << ".";

    MpResult retDirect = RemoveBorrowIdRedirectionRecursively(name);
    if (retDirect != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemFree][MemFreeExecute] Remove redirection of borrow_id=" << name << " failed.";
        return MEM_POOLING_ERROR;
    }

    return MEM_POOLING_OK;
}

MpResult MemBorrowExecutor::GenerateUniqueId(const std::string& nodeId, std::string& str, const bool isFault)
{
    // 1. 纳秒级时间戳
    uint64_t ts =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count();

    // 2. 进程内递增计数器（防止同一时间戳内冲突）
    static std::atomic<uint32_t> counter{0};
    uint32_t seq = counter.fetch_add(1, std::memory_order_relaxed);

    // 3. 轻量随机扰动（不依赖 random_device）
    uint32_t mix = static_cast<uint32_t>(ts ^ (ts >> 32) ^ seq);

    // 4. 拼成 128 bit = 32 hex
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << ts // 64 bit
       << std::setw(8) << seq                                  // 32 bit
       << std::setw(8) << mix;                                 // 32 bit

    str = nodeId + "-" + ss.str();
    if (isFault) {
        str += "-rep";
    }

    return MEM_POOLING_OK;
}

MpResult MpMemBorrowExecutorModule::DeleteFailedBorrowIds()
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[Reliability][MemFree] Start freeing borrowIds.";

    std::vector<std::string> borrowIdListNeedToFree;

    MpResult ret = BorrowIdsCompleted::Instance().Query(borrowIdListNeedToFree);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[Reliability][MemFree] Get borrowed borrowId list failed.";
    }

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[Reliability][MemFree] The number of borrowIds need to free is " << borrowIdListNeedToFree.size() << ".";

    int borrowIdFreedCnt = 0;

    for (const auto& borrowId : borrowIdListNeedToFree) {
        ret = MemBorrowExecutor::Instance().MemFreeWithOps(borrowId, false, true);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[Reliability][MemFree] Mem free by executor failed, borrowId: " << borrowId;
            continue;
        }
        ret = BorrowIdsCompleted::Instance().Remove(borrowId);
        if (ret != MEM_POOLING_OK) {
            // 可靠性保障，如果失败，不影响主功能
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[Reliability][MemFree] Remove borrowed borrowId failed, borrowId is " << borrowId << ".";
        }
        borrowIdFreedCnt++;
    }

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[Reliability][MemFree] The number of borrowIds freed is " << borrowIdFreedCnt;

    return MEM_POOLING_OK;
}

} // namespace mempooling