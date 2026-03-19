/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
*/

#ifndef UBSE_MANAGER_UBSE_MEM_CONTROLLER_RAS_HANDLER_H
#define UBSE_MANAGER_UBSE_MEM_CONTROLLER_RAS_HANDLER_H

#include <cstdint>

#include "ubse_com.h"
#include "ubse_common_def.h"
#include "ubse_ras.h"
#include "ubse_thread_pool_module.h"

namespace ubse::mem::controller {
using namespace ubse::com;
using namespace ubse::ras;
using namespace ubse::common::def;
using namespace ubse::task_executor;

struct UbseMemFaultInfo {
    uint64_t memId_ = 0;
    std::string memName_;
    std::string faultInfo_;

    UbseMemFaultInfo(uint64_t memId, const std::string &memName, const std::string &faultInfo)
        : memId_(memId),
          memName_(memName),
          faultInfo_(faultInfo) {};

    UbseMemFaultInfo() : memId_(0) {};
};

struct UbseMemFaultMsg {
    UbseMemFaultInfo info;
    uint32_t nodeNum = 0; // 应通知的节点数量
    std::string faultNode;
    std::string notifiedNode; // 确认已告知的节点

    explicit UbseMemFaultMsg(const UbseMemFaultInfo &faultInfo) : info(faultInfo) {};

    UbseMemFaultMsg(uint64_t memId, const std::string &memName, const std::string &faultInfo)
        : info(memId, memName, faultInfo) {};

    UbseMemFaultMsg() = default;
};

class UbseMemFaultManager {
public:
    static UbseResult InitMemFaultManager();

    static UbseResult DeInitMemFaultManager();

private:
    static UbseResult GetMemNameById(uint64_t memId, std::string &memName);

    static uint32_t MemFaultHandler(ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo);

    static void MemFaultReportHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

    static void MemFaultReportReplyHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

    static void MemFaultNotifyHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

    static void MemFaultNotifyReplyHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

    static void MemFaultReportTask(UbseMemFaultMsg msg);

    static void StartMemFaultReportTask(const UbseMemFaultMsg &msg);

    static void StartMemFaultDeliverTask(const std::vector<std::string> &users, const UbseMemFaultMsg &msg);

    static UbseResult CreateTaskExecutor(const std::string &name);

    static UbseResult RemoveTaskExecutor(const std::string &name);

    static UbseTaskExecutorPtr executorPtr;
};

} // namespace ubse::mem::controller

#endif // UBSE_MANAGER_UBSE_MEM_CONTROLLER_RAS_HANDLER_H
