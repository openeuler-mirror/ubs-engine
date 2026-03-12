/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_conf.h"
namespace ubse::config {
/**
 * @brief 获取无符号短整型类型配置
 * @param[in] section: 配置节
 * @param[in] configKey: 配置参数key
 * @param[out] configValue: 配置参数的值
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseGetUInt(const std::string &section, const std::string &configKey, uint32_t &configValue)
{
    if (configKey == "log.max.fileSize") {
        configValue = 30;
    } else if (configKey == "log.fileNums") {
        configValue = 20;
    } else if (configKey == "log.queue.maxItem") {
        configValue = 1024;
    } else if (configKey == "db.server.port") {
        configValue = 6387;
    } else if (configKey == "db.cluster.port") {
        configValue = 10001;
    } else if (configKey == "db.store.history.record") {
        configValue = 1000;
    } else if (configKey == "queue.maxItem") {
        configValue = 1024;
    } else if (configKey == "high.thread.maxItem") {
        configValue = 10;
    } else if (configKey == "medium.thread.maxItem") {
        configValue = 5;
    } else if (configKey == "low.thread.maxItem") {
        configValue = 2;
    }
    return 0;
}

/**
 * @brief 获取Float类型配置
 * @param[in] section: 配置节
 * @param[in] configKey: 配置参数key
 * @param[out] configValue: 配置参数的值
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseGetFloat(const std::string &section, const std::string &configKey, float &configValue)
{
    return 0;
}

/**
 * @brief 获取字符串类型配置
 * @param[in] section: 配置节
 * @param[in] configKey: 配置参数key
 * @param[out] configValue: 配置参数的值
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseGetStr(const std::string &section, const std::string &configKey, std::string &configValue)
{
    if (configKey == "nodeIds") {
        configValue = "NODE11347,NODE11348";
    } else if (configKey == "log.level") {
        configValue = "DEBUG";
    } else if (configKey == "nodeId") {
        configValue = "NODE11347";
    } else if ("nodeRole" == configKey) {
        configValue = "Master";
    } else if ("masterAddress" == configKey) {
        configValue = "NODE11347:10.10.0.2:1901";
    } else if ("eid" == configKey) {
        configValue = "10.10.0.2";
    } else if ("algo.log.level" == configKey) {
        configValue = "DEBUG";
    } else if ("rack.manager.vm.enable" == configKey) {
        configValue = "false";
    } else if ("rpc.worker.thread.num" == configKey) {
        configValue = "32";
    } else if ("obmm.clear.switch" == configKey) {
        configValue = "true";
    } else if ("enable.default.watermark.process" == configKey) {
        configValue = "false";
    } else if ("enable.smap" == configKey) {
        configValue = "true";
    } else if ("htrace.path" == configKey) {
        configValue = "/var/log/rm_mem";
    } else if ("ipc.worker.thread.num" == configKey) {
        configValue = "32";
    } else if ("smap.enable.app.list" == configKey) {
        configValue = "redis-server;spark;lib_diagnose;minio;";
    } else if ("high.watermark" == configKey) {
        configValue = "85";
    } else if ("low.watermark" == configKey) {
        configValue = "80";
    } else if ("node.reserved.memory" == configKey) {
        configValue = "NODE11347:262144;NODE11348:262144;";
    }
    return 0;
}

/**
 * @brief 获取布尔类型配置
 * @param[in] section: 配置节
 * @param[in] configKey: 配置参数key
 * @param[out] configValue: 配置参数的值
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseGetBool(const std::string &section, const std::string &configKey, bool &configValue)
{
    if (configKey == "mem.enable") {
        configValue = false;
    } else if (configKey == "ssu.enable") {
        configValue = false;
    } else if (configKey == "db.isMaster") {
        configValue = true;
    }
    return 0;
}

/**
 * @brief 获取无符号长整数类型配置
 * @param[in] section: 配置节
 * @param[in] configKey: 配置参数key
 * @param[out] configValue: 配置参数的值
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseGetULong(const std::string &section, const std::string &configKey, uint64_t &configValue)
{
    if (configKey == "log.max.fileSize") {
        configValue = 30;
    } else if (configKey == "log.fileNums") {
        configValue = 20;
    } else if (configKey == "log.queue.maxItem") {
        configValue = 1024;
    } else if (configKey == "db.server.port") {
        configValue = 6387;
    } else if (configKey == "db.cluster.port") {
        configValue = 10001;
    } else if (configKey == "db.store.history.record") {
        configValue = 1000;
    } else if (configKey == "queue.maxItem") {
        configValue = 1024;
    } else if (configKey == "high.thread.maxItem") {
        configValue = 10;
    } else if (configKey == "medium.thread.maxItem") {
        configValue = 5;
    } else if (configKey == "low.thread.maxItem") {
        configValue = 2;
    }
    return 0;
}

/**
 * @brief 获取配置事件ID
 * @param[in] section: 配置节
 * @param[out] string, 返回配置事件ID
 */
void RackGetConfigEventId(const std::string &section, std::string &eventId)
{
    return;
}
} // namespace ubse::config