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

#ifndef UBSE_LCNE_FE_EID_H
#define UBSE_LCNE_FE_EID_H

#include "ubse_common_def.h"  // for UbseResult
#include "ubse_http_common.h" // for UbseHttpMethod, UbseHttpResponse (ptr o...
#include "ubse_lcne_def.h"    // for LcneServer
#include "ubse_xml.h"
#include "adapter_plugins/mti/ubse_mti_def.h"

namespace ubse::lcne {
using namespace common::def;
using namespace ubse::http;
using namespace ubse::adapter_plugins::mti;
using namespace ubse::utils;

class UbseLcneFeEid {
public:
    static UbseLcneFeEid& GetInstance()
    {
        static UbseLcneFeEid instance("127.0.0.1", LcneServer::realPort); // 默认服务在本地 127.0.0.1 默认端口 8799;
        return instance;
    }
    // 发送请求
    UbseResult GetFeEid(UbseMtiIouInfo& iouInfo, std::vector<UbseMtiFeInfo>& allFeInfos);
    UbseResult GetComUrmaEidClos(UbseMtiIouInfo& iouInfo, UbseMtiEidGroup& feInfo);

private:
    UbseLcneFeEid(std::string host, int port) : host(std::move(host)), port(port) {}
    UbseResult UpdateFeType(UbseMtiIouInfo& iouInfo, std::vector<UbseMtiFeInfo>& allFeInfos);
    UbseResult ParseFeTypeListResponse(const std::string& responseStr, std::vector<UbseMtiFeInfo>& allFeInfos);
    UbseResult ParseGetFeEidResponse(const std::string& responseStr, std::vector<UbseMtiFeInfo>& allFeInfos);
    UbseResult ParseFeEidXml(std::shared_ptr<UbseXml> ubseEidXml, UbseMtiFeInfo& feInfo);
    std::vector<std::string> ueIdlistSplit(const std::string& str, const std::string& delimiter);
    UbseResult ExtractBasicInfoFromXml(const std::shared_ptr<UbseXml>& ubseXml, UbseMtiIouInfo& iouInfo);
    UbseMtiFeInfo* FindVfeInVector(UbseMtiIouInfo& iouInfo, std::string entityId,
                                   std::vector<UbseMtiFeInfo>& allFeInfos);
    UbseResult GetPortIdFromInterfaceName(std::string intfaceName, uint32_t& portId);
    UbseResult GetComEidInfo(std::vector<UbseMtiFeInfo>& allFeInfos, UbseMtiEidGroup& feInfo);
    std::string GetEidGroupId(std::string eid);

    std::string host;
    int port;
    const std::string GET_FE_LIST_URI = "/restconf/data/huawei-vbussw-service:vbussw-service/mue-ue-binding-infos";
    const std::string GET_FE_EID_URI =
        "/restconf/data/huawei-vbussw-service:vbussw-service/entity-urma-communication-infos";
};
} // namespace ubse::lcne
#endif // UBSE_LCNE_FE_EID_H