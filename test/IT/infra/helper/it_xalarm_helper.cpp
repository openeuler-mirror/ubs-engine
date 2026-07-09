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

#include "it_xalarm_helper.h"

#include <fcntl.h>
#include <unistd.h>
#include <chrono>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#include "ubse_error.h"
#include "it_console_log.h"

namespace ubse::it::infra {

using namespace ubse::common::def;

UbseResult ItXalarmHelper::InjectEvent(const std::string& fifoPath, unsigned short alarmId, const std::string& paras)
{
    if (fifoPath.empty()) {
        IT_LOG_ERROR << "FIFO path is empty";
        return UBSE_ERROR_DEF(1);
    }

    /* O_NONBLOCK so the helper does not block if no reader is on the other end */
    int fd = open(fifoPath.c_str(), O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        IT_LOG_ERROR << "Failed to open FIFO " << fifoPath << ": " << strerror(errno);
        return UBSE_ERROR_DEF(2);
    }

    std::string msg = std::to_string(alarmId) + "|" + paras + "\n";
    ssize_t written = write(fd, msg.c_str(), msg.size());
    if (written < 0) {
        IT_LOG_ERROR << "Failed to write to FIFO " << fifoPath << ": " << strerror(errno);
        close(fd);
        return UBSE_ERROR_DEF(3);
    }
    if (static_cast<size_t>(written) != msg.size()) {
        IT_LOG_WARN << "Partial write to FIFO " << fifoPath << " (" << written << "/" << msg.size() << " bytes)";
        close(fd);
        return UBSE_ERROR_DEF(4);
    }

    close(fd);
    IT_LOG_INFO << "Injected alarm event: alarmId=" << alarmId << ", paras=" << paras << " -> " << fifoPath;
    return UBSE_OK;
}

UbseResult ItXalarmHelper::InjectRebootEvent(const std::string& fifoPath, uint64_t msgId)
{
    return InjectEvent(fifoPath, 1003, std::to_string(msgId));
}

UbseResult ItXalarmHelper::InjectOomEvent(const std::string& fifoPath, uint64_t msgId, const std::vector<int>& nids,
                                          int sync, int timeout, int reason)
{
    /* Format: <msgId>_{nr_nid:<N>,nid:[<n1>,<n2>,...],sync:<0|1>,timeout:<N>,reason:<N>} */
    std::ostringstream oss;
    oss << msgId << "_{nr_nid:" << nids.size() << ",nid:[";
    for (size_t i = 0; i < nids.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << nids[i];
    }
    oss << "],sync:" << sync << ",timeout:" << timeout << ",reason:" << reason << "}";
    return InjectEvent(fifoPath, 1005, oss.str());
}

UbseResult ItXalarmHelper::InjectPanicEvent(const std::string& fifoPath, uint64_t msgId, const std::string& cna,
                                            const std::string& eid)
{
    /* Format: <msgId>_{cna:<ip>,eid:<eid>} */
    std::string paras = std::to_string(msgId) + "_{cna:" + cna + ",eid:" + eid + "}";
    return InjectEvent(fifoPath, 1007, paras);
}

UbseResult ItXalarmHelper::InjectKernelRebootEvent(const std::string& fifoPath, uint64_t msgId, const std::string& cna,
                                                   const std::string& eid)
{
    /* Same format as PANIC */
    std::string paras = std::to_string(msgId) + "_{cna:" + cna + ",eid:" + eid + "}";
    return InjectEvent(fifoPath, 1009, paras);
}

UbseResult ItXalarmHelper::InjectMemFaultEvent(const std::string& fifoPath, const std::string& info)
{
    return InjectEvent(fifoPath, 1013, info);
}

UbseResult ItXalarmHelper::WaitForAckResult(const std::string& workDir, unsigned short ackAlarmId, uint32_t timeoutMs,
                                            uint32_t& retCode)
{
    std::string ackPath = workDir + "/xalarm_ack_" + std::to_string(ackAlarmId);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        std::ifstream ifs(ackPath);
        if (ifs.is_open()) {
            std::string line;
            if (std::getline(ifs, line)) {
                // 格式: "msgId_retCode"，取最后一个下划线后的数字
                auto pos = line.rfind('_');
                if (pos != std::string::npos) {
                    try {
                        retCode = static_cast<uint32_t>(std::stoul(line.substr(pos + 1)));
                        IT_LOG_INFO << "RAS ack received: alarmId=" << ackAlarmId << ", retCode=" << retCode;
                        return UBSE_OK;
                    } catch (const std::exception& e) {
                        IT_LOG_WARN << "Failed to parse ack retCode from: " << line;
                    }
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    IT_LOG_ERROR << "Timeout waiting for RAS ack: " << ackPath;
    return UBSE_ERROR_DEF(1);
}

void ItXalarmHelper::ClearAckResult(const std::string& workDir, unsigned short ackAlarmId)
{
    std::string ackPath = workDir + "/xalarm_ack_" + std::to_string(ackAlarmId);
    unlink(ackPath.c_str());
}

} // namespace ubse::it::infra
