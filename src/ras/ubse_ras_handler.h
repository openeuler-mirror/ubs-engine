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
#include "ubse_ras.h"
#include "ubse_ras_com_handler.h"
#include "register_xalarm.h"
#include "sys/time.h"

namespace ubse::ras {
using ubse::common::def::UbseResult;

using XalarmReportEventFunc = int (*)(unsigned short, char*, size_t);
using HandlerMap = std::vector<std::pair<std::string, AlarmFaultHandler>>;
using NodeStateHandler = std::function<void(const std::string& faultInfo)>;
using NodeHandler = std::function<UbseResult(const std::string& nodeId)>;

constexpr std::array<char, 11> SPECIAL_CHAR_WHITE_LIST{'_', '{', '}', '\'', '\"', ':', '[', ']', ',', ' ', '-'};

inline bool IsAllowedSpecialChar(char ch)
{
    return std::find(SPECIAL_CHAR_WHITE_LIST.begin(), SPECIAL_CHAR_WHITE_LIST.end(), ch) !=
           SPECIAL_CHAR_WHITE_LIST.end();
}

inline bool IsDigitString(const std::string& str)
{
    return std::all_of(str.begin(), str.end(), [](char ch) { return std::isdigit(ch); });
}

inline bool hasInvalidChars(const std::string& str)
{
    // 数字、字母、白名单的特殊符号属于合法字符，其余为非法字符。函数检测是否包含非法字符，如果包含则返回false
    return std::any_of(str.begin(), str.end(), [](char ch) { return !(std::isalnum(ch) || IsAllowedSpecialChar(ch)); });
}
enum class NodeHandlerType {
    PRE_FAULT_STATE_HANDLER_TYPE = 0,
    PRE_FAULT_STATE_FAIL_HANDLER_TYPE = 1,
    NODE_FAULT_STATE_HANDLER_TYPE = 2,
    NODE_FAULT_STATE_CLEAR_HANDLER_TYPE = 3,
    NODE_HANDLER_TYPE_NUM = 4
};
class UbseRasHandler {
public:
    friend class UbseRasComHandler;
    /**
     * 获取UbseRasObserver单例
     * @return 返回UbseRasObserver单例
     */
    static UbseRasHandler& GetInstance();

    /**
     * 初始化RasHandler
     * @return
     */
    static UbseResult StartRasHandler();

    /**
     * 删除拷贝构造函数
     */
    UbseRasHandler(const UbseRasHandler&) = delete;

    /**
     * 删除赋值运算符
     * @return
     */
    UbseRasHandler& operator=(const UbseRasHandler&) = delete;

    /**
     * 节点级故障回调函数
     * @param fault_type
     * @param fault_info
     */
    UbseResult NodeFaultHandle(alarm_msg* alarmMsgPtr);

    /**
     * 注册故障处理函数
     * @param alarmHandler
     * @return
     */
    UbseResult RegisterAlarmFaultHandler(const AlarmHandler& alarmHandler);

    /**
     * 解注册故障处理函数
     * @param alarmFaultEvent
     * @param name
     * @return
     */
    uint32_t UnRegisterAlarmFaultHandler(ALARM_FAULT_TYPE alarmFaultEvent, std::string& name);

    /**
     * 注册node controller回调
     * @param handlerType
     * @param handler
     * @return
     */
    UbseResult RegisterNodeHandler(const NodeHandlerType& handlerType, const NodeHandler& handler);

    /**
    * 执行node controller回调
    * @param handlerType
    * @param nodeId
    * @return
    */
    UbseResult CallNodeHandle(const NodeHandlerType& handlerType, const std::string& nodeId);

    /**
     * 记录成功处理故障的msgId
     * @param msgId 故障对应的msgId
     */
    void AddProcessedMsgId(const std::string& msgId);

    /**
      * sysSentry内核重插时，清除所有msgId
      */
    void ClearAllMsgId();

    /**
       * 给定msgId，判断是否已处理过
       */
    bool MsgIdHasBeenProcessed(const std::string& msgId) const;

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
    static UbseResult HandleBMCFault(const std::string& info);

    /*
     * 处理Oom故障
     */
    UbseResult HandleOomFault(alarm_msg* msg);

    /*
     * 处理PANIC / Kernel Reboot 故障
     */
    UbseResult HandlePanicAndRebootFault(ALARM_FAULT_TYPE faultType, const std::string& info);

    /*
     * 处理内存故障
     */
    UbseResult HandleMemoryFault(ALARM_FAULT_TYPE faultType, std::string info);

    /*
     * 执行故障事件回调
     */
    UbseResult ExecuteFaultHandler(ALARM_FAULT_TYPE faultType, const std::string& faultInfo, const std::string& msg);
    UbseResult ExecuteFaultHandler(ALARM_FAULT_TYPE faultType, const std::string& faultInfo);

private:
    static UbseRasHandler instance;
    std::unordered_map<ALARM_FAULT_TYPE, std::map<AlarmHandlerPriority, HandlerMap>> faultHandlerMap{};
    NodeStateHandler nodeStateHandler;
    std::unordered_map<NodeHandlerType, std::vector<NodeHandler>> nodeHandlerMap;
    std::set<std::string> processedMsgId; // 已成功处理故障对应的msgId
};

bool IsMemInitFinished();
UbseResult HandleCnaAndEidMsg(const std::string& faultInfo, std::string& faultNodeId);
std::string QueryNodeIdByEid(const std::string& eid);
UbseResult ReportAckToSysSentry(ALARM_FAULT_TYPE alarmFaultType, const std::string& message);
void LogMemDebtInfoWithNode(ALARM_FAULT_TYPE faultType, const std::string& faultNode);
void ClearFaultHandlerResult(const std::string& msgId);

} // namespace ubse::ras
#endif // UBSE_MANAGER_UBSE_RAS_HANDLER_H
