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
};

} // namespace ubse::it::infra

#endif // IT_XALARM_HELPER_H
