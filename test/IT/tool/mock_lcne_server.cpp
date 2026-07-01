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

#include <sys/stat.h>
#include <unistd.h>
#include <chrono>
#include <filesystem>
#include <map>

#include "it_console_log.h"

namespace ubse::it::infra {

namespace {

std::string FormatUrmaEid(uint16_t g3, uint16_t g4, uint16_t g7, uint16_t g8)
{
    char buf[48];
    snprintf(buf, sizeof(buf), "%04X:%04X:%04X:%04X:%04X:%04X:%04X:%04X", 0, 0, g3, g4, 0x0010, 0, g7, g8);
    return std::string(buf);
}

std::string BuildUrmaEidInfoXml(const std::string& urmaEid, const std::string& portKey, const std::string& portValue)
{
    return "                <urma-eid-info>\n"
           "                    <urma-eid>" +
           urmaEid +
           "</urma-eid>\n"
           "                    <" +
           portKey + ">" + portValue + "</" + portKey +
           ">\n"
           "                </urma-eid-info>\n";
}

std::string BuildStaticUrmaEidXml(int slotId, int ubpuId, int entityId, const std::string& labelTag,
                                  const std::string& labelValue, const std::string& eidInfosXml)
{
    std::string xml = "        <static-urma-eid>\n";
    xml += "            <slot-id>" + std::to_string(slotId) + "</slot-id>\n";
    xml += "            <ubpu-id>" + std::to_string(ubpuId) + "</ubpu-id>\n";
    xml += "            <iou-id>1</iou-id>\n";
    xml += "            <entity-id>" + std::to_string(entityId) + "</entity-id>\n";
    xml += "            <" + labelTag + ">" + labelValue + "</" + labelTag + ">\n";
    xml += "            <urma-eid-infos>\n";
    xml += eidInfosXml;
    xml += "            </urma-eid-infos>\n";
    xml += "        </static-urma-eid>\n";
    return xml;
}

std::string GenerateUrmaEidInfosXml(int slot, int ubpu, int entity)
{
    uint16_t ubpuOff3 = static_cast<uint16_t>((ubpu - 1) * 0x40);
    uint16_t g7 = static_cast<uint16_t>(0xDF00 + (ubpu - 1) * 0x04);
    uint16_t slotOff8 = static_cast<uint16_t>((slot - 1) * 0x4000);
    // slots 2-4, ubpu 2, entity 4: use ubpu-1 style g3/g7
    bool revertToUbpu1 = (ubpu == 2 && slot >= 2 && entity == 4);
    if (revertToUbpu1) {
        ubpuOff3 = 0;
        g7 = 0xDF00;
    }

    std::string infos;
    if (entity == 2) {
        for (int p = 1; p <= 7; ++p) {
            uint16_t g3 = static_cast<uint16_t>(ubpuOff3 + p);
            uint16_t g8 = static_cast<uint16_t>((p + 1) * 0x0100 + slotOff8);
            infos += BuildUrmaEidInfoXml(FormatUrmaEid(g3, 0x0200, g7, g8), "physical-port", std::to_string(p));
        }
        uint16_t g3 = static_cast<uint16_t>(ubpuOff3 + 0x3F);
        uint16_t g8 = static_cast<uint16_t>(0x0B00 + slotOff8);
        infos += BuildUrmaEidInfoXml(FormatUrmaEid(g3, 0x0200, g7, g8), "port-group-id", "1");
    } else if (entity == 3) {
        uint16_t g8 = static_cast<uint16_t>(0x0100 + slotOff8);
        infos += BuildUrmaEidInfoXml(FormatUrmaEid(ubpuOff3, 0x0300, g7, g8), "physical-port", "0");
    } else if (entity == 4) {
        uint16_t g8_0 = static_cast<uint16_t>(0x0100 + slotOff8);
        uint16_t g8_8 = static_cast<uint16_t>(0x0900 + slotOff8);
        infos += BuildUrmaEidInfoXml(FormatUrmaEid(ubpuOff3, 0x0400, g7, g8_0), "physical-port", "0");
        infos += BuildUrmaEidInfoXml(FormatUrmaEid(static_cast<uint16_t>(ubpuOff3 + 8), 0x0400, g7, g8_8),
                                     "physical-port", "8");
    }
    return infos;
}

} // namespace

static const std::string SUBSCRIPTION_RESPONSE_XML = "<restconf>\n"
                                                     "  <rpc_reply xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">\n"
                                                     "    <result>ok</result>\n"
                                                     "  </rpc_reply>\n"
                                                     "</restconf>";

std::string MockLcneServer::GenerateBusInstanceXml() const
{
    uint32_t eid = 0x10000 + (slotId_ - 1) * 0x40 + 0x0A;
    char eidStr[16];
    snprintf(eidStr, sizeof(eidStr), "0x%05X", eid);
    return "<vbussw-inventory xmlns=\"urn:huawei:yang:huawei-vbussw-inventory\">\n"
           "  <logic-entity-mappings>\n"
           "    <logic-entity-mapping>\n"
           "      <host-bus-instance-eid>" +
           std::string(eidStr) +
           "</host-bus-instance-eid>\n"
           "      <physical-entity-mappings>\n"
           "        <physical-entity-mapping>\n"
           "          <slot-id>" +
           std::to_string(slotId_) +
           "</slot-id>\n"
           "          <chip-id>1</chip-id>\n"
           "          <die-id>1</die-id>\n"
           "        </physical-entity-mapping>\n"
           "        <physical-entity-mapping>\n"
           "          <slot-id>" +
           std::to_string(slotId_) +
           "</slot-id>\n"
           "          <chip-id>2</chip-id>\n"
           "          <die-id>1</die-id>\n"
           "        </physical-entity-mapping>\n"
           "      </physical-entity-mappings>\n"
           "    </logic-entity-mapping>\n"
           "  </logic-entity-mappings>\n"
           "</vbussw-inventory>";
}

std::string MockLcneServer::GenerateIoDieInfoXml() const
{
    std::string xml = "<vbussw-service xmlns=\"urn:huawei:yang:huawei-vbussw-service\">\n  <iou-infos>\n";
    for (int ubpu = 1; ubpu <= 2; ++ubpu) {
        uint32_t ctrlEid = (ubpu == 1 ? 0x40000 : 0x40400) + (slotId_ - 1) * 0x40 + 0x0A;
        uint32_t upi = 0x7C00 + (slotId_ - 1) * 0x20 + 0x1F;
        uint32_t primaryCna = (ubpu == 1 ? 0x0000 : 0x0400) + (slotId_ - 1) * 0x40 + 0x0A;
        const char* guid = (ubpu == 1) ? "cc08-a000-0-2-000000-0000000000000000" :
                                         "cc08-a000-0-2-000000-0000000000000100";
        char ctrlEidStr[16];
        char upiStr[16];
        char cnaStr[16];
        snprintf(ctrlEidStr, sizeof(ctrlEidStr), "0x%05X", ctrlEid);
        snprintf(upiStr, sizeof(upiStr), "0x%04X", upi);
        snprintf(cnaStr, sizeof(cnaStr), "0x%04X", primaryCna);
        xml += "    <iou-info>\n";
        xml += "      <slot-id>" + std::to_string(slotId_) + "</slot-id>\n";
        xml += "      <ubpu-id>" + std::to_string(ubpu) + "</ubpu-id>\n";
        xml += "      <iou-id>1</iou-id>\n";
        xml += "      <bus-controller-eid>" + std::string(ctrlEidStr) + "</bus-controller-eid>\n";
        xml += "      <guid>" + std::string(guid) + "</guid>\n";
        xml += "      <upi>" + std::string(upiStr) + "</upi>\n";
        xml += "      <primary-cna>" + std::string(cnaStr) + "</primary-cna>\n";
        xml += "      <ubpu-type>CPU</ubpu-type>\n";
        xml += "      <iou-status>normal</iou-status>\n";
        xml += "    </iou-info>\n";
    }
    xml += "  </iou-infos>\n</vbussw-service>";
    return xml;
}

std::string MockLcneServer::GenerateHostInfoXml() const
{
    uint32_t eid = 0x10000 + (slotId_ - 1) * 0x40 + 0x0A;
    uint32_t upi = 0x7C00 + (slotId_ - 1) * 0x20 + 0x1F;
    char eidStr[16];
    char upiStr[16];
    snprintf(eidStr, sizeof(eidStr), "0x%05X", eid);
    snprintf(upiStr, sizeof(upiStr), "0x%04X", upi);
    return "<vbussw-inventory xmlns=\"urn:huawei:yang:huawei-vbussw-inventory\">\n"
           "  <logic-entities>\n"
           "    <logic-entity>\n"
           "      <bus-instance-eid>" +
           std::string(eidStr) +
           "</bus-instance-eid>\n"
           "      <guid>cc08-a000-0-0-000000-0000000800000100</guid>\n"
           "      <type>host</type>\n"
           "      <upi>" +
           std::string(upiStr) +
           "</upi>\n"
           "      <state>online</state>\n"
           "      <try-times>0</try-times>\n"
           "    </logic-entity>\n"
           "  </logic-entities>\n"
           "</vbussw-inventory>";
}

std::string MockLcneServer::GenerateTopologyNodesXml() const
{
    // 真实环境数据规律: 每节点2个ubpu, 各9个physical-port
    // 端口状态: port 0/2/3/6/7/8=down, port 1/4/5=up
    // 对称连接: 同一条链路两端使用相同端口号
    // 连接映射表 (portId -> remoteSlotId):
    //   slot1: port1->2, port4->3, port5->4
    //   slot2: port1->1, port4->4, port5->3
    //   slot3: port1->4, port4->1, port5->2
    //   slot4: port1->3, port4->2, port5->1
    static const std::map<uint32_t, std::map<int, uint32_t>> upPortMap = {{1, {{1, 2}, {4, 3}, {5, 4}}},
                                                                          {2, {{1, 1}, {4, 4}, {5, 3}}},
                                                                          {3, {{1, 4}, {4, 1}, {5, 2}}},
                                                                          {4, {{1, 3}, {4, 2}, {5, 1}}}};

    std::string xml = "<lingqu-topology xmlns=\"urn:huawei:yang:huawei-lingqu-topology\">\n  <nodes>\n";
    for (int ubpu = 1; ubpu <= 2; ++ubpu) {
        xml += "    <node>\n";
        xml += "      <slot>" + std::to_string(slotId_) + "</slot>\n";
        xml += "      <ubpu>" + std::to_string(ubpu) + "</ubpu>\n";
        xml += "      <iou>1</iou>\n";
        xml += "      <ubpu-type>CPU-LINK</ubpu-type>\n";
        xml += "      <physical-ports>\n";
        for (int portId = 0; portId < 9; ++portId) {
            xml += "        <physical-port>\n";
            xml += "          <physical-port-id>" + std::to_string(portId) + "</physical-port-id>\n";
            xml += "          <interface-name>400GUB" + std::to_string(slotId_) + "/" + std::to_string(ubpu) + "/" +
                   std::to_string(portId + 1) + "</interface-name>\n";
            xml += "          <physical-port-role>internal-port</physical-port-role>\n";

            auto slotIt = upPortMap.find(slotId_);
            auto portIt = (slotIt != upPortMap.end()) ? slotIt->second.find(portId) : slotIt->second.end();
            if (portIt != slotIt->second.end()) {
                xml += "          <physical-port-status>up</physical-port-status>\n";
                xml += "          <remote-slot>" + std::to_string(portIt->second) + "</remote-slot>\n";
                xml += "          <remote-ubpu>" + std::to_string(ubpu) + "</remote-ubpu>\n";
                xml += "          <remote-iou>1</remote-iou>\n";
                xml += "          <remote-physical-port-id>" + std::to_string(portId) + "</remote-physical-port-id>\n";
            } else {
                xml += "          <physical-port-status>down</physical-port-status>\n";
                xml += "          <remote-slot>-</remote-slot>\n";
                xml += "          <remote-ubpu>-</remote-ubpu>\n";
                xml += "          <remote-iou>-</remote-iou>\n";
                xml += "          <remote-physical-port-id>-</remote-physical-port-id>\n";
            }
            xml += "        </physical-port>\n";
        }
        xml += "      </physical-ports>\n";
        xml += "    </node>\n";
    }
    xml += "  </nodes>\n</lingqu-topology>";
    return xml;
}

std::string MockLcneServer::GenerateTopologyCnaXml() const
{
    std::string xml = "<lingqu-topology xmlns=\"urn:huawei:yang:huawei-lingqu-topology\">\n  <addresses>\n";
    for (int ubpu = 1; ubpu <= 2; ++ubpu) {
        uint32_t baseCna = (ubpu == 1 ? 0x0000 : 0x0400) + (slotId_ - 1) * 0x40;
        uint32_t primaryCna = baseCna + 0x0A;
        uint32_t nodeCna = (ubpu == 1 ? 0xDF0000 : 0xDF0400) + (slotId_ - 1) * 0x40 + 0x0A;
        char primaryCnaStr[16];
        char nodeCnaStr[16];
        snprintf(primaryCnaStr, sizeof(primaryCnaStr), "0x%04x", primaryCna);
        snprintf(nodeCnaStr, sizeof(nodeCnaStr), "0x%06x", nodeCna);
        xml += "    <address>\n";
        xml += "      <slot>" + std::to_string(slotId_) + "</slot>\n";
        xml += "      <ubpu>" + std::to_string(ubpu) + "</ubpu>\n";
        xml += "      <iou>1</iou>\n";
        xml += "      <bus-primary-cna>" + std::string(primaryCnaStr) + "</bus-primary-cna>\n";
        xml += "      <node-cna>" + std::string(nodeCnaStr) + "</node-cna>\n";
        xml += "      <node-ip>-</node-ip>\n";
        xml += "      <physical-ports>\n";
        for (int port = 0; port < 9; ++port) {
            uint32_t busPortCna = baseCna + port + 1;
            uint32_t portCna = (ubpu == 1 ? 0xDF0000 : 0xDF0400) + (slotId_ - 1) * 0x40 + port + 1;
            char busPortCnaStr[16];
            char portCnaStr[16];
            snprintf(busPortCnaStr, sizeof(busPortCnaStr), "0x%04x", busPortCna);
            snprintf(portCnaStr, sizeof(portCnaStr), "0x%06x", portCna);
            xml += "        <physical-port>\n";
            xml += "          <physical-port-id>" + std::to_string(port) + "</physical-port-id>\n";
            xml += "          <interface-name>400GUB" + std::to_string(slotId_) + "/" + std::to_string(ubpu) + "/" +
                   std::to_string(port + 1) + "</interface-name>\n";
            xml += "          <bus-port-cna>" + std::string(busPortCnaStr) + "</bus-port-cna>\n";
            xml += "          <port-cna>" + std::string(portCnaStr) + "</port-cna>\n";
            xml += "          <port-ip>-</port-ip>\n";
            xml += "        </physical-port>\n";
        }
        xml += "      </physical-ports>\n";
        xml += "    </address>\n";
    }
    xml += "  </addresses>\n</lingqu-topology>";
    return xml;
}

std::string MockLcneServer::GenerateUrmaEidXml() const
{
    std::string xml = "<vbussw-service xmlns=\"urn:huawei:yang:huawei-vbussw-service\">\n    <static-urma-eids>\n";
    for (int slot = 1; slot <= 4; ++slot) {
        for (int ubpu = 1; ubpu <= 2; ++ubpu) {
            xml += BuildStaticUrmaEidXml(slot, ubpu, 2, "lable", "host-urma-entity",
                                         GenerateUrmaEidInfosXml(slot, ubpu, 2));
            xml += BuildStaticUrmaEidXml(slot, ubpu, 3, "label", "other", GenerateUrmaEidInfosXml(slot, ubpu, 3));
            xml += BuildStaticUrmaEidXml(slot, ubpu, 4, "label", "other", GenerateUrmaEidInfosXml(slot, ubpu, 4));
        }
    }
    xml += "    </static-urma-eids>\n</vbussw-service>";
    return xml;
}

std::string MockLcneServer::GenerateFeEidXml() const
{
    return "<vbussw-service xmlns=\"urn:huawei:yang:huawei-vbussw-service\">\n"
           "  <entity-urma-communication-infos>\n"
           "    <entity-urma-communication-info>\n"
           "      <slot-id>" +
           std::to_string(slotId_) +
           "</slot-id>\n"
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
           "      <slot-id>" +
           std::to_string(slotId_) +
           "</slot-id>\n"
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
                [this](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(GenerateHostInfoXml(), "application/yang-data+xml");
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

MockLcneServer::MockLcneServer(const std::string& udsPath, uint32_t slotId, const std::vector<uint32_t>& clusterSlotIds)
    : udsPath_(udsPath),
      slotId_(slotId),
      clusterSlotIds_(clusterSlotIds),
      running_(false)
{
    RegisterHandlers();
}

MockLcneServer::~MockLcneServer()
{
    Stop();
}

UbseResult MockLcneServer::Start()
{
    if (running_) {
        IT_LOG_INFO << "MockLcneServer already running on " << udsPath_;
        return UBSE_OK;
    }
    if (serverThread_.joinable()) {
        serverThread_.join();
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
    if (!running_ && !serverThread_.joinable()) {
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
    struct stat st {
    };
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
