/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef VM_ALARM_HANDLER_H
#define VM_ALARM_HANDLER_H

#include <ubse_mem_controller.h>
#include <ubse_ras.h>

#include "mem_handler.h"
#include "vm_struct.h"

namespace vm {
using namespace ubse::mem::controller;
const uint16_t HUGE_PAGE_OOM = 2;
const uint16_t DEFAULT_ASYNC_MIGRATE_WAIT_SECONDES = 10;

struct UbsVirtNumaMemoryDebtInfo : UbseNumaMemoryImportDebtInfo {
    int16_t numaId;

    UbsVirtNumaMemoryDebtInfo() = default;
    explicit UbsVirtNumaMemoryDebtInfo(const UbseNumaMemoryImportDebtInfo& base, int16_t id = 255)
        : UbseNumaMemoryImportDebtInfo(base),
          numaId(id)
    {
    }
};

class AlarmHandler {
public:
    static std::mutex alarmLock;

    VmResult Init();
    static AlarmHandler& GetInstance()
    {
        static AlarmHandler gInstance;
        return gInstance;
    }
    static VmResult AlarmEventHandler(AlarmNumaInfo& alarmNumaInfo, std::vector<UbsVirtNumaMemoryDebtInfo>& debtInfos,
                                      WatermarkWarningType eventType);
    static VmResult MemNotifyEventHandler(std::string& eventId, std::string& eventMessage);
    static VmResult OomEventHandler(const Notify& notify);

private:
    AlarmHandler() = default;
    ~AlarmHandler() = default;
    static GlobalNumaInfoMap GetGlobalResource(const AlarmNumaInfo& alarmNumaInfo,
                                               std::vector<UbsVirtNumaMemoryDebtInfo>& debtInfos);
    static VmResult BorrowClearEventHandler(const AlarmNumaInfo& alarmNumaInfo);
    static bool HandlerNoUsedBorrowIds(const AlarmNumaInfo& alarmNumaInfo, WatermarkWarningType eventType);
    static std::vector<std::string> GenVectorByBorrowItem(const AlarmNumaInfo& alarmNumaInfo);
    static VmResult GetVirtDebtInfos(std::vector<UbsVirtNumaMemoryDebtInfo>& virtDebtInfos);
    static VmResult GenAlarmNumaInfo(const Notify& notify, std::vector<UbsVirtNumaMemoryDebtInfo>& debtInfos,
                                     AlarmNumaInfo& alarmNumaInfo);
    static VmResult ConvertUbseDebtInfosToVirtDebtInfos(const std::vector<UbseNumaMemoryImportDebtInfo>& debtInfos,
                                                        std::vector<UbsVirtNumaMemoryDebtInfo>& virtDebtInfos);

    // RAS fault handler adapter for OOM events
    static uint32_t OomAdapter(ubse::ras::ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo);

    // Process OOM escape actions (debt infos → alarm info → dispatch → wait for result)
    static VmResult ProcessOomActions(const Notify& notify);
    static VmResult ParseOomMsg(const std::string& input, uint16_t& numaCount, std::vector<uint16_t>& numaIds,
                                uint8_t& reason);
};
} // namespace vm
#endif // VM_ALARM_HANDLER_H