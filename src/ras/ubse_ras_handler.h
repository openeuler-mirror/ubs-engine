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

#ifndef UBSE_MANAGER_UBSE_RAS_HANDLER_H
#define UBSE_MANAGER_UBSE_RAS_HANDLER_H
#include <unordered_map>
#include "ubse_common_def.h"
#include "register_xalarm.h"
#include "ubse_ras.h"
#include "ubse_ras_com_handler.h"
#include "ubse_topology_interface.h"

namespace ubse::ras {
using namespace ubse::common::def;

using XalarmReportEventFunc = int (*)(unsigned short, char *);
using HandlerMap = std::vector<std::pair<std::string, AlarmFaultHandler>>;
using NodeStateHandler = std::function<void(const std::string &faultInfo)>;

class UbseRasHandler {
public:
    friend class UbseRasComHandler;
    /**
     * 获取UbseRasObserver单例
     * @return 返回UbseRasObserver单例
     */
    static UbseRasHandler &GetInstance();

    /**
     * 初始化RasHandler
     * @return
     */
    UbseResult StartRasHandler();

    /**
     * 删除拷贝构造函数
     */
    UbseRasHandler(const UbseRasHandler &) = delete;

    /**
     * 删除赋值运算符
     * @return
     */
    UbseRasHandler &operator=(const UbseRasHandler &) = delete;

    /**
     * 节点级故障回调函数
     * @param fault_type
     * @param fault_info
     */
    UbseResult NodeFaultHandle(alarm_msg *alarmMsgPtr);

    /**
     * 注册故障处理函数
     * @param alarmHandler
     * @return
     */
    UbseResult RegisterAlarmFaultHandler(AlarmHandler alarmHandler);

    /**
     * 解注册故障处理函数
     * @param alarmFaultEvent
     * @param name
     * @return
     */
    uint32_t UnRegisterAlarmFaultHandler(ALARM_FAULT_TYPE alarmFaultEvent, std::string &name);

    /**
     * 设置修改节点状态回调函数
     */
    void SetNodeStateHandler(NodeStateHandler handler);

private:
    /*
     * 私有默认构造函数
     */
    UbseRasHandler() noexcept;
    /*
     * 私有析构函数
     */
    ~UbseRasHandler();

    /*
     * 处理BMC下电故障
     */
    UbseResult HandleBMCFault(std::string info);

    /*
     * 处理Oom故障
     */
    UbseResult HandleOomFault(alarm_msg *msg);

    /*
     * 处理PANIC / Kernel Reboot 故障
     */
    UbseResult HandlePanicAndRebootFault(ALARM_FAULT_TYPE faultType, std::string info);

    /*
     * 执行故障事件回调
     */
    UbseResult ExecuteFaultHandler(ALARM_FAULT_TYPE faultType, std::string faultInfo);

private:
    static UbseRasHandler instance;
    std::unordered_map<ALARM_FAULT_TYPE, std::map<AlarmHandlerPriority, HandlerMap>> faultHandlerMap{};
    NodeStateHandler nodeStateHandler;
};

bool IsMemInitFinished();
UbseResult HandleCnaAndEidMsg(const std::string faultInfo, std::string &faultNodeId);
std::string QueryNodeIdByEid(const std::string &eid);
UbseResult ReportAckToSysSentry(ALARM_FAULT_TYPE alarmFaultType, std::string message);

} // namespace ubse::ras
#endif // UBSE_MANAGER_UBSE_RAS_HANDLER_H
