/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "ubse_node_controller_util.h"
#include <optional>
#include <unordered_map>
#include "adapter_plugins/mti/ubse_mti_interface.h"
#include "securec.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_lcne_module.h"
#include "ubse_logger_module.h"
#include "ubse_mem_configuration.h"
#include "ubse_mem_constants.h"
#include "ubse_net_util.h"
#include "ubse_node_controller.h"
#include "ubse_node_controller_collector.h"
#include "ubse_str_util.h"
namespace ubse::nodeController {
using namespace ubse::common::def;
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::log;
using namespace ubse::utils;
using namespace ubse::mem::strategy;
using namespace ubse::mti;

UBSE_DEFINE_THIS_MODULE("ubse");

std::mutex UbseNodeControllerLockMgr::nodeControllerMutex_;
std::unordered_map<std::string, std::shared_ptr<std::shared_mutex>> UbseNodeControllerLockMgr::nodeControllerLocks_{};

bool ubEnable = false;

std::shared_ptr<std::shared_mutex> UbseNodeControllerLockMgr::GetLock(const std::string &nodeId)
{
    std::lock_guard<std::mutex> mapLock(nodeControllerMutex_);
    auto it = nodeControllerLocks_.find(nodeId);
    if (it != nodeControllerLocks_.end()) {
        return it->second;
    }
    auto lock = std::make_shared<std::shared_mutex>();
    nodeControllerLocks_.emplace(nodeId, lock);
    return lock;
}

void UbseNodeControllerLockMgr::WriteLock(const std::string &nodeId)
{
    GetLock(nodeId)->lock();
}

void UbseNodeControllerLockMgr::WriteUnLock(const std::string &nodeId)
{
    GetLock(nodeId)->unlock();
}

void UbseNodeControllerLockMgr::TryWriteLock(const std::string &nodeId)
{
    GetLock(nodeId)->try_lock();
}

void UbseNodeControllerLockMgr::ReadLock(const std::string &nodeId)
{
    GetLock(nodeId)->lock_shared();
}

void UbseNodeControllerLockMgr::ReadUnLock(const std::string &nodeId)
{
    GetLock(nodeId)->unlock_shared();
}

bool UbseNodeControllerLockMgr::TryReadLock(const std::string &nodeId)
{
    return GetLock(nodeId)->try_lock_shared();
}

static std::string g_allocator = "init";

UbseAllocator GetAllocator()
{
    std::unordered_map<std::string, UbseAllocator> allocatorMap = {
        {"hugetlb_pmd", UbseAllocator::HUGETLB_PMD},
        {"hugetlb_pud", UbseAllocator::HUGETLB_PUD},
        {"buddy_highmem", UbseAllocator::BUDDY_HIGHMEM},
    };

    if (g_allocator != "init") {
        return allocatorMap[g_allocator];
    }
    auto ret = GetUbseConf("obmm", "mempool_allocator", g_allocator);
    if (ret != UBSE_OK || allocatorMap.find(g_allocator) == allocatorMap.end()) {
        g_allocator = "buddy_highmem";
        UBSE_LOG_WARN << "Get allocator failed, Use default allocator " << g_allocator;
    }
    return allocatorMap[g_allocator];
}

uint32_t GetPmdMapping()
{
    uint32_t val;
    auto ret = GetUbseConf("os", "pmd_mapping", val);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "read pmd mapping config failed, section=os, key=pmd_mapping";
        return MAX_PERCENT;
    }
    if (val > MAX_PERCENT) {
        UBSE_LOG_WARN << "read pmd mapping config invalid, section=os, key=pmd_mapping";
        return MAX_PERCENT;
    }
    return val;
}

static uint32_t blockSize = 0;

uint32_t GetBlockSize(UbseAllocator allocator)
{
    if (blockSize != 0) {
        return blockSize;
    }
    std::string value;
    auto ret = GetUbseConf("ubse.memory", "obmm.memory.block.size", value);
    if (ret == UBSE_OK && !value.empty()) {
        uint32_t parsed = 0;
        if (ubse::utils::ConvertStrToUint32(value, parsed) == UBSE_OK && parsed >= MB_4M && parsed <= MB_4096M &&
            parsed % ubse::common::def::NO_2 == 0) {
            blockSize = parsed;
            return blockSize;
        }
    }
    if (allocator == UbseAllocator::HUGETLB_PUD) {
        blockSize = BLOCK_1G;
        return blockSize;
    }
    std::string osPageSize;
    ret = GetUbseConf("os", "page_size", osPageSize);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Get os page_size type failed, Use default value 4096";
        osPageSize = PAGE_SIZE_4K;
    }
    if (osPageSize == PAGE_SIZE_64K) {
        blockSize = BLOCK_512M;
    } else if (osPageSize == PAGE_SIZE_4K)  {
        blockSize = BLOCK_128M;
    } else {
        UBSE_LOG_WARN << "Get os page_size type is invalid, Use default value 4096";
        blockSize = BLOCK_128M;
    }
    return blockSize;
}

void GetCurNodeInfo(UbseNodeInfo &info)
{
    adapter_plugins::mti::UbseMtiNodeInfo ubseNodeInfo{};
    auto ret = adapter_plugins::mti::UbseMtiInterface::GetInstance().GetLocalNodeInfo(ubseNodeInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get local node info failed, " << FormatRetCode(ret);
        return;
    }
    info.nodeId = ubseNodeInfo.nodeId;
    auto cpyRet = strcpy_s(info.bondingEid, sizeof(info.bondingEid), ubseNodeInfo.eid.c_str());
    if (cpyRet != EOK) {
        UBSE_LOG_ERROR << "cpy eid failed, ErrorCode=" << cpyRet;
        return;
    }
    ret = ConvertStrToUint32(ubseNodeInfo.nodeId, info.slotId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Convert slotId failed, " << FormatRetCode(ret);
        return;
    }

    // 只在初始化时获取并存储guid
    if (info.guid.empty()) {
        auto lcneModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::mti::UbseLcneModule>();
        if (lcneModule != nullptr) {
            UbseLcneOSInfo lcneOsInfo = lcneModule->GetUbseLcneOSInfo();
            info.guid = lcneOsInfo.guid;
            if (info.guid.empty()) {
                UBSE_LOG_WARN << "GUID is empty from LcneOSInfo";
            }
        } else {
            UBSE_LOG_ERROR << "Failed to get UbseLcneModule instance";
            info.guid = "";
        }
    } else {
        UBSE_LOG_DEBUG << "Using cached GUID: " << info.guid << " for node: " << info.nodeId;
    }

    info.allocator = GetAllocator();
    // pud场景1G大页全用于内存借用，pmdMapping为100%
    info.pmdMapping = (info.allocator == UbseAllocator::HUGETLB_PUD) ? MAX_PERCENT : GetPmdMapping();
    info.blockSize = GetBlockSize(info.allocator);
    if (info.allocator == UbseAllocator::HUGETLB_PMD && info.pmdMapping != MAX_PERCENT) {
        UBSE_LOG_WARN << "hugetlb_pmd requires pmd mapping 100%, current=" << info.pmdMapping;
        return;
    }
    if (info.allocator == UbseAllocator::BUDDY_HIGHMEM && info.pmdMapping == MAX_PERCENT) {
        UBSE_LOG_WARN << "buddy_highmem not expected at 100%, continue if Ok";
    }
}
} // namespace ubse::nodeController