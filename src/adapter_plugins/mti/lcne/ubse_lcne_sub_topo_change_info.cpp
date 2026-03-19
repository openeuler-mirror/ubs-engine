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

#include "ubse_lcne_sub_topo_change_info.h"
#include "securec.h" // for memcpy_s, EOK
#include "src/adapter_plugins/mti/ubse_lcne_topology.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_net_util.h"
#include "ubse_pointer_process.h"
#include "ubse_xml.h"

namespace ubse::lcne {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::utils;
using namespace ubse::mti;
using namespace ubse::context;
using namespace ubse::config;

void GetTcpServerPort(uint32_t &port)
{
    port = DEFAULT_TCP_SERVER_PORT;
    auto module = UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "GetTcpServerPort GetModule failed.";
        return;
    }

    UbseResult ret = module->GetConf<uint32_t>(UBSE_UBFM_SECTION, UBSE_HTTP_TCP_SERVER_PORT, port);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetTcpServerPort get ubse.server.port failed, will use default value: "
                       << DEFAULT_TCP_SERVER_PORT;
    }
    if (!UbseNetUtil::IsPortVaLid(port)) {
        UBSE_LOG_ERROR << "Tcp server port: " << port << " is invalid, will use default value: "
                       << DEFAULT_TCP_SERVER_PORT;
        port = DEFAULT_TCP_SERVER_PORT;
    }
}

std::string BuildReqBodyStr()
{
    uint32_t port = DEFAULT_TCP_SERVER_PORT; // tcp server port，默认8082
    if (LcneServer::isTcpServer) {
        GetTcpServerPort(port);
    }
    std::string portStr = std::to_string(port);
    std::string udsStr = "unix://" + UBSE_UBM_UDS_ADDRESS;
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>();
    if (ubseXml == nullptr) {
        return "";
    }
    ubseXml->AddNode("create-subscription");
    ubseXml->Attr("xmlns", "urn:ietf:params:xml:ns:netconf:notification:1.0");
    ubseXml->AddNode("ip");
    ubseXml->Child("ip")->Text("127.0.0.1");
    ubseXml->AddNode("port");
    ubseXml->Child("port")->Text(portStr);
    if (!LcneServer::isTcpServer) {
        UBSE_LOG_INFO << udsStr;
        ubseXml->AddNode("address");
        ubseXml->Child("address")->Text(udsStr);
    }
    ubseXml->AddNode("url");
    ubseXml->Child("url")->Text(UbseLcneTopology::urlLinkUpAndDown);

    std::string xmlString;
    ubseXml->Printer(xmlString);
    return xmlString;
}

uint32_t UbseLcneLinkInfo::SubLcneLinkInfo()
{
    UbseHttpRequest req;
    UbseHttpResponse rsp;

    req.method = "POST";
    req.path = "/restconf/operations/notifications:create-subscription";
    req.headers.emplace("Accept", "application/yang-data+xml");
    req.headers.emplace("Content-Type", "application/yang-data+xml");
    req.body = BuildReqBodyStr();

    auto res = UbseHttpModule::HttpSend(req, rsp);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] Subscribe to LCNE topology change notifications is failed. " << FormatRetCode(res);
        return res;
    }
    // todo：此处状态码需要区分重复订阅与url错误，后续lcne合入后改变
    if (rsp.status != static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK)) {
        UBSE_LOG_WARN << "[MTI] Access the LCNE topology information interface is failed.The HTTP status code is "
                    << rsp.status;
        return UBSE_ERROR;
    }
    if (rsp.body.empty()) {
        UBSE_LOG_WARN << "[MTI] LCNE Topology response is empty.";
        return UBSE_ERROR;
    }

    // todo：消息体在后续lcne合入后改变解析方式
    res = ParseMonitorData(rsp.body);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "[MTI] Topology Info Parse XML data is failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t UbseLcneLinkInfo::ParseMonitorData(std::string &resBody)
{
    std::shared_ptr<UbseXml> ubseXml = SafeMakeShared<UbseXml>(resBody);
    if (ubseXml == nullptr) {
        return UBSE_ERROR_NOMEM;
    }
    const auto ret = ubseXml->Parse();
    if (ret != UbseXmlError::OK) {
        UBSE_LOG_ERROR << "Topology resBody parse failed.";
        return UBSE_ERROR;
    }
    ubseXml = ubseXml->Next("rpc_reply");
    if (ubseXml == nullptr) {
        UBSE_LOG_ERROR << "rpc_reply parse nodes failed.";
        return UBSE_ERROR;
    }
    ubseXml = ubseXml->Next("result");
    if (ubseXml == nullptr) {
        UBSE_LOG_ERROR << "result parse nodes failed.";
        return UBSE_ERROR;
    }
    if (ubseXml->Child("result") == nullptr) {
        UBSE_LOG_ERROR << "result parse nodes failed.";
        return UBSE_ERROR;
    }
    auto result = ubseXml->Child("result")->Text();
    if (result == "ok") {
        UBSE_LOG_INFO << "SubLcneLinkInfo success.";
    } else {
        UBSE_LOG_INFO << "SubLcneLinkInfo failed.";
    }
    return UBSE_OK;
}
} // namespace ubse::lcne
