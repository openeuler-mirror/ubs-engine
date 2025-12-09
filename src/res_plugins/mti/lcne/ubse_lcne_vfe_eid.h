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

#ifndef UBSE_LCNE_VFE_EID_H
#define UBSE_LCNE_VFE_EID_H

#include "ubse_common_def.h"  // for UbseResult
#include "ubse_http_common.h" // for UbseHttpMethod, UbseHttpResponse (ptr o...
#include "ubse_lcne_def.h"    // for LcneServer
#include "ubse_topology_interface.h"
#include "ubse_xml.h"

namespace ubse::lcne {
using namespace common::def;
using namespace ubse::http;
using namespace ubse::mti;
using namespace ubse::utils;

class UbseLcneVfeEid {
public:
    static UbseLcneVfeEid &GetInstance()
    {
        static UbseLcneVfeEid instance("127.0.0.1", LcneServer::realPort); // 默认服务在本地 127.0.0.1 默认端口 34256;
        return instance;
    }
    // 发送请求
    UbseResult GetVfeEid(UbseLcneIouInfo iouInfo, std::vector<UbseLcneFeInfo> &allFeInfos);

private:
    UbseLcneVfeEid(std::string host, int port) : host(std::move(host)), port(port) {}
    UbseResult UpdateVfeEid(UbseLcneIouInfo iouInfo, std::vector<UbseLcneFeInfo> &allFeInfos);
    UbseResult ParseGetFeListResponse(const std::string &responseStr, std::vector<UbseLcneFeInfo> &allFeInfos);
    UbseResult ParseGetFeEidResponse(const std::string &responseStr, std::vector<UbseLcneFeInfo> &allFeInfos);
    UbseResult ParseFeEidXml(std::shared_ptr<UbseXml> ubseEidXml, UbseLcneFeInfo &feInfo);
    std::vector<std::string> ueIdlistSplit(const std::string &str, const std::string &delimiter);
    UbseLcneFeInfo *FindVfeInVector(std::string slotId, std::string ubpuId, std::string iouId, std::string entityId,
                                std::vector<UbseLcneFeInfo> &allFeInfos);
    UbseResult GetPortIdFromInterfaceName(std::string intfaceName, uint32_t &portId);
    std::string host;
    int port;
    const std::string GET_VFE_LIST_URI = "/restconf/data/huawei-vbussw-service:vbussw-service/mue-ue-binding-infos";
    const std::string GET_VFE_EID_URI =
        "/restconf/data/huawei-vbussw-service:vbussw-service/entity-urma-communication-infos";
};
} // namespace ubse::lcne
#endif // UBSE_LCNE_VFE_EID_H