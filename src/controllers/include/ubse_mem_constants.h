/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MEM_STRATEGY_MEMORYFABRIC_CONSTANTS_H
#define MEM_STRATEGY_MEMORYFABRIC_CONSTANTS_H

#include <regex.h>
#include <securec.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <climits>
#include <cmath>
#include <cstring>
#include <fstream>
#include <list>
#include <random>
#include <set>
#include <string>
#include <vector>

#define TOPOLOGY_MAX_SOCKET_PER_HOST 2
#define TOPOLOGY_MAX_NUMA_PER_SOCKET 4
#define TOPOLOGY_MAX_NUMA_PER_HOST 4
#define TOPOLOGY_MAX_HOST_NUM 16
#define TOPOLOGY_MAX_TOTAL_NUMA 64

namespace ubse::mem::strategy {
constexpr int16_t INVALID_META_ID = -1;
constexpr int INVALID_SOCKET_ID = -1;
constexpr int16_t INVALID_WATER_MARK = 0;
constexpr auto INVALID_META_STRID = "";
constexpr int64_t INVALID_LEFT_SHAREMEM_SIZE = -1;
constexpr uint16_t MAX_WAIT_COUT = 5;
constexpr uint16_t MAX_PERCENT = 100;
constexpr uint32_t UBSE_NOT_MASTER_ERROR = 998;
constexpr uint32_t UBSE_ELECTION_NOT_INIT_ERROR = 997;
constexpr int ONE_M = 1024 * 1024;
constexpr uint64_t ALIGN_SIZE = 128 * 1024 * 1024;
constexpr uint64_t FOUR_G = 4 * 1024 * 1024 * 1024L;
constexpr uint64_t MAX_IPC_DATA_PACKAGE_LEN = 1047552;
constexpr uint32_t MIN_NODE_TOPOLOGY_SIZE = 2u;
constexpr int IPC_RETRY_TIMES = 3;
constexpr int RPC_RETRY_TIMES = 3;
constexpr auto UBSE_SHM_LOCAL_ID = "";
constexpr uint64_t INVALID_VALUE64 = 0;
constexpr uint16_t INVALID_VALUE16 = 0xFFFF;
constexpr uint32_t POOL_MEM_RATIO_VALID_MIN = 0;
constexpr uint32_t POOL_MEM_RATIO_VALID_MAX = 100;
constexpr int MAX_X_NODE_NUM = 16;
constexpr uint64_t MAX_EXPORT_MEM_SIZE_PER_SOCKET = 2 * 1024 * 1024 * 1024L * 1024L; // 单位B
constexpr uint64_t MAX_IMPORT_MEM_SIZE_PER_SOCKET = 2 * 1024L * 1024L;               // 单位MB
constexpr uint64_t MIN_IMPORT_MEM_SIZE_PER_SOCKET = 128L;                            // 单位MB
constexpr uint64_t MAX_OBMM_SYSTEM_POOL_MEM_RATIO = 100;
constexpr uint64_t DEFAULT_MEM_BORROW_LIMIT = 4194304L;
constexpr uint64_t OBMM_MEMORY_BLOCK_SIZE_CONFIG = 128L;
constexpr uint64_t MAX_BORROW_MEM_PER_NODE = UINT64_MAX / 2;
constexpr uint16_t API_TIME_OUT = 1800;
constexpr uint16_t MAX_TIME_OUT = 3600;
constexpr uint16_t THREAD_NUM = 64;
constexpr uint16_t Queue_Capacity = 1000;
constexpr uint32_t BLOCK_128M = 128u;
constexpr uint32_t BLOCK_512M = 512u;
constexpr uint32_t BLOCK_1G = 1024u;
constexpr uint32_t BLOCK_2M = 2u;
constexpr uint32_t MB_2M = 2;
constexpr uint32_t MB_4M = 4;
constexpr uint32_t MB_128M = 128;
constexpr uint32_t MB_4096M = 4096;
constexpr uint32_t KB_TO_B = 1024;
constexpr uint64_t MB_TO_BYTE = 1024 * 1024;
const std::string PAGE_SIZE_4K = "4096";
const std::string PAGE_SIZE_64K = "65536";

const std::string UBSE_METRIC_PREFIX = "/ubse.metric.";
const std::string UBSE_MEM_TYPE = "mem";
constexpr auto VM_SECTION_NAME = "plugin_vm";
constexpr auto VM_PAGE_TYPE = "virt.pageType";
const std::string UBSE_PAGE_DOWN_TYPE = "ubse.mem.fault.prediction";
const std::string UBSE_MEMID_DOWN_TYPE = "ALARM_MEM_FAULT_PREDICT_MEMID_EVENT";
const std::string UBSE_NODE_DOWN_TYPE = "ALARM_REBOOT_EVENT";
const std::string UBSE_SYSTEM_PANIC_TYPE = "ALARM_PANIC_EVENT";
const std::string UBSE_KERNEL_REBOOT_TYPE = "ALARM_KERNEL_REBOOT_EVENT";
const std::string MEM_NODE_DOWN_TYPE = "MEM_ALARM_REBOOT_EVENT";
const std::string UBSE_NODE_DOWN_ACK_TYPE = "ALARM_REBOOT_ACK_EVENT";
const std::string UBSE_EVENT_XALARM_PANIC_ACK = "ALARM_PANIC_ACK_EVENT";
const std::string UBSE_EVENT_XALARM_KERNEL_REBOOT_ACK = "ALARM_KERNEL_REBOOT_ACK_EVENT";
constexpr auto OBMM_MEM_MANAGER_CONFIG_SECTION_NAME = "ubse.memory";
constexpr auto OBMM_MEMORY_BLOCK_SIZE_CONFIG_KEY = "obmm.memory.block.size";

// 数字常量宏，代表数字1，2，3，4...
constexpr int16_t NO_0 = 0;
constexpr int16_t NO_1 = 1;
constexpr int16_t NO_2 = 2;
constexpr int16_t NO_3 = 3;
constexpr int16_t NO_4 = 4;
constexpr int16_t NO_16 = 16;
constexpr int16_t NO_32 = 32;
constexpr int16_t NO_64 = 64;
constexpr int16_t NO_128 = 128;
constexpr int16_t NO_512 = 512;
constexpr int16_t NO_1000 = 1000;
constexpr int16_t NO_1024 = 1024;
constexpr int16_t NO_2048 = 2048;
constexpr int32_t NO_60000 = 60000;
constexpr int16_t NO_NEGATIVE_1 = -1;
constexpr int16_t DUMP_DELETE_LEN = -1;
constexpr size_t MIN_JSON_LENGTH = 4;

constexpr auto MASTER_RPC_SERVER_NAME = "UbseMasterRpcServer";
constexpr auto AGENT_RPC_SERVER_NAME = "UbseAgentClient";
constexpr auto UBSE_AGENT_IPC_SERVER_NAME = "UbseAgentIpcServer";
constexpr auto UBSE_AGENT_IPC_SERVER_ENGINE_NAME = "UbseAgentIpcServerEngine";
constexpr auto UBSE_AGENT_IPC_CLIENT_ENGINE_NAME = "UbseAgentIpcClientEngine";
constexpr auto UBSE_COMMON_SECTION = "ubse.common";
constexpr auto UDS_ADDRESS = "/var/run/ubse/ubseAgentUds.socket";
const int16_t UBSE_UDS_MODE = 600;
const size_t HTTP_MAX_BOBY_SIZE = 524288;
const size_t HTTP_MAX_STR_SIZE = 504;
const size_t HTTP_MAX_QUERY_SIZE = 11264;

constexpr auto MEM_RESOURCE_TYPE = "MEM";
// 事件定义，内存池自己定义的事件ID必须在这里定义，方便管理。如果事件消息格式复杂，也需要详细说明
// 内存拓扑有变动，需要初始化算法或者做其他事件
constexpr auto UBSE_MEM_EVENT_TOPOLOGY_CHANGE = "UBSE_MEM_EVENT_TOPOLOGY_CHANGE";
constexpr auto UBSE_MEM_EVENT_APP_STATE_CHANGE = "ubse.mem.event.app.linkstate";
constexpr auto UBSE_EVENT_XALARM_OOM = "ALARM_OOM_EVENT";
constexpr auto UBSE_EVENT_XALARM_OOM_ACK = "ALARM_OOM_ACK_EVENT";
constexpr auto UBSE_CREATE_RESOURCE_KIND = "MEM";
constexpr auto APP_BORROW_REQUEST = "APP_BORROW";
constexpr auto APP_BORROW_NUMA_REQUEST = "APP_BORROW_NUMA";
constexpr auto SHARE_REQUEST = "SHARE";
constexpr auto WATER_BORROW_REQUEST = "WATER_BORROW";
constexpr auto APP_PRI_BORROW_REQUEST = "APP_PRI_BORROW";
constexpr auto MEM_CREATE_TYPE = "CREATE";
constexpr auto MEM_DELETE_TYPE = "DELETE";
constexpr auto MEM_ATTACH_TYPE = "ATTACH";
constexpr auto MEM_DETACH_TYPE = "DETACH";
constexpr auto MEM_INSPECTION_PERIOD = 86400; // 一天一次
constexpr auto COMM_MEMID_PATH = "/etc/urma/hccs/communication_memid_range";
constexpr auto HARD_LIMIT_BORROW_MB = 4 * 1024 * 1024;

constexpr auto MAX_OBMM_BUFFER_LENGTH = 128;
constexpr auto OBMM_META_PATH = "/sys/devices/obmm/";
constexpr auto OBMM_DEV_PATH = "/dev/";
} // namespace ubse::mem::strategy
#endif