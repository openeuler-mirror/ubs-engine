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

#include "vm_struct.h"
#include "mem_handler.h"

namespace vm {
using namespace ubse::mem::controller;

struct UbsVirtNumaMemoryDebtInfo : UbseNumaMemoryImportDebtInfo {
    int16_t numaId;

    UbsVirtNumaMemoryDebtInfo() = default;
    explicit UbsVirtNumaMemoryDebtInfo(const UbseNumaMemoryImportDebtInfo& base, int16_t id = 255)
        : UbseNumaMemoryImportDebtInfo(base), numaId(id) {}
};

class AlarmHandler {
public:
    static std::mutex alarmLock;

    VmResult Init();
    static AlarmHandler &GetInstance()
    {
        static AlarmHandler gInstance;
        return gInstance;
    }
    static VmResult AlarmEventHandler(AlarmNumaInfo &alarmNumaInfo, std::vector<UbsVirtNumaMemoryDebtInfo> &debtInfos,
                                      WatermarkWarningType eventType);
    static VmResult MemNotifyEventHandler(std::string &eventId, std::string &eventMessage);

private:
    AlarmHandler() = default;
    ~AlarmHandler() = default;
    static void FillGlobalWithNumaMemInfo(const AlarmNumaInfo &alarmNumaInfo,
                                          std::vector<UbsVirtNumaMemoryDebtInfo> &debtInfos,
                                          GlobalNumaInfoMap &globalNumaInfoMapIn);
    static GlobalNumaInfoMap GetGlobalResource(const AlarmNumaInfo &alarmNumaInfo,
                                               std::vector<UbsVirtNumaMemoryDebtInfo> &debtInfos);
    static VmResult BorrowClearEventHandler(const AlarmNumaInfo &alarmNumaInfo);
    static bool HandlerNoUsedBorrowIds(const AlarmNumaInfo &alarmNumaInfo, WatermarkWarningType eventType);
    static std::vector<std::string> GenVectorByBorrowItem(const AlarmNumaInfo &alarmNumaInfo);
    static VmResult GetVirtDebtInfos(std::vector<UbsVirtNumaMemoryDebtInfo> &virtDebtInfos);
    static VmResult GenAlarmNumaInfo(const Notify &notify, std::vector<UbsVirtNumaMemoryDebtInfo> &debtInfos,
                                     AlarmNumaInfo &alarmNumaInfo);
    static VmResult ConvertUbseDebtInfosToVirtDebtInfos(const std::vector<UbseNumaMemoryImportDebtInfo> &debtInfos,
                                                        std::vector<UbsVirtNumaMemoryDebtInfo> &virtDebtInfos);
    static VmResult ParseOomMessage(const std::string &eventMessage, Notify &notify);
};
} // namespace vm
#endif // VM_ALARM_HANDLER_H
