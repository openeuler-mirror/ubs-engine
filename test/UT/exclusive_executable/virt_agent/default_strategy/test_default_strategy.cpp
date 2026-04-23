/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "test_default_strategy.h"

#include <ubse_logger.h>
#include <ubse_ut_dir.h>
#include <securec.h>
#include <cstdint>
#include <fstream>
#include <string>
#include "default_strategy.h"
#include "default_struct.h"
#include "ubse_json_util.h"

#ifdef LOG_DEBUG_ON
#include <unistd.h>
#endif

using namespace ubse::log;
using namespace ubse::utils;
using namespace default_strategy;

namespace ubse::ut::df {
const auto STRATEGY_MODULE_NAME = std::string("strategy").c_str();
const uint64_t SPECS_1G = static_cast<uint64_t>(1) << 30;
const uint64_t SPECS_8G = static_cast<uint64_t>(8) << 30;
const uint16_t STRATEGY_MODULE_CODE = 410;
const float DEFAULT_SECOND_LINE = 0.92;
const float DEFAULT_FIRST_LINE = 0.85;
const float DEFAULT_RETURN_LINE = 0.8;
const int TIME_SECOND_180 = 180;
const StrategyConfig STRATEGY_CONFIG = {"0", "0", "0", 0, 0, 0, 0, 0};
Logfunc g_logFunc = [](uint32_t level, const char *msg) -> void {};

TestDefaultStrategy::TestDefaultStrategy() {}

// 设置测试环境
void TestDefaultStrategy::SetUp()
{
    Test::SetUp();
#ifdef LOG_DEBUG_ON
    LoggerOptions options{ UbseLogLevel::DEBUG, 2, 5, 1024, UbseLogLevel::INFO, "/var/log/scbus" };
    auto log = UbseLoggerManager::Instance();
    UbseLoggerWriter *writer = new (std::nothrow)
        UbseLoggerFilesink(options.logPath, options.maxFileSizeInMB * 1024 * 1024, options.rotationFileCount);
    log->Init(options, writer);
#endif
}

// 拆卸测试环境
void TestDefaultStrategy::TearDown()
{
    Test::TearDown();
}

DsResult JsonStringValueIsGet(const rapidjson::Value &pstJson, const std::string pcKey)
{
    if (pstJson.HasMember(pcKey.c_str()) && pstJson[pcKey.c_str()].IsString()) {
        return DS_OK;
    }
    return DS_ERROR;
}

// 增加类型转换，由Json中的char*转成自定义类型
DsResult StrToActionType(const std::string &name, EscapeActionType &escapeActionType)
{
    if (name == "BORROW") {
        escapeActionType = EscapeActionType::BORROW;
        return DS_OK;
    } else if (name == "RETURN") {
        escapeActionType = EscapeActionType::RETURN;
        return DS_OK;
    } else if (name == "NOPE") {
        escapeActionType = EscapeActionType::NOPE;
        return DS_OK;
    } else {
        UBSE_LOGGER_ERROR(STRATEGY_MODULE_NAME, STRATEGY_MODULE_CODE) << "The string don't match ";
        return DS_ERROR_INVAL;
    }
}

DsResult StrToNumaMigStatus(const std::string &name, NumaMigrateStatus &numaMigrateStatus)
{
    if (name == "NORMAL") {
        numaMigrateStatus = NumaMigrateStatus::NORMAL;
        return DS_OK;
    } else if (name == "MIGRATEIN") {
        numaMigrateStatus = NumaMigrateStatus::MIGRATEIN;
        return DS_OK;
    } else if (name == "MIGRATEOUT") {
        numaMigrateStatus = NumaMigrateStatus::MIGRATEOUT;
        return DS_OK;
    } else if (name == "MIGRATINGIN") {
        numaMigrateStatus = NumaMigrateStatus::MIGRATINGIN;
        return DS_OK;
    } else if (name == "MIGRATINGOUT") {
        numaMigrateStatus = NumaMigrateStatus::MIGRATINGOUT;
        return DS_OK;
    } else {
        UBSE_LOGGER_ERROR(STRATEGY_MODULE_NAME, STRATEGY_MODULE_CODE) << "The string don't match ";
        return DS_ERROR_INVAL;
    }
}

DsResult StrToVmMigStatus(const std::string &name, VmMigrateStatus &vmMigrateStatus)
{
    if (name == "MIGRATEABLE") {
        vmMigrateStatus = VmMigrateStatus::MIGRATEABLE;
        return DS_OK;
    } else if (name == "MIGRATING") {
        vmMigrateStatus = VmMigrateStatus::MIGRATING;
        return DS_OK;
    } else if (name == "MIGRATEUNABLE") {
        vmMigrateStatus = VmMigrateStatus::MIGRATEUNABLE;
        return DS_OK;
    } else {
        UBSE_LOGGER_ERROR(STRATEGY_MODULE_NAME, STRATEGY_MODULE_CODE) << "The string don't match ";
        return DS_ERROR_INVAL;
    }
}

DsResult StrToConState(const std::string &name, ConnectionState &state)
{
    if (name == "UP") {
        state = ConnectionState::UP;
        return DS_OK;
    } else if (name == "DOWN") {
        state = ConnectionState::DOWN;
        return DS_OK;
    } else {
        UBSE_LOGGER_ERROR(STRATEGY_MODULE_NAME, STRATEGY_MODULE_CODE) << "The string don't match ";
        return DS_ERROR_INVAL;
    }
}

DsResult StrToNumaStatus(const std::string &name, NumaStatus &numaStatus)
{
    if (name == "NORMAL") {
        numaStatus = NumaStatus::NORMAL;
        return DS_OK;
    } else if (name == "FAULT") {
        numaStatus = NumaStatus::FAULT;
        return DS_OK;
    } else if (name == "UNKNOWN") {
        numaStatus = NumaStatus::UNKNOWN;
        return DS_OK;
    } else {
        UBSE_LOGGER_ERROR(STRATEGY_MODULE_NAME, STRATEGY_MODULE_CODE) << "The string don't match ";
        return DS_ERROR_INVAL;
    }
}

// 读Json文件
DsResult ReadJsonFile(const std::string &filename, std::string &jsonString)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        UBSE_LOGGER_ERROR(STRATEGY_MODULE_NAME, STRATEGY_MODULE_CODE) << "Failed to open file: " << filename;
        return DS_ERROR_INVAL;
    }
    std::string line;
    while (std::getline(file, line)) {
        jsonString += line;
    }
    file.close();
    return DS_OK;
}

DsResult ReadVmBasicInfos(const rapidjson::Value &VmBasicInfos, AlarmNumaInfo &alarmNumaInfo)
{
    DsResult ret = DS_OK;
    if (!VmBasicInfos.IsArray()) {
        return DS_ERROR;
    }
    size_t infoNum = VmBasicInfos.Size();
    for (size_t i = 0; i < infoNum; i++) {
        const rapidjson::Value &VmBasicInfosPtr = VmBasicInfos[i];
        std::string uuid;
        ret |= UbseJsonUtil::GetStrFromJsonPtr(VmBasicInfosPtr, "uuid", uuid);
        alarmNumaInfo.vmBasicInfos[uuid].uuid = uuid;
        alarmNumaInfo.vmBasicInfos[uuid].name = alarmNumaInfo.vmBasicInfos[uuid].uuid;
        ret |= UbseJsonUtil::GetUint64FromJsonPtr(VmBasicInfosPtr, "maxMem", alarmNumaInfo.vmBasicInfos[uuid].maxMem);
        ret |= UbseJsonUtil::GetUint64FromJsonPtr(VmBasicInfosPtr, "remoteUsedMem",
                                                  alarmNumaInfo.vmBasicInfos[uuid].remoteUsedMem);

        double vmCreateTime = 0;
        double vmMigrateInTime = 0;
        double vmMigrateCount = 0;
        ret |= UbseJsonUtil::GetDoubleFromJsonPtr(VmBasicInfosPtr, "vmCreateTime", vmCreateTime);
        ret |= UbseJsonUtil::GetDoubleFromJsonPtr(VmBasicInfosPtr, "vmMigrateInTime", vmMigrateInTime);
        ret |= UbseJsonUtil::GetDoubleFromJsonPtr(VmBasicInfosPtr, "vmMigrateCount", vmMigrateCount);
        alarmNumaInfo.vmBasicInfos[uuid].vmCreateTime = static_cast<time_t>(vmCreateTime);
        alarmNumaInfo.vmBasicInfos[uuid].vmMigrateInTime = static_cast<time_t>(vmMigrateInTime);
        alarmNumaInfo.vmBasicInfos[uuid].vmMigrateCount = static_cast<uint16_t>(vmMigrateCount);
        std::string MigStatusStr;
        VmMigrateStatus MigStatus;
        ret |= UbseJsonUtil::GetStrFromJsonPtr(VmBasicInfosPtr, "vmMigrateStatus", MigStatusStr);
        DsResult resStatus = StrToVmMigStatus(MigStatusStr, MigStatus);
        EXPECT_EQ(resStatus, DS_OK);
        alarmNumaInfo.vmBasicInfos[uuid].vmMigrateStatus = MigStatus;
    }
    EXPECT_EQ(ret, DS_OK);
    return DS_OK;
}

DsResult ReadBorrowItemInfo(const rapidjson::Value &InputAlarmNumaInfo, Allocator &allocator,
                            AlarmNumaInfo &alarmNumaInfo)
{
    if (!InputAlarmNumaInfo.IsObject()) {
        return DS_ERROR;
    }
    DsResult ret = DS_OK;
    rapidjson::Value AlarmNumaInfoBorrowItemInfo(rapidjson::kObjectType);
    ret |= UbseJsonUtil::GetObjectFromJsonPtr(InputAlarmNumaInfo, "borrowItemInfo", allocator,
                                              AlarmNumaInfoBorrowItemInfo);
    rapidjson::Value AlarmNumaInfoBorrowItem(rapidjson::kArrayType);
    ret |= UbseJsonUtil::GetArrayFromJsonPtr(AlarmNumaInfoBorrowItemInfo, "borrowItem", allocator,
                                             AlarmNumaInfoBorrowItem);
    for (size_t indexBorrow = 0; indexBorrow < AlarmNumaInfoBorrowItem.Size(); indexBorrow++) {
        BorrowItem tmp;
        std::vector<BorrowItem> borrowItem;
        const rapidjson::Value &BorrowItemInfoPtr = AlarmNumaInfoBorrowItem[indexBorrow];
        double exportLocNum = 0;
        ret |= UbseJsonUtil::GetDoubleFromJsonPtr(BorrowItemInfoPtr, "exportLocNum", exportLocNum);
        tmp.exportLocNum = static_cast<uint16_t>(exportLocNum);

        rapidjson::Value ExportLocInfo;
        ret |= UbseJsonUtil::GetArrayFromJsonPtr(BorrowItemInfoPtr, "exportLocInfo", allocator, ExportLocInfo);
        // InputAlarmNumaInfoBorrowItemInfoExportLocInfo
        NodeLocInfo nodeLocInfo;
        for (int indexExportLocInfo = 0; indexExportLocInfo < tmp.exportLocNum; indexExportLocInfo++) {
            BorrowItem temp;
            const rapidjson::Value &ExportLocInfoPtr = ExportLocInfo[indexExportLocInfo];
            int index = 0;
            nodeLocInfo.hostId = ExportLocInfoPtr[index++].GetString();
            nodeLocInfo.socketId = static_cast<int16_t>(ExportLocInfoPtr[index++].GetInt());
            nodeLocInfo.numaId = static_cast<int16_t>(ExportLocInfoPtr[index].GetInt());
            tmp.exportLocInfo.push_back(nodeLocInfo);
        }
        // InputAlarmNumaInfoBorrowItemInfoRequestSize
        rapidjson::Value RequestSize(rapidjson::kArrayType);
        ret |= UbseJsonUtil::GetArrayFromJsonPtr(BorrowItemInfoPtr, "requestSize", allocator, RequestSize);
        for (int indexRequest = 0; indexRequest < RequestSize.Size(); indexRequest++) {
            tmp.requestSize.push_back(RequestSize[indexRequest].GetUint64());
        }
        ret |= UbseJsonUtil::GetUint64FromJsonPtr(BorrowItemInfoPtr, "importMemId", tmp.importMemId);
        ret |= UbseJsonUtil::GetStrFromJsonPtr(BorrowItemInfoPtr, "name", tmp.name);
        alarmNumaInfo.borrowItemInfo.borrowItem.emplace_back(tmp);
    }
    EXPECT_EQ(ret, DS_OK);
    return DS_OK;
}

DsResult ReadInputAlarmNumaInfo(const rapidjson::Value &pstJson, Allocator &allocator, AlarmNumaInfo &alarmNumaInfo)
{
    if (!pstJson.IsObject()) {
        return DS_ERROR;
    }
    DsResult ret = DS_OK;
    rapidjson::Value strategyInput(rapidjson::kObjectType);
    ret |= UbseJsonUtil::GetObjectFromJsonPtr(pstJson, "input", allocator, strategyInput);
    // InputAlarmNumaInfoBasic
    rapidjson::Value AlarmNumaInfo(rapidjson::kObjectType);
    ret |= UbseJsonUtil::GetObjectFromJsonPtr(strategyInput, "alarmNumaInfo", allocator, AlarmNumaInfo);
    ret |= UbseJsonUtil::GetBoolFromJsonPtr(AlarmNumaInfo, "isMemReturn", alarmNumaInfo.isMemReturn);
    uint32_t oomEventFlag = 0;
    ret |= UbseJsonUtil::GetUintFromJsonPtr(AlarmNumaInfo, "oomEventFlag", oomEventFlag);
    alarmNumaInfo.oomEventFlag = static_cast<uint16_t>(oomEventFlag);
    // InputAlarmNumaInfoNumaLoc
    rapidjson::Value AlarmNumaInfoNumaLoc(rapidjson::kArrayType);
    ret |= UbseJsonUtil::GetArrayFromJsonPtr(AlarmNumaInfo, "numaLoc", allocator, AlarmNumaInfoNumaLoc);
    int index = 0;
    alarmNumaInfo.numaLoc.hostId = AlarmNumaInfoNumaLoc[index++].GetString();
    alarmNumaInfo.numaLoc.socketId = static_cast<uint16_t>(AlarmNumaInfoNumaLoc[index++].GetUint());
    alarmNumaInfo.numaLoc.numaId = static_cast<uint16_t>(AlarmNumaInfoNumaLoc[index].GetUint());
    // InputAlarmNumaInfoVmBasicInfos
    rapidjson::Value AlarmNumaInfoVmBasicInfos(rapidjson::kArrayType);
    ret |= UbseJsonUtil::GetArrayFromJsonPtr(AlarmNumaInfo, "vmBasicInfos", allocator, AlarmNumaInfoVmBasicInfos);
    DsResult resVmBasicInfos = ReadVmBasicInfos(AlarmNumaInfoVmBasicInfos, alarmNumaInfo);
    EXPECT_EQ(resVmBasicInfos, DS_OK);
    // InputAlarmNumaInfoBorrowItemInfo
    DsResult resBorrowItemInfo = ReadBorrowItemInfo(AlarmNumaInfo, allocator, alarmNumaInfo);
    EXPECT_EQ(resBorrowItemInfo, DS_OK);
    EXPECT_EQ(ret, DS_OK);
    return DS_OK;
}

void AssignGlobalNumaLoc(const rapidjson::Value &GlobalNumaInfoPtr, Allocator &allocator,
                         GlobalNumaInfo &globalNumaInfo)
{
    DsResult ret = DS_OK;
    rapidjson::Value GlobalNumaLoc(rapidjson::kArrayType);
    ret |= UbseJsonUtil::GetArrayFromJsonPtr(GlobalNumaInfoPtr, "numaLoc", allocator, GlobalNumaLoc);
    // InputGlobalNumaInfoMapNumaLoc
    int index = 0;
    globalNumaInfo.numaLoc.hostId = GlobalNumaLoc[index++].GetString();
    globalNumaInfo.numaLoc.hostName = globalNumaInfo.numaLoc.hostId;
    globalNumaInfo.numaLoc.socketId = static_cast<uint16_t>(GlobalNumaLoc[index++].GetUint());
    globalNumaInfo.numaLoc.numaId = static_cast<uint16_t>(GlobalNumaLoc[index].GetUint());
    EXPECT_EQ(ret, DS_OK);
}

void AssignGlobalNumaInfo(const rapidjson::Value &GlobalNumaInfoPtr, Allocator &allocator,
                          GlobalNumaInfo &globalNumaInfo)
{
    DsResult ret = DS_OK;
    globalNumaInfo.numaCpuCounts = 0;
    globalNumaInfo.numaCpuUsedCounts = 0;
    ret |= UbseJsonUtil::GetUint64FromJsonPtr(GlobalNumaInfoPtr, "numaMemTotal", globalNumaInfo.numaMemTotal);
    ret |= UbseJsonUtil::GetUint64FromJsonPtr(GlobalNumaInfoPtr, "numaMemUsed", globalNumaInfo.numaMemUsed);
    ret |= UbseJsonUtil::GetUint64FromJsonPtr(GlobalNumaInfoPtr, "numaVMMemAllocated",
                                              globalNumaInfo.numaVMMemAllocated);
    ret |= UbseJsonUtil::GetUint64FromJsonPtr(GlobalNumaInfoPtr, "numaMemBorrow", globalNumaInfo.numaMemBorrow);

    std::string numaMigrateStatusStr;
    NumaMigrateStatus numaMigrateStatus;
    ret |= UbseJsonUtil::GetStrFromJsonPtr(GlobalNumaInfoPtr, "numaMigrateStatus", numaMigrateStatusStr);
    DsResult resNumaMigrateStatus = StrToNumaMigStatus(numaMigrateStatusStr, numaMigrateStatus);
    EXPECT_EQ(resNumaMigrateStatus, DS_OK);
    globalNumaInfo.numaMigrateStatus = numaMigrateStatus;

    double numaMigrateLastTime = 0;
    ret |= UbseJsonUtil::GetDoubleFromJsonPtr(GlobalNumaInfoPtr, "numaMigrateLastTime", numaMigrateLastTime);
    globalNumaInfo.numaMigrateLastTime = static_cast<time_t>(numaMigrateLastTime);
    EXPECT_EQ(ret, DS_OK);
}

DsResult ReadInputGlobalNumaInfoMap(const rapidjson::Value &pstJson, Allocator &allocator,
                                    GlobalNumaInfoMap &globalNumaInfoMap)
{
    DsResult ret = DS_OK;
    if (!pstJson.IsObject()) {
        return DS_ERROR;
    }
    rapidjson::Value strategyInput(rapidjson::kObjectType);
    ret |= UbseJsonUtil::GetObjectFromJsonPtr(pstJson, "input", allocator, strategyInput);
    rapidjson::Value GlobalNumaInfoMap(rapidjson::kArrayType);
    ret |= UbseJsonUtil::GetArrayFromJsonPtr(strategyInput, "globalNumaInfoMap", allocator, GlobalNumaInfoMap);
    for (int indexGlobal = 0; indexGlobal < GlobalNumaInfoMap.Size(); indexGlobal++) {
        GlobalNumaInfo globalNumaInfo;
        const rapidjson::Value &GlobalNumaInfoPtr = GlobalNumaInfoMap[indexGlobal].GetObject();
        // InputGlobalNumaInfoMapNumaLoc
        AssignGlobalNumaLoc(GlobalNumaInfoPtr, allocator, globalNumaInfo);
        // InputGlobalNumaInfo
        AssignGlobalNumaInfo(GlobalNumaInfoPtr, allocator, globalNumaInfo);

        if (JsonStringValueIsGet(GlobalNumaInfoPtr, "numaStatus") == 0) {
            NumaStatus numaStatus;
            std::string numaStatusStr;
            ret |= UbseJsonUtil::GetStrFromJsonPtr(GlobalNumaInfoPtr, "numaStatus", numaStatusStr);
            DsResult resNumaStatus = StrToNumaStatus(numaStatusStr, numaStatus);
            EXPECT_EQ(resNumaStatus, DS_OK);
            globalNumaInfo.numaStatus = numaStatus;
        }
        globalNumaInfoMap.insert(std::pair<VMNodeLocInfo, GlobalNumaInfo>(globalNumaInfo.numaLoc, globalNumaInfo));
    }
    EXPECT_EQ(ret, DS_OK);
    return DS_OK;
}

DsResult ReadOutput(const rapidjson::Value &pstJson, Allocator &allocator, EscapeAction &escapeAction)
{
    DsResult ret = DS_OK;
    rapidjson::Value strategyOutput(rapidjson::kObjectType);
    ret |= UbseJsonUtil::GetObjectFromJsonPtr(pstJson, "output", allocator, strategyOutput);
    EscapeActionType actionType;
    std::string actionTypeStr;
    ret |= UbseJsonUtil::GetStrFromJsonPtr(strategyOutput, "escapeActionType", actionTypeStr);
    DsResult resActionType = StrToActionType(actionTypeStr, actionType);
    EXPECT_EQ(resActionType, DS_OK);
    escapeAction.actionType = actionType;
    // OutputreturnMemNames
    rapidjson::Value strategyOutputreturnMemNames(rapidjson::kArrayType);
    ret |= UbseJsonUtil::GetArrayFromJsonPtr(strategyOutput, "returnMemNames", allocator, strategyOutputreturnMemNames);
    for (int index = 0; index < strategyOutputreturnMemNames.Size(); index++) {
        escapeAction.returnMemNames.emplace_back(strategyOutputreturnMemNames[index].GetString());
    }
    // OutputBorrowSizes
    rapidjson::Value strategyOutputBorrowSizes(rapidjson::kArrayType);
    ret |= UbseJsonUtil::GetArrayFromJsonPtr(strategyOutput, "borrowSizes", allocator, strategyOutputBorrowSizes);
    for (int index = 0; index < strategyOutputBorrowSizes.Size(); index++) {
        escapeAction.borrowSizes.emplace_back(strategyOutputBorrowSizes[index].GetUint64());
    }
    EXPECT_EQ(ret, DS_OK);
    return DS_OK;
}

DsResult CheckConf(StrategyConfig &strategyConf)
{
    auto &defaultStrategy = DefaultStrategy::GetInstance();
    EXPECT_EQ(defaultStrategy.InitLogFunc(g_logFunc), DS_OK);

    // escape
    EXPECT_EQ(defaultStrategy.SetWaterLine(std::stoi(strategyConf.borrowWatermark) * 0.01f,
        std::stoi(strategyConf.highWatermark) * 0.01f, std::stoi(strategyConf.lowWatermark) * 0.01f),
        DS_OK);

    // mem
    EXPECT_EQ(defaultStrategy.SetMaxMemBorrow(strategyConf.maxMemBorrow), DS_OK);
    EXPECT_EQ(defaultStrategy.SetMaxMemBorrow(1), DS_ERR_SET_INVAL);

    EXPECT_EQ(defaultStrategy.SetPerBorrowBound(strategyConf.minMemPerBorrowBytes, strategyConf.maxMemPerBorrowBytes),
              DS_OK);
    EXPECT_EQ(defaultStrategy.SetMaxPerTotalMemBorrowBytes(strategyConf.maxPerTotalMemBorrowBytes), DS_OK);
    EXPECT_EQ(defaultStrategy.SetMaxPerTotalMemBorrowBytes(SPECS_1G), DS_ERR_SET_INVAL);
    EXPECT_EQ(defaultStrategy.SetOomBorrowMemSize(strategyConf.oomEventBorrowBytes), DS_OK);
    EXPECT_EQ(defaultStrategy.SetOomBorrowMemSize(SPECS_8G), DS_ERR_SET_INVAL);
    EXPECT_EQ(defaultStrategy.GetOomBorrowMemSize(), strategyConf.oomEventBorrowBytes);

    return DS_OK;
}

DsResult ReadConf(const rapidjson::Value &pstJson, Allocator  &allocator, StrategyConfig &vmStrategyConf)
{
    DsResult ret = DS_OK;
    rapidjson::Value strategyConf(rapidjson::kObjectType);
    ret |= UbseJsonUtil::GetObjectFromJsonPtr(pstJson, "conf", allocator, strategyConf);
    // escape
    ret |= UbseJsonUtil::GetStrFromJsonPtr(strategyConf, "memSecondLine", vmStrategyConf.borrowWatermark);
    ret |= UbseJsonUtil::GetStrFromJsonPtr(strategyConf, "memReturnLine", vmStrategyConf.lowWatermark);
    ret |= UbseJsonUtil::GetStrFromJsonPtr(strategyConf, "memFirstLine", vmStrategyConf.highWatermark);
    // mem
    ret |= UbseJsonUtil::GetFloatFromJsonPtr(strategyConf, "maxMemBorrow", vmStrategyConf.maxMemBorrow);
    ret |= UbseJsonUtil::GetUint64FromJsonPtr(strategyConf, "maxMemPerBorrowBytes",
                                              vmStrategyConf.maxMemPerBorrowBytes);
    ret |= UbseJsonUtil::GetUint64FromJsonPtr(strategyConf, "minMemPerBorrowBytes",
                                              vmStrategyConf.minMemPerBorrowBytes);
    ret |= UbseJsonUtil::GetUint64FromJsonPtr(strategyConf, "maxPerTotalMemBorrowBytes",
                                              vmStrategyConf.maxPerTotalMemBorrowBytes);
    ret |= UbseJsonUtil::GetUint64FromJsonPtr(strategyConf, "oomEventBorrowBytes", vmStrategyConf.oomEventBorrowBytes);
    EXPECT_EQ(CheckConf(vmStrategyConf), DS_OK);
    EXPECT_EQ(ret, DS_OK);
    return DS_OK;
}

DsResult ReadJsonAndConf(std::string fileName, AlarmNumaInfo &alarmNumaInfo, GlobalNumaInfoMap &globalNumaInfoMap,
    EscapeAction &escapeAction, StrategyConfig &vmStrategyConf)
{
    std::string fileContent;
    EXPECT_EQ(ReadJsonFile(fileName, fileContent), DS_OK);

    rapidjson::Document pstJson;
    pstJson.Parse(fileContent.c_str());
    auto &allocator = pstJson.GetAllocator();

    EXPECT_EQ(ReadInputAlarmNumaInfo(pstJson, allocator, alarmNumaInfo), DS_OK);
    EXPECT_EQ(ReadInputGlobalNumaInfoMap(pstJson, allocator, globalNumaInfoMap), DS_OK);
    EXPECT_EQ(ReadOutput(pstJson, allocator, escapeAction), DS_OK);
    EXPECT_EQ(ReadConf(pstJson, allocator, vmStrategyConf), DS_OK);
    return DS_OK;
}

void EscapeStrategyUT(const std::string fileName)
{
    std::string caseDir = std::string(UT_DIRECTORY) + "/exclusive_executable/virt_agent/default_strategy/case/";
    AlarmNumaInfo alarmNumaInfo = { 0 };
    GlobalNumaInfoMap globalNumaInfoMap;
    EscapeAction escapeAction;
    EscapeAction expectedEscapeAction;
    StrategyConfig strategyConf{};
    EXPECT_EQ(ReadJsonAndConf(caseDir + fileName, alarmNumaInfo, globalNumaInfoMap, expectedEscapeAction, strategyConf),
        DS_OK);
    EXPECT_EQ(EscapeAlgorithmInit(strategyConf, g_logFunc), DS_OK);

    // 执行测试函数
    DsResult result = EscapeAlgorithm(strategyConf, alarmNumaInfo, globalNumaInfoMap, escapeAction);
#ifdef LOG_DEBUG_ON
    sleep(1); // 保证日志的完整输出
#endif
    // 断言测试结果
    EXPECT_EQ(result, DS_OK);
    EXPECT_EQ(escapeAction.actionType, expectedEscapeAction.actionType);
    EXPECT_EQ(escapeAction.borrowSizes, expectedEscapeAction.borrowSizes);
    EXPECT_EQ(escapeAction.returnMemNames, expectedEscapeAction.returnMemNames);
    EXPECT_EQ(escapeAction.ToString(), escapeAction.ToString());
}

TEST_F(TestDefaultStrategy, VMEscapeStrategySetPerBorrowBoundTest1)
{
    // DTS2024112531454
    auto &defaultStrategy = DefaultStrategy::GetInstance();
    EXPECT_EQ(defaultStrategy.InitLogFunc(g_logFunc), DS_OK);
    uint64_t newMaxMemPerBorrowBytes = static_cast<uint64_t>(1) << 30;
    uint64_t newMinMemPerBorrowBytes = static_cast<uint64_t>(4) << 30;
    EXPECT_EQ(defaultStrategy.SetPerBorrowBound(newMinMemPerBorrowBytes, newMaxMemPerBorrowBytes),
        DS_ERR_SET_INVAL);
    EXPECT_EQ(defaultStrategy.GetMaxMemPerBorrowBytes(), newMinMemPerBorrowBytes);
    EXPECT_EQ(defaultStrategy.GetMinMemPerBorrowBytes(), newMaxMemPerBorrowBytes);
}

TEST_F(TestDefaultStrategy, JsonReadAndConfTest)
{
    // 初始化测试对象
    std::string caseDir = std::string(UT_DIRECTORY) + "/exclusive_executable/virt_agemt/default_strategy/case/";
    std::string fileName = "strategy_case_default.json";
    AlarmNumaInfo alarmNumaInfo;
    GlobalNumaInfoMap globalNumaInfoMap;
    EscapeAction escapeActionFromStrategy;
    StrategyConfig strategyConf{};
    // 构造测试数据
    DsResult res =
        ReadJsonAndConf(caseDir + fileName, alarmNumaInfo, globalNumaInfoMap, escapeActionFromStrategy, strategyConf);

    // 断言测试结果
    EXPECT_EQ(res, DS_OK);
}

TEST_F(TestDefaultStrategy, VMEscapeStrategyBorrowTwiceDifferentTest1)
{
    EscapeStrategyUT("strategy_borrow_twice_different.json");
}

TEST_F(TestDefaultStrategy, VMEscapeStrategyBorrowMemUpperLimitTestTest1)
{
    EscapeStrategyUT("strategy_borrow_upper_limit.json");
}

TEST_F(TestDefaultStrategy, VMEscapeStrategyBorrowMemTest1)
{
    EscapeStrategyUT("strategy_borrow_mem.json");
}

TEST_F(TestDefaultStrategy, VMEscapeStrategyBorrowMemFailTest1)
{
    EscapeStrategyUT("strategy_borrow_mem_fail.json");
}

TEST_F(TestDefaultStrategy, VMEscapeStrategyMigrateListExceptionTest1)
{
    // incomplete environment
    EscapeStrategyUT("strategy_migrate_list_exception.json");
}

TEST_F(TestDefaultStrategy, VMEscapeStrategySetWaterLineCornerCaseTest2)
{
    // WaterLine 100 100 60
    EscapeStrategyUT("strategy_no_borrow_migrate.json");
}

TEST_F(TestDefaultStrategy, VMEscapeStrategySetWaterLineCornerCaseTest3)
{
    // WaterLine 90 80 60
    EscapeStrategyUT("strategy_borrow_and_migrate.json");
}

TEST_F(TestDefaultStrategy, VMEscapeStrategySetWaterLineCornerCaseTest4)
{
    // WaterLine 80 80 60
    EscapeStrategyUT("strategy_only_borrow_no_migrate.json");
}

TEST_F(TestDefaultStrategy, VMEscapeStrategyPreFinLogTest1)
{
    // DTS2024120903598
    EscapeStrategyUT("strategy_migrate_nope.json");
}

TEST_F(TestDefaultStrategy, VMEscapeStrategycreatejsonTest1)
{
    EscapeStrategyUT("strategy_create_json.json");
}

TEST_F(TestDefaultStrategy, VMEscapeStrategyReturnTest1)
{
    EscapeStrategyUT("strategy_return1.json");
}

TEST_F(TestDefaultStrategy, VMEscapeStrategyReturnTest2)
{
    EscapeStrategyUT("strategy_return2.json");
}

TEST_F(TestDefaultStrategy, VMEscapeStrategyOomTest1)
{
    EscapeStrategyUT("strategy_oom.json");
}

TEST_F(TestDefaultStrategy, VMEscapeStrategySmallSizeReturnTest)
{
    EscapeStrategyUT("strategy_small_size_return.json");
}

TEST_F(TestDefaultStrategy, VMEscapeStrategyConfParameter1Test)
{
    // 初始化测试对象
    auto &defaultStrategy = DefaultStrategy::GetInstance();
    EXPECT_EQ(defaultStrategy.InitLogFunc(g_logFunc), DS_OK);

    // escape
    const float newMemReturnLine = 0.5;
    const float newMemFirstLine = 0.6;
    const float newGetMemSecondLine = 0.7;

    EXPECT_EQ(defaultStrategy.SetWaterLine(newGetMemSecondLine, newMemFirstLine, newMemReturnLine), DS_OK);
    EXPECT_FLOAT_EQ(defaultStrategy.GetMemReturnLine(), newMemReturnLine);
    EXPECT_FLOAT_EQ(defaultStrategy.GetMemFirstLine(), newMemFirstLine);
    EXPECT_FLOAT_EQ(defaultStrategy.GetMemSecondLine(), newGetMemSecondLine);
    EXPECT_EQ(defaultStrategy.SetWaterLine(DEFAULT_SECOND_LINE, DEFAULT_FIRST_LINE, DEFAULT_RETURN_LINE), DS_OK);

    const float newMinMemOverageRecovery = 0.03;
    EXPECT_EQ(defaultStrategy.SetMinMemOverageRecovery(1), DS_ERR_SET_INVAL);
    EXPECT_EQ(defaultStrategy.SetMinMemOverageRecovery(newMinMemOverageRecovery), DS_OK);
    EXPECT_FLOAT_EQ(defaultStrategy.GetMinMemOverageRecovery(), newMinMemOverageRecovery);

    // mem
    const float newMaxMemBorrow = 0.2;
    EXPECT_EQ(defaultStrategy.SetMaxMemBorrow(newMaxMemBorrow), DS_OK);
    EXPECT_FLOAT_EQ(defaultStrategy.GetMaxMemBorrow(), newMaxMemBorrow);

    const uint64_t memBorrowAlignBytes = static_cast<uint64_t>(1) << 27;
    EXPECT_EQ(defaultStrategy.SetMemBorrowAlignBytes(SPECS_8G), DS_ERR_SET_INVAL);
    EXPECT_EQ(defaultStrategy.SetMemBorrowAlignBytes(memBorrowAlignBytes), DS_OK);
    EXPECT_EQ(defaultStrategy.GetMemBorrowAlignBytes(), memBorrowAlignBytes);

    const uint64_t newMaxMemPerBorrowBytes1 = static_cast<uint64_t>(3) << 30;
    const uint64_t newMinMemPerBorrowBytes1 = static_cast<uint64_t>(2) << 30;
    const uint64_t newMaxMemPerBorrowBytes2 = static_cast<uint64_t>(4) << 30;
    const uint64_t newMinMemPerBorrowBytes2 = static_cast<uint64_t>(1) << 30;
    EXPECT_EQ(defaultStrategy.SetPerBorrowBound(newMinMemPerBorrowBytes1, newMaxMemPerBorrowBytes1), DS_OK);
    EXPECT_EQ(defaultStrategy.GetMaxMemPerBorrowBytes(), newMaxMemPerBorrowBytes1);
    EXPECT_EQ(defaultStrategy.GetMinMemPerBorrowBytes(), newMinMemPerBorrowBytes1);
    EXPECT_EQ(defaultStrategy.SetPerBorrowBound(newMinMemPerBorrowBytes2, newMaxMemPerBorrowBytes2), DS_OK);

    const uint64_t maxPerTotalMemBorrowBytes = static_cast<uint64_t>(4) << 30;
    EXPECT_EQ(defaultStrategy.SetMaxPerTotalMemBorrowBytes(maxPerTotalMemBorrowBytes), DS_OK);
    EXPECT_EQ(defaultStrategy.GetMaxPerTotalMemBorrowBytes(), maxPerTotalMemBorrowBytes);
}

void CheckRestOfWaterLine()
{
    auto &defaultStrategy = DefaultStrategy::GetInstance();
    EXPECT_EQ(defaultStrategy.InitLogFunc(g_logFunc), DS_OK);

    float secondLine5 = 0.7;
    float firstLine5 = 0.93;
    float returnLine5 = 0.6;
    EXPECT_EQ(defaultStrategy.SetWaterLine(secondLine5, firstLine5, returnLine5), DS_ERR_SET_INVAL);
    EXPECT_NEAR(defaultStrategy.GetMemSecondLine(), DEFAULT_SECOND_LINE, 0.001f);
    EXPECT_NEAR(defaultStrategy.GetMemFirstLine(), DEFAULT_FIRST_LINE, 0.001f);
    EXPECT_NEAR(defaultStrategy.GetMemReturnLine(), DEFAULT_RETURN_LINE, 0.001f);

    float secondLine6 = 0.7;
    float firstLine6 = -1;
    float returnLine6 = -1;
    EXPECT_EQ(defaultStrategy.SetWaterLine(secondLine6, firstLine6, returnLine6), DS_ERR_SET_INVAL);
    EXPECT_NEAR(defaultStrategy.GetMemSecondLine(), DEFAULT_SECOND_LINE, 0.001f);
    EXPECT_NEAR(defaultStrategy.GetMemFirstLine(), DEFAULT_FIRST_LINE, 0.001f);
    EXPECT_NEAR(defaultStrategy.GetMemReturnLine(), DEFAULT_RETURN_LINE, 0.001f);

    float secondLine7 = 0.7;
    float firstLine7 = 0.7;
    float returnLine7 = 0.7;
    EXPECT_EQ(defaultStrategy.SetWaterLine(secondLine7, firstLine7, returnLine7), DS_ERR_SET_INVAL);
    EXPECT_NEAR(defaultStrategy.GetMemSecondLine(), DEFAULT_SECOND_LINE, 0.001f);
    EXPECT_NEAR(defaultStrategy.GetMemFirstLine(), DEFAULT_FIRST_LINE, 0.001f);
    EXPECT_NEAR(defaultStrategy.GetMemReturnLine(), DEFAULT_RETURN_LINE, 0.001f);

    float secondLine8 = 0.5;
    float firstLine8 = 0.6;
    float returnLine8 = 0.7;
    EXPECT_EQ(defaultStrategy.SetWaterLine(secondLine8, firstLine8, returnLine8), DS_ERR_SET_INVAL);
    EXPECT_NEAR(defaultStrategy.GetMemSecondLine(), DEFAULT_SECOND_LINE, 0.001f);
    EXPECT_NEAR(defaultStrategy.GetMemFirstLine(), DEFAULT_FIRST_LINE, 0.001f);
    EXPECT_NEAR(defaultStrategy.GetMemReturnLine(), DEFAULT_RETURN_LINE, 0.001f);

    float secondLine9 = 0.7;
    float firstLine9 = 0.7;
    float returnLine9 = 0.68;
    EXPECT_EQ(defaultStrategy.SetWaterLine(secondLine9, firstLine9, returnLine9), DS_ERR_SET_INVAL);
    EXPECT_NEAR(defaultStrategy.GetMemSecondLine(), DEFAULT_SECOND_LINE, 0.001f);
    EXPECT_NEAR(defaultStrategy.GetMemFirstLine(), DEFAULT_FIRST_LINE, 0.001f);
    EXPECT_NEAR(defaultStrategy.GetMemReturnLine(), DEFAULT_RETURN_LINE, 0.001f);
}

TEST_F(TestDefaultStrategy, VMEscapeStrategySetWaterLinrCornerCaseTest1)
{
    auto &defaultStrategy = DefaultStrategy::GetInstance();
    EXPECT_EQ(defaultStrategy.InitLogFunc(g_logFunc), DS_OK);
    float secondLine1 = 1;
    float firstLine1 = 0.5;
    float returnLine1 = 0.6;
    EXPECT_EQ(defaultStrategy.SetWaterLine(secondLine1, firstLine1, returnLine1), DS_ERR_SET_INVAL);
    EXPECT_NEAR(defaultStrategy.GetMemSecondLine(), DEFAULT_SECOND_LINE, 0.001f);
    EXPECT_NEAR(defaultStrategy.GetMemFirstLine(), DEFAULT_FIRST_LINE, 0.001f);
    EXPECT_NEAR(defaultStrategy.GetMemReturnLine(), DEFAULT_RETURN_LINE, 0.001f);

    float secondLine2 = 0.9;
    float firstLine2 = 0.5;
    float returnLine2 = 0.6;
    EXPECT_EQ(defaultStrategy.SetWaterLine(secondLine2, firstLine2, returnLine2), DS_ERR_SET_INVAL);
    EXPECT_NEAR(defaultStrategy.GetMemSecondLine(), DEFAULT_SECOND_LINE, 0.001f);
    EXPECT_NEAR(defaultStrategy.GetMemFirstLine(), DEFAULT_FIRST_LINE, 0.001f);
    EXPECT_NEAR(defaultStrategy.GetMemReturnLine(), DEFAULT_RETURN_LINE, 0.001f);

    float secondLine3 = 0.7;
    float firstLine3 = 0.5;
    float returnLine3 = 0.6;
    EXPECT_EQ(defaultStrategy.SetWaterLine(secondLine3, firstLine3, returnLine3), DS_ERR_SET_INVAL);
    EXPECT_NEAR(defaultStrategy.GetMemSecondLine(), DEFAULT_SECOND_LINE, 0.001f);
    EXPECT_NEAR(defaultStrategy.GetMemFirstLine(), DEFAULT_FIRST_LINE, 0.001f);
    EXPECT_NEAR(defaultStrategy.GetMemReturnLine(), DEFAULT_RETURN_LINE, 0.001f);

    float secondLine4 = 0.7;
    float firstLine4 = 0.92;
    float returnLine4 = 0.6;
    EXPECT_EQ(defaultStrategy.SetWaterLine(secondLine4, firstLine4, returnLine4), DS_ERR_SET_INVAL);
    EXPECT_NEAR(defaultStrategy.GetMemSecondLine(), DEFAULT_SECOND_LINE, 0.001f);
    EXPECT_NEAR(defaultStrategy.GetMemFirstLine(), DEFAULT_FIRST_LINE, 0.001f);
    EXPECT_NEAR(defaultStrategy.GetMemReturnLine(), DEFAULT_RETURN_LINE, 0.001f);

    CheckRestOfWaterLine();
}
}