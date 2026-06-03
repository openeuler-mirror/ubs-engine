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
#include "ubse_ssu_scheduler.h"

#include <algorithm>
#include <unordered_set>
#include "ubse_logger.h"

namespace ubse::ssu::scheduler {
UBSE_DEFINE_THIS_MODULE("ubse");

using namespace ubse::filter_chain;

constexpr uint64_t DEFAULT_SECTOR_SIZE = 512;
constexpr uint64_t LBA0_SECTOR_SIZE = 512;
constexpr uint64_t LBA1_SECTOR_SIZE = 4096;
constexpr uint64_t MAX_NS_PER_DEV = 128;

// 生成分配失败结果
UbseSsuAllocationResult MakeError(UbseSsuAllocRetCode code, const std::string &msg)
{
    UbseSsuAllocationResult res;
    res.ret = code;
    res.msg = msg;
    return res;
}

// 条带化分配默认实现
UbseSsuAllocationResult AllocateStriped(const std::vector<UbseSsuFilterDev> &filterDevs,
                                        const UbseSsuAllocRequest &request)
{
    // 条带大小
    uint64_t stripeSize = static_cast<uint64_t>(request.chunkSize) * request.nsNum;
    if (request.nsNum == 0 || stripeSize == 0) {
        UBSE_LOG_ERROR << "chunkSize is zero or stripeSize is zero or nsNum is zero";
        return MakeError(UbseSsuAllocRetCode::INVALID_PARAM,
                         "Allocation Failed: chunkSize is zero or stripeSize is zero or nsNum is zero");
    }
    // 分配总大小需要为条带倍数，向上对齐
    uint64_t alignedReqAllocSize = ((request.allocSize + stripeSize - 1) / stripeSize) * stripeSize;
    uint64_t alignedNsSize = alignedReqAllocSize / request.nsNum;
    // 前nsNum个设备中剩余空间最小的设备是否足够存储每个ns
    uint64_t minFreeInGroup = filterDevs[request.nsNum - 1].freeBytes;
    if (alignedNsSize > minFreeInGroup) {
        UBSE_LOG_ERROR << "each NS requires " << alignedNsSize
                       << " bytes but minimum free device only has " << minFreeInGroup << " bytes";
        return MakeError(UbseSsuAllocRetCode::INSUFFICIENT_SPACE,
                         "Allocation Failed: each NS requires " + std::to_string(alignedNsSize) +
                         " bytes but minimum free device only has " + std::to_string(minFreeInGroup) + " bytes");
    }

    UbseSsuAllocationResult res = {UbseSsuAllocRetCode::OK,
                                   "Success: allocated " + std::to_string(request.nsNum) + " namespaces",
                                   request.nsNum,
                                   filterDevs[0].sectorBytes,
                                   {}};
    for (uint32_t i = 0; i < request.nsNum; ++i) {
        res.eidNsSizeList.push_back({filterDevs[i].eid, alignedNsSize});
    }
    UBSE_LOG_INFO << "Striped Allocation Result: " << res.msg;
    return res;
}

// 负载均衡算法，用于线性编制
uint64_t LoadBalanceAlgorithm(const std::vector<UbseSsuFilterDev> &selectedDevs, uint64_t curAlignedReq, uint32_t nsNum,
                              uint64_t sectorSize, std::vector<std::pair<std::string, uint64_t>> &eidNsSizeList)
{
    uint64_t totalFreeBytes = 0;
    for (size_t i = 0; i < nsNum; ++i) {
        totalFreeBytes += selectedDevs[i].freeBytes;
    }

    // 防御性校验
    if (totalFreeBytes < curAlignedReq) {
        return curAlignedReq;
    }
    if (sectorSize == 0) {
        UBSE_LOG_ERROR << "sectorSize is zero";
        return curAlignedReq;
    }

    uint64_t totalAllocated = 0;
    std::vector<uint64_t> devAllocatedSizes(nsNum, 0);
    // 第一轮：按权重比例向下对齐分配
    for (size_t i = 0; i < nsNum; ++i) {
        const auto &dev = selectedDevs[i];
        uint64_t target = (uint64_t)(((__uint128_t)curAlignedReq * dev.freeBytes) / totalFreeBytes);
        
        // 向下对齐到扇区
        uint64_t alignedTarget = (target / sectorSize) * sectorSize;
        if (alignedTarget > dev.freeBytes) {
            // 如果计算值爆了，直接降级取该设备物理上能够给出的最大对齐空间
            alignedTarget = (dev.freeBytes / sectorSize) * sectorSize;
        }
        
        devAllocatedSizes[i] = alignedTarget;
        totalAllocated += alignedTarget;
    }

    // 第二轮：由于双重向下对齐（权重计算+扇区对齐）产生的余量，用贪心轮询法补齐
    uint64_t remainingToAllocate = curAlignedReq - totalAllocated;
    if (remainingToAllocate > 0) {
        for (size_t i = 0; i < nsNum && remainingToAllocate > 0; ++i) {
            uint64_t alreadyAllocated = devAllocatedSizes[i];
            
            // 算一下真正的富余空间，并向下对齐到 sectorSize，得到当前盘能给的最大值
            uint64_t alignedFree = (selectedDevs[i].freeBytes - alreadyAllocated) / sectorSize * sectorSize;
            if (alignedFree == 0) {
                continue;
            }

            // 决定给多少：取当前盘能给的最大值和系统需要的最小值的交集
            uint64_t canGiveExtra = std::min(alignedFree, remainingToAllocate) / sectorSize * sectorSize;
            
            devAllocatedSizes[i] += canGiveExtra;
            remainingToAllocate -= canGiveExtra;
        }
    }
    // 构建结果
    for (size_t i = 0; i < nsNum; ++i) {
        eidNsSizeList.push_back({selectedDevs[i].eid, devAllocatedSizes[i]});
    }
    return remainingToAllocate;
}

// 线性分配默认实现
UbseSsuAllocationResult AllocateLinear(const std::vector<UbseSsuFilterDev> &filterDevs,
                                       const UbseSsuAllocRequest &request)
{
    uint64_t sectorSize = request.lbaSize;
    if (sectorSize == 0) {
        UBSE_LOG_ERROR << "sectorSize is zero";
        return MakeError(UbseSsuAllocRetCode::INVALID_PARAM, "Allocation Failed: sectorSize is zero");
    }
    uint64_t curAlignedReq = ((request.allocSize + sectorSize - 1) / sectorSize) * sectorSize;

    if (request.nsNum == 0) {
        UBSE_LOG_ERROR << "nsNum is zero";
        return MakeError(UbseSsuAllocRetCode::INVALID_PARAM, "Allocation Failed: nsNum is zero");
    }

    // 调用负载均衡算法
    UbseSsuAllocationResult res;
    res.eidNsSizeList = {};
    uint64_t remainingToAllocate = LoadBalanceAlgorithm(filterDevs, curAlignedReq, request.nsNum,
                                                        sectorSize, res.eidNsSizeList);
    // 检查是否还有剩余请求未分配
    if (remainingToAllocate > 0 || res.eidNsSizeList.size() != request.nsNum) {
        UBSE_LOG_ERROR << "Only " << request.nsNum << " devices can satisfy " << request.allocSize
                       << " bytes request, but " << remainingToAllocate << " bytes remaining";
        return MakeError(UbseSsuAllocRetCode::INSUFFICIENT_SPACE,
                         "Allocation Failed: " + std::to_string(request.nsNum) +
                         " devices cannot satisfy " + std::to_string(request.allocSize) + " bytes request");
    }
    res = {UbseSsuAllocRetCode::OK,
           "Linear Success: Evenly distribution across " + std::to_string(request.nsNum) + " devices. ",
           request.nsNum,
           sectorSize,
           res.eidNsSizeList};
    UBSE_LOG_INFO << "Linear Allocation Result: " << res.msg;
    return res;
}

// 预处理设备，初步过滤出符合条件的设备
std::vector<UbseSsuFilterDev> PreprocessDevices(const std::vector<UbseSsuDevInfo> &devices,
                                                const UbseSsuAllocRequest &request)
{
    if (request.nsNum == 0) {
        UBSE_LOG_ERROR << "nsNum is zero";
        return {};
    }
    std::vector<UbseSsuFilterDev> filterDevs;
    for (const auto &dev : devices) {
        // 过滤非ONLINE状态设备或已达最大NS数量的设备
        if (dev.state != UbseSsuState::ONLINE || dev.nameSpaces.size() >= MAX_NS_PER_DEV) {
            continue;
        }
        uint64_t freeBytes = (dev.totalBytes > dev.usedBytes) ? (dev.totalBytes - dev.usedBytes) : 0;
        uint64_t sectorSize = request.lbaSize == 0 ? DEFAULT_SECTOR_SIZE : request.lbaSize;
        uint32_t nsCount = static_cast<uint32_t>(dev.nameSpaces.size());
        filterDevs.push_back({dev.subSystem.eid, freeBytes, sectorSize, nsCount});
    }

    // 针对条带化增加容量初筛策略，过滤掉不足容纳条带化Namespace分配的设备
    if (request.addressingType == UbseSsuAddressingType::STRIPED) {
        // 条带大小
        uint64_t stripeSize = static_cast<uint64_t>(request.chunkSize) * request.nsNum;
        uint64_t alignedReqAllocSize = ((request.allocSize + stripeSize - 1) / stripeSize) * stripeSize;
        uint64_t targetPerNs = alignedReqAllocSize / request.nsNum;
        // 过滤空间不够单Namespace分配的设备
        filterDevs.erase(std::remove_if(filterDevs.begin(), filterDevs.end(),
                                        [targetPerNs](const UbseSsuFilterDev &dev) {
                                            return dev.freeBytes < targetPerNs;
                                        }),
                         filterDevs.end());
    }

    // 按剩余空间降序（优先分配空间大的设备，提高分配成功率）、nsCount升序，兼顾均衡
    std::sort(filterDevs.begin(), filterDevs.end(), [](const UbseSsuFilterDev &a, const UbseSsuFilterDev &b) {
        if (a.freeBytes != b.freeBytes) {
            return a.freeBytes > b.freeBytes;
        }
        return a.nsCount < b.nsCount;
    });
    return filterDevs;
}

bool PreCheckHandler::Handle(UbseSsuAllocationContext &ctx)
{
    // VALIDATE - 检查参数是否有效
    if (ctx.request.allocSize == 0 || ctx.request.nsNum == 0) {
        UBSE_LOG_ERROR << "Invalid parameters: allocSize, nsNum is zero";
        ctx.result = MakeError(UbseSsuAllocRetCode::INVALID_PARAM,
                               "Allocation Failed: Invalid parameters: allocSize, nsNum is zero");
        return false;
    }

    // PREPROCESS - 设备信息预过滤，过滤掉不满足条件的设备，并按剩余空间降序、nsCount升序排序
    ctx.selectedDevs = PreprocessDevices(ctx.devices, ctx.request);
    if (ctx.selectedDevs.empty()) {
        UBSE_LOG_ERROR << "No online devices available";
        ctx.result = MakeError(UbseSsuAllocRetCode::INSUFFICIENT_SPACE, "No online devices available");
        return false;
    }

    // 检查在线设备数量是否足够
    if (ctx.selectedDevs.size() < ctx.request.nsNum) {
        UBSE_LOG_ERROR << "only " << ctx.selectedDevs.size() << " filtered devices available, but "
                       << ctx.request.nsNum << " required";
        ctx.result = MakeError(UbseSsuAllocRetCode::INSUFFICIENT_SPACE,
                               "Allocation Failed: only " + std::to_string(ctx.selectedDevs.size()) +
                               " devices available, but " + std::to_string(ctx.request.nsNum) + " required");
        return false;
    }
    return true;
}

bool UbseSsuAllocateAlgorithmHandler::Handle(UbseSsuAllocationContext &ctx)
{
    // ALLOCATE - 根据编址方式进行分配
    if (ctx.request.addressingType == UbseSsuAddressingType::LINEAR) {
        ctx.result = AllocateLinear(ctx.selectedDevs, ctx.request);
    } else {
        ctx.result = AllocateStriped(ctx.selectedDevs, ctx.request);
    }
    if (ctx.result.ret != UbseSsuAllocRetCode::OK) {
        UBSE_LOG_ERROR << "Allocation failed: " << ctx.result.msg;
        return false;
    }
    // 按分配结果过滤selectedDevs，仅保留被选中的设备
    std::unordered_set<std::string> selectedEids;
    for (const auto &eidNsSize : ctx.result.eidNsSizeList) {
        selectedEids.insert(eidNsSize.first);
    }
    ctx.selectedDevs.erase(std::remove_if(ctx.selectedDevs.begin(), ctx.selectedDevs.end(),
                                          [&selectedEids](const UbseSsuFilterDev &dev) {
                                              return selectedEids.find(dev.eid) == selectedEids.end();
                                        }),
                           ctx.selectedDevs.end());
    return true;
}

UbseSsuScheduler::UbseSsuScheduler()
{
    chain_.AddFilter(static_cast<uint32_t>(UbseSsuFilterGroupId::PRE_CHECK),
                     std::make_shared<PreCheckHandler>());
    chain_.AddFilter(static_cast<uint32_t>(UbseSsuFilterGroupId::ALGORITHM),
                     std::make_shared<UbseSsuAllocateAlgorithmHandler>());
}

UbseSsuAllocRetCode UbseSsuScheduler::Execute(UbseSsuAllocationContext &ctx)
{
    chain_.Execute(ctx);
    return ctx.result.ret;
}

void UbseSsuScheduler::AddFilter(UbseSsuFilterGroupId groupId,
                                 std::shared_ptr<UbseFilter<UbseSsuAllocationContext>> filter)
{
    if (!filter) {
        UBSE_LOG_ERROR << "AddFilter: filter is nullptr";
        return;
    }
    chain_.AddFilter(static_cast<uint32_t>(groupId), std::move(filter));
}
} // namespace ubse::ssu::scheduler
