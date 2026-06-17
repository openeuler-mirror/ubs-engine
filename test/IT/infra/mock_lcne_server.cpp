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

#include "mock_lcne_server.h"

#include <chrono>
#include <filesystem>
#include <sys/stat.h>
#include <unistd.h>

#include "it_console_log.h"

namespace ubse::it::infra {

static const std::string HOST_INFO_XML =
    "<vbussw-inventory xmlns=\"urn:huawei:yang:huawei-vbussw-inventory\">\n"
    "  <logic-entities>\n"
    "    <logic-entity>\n"
    "      <bus-instance-eid>0x413</bus-instance-eid>\n"
    "      <guid>12-3456-7-8-abcd-ef01-234512-6789abcd</guid>\n"
    "      <type>host</type>\n"
    "      <upi>0x1231</upi>\n"
    "      <state>online</state>\n"
    "      <try-times>1</try-times>\n"
    "    </logic-entity>\n"
    "  </logic-entities>\n"
    "</vbussw-inventory>";

static const std::string SUBSCRIPTION_RESPONSE_XML =
    "<restconf>\n"
    "  <rpc_reply xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">\n"
    "    <result>ok</result>\n"
    "  </rpc_reply>\n"
    "</restconf>";

std::string MockLcneServer::GenerateBusInstanceXml() const
{
    return "<vbussw-inventory xmlns=\"urn:huawei:yang:huawei-vbussw-inventory\">\n"
           "     <logic-entity-mappings>\n"
           "       <logic-entity-mapping>\n"
           "         <host-bus-instance-eid>0x10401</host-bus-instance-eid>\n"
           "         <physical-entity-mappings>\n"
           "           <physical-entity-mapping>\n"
           "             <slot-id>" + std::to_string(slotId_) + "</slot-id>\n"
           "             <chip-id>1</chip-id>\n"
           "             <die-id>1</die-id>\n"
           "           </physical-entity-mapping>\n"
           "           <physical-entity-mapping>\n"
           "             <slot-id>" + std::to_string(slotId_) + "</slot-id>\n"
           "             <chip-id>2</chip-id>\n"
           "             <die-id>1</die-id>\n"
           "           </physical-entity-mapping>\n"
           "         </physical-entity-mappings>\n"
           "       </logic-entity-mapping>\n"
           "     </logic-entity-mappings>\n"
           "   </vbussw-inventory>";
}

std::string MockLcneServer::GenerateIoDieInfoXml() const
{
    return "<vbussw-service xmlns=\"urn:huawei:yang:huawei-vbussw-service\">\n"
           "  <iou-infos>\n"
           "    <iou-info>\n"
           "      <slot-id>" + std::to_string(slotId_) + "</slot-id>\n"
           "      <ubpu-id>1</ubpu-id>\n"
           "      <iou-id>1</iou-id>\n"
           "      <bus-controller-eid>0x00000</bus-controller-eid>\n"
           "      <guid>01-0101-0-1-0101-0101-010101-0101010101</guid>\n"
           "      <upi>0x0003</upi>\n"
           "      <primary-cna>0x0085a7</primary-cna>\n"
           "      <ubpu-type>CPU</ubpu-type>\n"
           "      <iou-status>normal</iou-status>\n"
           "    </iou-info>\n"
           "  </iou-infos>\n"
           "</vbussw-service>";
}

std::string MockLcneServer::GenerateTopologyNodesXml() const
{
    std::string xml = "<lingqu-topology xmlns=\"urn:huawei:yang:huawei-lingqu-topology\">\n  <nodes>\n";

    int portId = 0;
    xml += "    <node>\n"
           "      <slot>" + std::to_string(slotId_) + "</slot>\n"
           "      <ubpu>1</ubpu>\n"
           "      <iou>1</iou>\n"
           "      <ubpu-type>CPU</ubpu-type>\n"
           "      <physical-ports>\n";

    // Self-loop port (port 0)
    xml += "        <physical-port>\n"
           "          <physical-port-id>" + std::to_string(portId) + "</physical-port-id>\n"
           "          <interface-name>eth" + std::to_string(portId) + "</interface-name>\n"
           "          <physical-port-role>server</physical-port-role>\n"
           "          <physical-port-status>up</physical-port-status>\n"
           "          <remote-slot>" + std::to_string(slotId_) + "</remote-slot>\n"
           "          <remote-ubpu>1</remote-ubpu>\n"
           "          <remote-iou>1</remote-iou>\n"
           "          <remote-physical-port-id>0</remote-physical-port-id>\n"
           "        </physical-port>\n";
    portId++;

    // Cross-node ports (connect to all other cluster nodes)
    for (uint32_t remoteSlot : clusterSlotIds_) {
        if (remoteSlot == slotId_) {
            continue;
        }
        xml += "        <physical-port>\n"
               "          <physical-port-id>" + std::to_string(portId) + "</physical-port-id>\n"
               "          <interface-name>eth" + std::to_string(portId) + "</interface-name>\n"
               "          <physical-port-role>server</physical-port-role>\n"
               "          <physical-port-status>up</physical-port-status>\n"
               "          <remote-slot>" + std::to_string(remoteSlot) + "</remote-slot>\n"
               "          <remote-ubpu>1</remote-ubpu>\n"
               "          <remote-iou>1</remote-iou>\n"
               "          <remote-physical-port-id>0</remote-physical-port-id>\n"
               "        </physical-port>\n";
        portId++;
    }

    xml += "      </physical-ports>\n"
           "    </node>\n";

    // Remote nodes (minimal entries so the daemon learns about all cluster members)
    for (uint32_t remoteSlot : clusterSlotIds_) {
        if (remoteSlot == slotId_) {
            continue;
        }
        xml += "    <node>\n"
               "      <slot>" + std::to_string(remoteSlot) + "</slot>\n"
               "      <ubpu>1</ubpu>\n"
               "      <iou>1</iou>\n"
               "      <ubpu-type>CPU</ubpu-type>\n"
               "      <physical-ports>\n"
               "        <physical-port>\n"
               "          <physical-port-id>0</physical-port-id>\n"
               "          <interface-name>eth0</interface-name>\n"
               "          <physical-port-role>server</physical-port-role>\n"
               "          <physical-port-status>up</physical-port-status>\n"
               "          <remote-slot>" + std::to_string(slotId_) + "</remote-slot>\n"
               "          <remote-ubpu>1</remote-ubpu>\n"
               "          <remote-iou>1</remote-iou>\n"
               "          <remote-physical-port-id>0</remote-physical-port-id>\n"
               "        </physical-port>\n"
               "      </physical-ports>\n"
               "    </node>\n";
    }

    xml += "  </nodes>\n</lingqu-topology>";
    return xml;
}

std::string MockLcneServer::GenerateTopologyCnaXml() const
{
    return "<lingqu-topology xmlns=\"urn:huawei:yang:huawei-lingqu-topology\">\n"
           "  <addresses>\n"
           "    <address>\n"
           "      <slot>" + std::to_string(slotId_) + "</slot>\n"
           "      <ubpu>1</ubpu>\n"
           "      <iou>1</iou>\n"
           "      <cna-id>0x0085a7</cna-id>\n"
           "      <cna-type>upi</cna-type>\n"
           "      <cna-status>normal</cna-status>\n"
           "      <bus-primary-cna>0x0003</bus-primary-cna>\n"
           "      <physical-ports>\n"
           "        <physical-port>\n"
           "          <physical-port-id>0</physical-port-id>\n"
           "          <bus-port-cna>0x0085a7</bus-port-cna>\n"
           "        </physical-port>\n"
           "      </physical-ports>\n"
           "    </address>\n"
           "  </addresses>\n"
           "</lingqu-topology>";
}

std::string MockLcneServer::GenerateUrmaEidXml() const
{
    return "<vbussw-service xmlns=\"urn:huawei:yang:huawei-vbussw-service\">\n"
           "  <static-urma-eids>\n"
           "    <static-urma-eid>\n"
           "      <slot-id>" + std::to_string(slotId_) + "</slot-id>\n"
           "      <ubpu-id>1</ubpu-id>\n"
           "      <iou-id>1</iou-id>\n"
           "      <entity-id>0</entity-id>\n"
           "      <urma-eid-infos>\n"
           "        <urma-eid-info>\n"
           "          <urma-eid>0000:0000:0000:0000:0000:0000:0000:0001</urma-eid>\n"
           "          <port-group-id>0</port-group-id>\n"
           "        </urma-eid-info>\n"
           "        <urma-eid-info>\n"
           "          <urma-eid>0000:0000:0000:0000:0000:0000:0000:0002</urma-eid>\n"
           "          <physical-port>400GUB8/1/4</physical-port>\n"
           "        </urma-eid-info>\n"
           "      </urma-eid-infos>\n"
           "    </static-urma-eid>\n"
           "  </static-urma-eids>\n"
           "</vbussw-service>";
}

std::string MockLcneServer::GenerateFeEidXml() const
{
    return "<vbussw-service xmlns=\"urn:huawei:yang:huawei-vbussw-service\">\n"
           "  <entity-urma-communication-infos>\n"
           "    <entity-urma-communication-info>\n"
           "      <slot-id>" + std::to_string(slotId_) + "</slot-id>\n"
           "      <ubpu-id>1</ubpu-id>\n"
           "      <iou-id>1</iou-id>\n"
           "      <urma-communication-entity-ids>\n"
           "        <urma-communication-entity-id>\n"
           "          <entity-id>1</entity-id>\n"
           "          <urma-communication-infos>\n"
           "            <urma-communication-info>\n"
           "              <urma-eid>0000:0000:0000:0000:0000:0000:0000:0003</urma-eid>\n"
           "              <port-group-id>0</port-group-id>\n"
           "            </urma-communication-info>\n"
           "            <urma-communication-info>\n"
           "              <urma-eid>0000:0000:0000:0000:0000:0000:0000:0004</urma-eid>\n"
           "              <interface-name>400GUB8/1/4</interface-name>\n"
           "            </urma-communication-info>\n"
           "          </urma-communication-infos>\n"
           "        </urma-communication-entity-id>\n"
           "      </urma-communication-entity-ids>\n"
           "    </entity-urma-communication-info>\n"
           "  </entity-urma-communication-infos>\n"
           "</vbussw-service>";
}

std::string MockLcneServer::GenerateFeBindingXml() const
{
    return "<vbussw-service xmlns=\"urn:huawei:yang:huawei-vbussw-service\">\n"
           "  <mue-ue-binding-infos>\n"
           "    <mue-ue-binding-info>\n"
           "      <slot-id>" + std::to_string(slotId_) + "</slot-id>\n"
           "      <ubpu-id>1</ubpu-id>\n"
           "      <iou-id>1</iou-id>\n"
           "      <mue-ue-bindings>\n"
           "        <mue-ue-binding>\n"
           "          <mue-id>1</mue-id>\n"
           "        </mue-ue-binding>\n"
           "      </mue-ue-bindings>\n"
           "    </mue-ue-binding-info>\n"
           "  </mue-ue-binding-infos>\n"
           "</vbussw-service>";
}

std::string MockLcneServer::GenerateAddDecoderResponseJson() const
{
    return "{\n"
           "  \"huawei-vbussw-service:ub-memory-decoder\": {\n"
           "    \"huawei-vbussw-service:output\": {\n"
           "      \"result\": \"success\",\n"
           "      \"hpa\": \"0\",\n"
           "      \"handle\": \"0\"\n"
           "    }\n"
           "  }\n"
           "}";
}

std::string MockLcneServer::GenerateDeleteDecoderResponseJson() const
{
    return "{\n"
           "  \"huawei-vbussw-service:ub-memory-decoder-delete\": {\n"
           "    \"huawei-vbussw-service:output\": {\n"
           "      \"result\": \"success\"\n"
           "    }\n"
           "  }\n"
           "}";
}

std::string MockLcneServer::GenerateInvalidateDecoderResponseJson() const
{
    return "{\n"
           "  \"huawei-vbussw-service:ub-memory-decoder-invalid\": {\n"
           "    \"huawei-vbussw-service:output\": {\n"
           "      \"result\": \"success\"\n"
           "    }\n"
           "  }\n"
           "}";
}

std::string MockLcneServer::GenerateDecoderHandleResponseJson() const
{
    return "{\n"
           "  \"huawei-vbussw-service:ub-memory-handle\": {\n"
           "    \"huawei-vbussw-service:output\": {\n"
           "      \"ub-memory-handles\": {}\n"
           "    }\n"
           "  }\n"
           "}";
}

void MockLcneServer::RegisterHandlers()
{
    server_.Get("/restconf/data/huawei-vbussw-inventory:vbussw-inventory/logic-entity-mappings",
                [this](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(GenerateBusInstanceXml(), "application/yang-data+xml");
                });

    server_.Get("/restconf/data/huawei-vbussw-service:vbussw-service/iou-infos",
                [this](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(GenerateIoDieInfoXml(), "application/yang-data+xml");
                });

    server_.Get("/restconf/data/huawei-vbussw-inventory:vbussw-inventory/logic-entities",
                [](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(HOST_INFO_XML, "application/yang-data+xml");
                });

    server_.Get("/restconf/data/huawei-lingqu-topology:lingqu-topology/nodes",
                [this](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(GenerateTopologyNodesXml(), "application/yang-data+xml");
                });

    server_.Post("/restconf/operations/notifications:create-subscription",
                 [](const httplib::Request&, httplib::Response& res) {
                     res.status = httplib::OK_200;
                     res.set_header("Content-Type", "application/yang-data+xml");
                     res.set_content(SUBSCRIPTION_RESPONSE_XML, "application/yang-data+xml");
                 });

    // Decoder entry operations (RPC-style POST endpoints)
    server_.Post("/restconf/operations/huawei-vbussw-service:ub-memory-decoder",
                 [this](const httplib::Request&, httplib::Response& res) {
                     res.status = httplib::OK_200;
                     res.set_header("Content-Type", "application/json");
                     res.set_content(GenerateAddDecoderResponseJson(), "application/json");
                 });

    server_.Post("/restconf/operations/huawei-vbussw-service:ub-memory-decoder-delete",
                 [this](const httplib::Request&, httplib::Response& res) {
                     res.status = httplib::OK_200;
                     res.set_header("Content-Type", "application/json");
                     res.set_content(GenerateDeleteDecoderResponseJson(), "application/json");
                 });

    server_.Post("/restconf/operations/huawei-vbussw-service:ub-memory-decoder-invalid",
                 [this](const httplib::Request&, httplib::Response& res) {
                     res.status = httplib::OK_200;
                     res.set_header("Content-Type", "application/json");
                     res.set_content(GenerateInvalidateDecoderResponseJson(), "application/json");
                 });

    server_.Post("/restconf/operations/huawei-vbussw-service:ub-memory-handle",
                 [this](const httplib::Request&, httplib::Response& res) {
                     res.status = httplib::OK_200;
                     res.set_header("Content-Type", "application/json");
                     res.set_content(GenerateDecoderHandleResponseJson(), "application/json");
                 });

    server_.Get("/restconf/data/huawei-lingqu-topology:lingqu-topology/addresses",
                [this](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(GenerateTopologyCnaXml(), "application/yang-data+xml");
                });

    server_.Get("/restconf/data/huawei-vbussw-service:vbussw-service/static-urma-eids",
                [this](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(GenerateUrmaEidXml(), "application/yang-data+xml");
                });

    server_.Get("/restconf/data/huawei-vbussw-service:vbussw-service/entity-urma-communication-infos",
                [this](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(GenerateFeEidXml(), "application/yang-data+xml");
                });

    server_.Get(R"(/restconf/data/huawei-vbussw-service:vbussw-service/entity-urma-communication-infos/.+)",
                [this](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(GenerateFeEidXml(), "application/yang-data+xml");
                });

    server_.Get("/restconf/data/huawei-vbussw-service:vbussw-service/mue-ue-binding-infos",
                [this](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(GenerateFeBindingXml(), "application/yang-data+xml");
                });

    server_.Get(R"(/restconf/data/huawei-vbussw-service:vbussw-service/mue-ue-binding-infos/.+)",
                [this](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(GenerateFeBindingXml(), "application/yang-data+xml");
                });
}

MockLcneServer::MockLcneServer(const std::string& udsPath, uint32_t slotId,
                               const std::vector<uint32_t>& clusterSlotIds)
    : udsPath_(udsPath), slotId_(slotId), clusterSlotIds_(clusterSlotIds), running_(false)
{
    RegisterHandlers();
}

UbseResult MockLcneServer::Start()
{
    if (running_) {
        IT_LOG_INFO << "MockLcneServer already running on " << udsPath_;
        return UBSE_OK;
    }

    std::filesystem::create_directories(std::filesystem::path(udsPath_).parent_path());
    unlink(udsPath_.c_str());
    server_.set_address_family(AF_UNIX);

    running_ = true;
    serverThread_ = std::thread([this]() {
        bool ret = server_.listen(udsPath_, 80);
        if (!ret) {
            IT_LOG_ERROR << "MockLcneServer failed to listen on " << udsPath_;
            running_ = false;
        }
        IT_LOG_INFO << "MockLcneServer stopped";
    });

    IT_LOG_INFO << "MockLcneServer started on " << udsPath_;
    return UBSE_OK;
}

UbseResult MockLcneServer::Stop()
{
    if (!running_) {
        IT_LOG_INFO << "MockLcneServer not running";
        return UBSE_OK;
    }

    running_ = false;
    server_.stop();
    if (serverThread_.joinable()) {
        serverThread_.join();
    }

    unlink(udsPath_.c_str());

    IT_LOG_INFO << "MockLcneServer stopped and socket unlinked";
    return UBSE_OK;
}

bool MockLcneServer::IsReady()
{
    struct stat st {};
    return stat(udsPath_.c_str(), &st) == 0;
}

UbseResult MockLcneServer::WaitForReady(uint32_t timeoutMs)
{
    constexpr uint32_t pollIntervalMs = 100;
    uint32_t elapsed = 0;
    while (elapsed < timeoutMs) {
        if (IsReady()) {
            IT_LOG_INFO << "MockLcneServer ready at " << udsPath_;
            return UBSE_OK;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
        elapsed += pollIntervalMs;
    }
    IT_LOG_ERROR << "MockLcneServer not ready after " << timeoutMs << "ms";
    return UBSE_ERROR_DEF(1);
}

} // namespace ubse::it::infra
