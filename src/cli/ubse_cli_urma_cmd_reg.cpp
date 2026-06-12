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

#include "ubse_cli_urma_cmd_reg.h"
#include <map>
#include <netinet/in.h>
#include <securec.h>
#include <set>
#include <sstream>
#include "ubse_cli_buffer_guard.h"
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_ipc_common.h"
#include "ubse_serial_util.h"

namespace ubse::cli::reg {
UBSE_CLI_REGISTER_MODULE("CLI_URMA_MODULE", UbseCliRegUrmaModule);
using namespace ubse::cli::framework;
using namespace ubse::serial;
using namespace ubse::common::def;

static const std::string DE_SERIALIZATION_ERROR = "ERROR: Deserialization failed in client.";
static const std::string URMA_NODE_OPT = "node";
static const std::string URMA_DEVICE_OPT = "dev";
static const std::string URMA_INTERNAL_ERROR = "ERROR: Internal error.";
static const std::string URMA_NODE_STATE_ERROR = "ERROR: Node state is abnormal, maybe fault or node down.";
static const std::string URMA_NODE_ID_ERROR =
    "ERROR: Invalid request param,The option is as follow: node-id(1 ~ max node-id).";
static const std::string URMA_EMPTY_ERROR = "ERROR: The urma List is empty.";
static const std::string URMA_NODE_NOT_SUPPORT =
    "ERROR: Invalid request param, param -n is not supported in current topo";
static const std::string URMA_QUERY_OPTION_DES_NODE =
    "Query urma information for a specific node. Parameter 'node' between 1 and the maximum node ID."
    "Parameter 'node' is not supported in CLOS mesh type topo. "
    "If omitted, defaults to querying the local node.";
static const std::string URMA_QUERY_OPTION_DES_NAME =
    "Query urma information for a specific device. Parameter 'dev' expects a valid device name string."
    "If omitted, defaults to querying all devices.";
static const std::string URMA_QOS_PRI_OPT = "pri";
static const std::string URMA_QOS_BAND_OPT = "cir";
static const std::string URMA_QOS_CREATE_OPT_DES_PRI =
    "Priority level. Required. Valid values: 0, 1. Supports comma-separated list (e.g., 0,1).";
static const std::string URMA_QOS_CREATE_OPT_DES_CIR = "Committed Information Rate (bandwidth). Required. Unit: Gbps. "
                                                       "Non-negative integer. Supports comma-separated list.";
static const std::string URMA_QOS_CREATE_OPT_DES_NODE = "Node ID. Not supported in CLOS networking mode.";
static const std::string URMA_QOS_DELETE_OPT_DES_NODE = "Node ID. Not supported in CLOS networking mode.";
static const std::string URMA_QOS_DISPLAY_OPT_DES_NODE =
    "Node ID to query QoS configuration. Not supported in CLOS networking mode.";
static const std::string URMA_CLI_URMA_STATUS_ERROR = "ERROR: Invalid URMA status.";
static const std::string URMA_NAME_FORMAT_ERROR = "ERROR: Invalid device name list format.";
constexpr uint32_t URMA_INFO_STATUS_NUM = 4;
std::array<std::string, URMA_INFO_STATUS_NUM> urmaStatusArr = {"active", "inactive", "unknown", "down"};
static const std::string SERIALIZATION_ERROR = "ERROR: Serialization failed in client.";
static const std::string URMA_QOS_CLOS_NOT_SUPPORTED = "ERROR: CLOS networking does not support -n option.";
static const std::string URMA_QOS_PRI_BAND_REQUIRED = "ERROR: Both --pri and --cir are required.";
static const std::string URMA_QOS_INVALID_PRI_BAND = "ERROR: Invalid priority or bandwidth value.";
static const std::string URMA_QOS_PRI_RANGE_ERROR = "ERROR: Priority must be 0 or 1.";
static const std::string URMA_QOS_INTERNAL_ERROR = "ERROR: Internal error with error code ";
static const std::string URMA_QOS_CREATE_SUCCESS = "create successfully";
static const std::string URMA_QOS_DELETE_SUCCESS = "delete successfully";
static const std::string URMA_QOS_PRI_BAND_MISMATCH = "ERROR: --pri and --cir count mismatch.";
static const std::string URMA_QOS_COUNT_EXCEED = "ERROR: QoS config count exceeds limit (max 2).";
static const std::string URMA_QOS_PRI_DUPLICATE = "ERROR: Duplicate priorities are not allowed.";
static const std::string URMA_QOS_NULLPTR_ERROR = "ERROR: QoS template not initialized.";
static const std::string URMA_QOS_CONNECT_UBM_ERROR =
    "ERROR: Failed to access UBM interface, please check UBM service status.";
static const std::string URMA_QOS_TEMPLATE_NOT_EXISTED =
    "ERROR: Failed to create ETS QoS template, No ETS QoS template exists.";
static const std::string URMA_QOS_QUERY_EMPTY_ERROR =
    "No ETS QoS priority groups has been created, please run: ubsectl create urma-qos.";
static const std::string URMA_QOS_PRIO_GROUP_EXIST_ERROR =
    "ERROR: ETS QoS priority group already exists, please delete existing QoS config first.";
static const std::string URMA_QOS_PRIO_GROUP_EXIST_BUT_NOT_APPLIED =
    "ERROR: ETS QoS priority group already exists, but apply failed.";

void UbseCliRegUrmaModule::UbseCliSignUp()
{
    this->cmd_.emplace_back(UbseCliQueryUrmaDevInfo());
    this->cmd_.emplace_back(UbseCliCreateUrmaQos());
    this->cmd_.emplace_back(UbseCliDeleteUrmaQos());
    this->cmd_.emplace_back(UbseCliDisplayUrmaQos());
    return;
}
UbseCliCommandInfo UbseCliRegUrmaModule::UbseCliQueryUrmaDevInfo()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("display")
        .UbseCliSetType("urma")
        .UbseCliAddOption("n", URMA_NODE_OPT, URMA_QUERY_OPTION_DES_NODE)
        .UbseCliAddOption("d", URMA_DEVICE_OPT, URMA_QUERY_OPTION_DES_NAME)
        .UbseCliSetFunc(UbseQueryUrmaDevInfoFunc);
    return builder.UbseCliBuild();
}

UbseCliCommandInfo UbseCliRegUrmaModule::UbseCliCreateUrmaQos()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("create")
        .UbseCliSetType("urma-qos")
        .UbseCliAddOption("p", URMA_QOS_PRI_OPT, URMA_QOS_CREATE_OPT_DES_PRI)
        .UbseCliAddOption("c", URMA_QOS_BAND_OPT, URMA_QOS_CREATE_OPT_DES_CIR)
        .UbseCliAddOption("n", URMA_NODE_OPT, URMA_QOS_CREATE_OPT_DES_NODE)
        .UbseCliSetFunc(UbseCreateUrmaQosFunc);
    return builder.UbseCliBuild();
}

UbseCliCommandInfo UbseCliRegUrmaModule::UbseCliDeleteUrmaQos()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("delete")
        .UbseCliSetType("urma-qos")
        .UbseCliAddOption("n", URMA_NODE_OPT, URMA_QOS_DELETE_OPT_DES_NODE)
        .UbseCliSetFunc(UbseDeleteUrmaQosFunc);
    return builder.UbseCliBuild();
}

UbseCliCommandInfo UbseCliRegUrmaModule::UbseCliDisplayUrmaQos()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("display")
        .UbseCliSetType("urma-qos")
        .UbseCliAddOption("n", URMA_NODE_OPT, URMA_QOS_DISPLAY_OPT_DES_NODE)
        .UbseCliSetFunc(UbseDisplayUrmaQosFunc);
    return builder.UbseCliBuild();
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegUrmaModule::UbseCliProcessUrmaDevInfoTable(
    UbseDeSerialization &ubseDeSerial, uint32_t urmaSize)
{
    UbseCliResBuilder variableCellBuilder(UBSE_CLI_NUM_7, UBSE_CLI_NUM_7 * UBSE_CLI_NUM_10);
    size_t row = variableCellBuilder.UbseCliAddRow();
    variableCellBuilder.UbseCliAddlineSeparate(row);
    variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "urma-name");
    variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "dev-eid");
    variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "dev1-name");
    variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, "dev2-name");
    variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_5, "dev1-eid");
    variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_6, "dev2-eid");
    variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_7, "status");
    variableCellBuilder.UbseCliAddBottomlineSeparate();
    for (uint32_t i = 0; i < urmaSize; i++) {
        row = variableCellBuilder.UbseCliAddRow();
        std::string uramName{};
        uint32_t urmaType{};
        std::vector<std::string> feNames;
        std::vector<std::string> feEids;
        uint32_t urmaStatus{};
        std::string devEid{};
        ubseDeSerial >> uramName >> urmaType >> devEid >> feNames >> feEids >> urmaStatus;
        if ((!ubseDeSerial.Check()) || (feNames.size() <= 1 || feEids.size() <= 1)) {
            return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
        }
        if (urmaStatus >= urmaStatusArr.size()) {
            return UbseCliStringPromptReply(URMA_CLI_URMA_STATUS_ERROR);
        }
        variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, uramName);
        variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, devEid);
        variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, feNames[0]);
        variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, feNames[1]);
        variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_5, feEids[0]);
        variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_6, feEids[1]);
        variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_7, urmaStatusArr[urmaStatus]);
    }
    variableCellBuilder.UbseCliAddBottomlineSeparate();
    return UbseCliVariableCelReply(variableCellBuilder.UbseCliVariableCellBuild());
}
std::shared_ptr<UbseCliResultEcho> UbseCliRegUrmaModule::UbseCliProcessUrmaQosTable(UbseDeSerialization &ubseDeSerial,
                                                                                    uint32_t urmaQosSize)
{
    UbseCliResBuilder variableCellBuilder(UBSE_CLI_NUM_2, UBSE_CLI_NUM_2 * UBSE_CLI_NUM_10);
    size_t row = variableCellBuilder.UbseCliAddRow();
    variableCellBuilder.UbseCliAddlineSeparate(row);
    variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "priority");
    variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "bandwidth(Gbps)");
    variableCellBuilder.UbseCliAddBottomlineSeparate();
    for (uint32_t i = 0; i < urmaQosSize; i++) {
        row = variableCellBuilder.UbseCliAddRow();
        uint32_t priority{};
        uint32_t bandwidth{};
        ubseDeSerial >> priority >> bandwidth;
        if (!ubseDeSerial.Check()) {
            return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
        }
        variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, std::to_string(priority));
        variableCellBuilder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, std::to_string(bandwidth));
    }
    variableCellBuilder.UbseCliAddBottomlineSeparate();
    return UbseCliVariableCelReply(variableCellBuilder.UbseCliVariableCellBuild());
}

bool UbseCliRegUrmaModule::IsPositiveInteger(const std::string &str)
{
    if (str.empty()) {
        return false;
    }
    for (char c : str) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    return true;
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegUrmaModule::ValidateSingleQosParam(
    const std::string &priToken, const std::string &cirToken,
    uint32_t &priority, uint32_t &bandwidth)
{
    if (!IsPositiveInteger(priToken) || !IsPositiveInteger(cirToken)) {
        return UbseCliStringPromptReply(URMA_QOS_INVALID_PRI_BAND);
    }
    try {
        priority = static_cast<uint32_t>(std::stoul(priToken));
        bandwidth = static_cast<uint32_t>(std::stoul(cirToken));
    } catch (const std::exception &e) {
        return UbseCliStringPromptReply(URMA_QOS_INVALID_PRI_BAND);
    }
    if (priority > UBSE_CLI_NUM_1) {
        return UbseCliStringPromptReply(URMA_QOS_PRI_RANGE_ERROR);
    }
    if (bandwidth == 0) {
        return UbseCliStringPromptReply(URMA_QOS_INVALID_PRI_BAND);
    }
    return nullptr;
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegUrmaModule::ParseAndValidateQosParams(
    const std::string &priStr, const std::string &cirStr,
    std::vector<uint32_t> &priorities, std::vector<uint32_t> &bandwidths)
{
    std::vector<std::string> priList = ParseCommaSeparatedDeviceList(priStr);
    std::vector<std::string> cirList = ParseCommaSeparatedDeviceList(cirStr);

    if (priList.empty() || cirList.empty()) {
        return UbseCliStringPromptReply(URMA_QOS_INVALID_PRI_BAND);
    }
    if (priList.size() != cirList.size()) {
        return UbseCliStringPromptReply(URMA_QOS_PRI_BAND_MISMATCH);
    }
    if (priList.size() > UBSE_CLI_NUM_2) {
        return UbseCliStringPromptReply(URMA_QOS_COUNT_EXCEED);
    }
    for (size_t i = 0; i < priList.size(); ++i) {
        uint32_t priority;
        uint32_t bandwidth;
        auto result = ValidateSingleQosParam(priList[i], cirList[i], priority, bandwidth);
        if (result != nullptr) {
            return result;
        }
        priorities.push_back(priority);
        bandwidths.push_back(bandwidth);
    }
    std::set<uint32_t> uniquePriorities(priorities.begin(), priorities.end());
    if (uniquePriorities.size() != priorities.size()) {
        return UbseCliStringPromptReply(URMA_QOS_PRI_DUPLICATE);
    }
    return nullptr;
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegUrmaModule::MapQosErrorToMessage(uint32_t ret)
{
    static const std::map<uint32_t, std::string> errorMap = {
        {UBSE_ERROR_NULLPTR, URMA_QOS_NULLPTR_ERROR},
        {UBSE_ERROR_INVAL, URMA_QOS_INVALID_PRI_BAND},
        {UBSE_URMACONTRL_ERROR_ACCESS_MTI_FAILED, URMA_QOS_CONNECT_UBM_ERROR},
        {UBSE_URMACONTRL_ERROR_PRIO_GROUP_EXIST, URMA_QOS_PRIO_GROUP_EXIST_ERROR},
        {UBSE_URMACONTRL_ERROR_ETS_TEMPLATE_NOT_EXISTED, URMA_QOS_TEMPLATE_NOT_EXISTED}};
    auto it = errorMap.find(ret);
    if (it != errorMap.end()) {
        return UbseCliStringPromptReply(it->second);
    }
    return UbseCliStringPromptReply(URMA_QOS_INTERNAL_ERROR + std::to_string(ret));
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegUrmaModule::UbseCreateUrmaQosFunc([
    [maybe_unused]] const std::map<std::string, std::string> &params)
{
    if (params.find(URMA_NODE_OPT) != params.end()) {
        return UbseCliStringPromptReply(URMA_QOS_CLOS_NOT_SUPPORTED);
    }
    auto priOpt = params.find(URMA_QOS_PRI_OPT);
    auto bandOpt = params.find(URMA_QOS_BAND_OPT);
    if (priOpt == params.end() || bandOpt == params.end()) {
        return UbseCliStringPromptReply(URMA_QOS_PRI_BAND_REQUIRED);
    }

    std::vector<uint32_t> priorities;
    std::vector<uint32_t> bandwidths;
    auto parseResult = ParseAndValidateQosParams(priOpt->second, bandOpt->second, priorities, bandwidths);
    if (parseResult != nullptr) {
        return parseResult;
    }

    UbseSerialization ubseReqSerial;
    uint32_t itemCount = static_cast<uint32_t>(priorities.size());
    ubseReqSerial << itemCount;
    for (size_t i = 0; i < priorities.size(); ++i) {
        ubseReqSerial << priorities[i] << bandwidths[i];
    }
    if (!ubseReqSerial.Check()) {
        return UbseCliStringPromptReply(SERIALIZATION_ERROR);
    }

    ubse_api_buffer_t ubseReqBuffer{ubseReqSerial.GetBuffer(), static_cast<uint32_t>(ubseReqSerial.GetLength())};
    ubse_api_buffer_t ubseResBuffer{};
    uint32_t ret = ubse_invoke_call(UBSE_URMA, UBSE_URMA_CLI_QOS_CREATE, &ubseReqBuffer, &ubseResBuffer);
    UbseCliBufferGuard ubseCliBufferGuard(ubseResBuffer);
    if (ret != UBSE_OK) {
        return MapQosErrorToMessage(ret);
    }

    return UbseCliStringPromptReply(URMA_QOS_CREATE_SUCCESS);
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegUrmaModule::UbseDeleteUrmaQosFunc([
    [maybe_unused]] const std::map<std::string, std::string> &params)
{
    if (params.find(URMA_NODE_OPT) != params.end()) {
        return UbseCliStringPromptReply(URMA_QOS_CLOS_NOT_SUPPORTED);
    }
    ubse_api_buffer_t ubseReqBuffer{nullptr, 0};
    ubse_api_buffer_t ubseResBuffer{};
    uint32_t ret = ubse_invoke_call(UBSE_URMA, UBSE_URMA_QOS_DELETE, &ubseReqBuffer, &ubseResBuffer);
    UbseCliBufferGuard ubseCliBufferGuard(ubseResBuffer);
    if (ret != UBSE_OK) {
        return MapQosErrorToMessage(ret);
    }

    return UbseCliStringPromptReply(URMA_QOS_DELETE_SUCCESS);
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegUrmaModule::UbseDisplayUrmaQosFunc([
    [maybe_unused]] const std::map<std::string, std::string> &params)
{
    if (params.find(URMA_NODE_OPT) != params.end()) {
        return UbseCliStringPromptReply(URMA_QOS_CLOS_NOT_SUPPORTED);
    }
    ubse_api_buffer_t ubseReqBuffer{nullptr, 0};
    ubse_api_buffer_t ubseResBuffer{};
    uint32_t ret = ubse_invoke_call(UBSE_URMA, UBSE_URMA_CLI_QOS_GET, &ubseReqBuffer, &ubseResBuffer);
    UbseCliBufferGuard ubseCliBufferGuard(ubseResBuffer);
    if (ret != UBSE_OK && ret != UBSE_URMACONTRL_ERROR_ETS_TEMPLATE_NOT_APPLIED) {
        return MapQosErrorToMessage(ret);
    }

    UbseDeSerialization ubseDeSerial(ubseResBuffer.buffer, ubseResBuffer.length);
    uint32_t urmaQosSize{};
    ubseDeSerial >> urmaQosSize;
    if (!ubseDeSerial.Check()) {
        return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
    }
    if (urmaQosSize == 0) {
        return UbseCliStringPromptReply(URMA_QOS_QUERY_EMPTY_ERROR);
    }
    if (ret == UBSE_URMACONTRL_ERROR_ETS_TEMPLATE_NOT_APPLIED) {
        return UbseCliStringPromptReply(URMA_QOS_PRIO_GROUP_EXIST_BUT_NOT_APPLIED);
    }

    return UbseCliProcessUrmaQosTable(ubseDeSerial, urmaQosSize);
}

std::vector<std::string> UbseCliRegUrmaModule::ParseCommaSeparatedDeviceList(const std::string &deviceStr)
{
    std::vector<std::string> result;
    if (deviceStr.empty()) {
        return result;
    }

    std::stringstream ss(deviceStr);
    std::string token;

    while (std::getline(ss, token, ',')) {
        // 去除首尾空格
        size_t first = token.find_first_not_of(' ');
        if (first == std::string::npos) {
            continue;  // 整个token都是空格，跳过
        }
        size_t last = token.find_last_not_of(' ');
        std::string trimmed = token.substr(first, last - first + 1);
        if (!trimmed.empty()) {
            result.push_back(std::move(trimmed));
        }
    }

    return result;
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegUrmaModule::ParseAndValidateUrmaParams(
    const std::map<std::string, std::string> &params, uint32_t &nodeId, std::vector<std::string> &deviceNameList)
{
    auto urmaNodeCli = params.find(URMA_NODE_OPT);
    auto urmaDeviceCli = params.find(URMA_DEVICE_OPT);
    // 处理节点参数（可选）
    if (urmaNodeCli == params.end()) {
        nodeId = UINT32_MAX;  // 默认值：表示查询本节点的 URMA 信息
    } else {
        try {
            nodeId = static_cast<uint32_t>(std::stoul(urmaNodeCli->second));
        } catch (const std::exception &e) {
            return UbseCliStringPromptReply(URMA_NODE_ID_ERROR);
        }
    }
    // 处理设备名称参数（可选）
    if (urmaDeviceCli == params.end()) {
        deviceNameList.clear();  // 不传设备参数，deviceNameList 为空
    } else {
        deviceNameList = ParseCommaSeparatedDeviceList(urmaDeviceCli->second);
        if (deviceNameList.empty()) {
            return UbseCliStringPromptReply(URMA_NAME_FORMAT_ERROR);
        }
    }
    // 校验：显式传入 UINT32_MAX 或非法类型值视为错误
    if (nodeId == UINT32_MAX && urmaNodeCli != params.end()) {
        return UbseCliStringPromptReply(URMA_NODE_ID_ERROR);
    }
    return nullptr;  // 解析成功
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegUrmaModule::UbseQueryUrmaDevInfoFunc([
    [maybe_unused]] const std::map<std::string, std::string> &params)
{
    uint32_t nodeId{};
    std::vector<std::string> deviceNameList{};
    auto parseResult = ParseAndValidateUrmaParams(params, nodeId, deviceNameList);
    if (parseResult != nullptr) {
        return parseResult;
    }
    UbseSerialization ubseReqSerial;
    ubseReqSerial << nodeId;
    uint32_t deviceListSize = static_cast<uint32_t>(deviceNameList.size());
    ubseReqSerial << deviceListSize;
    for (const auto &deviceName : deviceNameList) {
        ubseReqSerial << deviceName;
    }
    if (!ubseReqSerial.Check()) {
        return UbseCliStringPromptReply(URMA_INTERNAL_ERROR);
    }
    ubse_api_buffer_t ubseReqBuffer{ubseReqSerial.GetBuffer(), static_cast<uint32_t>(ubseReqSerial.GetLength())};
    ubse_api_buffer_t ubseResBuffer{};
    uint32_t ret = ubse_invoke_call(UBSE_URMA, UBSE_URMA_CLI_DEV_GET, &ubseReqBuffer, &ubseResBuffer);
    UbseCliBufferGuard ubseCliBufferGuard(ubseResBuffer);
    if (ret == UBSE_ERR_NOT_EXIST) {
        return UbseCliStringPromptReply(URMA_NODE_ID_ERROR);
    }
    if (ret == UBSE_ERR_NOT_SUPPORTED) {
        return UbseCliStringPromptReply(URMA_NODE_NOT_SUPPORT);
    }
    if (ret == UBSE_ERROR_INVAL) {
        return UbseCliStringPromptReply(URMA_NODE_STATE_ERROR);
    }
    if (ret != UBSE_OK) {
        return UbseCliStringPromptReply(URMA_INTERNAL_ERROR);
    }
    UbseDeSerialization ubseDeSerial(ubseResBuffer.buffer, ubseResBuffer.length);
    uint32_t urmaSize{};
    ubseDeSerial >> urmaSize;
    if (!ubseDeSerial.Check()) {
        return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
    }
    if (urmaSize == 0) {
        return UbseCliStringPromptReply(URMA_EMPTY_ERROR);
    }
    return UbseCliProcessUrmaDevInfoTable(ubseDeSerial, urmaSize);
}

}  // namespace ubse::cli::reg