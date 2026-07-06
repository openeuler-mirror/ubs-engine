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
#ifndef UBSE_SSU_SCHEDULER_H
#define UBSE_SSU_SCHEDULER_H

#include <memory>
#include <vector>
#include "ubse_filter_chain.h"
#include "ubse_ssu_def.h"

namespace ubse::ssu::scheduler {

using namespace ubse::adapter_plugins::ssu::def;
using namespace ubse::filter_chain;

struct UbseSsuAllocRequest {
    uint64_t allocSize = 0; // 请求分配大小（字节）
    uint32_t nsNum = 0; // 分配设备（namespace）数量，分配不了这数量将返回失败
    uint32_t lbaSize = 0; // 扇区大小（字节)
    UbseSsuAddressingType addressingType = UbseSsuAddressingType::LINEAR; // 编址类型
    std::string tenant; // 请求方租户（租户隔离标识）
};

enum class UbseSsuAllocRetCode : uint32_t {
    OK = 0,                // 分配成功
    INVALID_PARAM = 1,     // 参数不合法
    INSUFFICIENT_SPACE = 2, // 可分配空间不足
    INTERNAL_ERROR = 3,    // 内部错误
};

struct UbseSsuAllocationResult {
    UbseSsuAllocRetCode ret = UbseSsuAllocRetCode::INTERNAL_ERROR; // 分配结果错误码
    std::string msg; // 分配结果消息
    uint32_t nsNum = 0; // 分配设备（namespace）数量
    uint64_t sectorSize = 0; // 扇区大小（字节）
    std::vector<std::pair<std::string, uint64_t>> eidNsSizeList = {}; // 每个设备的要分配的命名空间大小,条带化时元素大小一致
};

// 责任链各节点输出中间结果
struct UbseSsuFilterDev {
    std::string eid; // 设备EID
    uint64_t freeBytes = 0; // 设备可用空间（字节）
    uint64_t sectorBytes = 0; // 扇区大小（字节）
    uint32_t nsCount = 0; // 设备已有的namespace数量
    std::string tenant; // 设备租户（租户隔离标识）
};

class UbseSsuAllocationContext {
public:
    const std::vector<UbseSsuDevInfo> devices; // 原始输入设备列表（只读）
    const UbseSsuAllocRequest &request;
    UbseSsuAllocationResult result;
    std::vector<UbseSsuFilterDev> selectedDevs; // filter过滤后，符合条件的设备

    UbseSsuAllocationContext(const std::vector<UbseSsuDevInfo> &devs, const UbseSsuAllocRequest &req)
        : devices(devs), request(req)
    {
    }
};

class PreCheckHandler : public UbseFilter<UbseSsuAllocationContext> {
public:
    bool Handle(UbseSsuAllocationContext &ctx) override;
};

class UbseSsuTenantIsolationFilter : public UbseFilter<UbseSsuAllocationContext> {
public:
    bool Handle(UbseSsuAllocationContext &ctx) override;
};

// 默认实现：考虑分配均衡，将优先使用容量大的设备为第一优先级，namespace数量少的设备为第二优先级。
class UbseSsuAllocateAlgorithmHandler : public UbseFilter<UbseSsuAllocationContext> {
public:
    bool Handle(UbseSsuAllocationContext &ctx) override;
};

enum class UbseSsuFilterGroupId : uint32_t {
    PRE_CHECK = 0,
    ALGORITHM = 1,
    POST_CHECK = 2,
};

// 责任链模式：处理分配请求，每个group的filter负责一部分逻辑，按照groupId顺序执行。
class UbseSsuScheduler {
public:
    // 构造函数添加默认的filter
    UbseSsuScheduler();

    // group 顺序来执行，同一个group按照vector顺序
    UbseSsuAllocRetCode Execute(UbseSsuAllocationContext &ctx);

    // 指定group添加filter
    void AddFilter(UbseSsuFilterGroupId groupId, std::shared_ptr<UbseFilter<UbseSsuAllocationContext>> filter);
private:
    UbseFilterChain<UbseSsuAllocationContext> chain_;
};

} // namespace ubse::ssu::scheduler

#endif // UBSE_SSU_SCHEDULER_H
