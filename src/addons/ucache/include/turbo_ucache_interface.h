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

#ifndef TURBO_UCACHE_INTERFACE_H
#define TURBO_UCACHE_INTERFACE_H

#include <cmath>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace turbo::ucache {

const uint32_t EXECUTE_DEFAULT_ERROR = 1;

constexpr uint64_t MAX_COUNTER = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
constexpr uint64_t MAX_MEM_BYTES = 1ULL << 50;
constexpr double MAX_PSI = 100.0;

struct CgroupIoInfo {
    uint64_t ioReadBytes{};
    uint64_t ioWriteBytes{};
    uint64_t ioReadTimes{};
    uint64_t ioWriteTimes{};

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "ioReadBytes:" << ioReadBytes << ",";
        oss << "ioWriteBytes:" << ioWriteBytes << ",";
        oss << "ioReadTimes:" << ioReadTimes << ",";
        oss << "ioWriteTimes:" << ioWriteTimes;
        oss << "}";
        return oss.str();
    }

    [[nodiscard]] bool Validate() const
    {
        if (ioReadBytes > MAX_COUNTER || ioWriteBytes > MAX_COUNTER || ioReadTimes > MAX_COUNTER ||
            ioWriteTimes > MAX_COUNTER) {
            return false;
        }
        return true;
    }
};

struct CgroupPageCacheInfo {
    uint64_t totalActiveFileBytes{};
    uint64_t totalInactiveFileBytes{};
    uint64_t pageCacheIn{};
    uint64_t pageCacheOut{};

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "totalActiveFileBytes:" << totalActiveFileBytes << ",";
        oss << "totalInactiveFileBytes:" << totalInactiveFileBytes << ",";
        oss << "pageCacheIn:" << pageCacheIn << ",";
        oss << "pageCacheOut:" << pageCacheOut;
        oss << "}";
        return oss.str();
    }

    [[nodiscard]] bool Validate() const
    {
        if (totalActiveFileBytes > MAX_MEM_BYTES || totalInactiveFileBytes > MAX_MEM_BYTES) {
            return false;
        }
        if (pageCacheIn > MAX_COUNTER || pageCacheOut > MAX_COUNTER) {
            return false;
        }
        return true;
    }
};

struct CgroupInfos {
    struct CgroupIoInfo ioInfo {};
    struct CgroupPageCacheInfo pageCacheInfo {};

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "CgroupIoInfo:" << ioInfo.ToString() << ",";
        oss << "CgroupPageCacheInfo:" << pageCacheInfo.ToString();
        oss << "}";
        return oss.str();
    }

    [[nodiscard]] bool Validate() const
    {
        return ioInfo.Validate() && pageCacheInfo.Validate();
    }
};

struct NodeInfo {
    uint64_t memTotalBytes{};
    uint64_t memFreeBytes{};
    uint64_t memUsedBytes{};
    uint64_t activeFileBytes{};
    uint64_t inactiveFileBytes{};
    bool isRemote{};

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "memTotalBytes:" << memTotalBytes << ",";
        oss << "memFreeBytes:" << memFreeBytes << ",";
        oss << "memUsedBytes:" << memUsedBytes << ",";
        oss << "activeFileBytes:" << activeFileBytes << ",";
        oss << "inactiveFileBytes:" << inactiveFileBytes << ",";
        oss << "isRemote:" << (isRemote ? "true" : "false");
        oss << "}";
        return oss.str();
    }

    [[nodiscard]] bool Validate() const
    {
        if (memTotalBytes > MAX_MEM_BYTES || memFreeBytes > MAX_MEM_BYTES || memUsedBytes > MAX_MEM_BYTES ||
            activeFileBytes > MAX_MEM_BYTES || inactiveFileBytes > MAX_MEM_BYTES) {
            return false;
        }
        if (memUsedBytes > memTotalBytes) {
            return false;
        }
        return true;
    }
};

struct MemWatermarkInfo {
    uint64_t minFreeKBytes{};

    [[nodiscard]] bool Validate() const
    {
        if (minFreeKBytes > MAX_MEM_BYTES) {
            return false;
        }
        return true;
    }
};

struct MigrationStrategyParam {
    int32_t dstNid{};
    uint64_t highWatermarkPages{};
    uint64_t lowWatermarkPages{};
    std::vector<std::string> dockerIds{};
    std::vector<int32_t> srcNids{};
    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "dstNid:" << dstNid << ",";
        oss << "highWatermarkPages:" << highWatermarkPages << ",";
        oss << "lowWatermarkPages:" << lowWatermarkPages << ",";
        oss << "dockerIds:[";
        for (size_t i = 0; i < dockerIds.size(); ++i) {
            oss << "\"" << dockerIds[i] << "\"";
            if (i != dockerIds.size() - 1) {
                oss << ", ";
            }
        }
        oss << "],";
        oss << "srcNids:[";
        for (size_t i = 0; i < srcNids.size(); ++i) {
            oss << srcNids[i];
            if (i != srcNids.size() - 1) {
                oss << ", ";
            }
        }
        oss << "]";
        oss << "}";
        return oss.str();
    }
};

enum class TaskType : uint32_t {
    INVALID = 0xFFFFFFFF,
    COLLECT_RESOURCE = 0,
    MIGRATION_STRATEGY = 1,
};

inline const char* TaskTypeToString(TaskType t)
{
    switch (t) {
        case TaskType::COLLECT_RESOURCE:
            return "CollectResource";
        case TaskType::MIGRATION_STRATEGY:
            return "MigrationStrategy";
        case TaskType::INVALID:
            return "InvalidTaskType";
    }
    return "UnknownTaskType";
}

inline bool IsValidTaskType(const TaskType t)
{
    switch (t) {
        case TaskType::COLLECT_RESOURCE:
        case TaskType::MIGRATION_STRATEGY:
            return true;
        case TaskType::INVALID:
            return false;
    }
    return false;
}

enum class ResourceQueryType : uint32_t {
    NUMA_INFO = 0,
    CGROUP_INFO = 1,
    MEM_WATERMARK = 2,
};

inline const char* ResourceQueryTypeToString(ResourceQueryType t)
{
    switch (t) {
        case ResourceQueryType::NUMA_INFO:
            return "NumaInfo";
        case ResourceQueryType::CGROUP_INFO:
            return "CgroupInfo";
        case ResourceQueryType::MEM_WATERMARK:
            return "MemWatermark";
    }
    return "UnknownResourceQueryType";
}

inline bool IsValidResourceQueryType(const ResourceQueryType t)
{
    switch (t) {
        case ResourceQueryType::NUMA_INFO:
            return true;
        case ResourceQueryType::CGROUP_INFO:
            return true;
        case ResourceQueryType::MEM_WATERMARK:
            return true;
    }
    return false;
}

struct TaskRequest {
    TaskType type = TaskType::INVALID;
    std::string payload{};
};

struct TaskResponse {
    uint32_t resCode = EXECUTE_DEFAULT_ERROR;
    uint64_t timestamp{};
    std::string payload{};
};

extern "C" {
/**
 * @brief 下发ucache任务执行指令
 * @param tReq [IN] ucache执行参数
 * @param tResp [OUT] ucache执行结果
 * @return 0为IPC执行成功，非0为IPC执行异常
 */
uint32_t UBTurboUCacheExecuteTask(const TaskRequest& tReq, TaskResponse& tResp);
}
} // namespace turbo::ucache
#endif // TURBO_UCACHE_INTERFACE_H