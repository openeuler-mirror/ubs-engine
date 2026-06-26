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

#ifndef UBSE_COMMON_DEF_H
#define UBSE_COMMON_DEF_H
#include <cstdint>
#include <map>
#include <string>

namespace ubse::common::def {
using IpAddress = std::pair<std::string, uint16_t>;
using UbseResult = uint32_t;

#ifndef UBSE_LIKELY
#define UBSE_LIKELY(x) (__builtin_expect(!!(x), 1) != 0)
#endif

#ifndef UBSE_UNLIKELY
#define UBSE_UNLIKELY(x) (__builtin_expect(!!(x), 0) != 0)
#endif

// 数字常量宏，代表数字1，2，3，4...
constexpr int16_t NO_0 = 0;
constexpr int16_t NO_1 = 1;
constexpr int16_t NO_2 = 2;
constexpr int16_t NO_3 = 3;
constexpr int16_t NO_4 = 4;
constexpr int16_t NO_5 = 5;
constexpr int16_t NO_6 = 6;
constexpr int16_t NO_7 = 7;
constexpr int16_t NO_8 = 8;
constexpr int16_t NO_9 = 9;
constexpr int16_t NO_10 = 10;
constexpr int16_t NO_11 = 11;
constexpr int16_t NO_12 = 12;
constexpr int16_t NO_13 = 13;
constexpr int16_t NO_14 = 14;
constexpr int16_t NO_15 = 15;
constexpr int16_t NO_16 = 16;
constexpr int16_t NO_20 = 20;
constexpr int16_t NO_32 = 32;
constexpr int16_t NO_50 = 50;
constexpr int16_t NO_60 = 60;
constexpr int16_t NO_64 = 64;
constexpr int16_t NO_128 = 128;
constexpr int16_t NO_500 = 500;
constexpr int16_t NO_1000 = 1000;
constexpr int16_t NO_1024 = 1024;
constexpr int16_t NO_2048 = 2048;
constexpr int32_t NO_60000 = 60000;
constexpr int16_t NO_NEGATIVE_1 = -1;
constexpr int MAX_IPC_DATA_PACKAGE_LEN = 10485760;
constexpr int16_t DUMP_DELETE_LEN = -1;
constexpr int16_t UBSE_UDS_MODE = 666;
constexpr size_t httpMaxBodySize = 524288;
constexpr size_t httpMaxStrSize = 504;
constexpr size_t httpMaxQuerySize = 11264;
constexpr uint32_t STACK_WANT_DEPTH = 20;
constexpr uint32_t STACK_IGNORE_DEPTH = 0;
constexpr uint32_t PRINTSIG = 35;
constexpr uint32_t DEFAULTTIMEOUT = 60;
constexpr uint32_t DEFAULTHBTIMEOUT = 30;
constexpr uint32_t UBSE_CLOS_MAX_NODE_NUM = 64;

const uint16_t MIN_PORT = 1024;
const uint16_t MAX_PORT = 65535;
const uint32_t DEFAULT_TCP_SERVER_PORT = 8082;
const uint32_t DEFAULT_UBM_SERVER_PORT = 8799;
const uint16_t TCP_LISTEN_PORT = 1901;
const uint16_t URMA_LISTEN_JETTY = 999;
const uint32_t DEFAULT_HCOM_HB_TIMEOUT = 6;
const std::string MASTER_RPC_SERVER_NAME = "UbseMasterRpcServer";
const std::string AGENT_RPC_SERVER_NAME = "UbseAgentClient";
const std::string UBSE_HCOM_FILE_PATH_PREFIX = "HCOM_FILE_PATH_PREFIX";
const std::string UBSE_AGENT_IPC_SERVER_ENGINE_NAME = "UbseAgentIpcServerEngine";
const std::string UBSE_AGENT_IPC_CLIENT_ENGINE_NAME = "UbseAgentIpcClientEngine";
const std::string UBSE_ROLE_MASTER = "master";
const std::string UBSE_ROLE_SLAVE = "standby";
const std::string UBSE_ROLE_AGENT = "agent";
const std::string UBSE_COMMON_SECTION = "ubse.common";
const std::string DEFAULT_UDS_ADDRESS = "/var/run/ubse/ubseAgentUds.socket";
const std::string UBM_UDS_ADDRESS = "/run/ubm/socket/ubm_nuds/restconf.sock";
const std::string UBSE_UBM_UDS_ADDRESS = "/var/run/ubse/ubse_ubm.socket";
const std::string UBM_GROUP = "ubm_nuds";
const std::string SYSTEM_GROUP = "ubse";
const std::string UBSE_UDS_USER_VERIFY_SECTION = "ubse_uds_user_verify";
const std::string UBSE_ACCEPT_SERVICE_LIST_USER_KEY = "accept.service.list.user";
const std::string UBSE_EVENT_SECTION = "ubse.event";
const std::string UBSE_EVENT_QUEUE_MAX_ITEM = "queue.maxItem";
const std::string UBSE_EVENT_HIGH_THREAD_MAX_ITEM = "high.thread.maxItem";
const std::string UBSE_EVENT_MEDIUM_THREAD_MAX_ITEM = "medium.thread.maxItem";
const std::string UBSE_EVENT_LOW_THREAD_MAX_ITEM = "low.thread.maxItem";
const std::string UBSE_EVENT_NODE_STATE = "ubse.node.state";
const std::string UBSE_EVENT_NODE_RECONNECT = "ubse.node.reconnect";
const std::string UBSE_EVENT_NODE_JOIN = "UbseNodeJoinEvent";
const std::string UBSE_EVENT_NODE_TOPO_LINK_CHANGE = "UbseNodeTopoLinkChangeEvent";
const std::string UBSE_EVENT_MEM_FAULT = "mem.fault.collection";
const std::string UBSE_EVENT_MEM_PREDICT_FAULT = "mem.fault.prediction";
const std::string UBSE_EVENT_XALARM_REBOOT = "ALARM_REBOOT_EVENT";
const std::string UBSE_EVENT_XALARM_REBOOT_ACK = "ALARM_REBOOT_ACK_EVENT";
const std::string UBSE_EVENT_XALARM_OOM = "ALARM_OOM_EVENT";
const std::string UBSE_EVENT_XALARM_OOM_ACK = "ALARM_OOM_ACK_EVENT";
const std::string UBSE_EVENT_XALARM_PANIC = "ALARM_PANIC_EVENT";
const std::string UBSE_EVENT_XALARM_PANIC_ACK = "ALARM_PANIC_ACK_EVENT";
const std::string UBSE_EVENT_XALARM_KERNEL_REBOOT = "ALARM_KERNEL_REBOOT_EVENT";
const std::string UBSE_EVENT_XALARM_KERNEL_REBOOT_ACK = "ALARM_KERNEL_REBOOT_ACK_EVENT";
const std::string UBSE_EVENT_FAULT_UPDATE = "ubse.resource.fault.update";
const std::string UBSE_EVENT_TOPOLOGY_CHANGE = "UbseTopologyChangeEvent";
const std::string UBSE_EVENT_CLUSTER_TOPOLOGY_CHANGE = "UbseClusterTopologyChangeEvent";
const std::string UBSE_EVENT_NODE_DISCOVERY = "ubse.node.discovery";
const std::string MEM_FAULT_FILE_PATH = "/var/log/ubse/mem_fault";
const std::string ORCHESTRATE_REQUEST_URL = "/ubse/resource_request/";
const std::string UBSE_UDS_ADDRESS_ENV_VAR = "UBSE_UDS_ADDRESS";
const std::string UBSE_HTTP_INFO_LOG = "info.log";
const std::string UBSE_CONFIG_STATUS_SUCCESS = "Success";
const std::string UBSE_CONFIG_STATUS_FAIL = "Fail";
const std::string SSU_ONLINE_EVENT = "ssu.online";
const std::string SSU_OFFLINE_EVENT = "ssu.offline";
const std::string SSU_ONLINE_UPDATE_EVENT = "ssu.online.update";
const std::string SSU_OFFLINE_UPDATE_EVENT = "ssu.offline.update";
const std::string UBSE_ELECTION_SECTION = "ubse.election";
const std::string UBSE_ELECTION_HEARTBEAT_TIME_INTERVAL = "heartbeat.timeInterval";
const std::string UBSE_ELECTION_HEARTBEAT_LOST_THRESHOLD = "heartbeat.lostThreshold";
const std::string UBSE_UBFM_SECTION = "ubse.ubfm";
const std::string UBSE_HTTP_TCP_SERVER_PORT = "ubse.server.port";
const std::string UBSE_ELECTION_CANDIDATE = "election.candidate";
const std::string UBSE_ELECTION_WAIT = "election.wait";
const std::string COMMON_PSK_ID = "common_psk_id";
const std::string KMC_LOG_PATH = "/var/log/ubse/kmc_process.log";
const std::string realUbfmDefaultPort = "8799";
const std::string ELECTION_TASK_EXECUTOR_NAME = "ElectionLinkTask";
const std::string ELECTION_GLOBAL_TASK_EXECUTOR_NAME = "ElectionGlobalLinkTask";
const std::string UBSE_UDS_SOCKET_PATH = "/var/run/ubse/ubse.sock";
const std::string UBSE_LOG_PATH = "/var/log/ubse";
const std::string UBSE_AGENT_IPC_SERVER_NAME = "RackAgentIpcServer";
} // namespace ubse::common::def
#endif // UBSE_COMMON_DEF_H
