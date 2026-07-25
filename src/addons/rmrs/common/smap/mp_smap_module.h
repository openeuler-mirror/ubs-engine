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

#ifndef RACK_MANAGER_SMAP_MODULE_H
#define RACK_MANAGER_SMAP_MODULE_H

#include <dlfcn.h>
#include <sstream>
#include "ubse_common_def.h"
#include "mp_error.h"

namespace mempooling::smap {
const char* const SMAP_LIBSMAPSO_PATH = "/usr/lib64/libubturbo_client.so";
const uint32_t PAGE_TYPE = 1;
const int RUN_MODE_VM = 0;
const int RUN_MODE_MP = 1;
const int PID_TYPE = 1;
const int SMAP_RATIO_MP = 25;
const int SMAP_MAX_RATIO_MP = 100;
const int MAX_NR_MIGOUT_MP = 40;
const int REMOTE_NUMA_NUM = 18;
const int MAX_NR_MIGBACK_MP = 50;
const int MAX_NR_REMOVE_MP = 40;
const int MAX_NR_MIGNUMA = 50;
constexpr uint32_t GET_RET_TIME = 1;
constexpr uint32_t RETRY_MAX_COUNT = 20;
constexpr uint16_t FAIL_RETRY_COUNT = 10;
const int MAX_NR_MIGOUT = 40;
constexpr int MAX_NR_GROUPED_MIGOUT = MAX_NR_MIGOUT;
constexpr int MAX_MIGRATION_GROUP_NUM = 8;
constexpr int MAX_GROUP_LOCAL_NUMA = 4;
constexpr int MAX_GROUP_REMOTE_NUMA = REMOTE_NUMA_NUM;

// 迁出接口超时返回码
const int MIGRATEOUT_TIMEOUT_RES = -16;

// SMAP BACK 错误码
const int SMAP_BACK_ERROR_UNINIT = -1;
const int SMAP_BACK_ERROR_NOMEM = -12;
const int SMAP_BACK_ERROR_NOIPC = -16;
const int SMAP_BACK_ERROR_INVAL = -22;
const int SMAP_BACK_ERROR_AGAIN = -11;

typedef enum
{
    MIG_RATIO_MODE = 0, // 按照比例迁移
    MIG_MEMSIZE_MODE,   // 按照内存大小迁移
} MigrateMode;

struct MigrateOutPayloadInner {
    int destNid{};             // remoteNumaId
    int ratio = SMAP_RATIO_MP; // 默认冷数据迁移比例
    uint64_t memSize;          // 新增字段： 内存迁移大小(KB)
    MigrateMode migrateMode;   // 新增字段： 内存迁移模式，按照比例或是大小
};

struct MigrateOutPayload {
    int srcNid; // 是否指定迁出源节点（-1表示不指定）
    pid_t pid;  // 待迁移冷数据的虚机实例进程pid
    int count;
    struct MigrateOutPayloadInner inner[REMOTE_NUMA_NUM];
};

struct MigrateOutMsg {
    int count{};
    MigrateOutPayload payload[MAX_NR_MIGOUT_MP];
};

struct RemoteNumaInfo {
    int srcNid;
    int destNid;
    uint64_t size;

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{srcNid_id=" << srcNid << ", ";
        oss << "destNid_id=" << destNid << ", ";
        oss << "size=" << size << " MB}";
        return oss.str();
    }
};

struct RemovePayload {
    pid_t pid{};
    int count{};
    int nid[REMOTE_NUMA_NUM]{};

    std::string ToString() const
    {
        std::ostringstream oss;

        oss << R"({"pid":)" << pid << R"(,)";
        oss << R"("count":)" << count << R"(,)";
        oss << R"("nid":[)";

        int limit = std::min(count, REMOTE_NUMA_NUM);

        for (int i = 0; i < limit; ++i) {
            if (i != 0)
                oss << ",";
            oss << nid[i];
        }

        oss << "]}";

        return oss.str();
    }
};

struct RemoveMsg {
    int count{};
    RemovePayload payload[MAX_NR_REMOVE_MP];

    std::string ToString() const
    {
        std::ostringstream oss;

        oss << R"({"count":)" << count << R"(,)";
        oss << R"("payload":[)";

        int limit = std::min(count, MAX_NR_REMOVE_MP);

        for (int i = 0; i < limit; ++i) {
            if (i != 0)
                oss << ",";
            oss << payload[i].ToString();
        }

        oss << "]}";

        return oss.str();
    }
};

struct MigrateBackPayload {
    int srcNid;
    int destNid;
    uint64_t memid;
};

struct MigrateBackMsg {
    unsigned long long taskID;
    int count;
    struct MigrateBackPayload payload[MAX_NR_MIGBACK_MP];
};

struct MigrateBackRes {
    uint32_t result;
};

struct EnableNodeMsg {
    int enable;
    int nid;
};

struct EnableNodeRes {
    uint32_t result;
};

enum NumaEnable : int32_t
{
    SMAP_DISABLE_NUMA = 0,
    SMAP_ENABLE_NUMA = 1,
    SMAP_BUTT
};

enum SampTaskResult : uint16_t
{
    MB_TASK_CREATED = 0,
    MB_TASK_WAITING = 1,
    MB_TASK_DONE = 2,
    MB_TASK_ERR = 3,
    MB_TASK_BUTT
};

struct MigrateNumaPayload {
    // 地址段起始地址
    uint64_t paStart;
    // 地址段结束地址
    uint64_t paEnd;
};

struct MigrateNumaMsg {
    // 源 远端NUMAID
    int srcNid;
    // 目的 远端NUMAID
    int destNid;
    // 1-50的整数  源NUMA地址段数量
    int count;
    // 长度最大为50  地址段信息
    uint64_t memids[MAX_NR_MIGNUMA];
};

struct NumaPayload {
    uint8_t local;  // L1 NUMA ID，0-255
    uint8_t remote; // L2 NUMA ID，0-255
    uint32_t size;  // Available size, unit is MB
};

struct ProcessPayload {
    pid_t pid;
    uint8_t ratio;    // remote ratio set by upstream component
    uint8_t scanType; // 0 Ham 确定性热迁移， 1 smap migrateout
    uint8_t type;
    uint8_t state;
    uint8_t migrateMode;
    int16_t l1Node[4];
    int16_t l2Node[4];
    uint32_t scanTime;
    uint64_t memSize;
};

struct MigrateEscapePayload {
    pid_t pid;
    int srcNid;
    int destNid;
    int ratio;
    uint64_t memSize;        // 内存迁移大小(KB)
    MigrateMode migrateMode; // 内存迁移模式，按照比例或是大小
};

struct MigrateEscapeMsg {
    int count;
    struct MigrateEscapePayload payload[MAX_NR_MIGOUT];
};

struct MigrationNode {
    int nid;
    uint64_t size; // local: reserve size in KB; target: quota size in KB
};
struct MigrationGroup {
    int localCount;
    struct MigrationNode locals[MAX_GROUP_LOCAL_NUMA];
    int targetCount;
    struct MigrationNode targets[MAX_GROUP_REMOTE_NUMA];
};
struct GroupedMigrateOutPayload {
    pid_t pid;
    int groupCount;
    struct MigrationGroup groups[MAX_MIGRATION_GROUP_NUM];
};
struct GroupedMigrateOutMsg {
    int count;
    struct GroupedMigrateOutPayload payload[MAX_NR_GROUPED_MIGOUT];
};

using SmapInitFunc = int (*)(const uint32_t, void(int, const char*, const char*));
using SmapMigrateOutFunc = int (*)(MigrateOutMsg*, int);
using SmapMigrateOutSyncFunc = int (*)(MigrateOutMsg*, int, uint64_t);
using SmapQueryVmFreqFunc = int (*)(int, uint16_t*, uint32_t, uint32_t&, int);
using SetSmapRemoteNumaInfoFunc = int (*)(RemoteNumaInfo*);
using SetSmapRunModeFunc = int (*)(int);
using SmapRemoveFunc = int (*)(RemoveMsg*, int);
using SmapMigrateBackFunc = int (*)(MigrateBackMsg*);
using SmapEnableNodeFunc = int (*)(EnableNodeMsg*);
using SmapMigrateRemoteNumaFunc = int (*)(MigrateNumaMsg*);
using SmapMigratePidRemoteNumaFunc = int (*)(MigrateEscapeMsg*);
using SmapEnableProcessMigrateFunc = int (*)(pid_t*, int, int, int);
using SmapGetRemotePidsFunc = int (*)(int, struct ProcessPayload*, int, int*);
using SmapAddProcessTrackingFunc = int (*)(pid_t*, uint32_t*, uint32_t*, int, int);
using SmapRemoveProcessTrackingFunc = int (*)(pid_t*, int, int);
using SmapQueryProcessConfigFunc = int (*)(int, ProcessPayload*, int, int*);
using SmapMigrateOutGroupedFunc = int (*)(GroupedMigrateOutMsg*, int);
class SmapModule {
public:
    static MpResult Init();

    static SmapInitFunc GetSmapInit();

    static SetSmapRemoteNumaInfoFunc GetSetSmapRemoteNumaInfo();

    static SmapMigrateOutFunc GetSmapMigrateOut();

    static SmapMigrateOutSyncFunc GetSmapMigrateOutSync();

    static SmapQueryVmFreqFunc GetSmapQueryVmFreq();

    static void CloseSmapHandle();

    static void RackVmLog(int level, const char* str, const char* moduleName);

    static SetSmapRunModeFunc GetSetSmapRunModeFunc();

    static SmapRemoveFunc GetSmapRemoveFunc();

    static SmapMigrateBackFunc GetSmapMigrateBackFunc();

    static SmapMigrateBackFunc GetSmapMigrateBackSyncFunc();

    static SmapEnableNodeFunc GetSmapEnableNodeFunc();

    static SmapMigrateRemoteNumaFunc GetSmapMigrateRemoteNumaFunc();

    static SmapMigratePidRemoteNumaFunc GetSmapMigratePidRemoteNumaFunc();

    static SmapEnableProcessMigrateFunc GetSmapEnableProcessMigrateFunc();

    static SmapGetRemotePidsFunc GetSmapGetRemoteProcessesFunc();

    static SmapAddProcessTrackingFunc GetSmapAddProcessTrackingFunc();

    static SmapRemoveProcessTrackingFunc GetSmapRemoveProcessTrackingFunc();

    static SmapQueryProcessConfigFunc GetSmapQueryProcessConfigFunc();

    static SmapMigrateOutGroupedFunc GetSmapMigrateOutGroupedFunc();

private:
    static void* smapHandle;
    static SmapInitFunc smapInitFunc;
    static SmapMigrateOutFunc smapMigrateOutFunc;
    static SmapMigrateOutSyncFunc smapMigrateOutSyncFunc;
    static SmapQueryVmFreqFunc smapQueryVmFreqFunc;
    static SetSmapRemoteNumaInfoFunc setSmapRemoteNumaInfoFunc;
    static SetSmapRunModeFunc setSmapRunModeFunc;
    static SmapRemoveFunc smapRemoveFunc;
    static SmapMigrateBackFunc smapMigrateBackFunc;
    static SmapEnableNodeFunc smapEnableNodeFunc;
    static SmapMigrateRemoteNumaFunc smapMigrateRemoteNumaFunc;
    static SmapMigratePidRemoteNumaFunc smapMigratePidRemoteNumaFunc;
    static SmapEnableProcessMigrateFunc smapEnableProcessMigrateFunc;
    static SmapGetRemotePidsFunc smapGetRemotePidsFunc;
    static SmapAddProcessTrackingFunc smapAddProcessTrackingFunc;
    static SmapRemoveProcessTrackingFunc smapRemoveProcessTrackingFunc;
    static SmapQueryProcessConfigFunc smapQueryProcessConfigFunc;
    static SmapMigrateOutGroupedFunc smapMigrateOutGroupedFunc;
};
} // namespace mempooling::smap
#endif // RACK_MANAGER_SMAP_MODULE_H
