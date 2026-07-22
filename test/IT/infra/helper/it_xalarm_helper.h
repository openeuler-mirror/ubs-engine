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

#ifndef IT_XALARM_HELPER_H
#define IT_XALARM_HELPER_H

#include <cstdint>
#include <string>
#include <vector>

#include "ubse_common_def.h"
#include "ubse_error.h"

namespace ubse::it::infra {

using ubse::common::def::UbseResult;

/**
 * @brief Helper for injecting xalarm fault events into IT daemon processes.
 *
 * Writes events to the FIFO created by xalarm_stub so the daemon's
 * SentryEventListen thread picks them up and processes them through
 * the real RAS handler chain.
 *
 * FIFO message format: <alarmId>|<paras>\n
 */
class ItXalarmHelper {
public:
    /**
     * @brief Inject a raw alarm event to the specified FIFO.
     * @param fifoPath Path to the xalarm FIFO for the target node
     * @param alarmId  Alarm event ID (e.g. 1003 for BMC reboot)
     * @param paras    Event parameters string (format depends on alarmId)
     * @return UBSE_OK on success
     */
    static UbseResult InjectEvent(const std::string& fifoPath, unsigned short alarmId, const std::string& paras);

    /**
     * @brief Inject a BMC reboot event (alarmId=1003).
     *
     * paras format: "<msgId>"  (numeric string)
     * The receiving node treats itself as the fault node.
     *
     * @param fifoPath Path to the xalarm FIFO
     * @param msgId    Message ID for deduplication / ack
     */
    static UbseResult InjectRebootEvent(const std::string& fifoPath, uint64_t msgId);

    /**
     * @brief Inject an OOM event (alarmId=1005).
     *
     * paras format: "<msgId>_{nr_nid:<N>,nid:[<n1>,<n2>,...],sync:<0|1>,timeout:<N>,reason:<N>}"
     *
     * @param fifoPath Path to the xalarm FIFO
     * @param msgId    Message ID
     * @param nids     NUMA node IDs where OOM occurred
     * @param sync     1 = need ack to sysSentry, 0 = no ack
     * @param timeout  Timeout for OOM handling (seconds)
     * @param reason   OOM reason code (2 = hugepage OOM)
     */
    static UbseResult InjectOomEvent(const std::string& fifoPath, uint64_t msgId, const std::vector<int>& nids,
                                     int sync = 1, int timeout = 30, int reason = 2);

    /**
     * @brief Inject a PANIC event (alarmId=1007).
     *
     * paras format: "<msgId>_{cna:<ip>,eid:<eid>}"
     * Fault node is resolved from eid via MTI mapping table.
     *
     * @param fifoPath Path to the xalarm FIFO
     * @param msgId    Message ID
     * @param cna      CNA address (e.g. "0.0.0.1")
     * @param eid      EID that maps to the fault node (e.g. "192.168.100.100")
     */
    static UbseResult InjectPanicEvent(const std::string& fifoPath, uint64_t msgId, const std::string& cna,
                                       const std::string& eid);

    /**
     * @brief Inject a kernel reboot event (alarmId=1009).
     *
     * Same format as PANIC. Fault node is resolved from eid.
     *
     * @param fifoPath Path to the xalarm FIFO
     * @param msgId    Message ID
     * @param cna      CNA address
     * @param eid      EID that maps to the fault node
     */
    static UbseResult InjectKernelRebootEvent(const std::string& fifoPath, uint64_t msgId, const std::string& cna,
                                              const std::string& eid);

    /**
     * @brief Inject a memory fault event (alarmId=1013).
     *
     * paras format: free-form string passed directly to ExecuteFaultHandler.
     *
     * @param fifoPath Path to the xalarm FIFO
     * @param info     Fault info string
     */
    static UbseResult InjectMemFaultEvent(const std::string& fifoPath, const std::string& info);

    /**
     * @brief 等待RAS处理完成的ack结果
     *
     * xalarm_stub在xalarm_report_event中将ack内容写入文件，
     * 格式为 "msgId_retCode\n"。此方法轮询等待该文件出现并解析结果。
     *
     * @param workDir  目标节点的work dir（daemon CWD）
     * @param ackAlarmId ack的alarm ID（原始事件ID+1，如OOM=1005→ack=1006）
     * @param timeoutMs 超时毫秒数
     * @param[out] retCode RAS handler的返回码，0=成功
     * @return UBSE_OK 等到结果, UBSE_ERROR_DEF 超时
     */
    static UbseResult WaitForAckResult(const std::string& workDir, unsigned short ackAlarmId, uint32_t timeoutMs,
                                       uint32_t& retCode);

    /**
     * @brief 删除ack结果文件（测试前清理用）
     */
    static void ClearAckResult(const std::string& workDir, unsigned short ackAlarmId);
};

} // namespace ubse::it::infra

#endif // IT_XALARM_HELPER_H
