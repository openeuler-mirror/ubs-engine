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

#ifndef UBSE_LCNE_QOS_H
#define UBSE_LCNE_QOS_H

#include <cstdint>
#include <string>             // for string, basic_string
#include <utility>            // for move
#include "ubse_common_def.h"  // for UbseResult
#include "ubse_http_common.h" // for UbseHttpMethod, UbseHttpResponse (ptr o...
#include "ubse_lcne_def.h"    // for LcneServer
#include "ubse_topology_interface.h"

namespace ubse::lcne {
using namespace common::def;
using namespace ubse::http;
using namespace ubse::mti;

class UbseLcneQos {
public:
    static UbseLcneQos &GetInstance()
    {
        static UbseLcneQos instance("127.0.0.1", LcneServer::realPort); // 默认服务在本地 127.0.0.1 默认端口 8088;
        return instance;
    }

    /* 创建Qos Profile模板信息 */
    UbseResult CreatQosProfile(UbseLcneQosProfile ubseLcneQosProfile);
    /* 删除Qos Profile模板信息 */
    UbseResult DeleteQosProfile(std::string proflieName);
    /* 查询Qos Profile模板信息 */
    UbseResult QureyQosProfile(std::string proflieName, UbseLcneQosProfile &ubseLcneQosProfile);
    /* 应用Qos Profile到vfe上 */
    UbseResult ApplyVfeQos(UbseLcneFeInfo ubseFeInfo, std::string proflieName);
    /* 删除vfe上的Qos Profile */
    UbseResult DeleteVfeQos(UbseLcneFeInfo ubseFeInfo);
    /* 查询vfe上的Qos Profile */
    UbseResult QueryVfeQos(UbseLcneFeInfo ubseFeInfo, std::string &proflieName);

private:
    UbseLcneQos(std::string host, int port) : host(std::move(host)), port(port) {}
    UbseResult BuildQoSProfileXml(UbseLcneQosProfile ubseLcneQosProfile, std::string &xmlStr);
    UbseResult BuildQoSXml(UbseLcneFeInfo ubseFeInfo, std::string profileName, std::string &xmlStr);
    UbseResult ParseQosProfileResponse(std::string body, UbseLcneQosProfile &ubseLcneQosProfile);
    UbseResult ParseVfeQosResponse(std::string body, std::string &proflieName);
    std::string host;
    int port;
};
} // namespace ubse::lcne

#endif // UBSE_LCNE_QOS_H