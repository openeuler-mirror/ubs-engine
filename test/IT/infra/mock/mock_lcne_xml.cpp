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

#include "mock_lcne_xml.h"

#include <map>

namespace ubse::it::infra::lcne_xml {

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

std::string BusInstanceXml(uint32_t slotId)
{
    uint32_t eid = 0x10000 + (slotId - 1) * 0x40 + 0x0A;
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
           std::to_string(slotId) +
           "</slot-id>\n"
           "          <chip-id>1</chip-id>\n"
           "          <die-id>1</die-id>\n"
           "        </physical-entity-mapping>\n"
           "        <physical-entity-mapping>\n"
           "          <slot-id>" +
           std::to_string(slotId) +
           "</slot-id>\n"
           "          <chip-id>2</chip-id>\n"
           "          <die-id>1</die-id>\n"
           "        </physical-entity-mapping>\n"
           "      </physical-entity-mappings>\n"
           "    </logic-entity-mapping>\n"
           "  </logic-entity-mappings>\n"
           "</vbussw-inventory>";
}

std::string IoDieInfoXml(uint32_t slotId)
{
    std::string xml = "<vbussw-service xmlns=\"urn:huawei:yang:huawei-vbussw-service\">\n  <iou-infos>\n";
    for (int ubpu = 1; ubpu <= 2; ++ubpu) {
        uint32_t ctrlEid = (ubpu == 1 ? 0x40000 : 0x40400) + (slotId - 1) * 0x40 + 0x0A;
        uint32_t upi = 0x7C00 + (slotId - 1) * 0x20 + 0x1F;
        uint32_t primaryCna = (ubpu == 1 ? 0x0000 : 0x0400) + (slotId - 1) * 0x40 + 0x0A;
        const char* guid = (ubpu == 1) ? "cc08-a000-0-2-000000-0000000000000000" :
                                         "cc08-a000-0-2-000000-0000000000000100";
        char ctrlEidStr[16];
        char upiStr[16];
        char cnaStr[16];
        snprintf(ctrlEidStr, sizeof(ctrlEidStr), "0x%05X", ctrlEid);
        snprintf(upiStr, sizeof(upiStr), "0x%04X", upi);
        snprintf(cnaStr, sizeof(cnaStr), "0x%04X", primaryCna);
        xml += "    <iou-info>\n";
        xml += "      <slot-id>" + std::to_string(slotId) + "</slot-id>\n";
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

std::string HostInfoXml(uint32_t slotId)
{
    uint32_t eid = 0x10000 + (slotId - 1) * 0x40 + 0x0A;
    uint32_t upi = 0x7C00 + (slotId - 1) * 0x20 + 0x1F;
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

std::string TopologyNodesXml(uint32_t slotId, const std::vector<uint32_t>& /*clusterSlotIds*/)
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
        xml += "      <slot>" + std::to_string(slotId) + "</slot>\n";
        xml += "      <ubpu>" + std::to_string(ubpu) + "</ubpu>\n";
        xml += "      <iou>1</iou>\n";
        xml += "      <ubpu-type>CPU-LINK</ubpu-type>\n";
        xml += "      <physical-ports>\n";
        for (int portId = 0; portId < 9; ++portId) {
            xml += "        <physical-port>\n";
            xml += "          <physical-port-id>" + std::to_string(portId) + "</physical-port-id>\n";
            xml += "          <interface-name>400GUB" + std::to_string(slotId) + "/" + std::to_string(ubpu) + "/" +
                   std::to_string(portId + 1) + "</interface-name>\n";
            xml += "          <physical-port-role>internal-port</physical-port-role>\n";

            auto slotIt = upPortMap.find(slotId);
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

std::string TopologyCnaXml(uint32_t slotId)
{
    std::string xml = "<lingqu-topology xmlns=\"urn:huawei:yang:huawei-lingqu-topology\">\n  <addresses>\n";
    for (int ubpu = 1; ubpu <= 2; ++ubpu) {
        uint32_t baseCna = (ubpu == 1 ? 0x0000 : 0x0400) + (slotId - 1) * 0x40;
        uint32_t primaryCna = baseCna + 0x0A;
        uint32_t nodeCna = (ubpu == 1 ? 0xDF0000 : 0xDF0400) + (slotId - 1) * 0x40 + 0x0A;
        char primaryCnaStr[16];
        char nodeCnaStr[16];
        snprintf(primaryCnaStr, sizeof(primaryCnaStr), "0x%04x", primaryCna);
        snprintf(nodeCnaStr, sizeof(nodeCnaStr), "0x%06x", nodeCna);
        xml += "    <address>\n";
        xml += "      <slot>" + std::to_string(slotId) + "</slot>\n";
        xml += "      <ubpu>" + std::to_string(ubpu) + "</ubpu>\n";
        xml += "      <iou>1</iou>\n";
        xml += "      <bus-primary-cna>" + std::string(primaryCnaStr) + "</bus-primary-cna>\n";
        xml += "      <node-cna>" + std::string(nodeCnaStr) + "</node-cna>\n";
        xml += "      <node-ip>-</node-ip>\n";
        xml += "      <physical-ports>\n";
        for (int port = 0; port < 9; ++port) {
            uint32_t busPortCna = baseCna + port + 1;
            uint32_t portCna = (ubpu == 1 ? 0xDF0000 : 0xDF0400) + (slotId - 1) * 0x40 + port + 1;
            char busPortCnaStr[16];
            char portCnaStr[16];
            snprintf(busPortCnaStr, sizeof(busPortCnaStr), "0x%04x", busPortCna);
            snprintf(portCnaStr, sizeof(portCnaStr), "0x%06x", portCna);
            xml += "        <physical-port>\n";
            xml += "          <physical-port-id>" + std::to_string(port) + "</physical-port-id>\n";
            xml += "          <interface-name>400GUB" + std::to_string(slotId) + "/" + std::to_string(ubpu) + "/" +
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

std::string UrmaEidXml()
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

std::string FeEidXml(uint32_t slotId)
{
    return "<vbussw-service xmlns=\"urn:huawei:yang:huawei-vbussw-service\">\n"
           "  <entity-urma-communication-infos>\n"
           "    <entity-urma-communication-info>\n"
           "      <slot-id>" +
           std::to_string(slotId) +
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

std::string FeBindingXml(uint32_t slotId)
{
    return "<vbussw-service xmlns=\"urn:huawei:yang:huawei-vbussw-service\">\n"
           "  <mue-ue-binding-infos>\n"
           "    <mue-ue-binding-info>\n"
           "      <slot-id>" +
           std::to_string(slotId) +
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

// ============================================================
// CLOS-specific XML generators
// CLOS uses nodeId(=serverIdx+1) for all EID/CNA calculations.
// offset = 0x80 * (nodeId - 1) per node.
// slot-id is always 1 in CLOS (slotId=255 from SMBIOS is unused).
// ============================================================

namespace {
constexpr uint32_t CLOS_EID_BASE = 0x10009;       // ①③ host-bus-instance-eid base
constexpr uint32_t CLOS_CTRL_EID_UBPU1 = 0x40009; // ② ubpu1 bus-controller-eid base
constexpr uint32_t CLOS_CTRL_EID_UBPU2 = 0x40049; // ② ubpu2 bus-controller-eid base
constexpr uint32_t CLOS_CNA_UBPU1 = 0x0009;       // ⑤ ubpu1 bus-primary-cna base
constexpr uint32_t CLOS_CNA_UBPU2 = 0x0049;       // ⑤ ubpu2 bus-primary-cna base
constexpr uint32_t CLOS_CNA_PORT_UBPU1 = 0x0001;  // ⑤ ubpu1 bus-port-cna base
constexpr uint32_t CLOS_CNA_PORT_UBPU2 = 0x0041;  // ⑤ ubpu2 bus-port-cna base
constexpr uint32_t CLOS_STEP = 0x80;              // per-node step

std::string Hex5(uint32_t v)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%05X", v);
    return std::string(buf);
}
std::string Hex4(uint32_t v)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%04X", v);
    return std::string(buf);
}
uint32_t ClosOffset(uint32_t nodeId)
{
    return CLOS_STEP * (nodeId - 1);
}
} // namespace

std::string ClosBusInstanceXml(uint32_t nodeId)
{
    uint32_t eid = CLOS_EID_BASE + ClosOffset(nodeId);
    return "<vbussw-inventory xmlns=\"urn:huawei:yang:huawei-vbussw-inventory\">\n"
           "  <logic-entity-mappings>\n"
           "    <logic-entity-mapping>\n"
           "      <host-bus-instance-eid>" +
           Hex5(eid) +
           "</host-bus-instance-eid>\n"
           "      <physical-entity-mappings>\n"
           "        <physical-entity-mapping>\n"
           "          <slot-id>1</slot-id>\n"
           "        </physical-entity-mapping>\n"
           "      </physical-entity-mappings>\n"
           "    </logic-entity-mapping>\n"
           "  </logic-entity-mappings>\n"
           "</vbussw-inventory>";
}

std::string ClosIoDieInfoXml(uint32_t nodeId)
{
    uint32_t offset = ClosOffset(nodeId);
    std::string xml = "<vbussw-service xmlns=\"urn:huawei:yang:huawei-vbussw-service\">\n  <iou-infos>\n";
    for (int ubpu = 1; ubpu <= 2; ++ubpu) {
        uint32_t ctrlEid = (ubpu == 1 ? CLOS_CTRL_EID_UBPU1 : CLOS_CTRL_EID_UBPU2) + offset;
        uint32_t primaryCna = (ubpu == 1 ? CLOS_CNA_UBPU1 : CLOS_CNA_UBPU2) + offset;
        const char* guid = (ubpu == 1) ? "CC08-A000-0-2-000000-0000000000000000"
                                        : "CC08-A000-0-2-000000-0000000000000100";
        xml += "    <iou-info>\n";
        xml += "      <slot-id>1</slot-id>\n";
        xml += "      <ubpu-id>" + std::to_string(ubpu) + "</ubpu-id>\n";
        xml += "      <iou-id>1</iou-id>\n";
        xml += "      <bus-controller-eid>" + Hex5(ctrlEid) + "</bus-controller-eid>\n";
        xml += "      <guid>" + std::string(guid) + "</guid>\n";
        xml += "      <upi>0x7FFF</upi>\n";
        xml += "      <primary-cna>" + Hex4(primaryCna) + "</primary-cna>\n";
        xml += "      <ubpu-type>CPU</ubpu-type>\n";
        xml += "      <iou-status>normal</iou-status>\n";
        xml += "    </iou-info>\n";
    }
    xml += "  </iou-infos>\n</vbussw-service>";
    return xml;
}

std::string ClosHostInfoXml(uint32_t nodeId)
{
    uint32_t eid = CLOS_EID_BASE + ClosOffset(nodeId);
    return "<vbussw-inventory xmlns=\"urn:huawei:yang:huawei-vbussw-inventory\">\n"
           "  <logic-entities>\n"
           "    <logic-entity>\n"
           "      <bus-instance-eid>" +
           Hex5(eid) +
           "</bus-instance-eid>\n"
           "      <guid>CC08-A000-0-0-000000-0000000800000100</guid>\n"
           "      <type>host</type>\n"
           "      <upi>0x7FFF</upi>\n"
           "      <state>online</state>\n"
           "    </logic-entity>\n"
           "  </logic-entities>\n"
           "</vbussw-inventory>";
}

std::string ClosTopologyNodesXml()
{
    // CLOS: 2 ubpu × 8 ports (0-7), port 4 UP, all remote-* = "-"
    // Switch-forwarded topology: no direct peer node info.
    std::string xml = "<lingqu-topology xmlns=\"urn:huawei:yang:huawei-lingqu-topology\">\n  <nodes>\n";
    for (int ubpu = 1; ubpu <= 2; ++ubpu) {
        xml += "    <node>\n";
        xml += "      <slot>1</slot>\n";
        xml += "      <ubpu>" + std::to_string(ubpu) + "</ubpu>\n";
        xml += "      <iou>1</iou>\n";
        xml += "      <ubpu-type>CPU-LINK</ubpu-type>\n";
        xml += "      <physical-ports>\n";
        for (int portId = 0; portId < 8; ++portId) {
            xml += "        <physical-port>\n";
            xml += "          <physical-port-id>" + std::to_string(portId) + "</physical-port-id>\n";
            xml += "          <interface-name>400GUB1/" + std::to_string(ubpu) + "/" +
                   std::to_string(portId + 1) + "</interface-name>\n";
            xml += "          <physical-port-role>internal-port</physical-port-role>\n";
            if (portId == 4) {
                xml += "          <physical-port-status>up</physical-port-status>\n";
            } else {
                xml += "          <physical-port-status>down</physical-port-status>\n";
            }
            xml += "          <remote-slot>-</remote-slot>\n";
            xml += "          <remote-ubpu>-</remote-ubpu>\n";
            xml += "          <remote-iou>-</remote-iou>\n";
            xml += "          <remote-physical-port-id>-</remote-physical-port-id>\n";
            xml += "        </physical-port>\n";
        }
        xml += "      </physical-ports>\n";
        xml += "    </node>\n";
    }
    xml += "  </nodes>\n</lingqu-topology>";
    return xml;
}

std::string ClosTopologyCnaXml(uint32_t nodeId)
{
    uint32_t offset = ClosOffset(nodeId);
    std::string xml = "<lingqu-topology xmlns=\"urn:huawei:yang:huawei-lingqu-topology\">\n  <addresses>\n";
    for (int ubpu = 1; ubpu <= 2; ++ubpu) {
        uint32_t primaryCna = (ubpu == 1 ? CLOS_CNA_UBPU1 : CLOS_CNA_UBPU2) + offset;
        uint32_t portBase = (ubpu == 1 ? CLOS_CNA_PORT_UBPU1 : CLOS_CNA_PORT_UBPU2) + offset;
        xml += "    <address>\n";
        xml += "      <slot>1</slot>\n";
        xml += "      <ubpu>" + std::to_string(ubpu) + "</ubpu>\n";
        xml += "      <iou>1</iou>\n";
        xml += "      <bus-primary-cna>" + Hex4(primaryCna) + "</bus-primary-cna>\n";
        xml += "      <physical-ports>\n";
        for (int port = 0; port < 8; ++port) {
            uint32_t busPortCna = portBase + port;
            xml += "        <physical-port>\n";
            xml += "          <physical-port-id>" + std::to_string(port) + "</physical-port-id>\n";
            xml += "          <interface-name>400GUB1/" + std::to_string(ubpu) + "/" +
                   std::to_string(port + 1) + "</interface-name>\n";
            xml += "          <bus-port-cna>" + Hex4(busPortCna) + "</bus-port-cna>\n";
            xml += "        </physical-port>\n";
        }
        xml += "      </physical-ports>\n";
        xml += "    </address>\n";
    }
    xml += "  </addresses>\n</lingqu-topology>";
    return xml;
}

std::string ClosFeEidXml(uint32_t nodeId)
{
    // CLOS ⑦: entity-urma-communication-info for slot=1,ubpu=1,iou=1 (key-based query)
    // entity-id=2, multiple urma-communication-info entries.
    // ubse parses: urma-eid + (port-group-id | interface-name)
    // Generates representative entries matching real CLOS data pattern.
    uint32_t offset = ClosOffset(nodeId);
    std::string xml = "<vbussw-service xmlns=\"urn:huawei:yang:huawei-vbussw-service\">\n";
    xml += "  <entity-urma-communication-infos>\n";
    xml += "    <entity-urma-communication-info>\n";
    xml += "      <slot-id>1</slot-id>\n";
    xml += "      <ubpu-id>1</ubpu-id>\n";
    xml += "      <iou-id>1</iou-id>\n";
    xml += "      <urma-communication-entity-ids>\n";
    xml += "        <urma-communication-entity-id>\n";
    xml += "          <entity-id>2</entity-id>\n";
    xml += "          <urma-communication-infos>\n";
    // interface-name entries (port 5 = interface 400GUB1/1/5, up port)
    char eidBuf[64];
    // urma-eid format: 0000:0000:GG3:0200:0010:0000:GG7:GG8
    // g3=0x0004, g7=0x1400, g8=0x0500+offset for interface 400GUB1/1/5
    snprintf(eidBuf, sizeof(eidBuf), "0000:0000:0004:0200:0010:0000:1400:%04X", 0x0500 + offset);
    xml += "            <urma-communication-info>\n";
    xml += "              <urma-eid>" + std::string(eidBuf) + "</urma-eid>\n";
    xml += "              <interface-name>400GUB1/1/5</interface-name>\n";
    xml += "            </urma-communication-info>\n";
    // port-group-id entry: g3=0x003F, g7=0x1000, g8=0x0A80+offset
    snprintf(eidBuf, sizeof(eidBuf), "0000:0000:003F:0200:0010:0000:1000:%04X", 0x0A80 + offset);
    xml += "            <urma-communication-info>\n";
    xml += "              <urma-eid>" + std::string(eidBuf) + "</urma-eid>\n";
    xml += "              <port-group-id>1</port-group-id>\n";
    xml += "            </urma-communication-info>\n";
    xml += "          </urma-communication-infos>\n";
    xml += "        </urma-communication-entity-id>\n";
    xml += "      </urma-communication-entity-ids>\n";
    xml += "    </entity-urma-communication-info>\n";
    xml += "  </entity-urma-communication-infos>\n";
    xml += "</vbussw-service>";
    return xml;
}

std::string ClosFeBindingXml()
{
    // CLOS ⑧: mue-ue-binding-info for slot=1,ubpu=1,iou=1
    // mue-id 2..9 (all PHYSICAL_TYPE), same for all nodes.
    std::string xml = "<vbussw-service xmlns=\"urn:huawei:yang:huawei-vbussw-service\">\n";
    xml += "  <mue-ue-binding-infos>\n";
    xml += "    <mue-ue-binding-info>\n";
    xml += "      <slot-id>1</slot-id>\n";
    xml += "      <ubpu-id>1</ubpu-id>\n";
    xml += "      <iou-id>1</iou-id>\n";
    xml += "      <mue-ue-bindings>\n";
    for (int mueId = 2; mueId <= 9; ++mueId) {
        xml += "        <mue-ue-binding><mue-id>" + std::to_string(mueId) + "</mue-id></mue-ue-binding>\n";
    }
    xml += "      </mue-ue-bindings>\n";
    xml += "    </mue-ue-binding-info>\n";
    xml += "  </mue-ue-binding-infos>\n";
    xml += "</vbussw-service>";
    return xml;
}

std::string SubscriptionResponseXml()
{
    return "<restconf>\n"
           "  <rpc_reply xmlns=\"urn:ietf:params:xml:ns:netconf:base:1.0\">\n"
           "    <result>ok</result>\n"
           "  </rpc_reply>\n"
           "</restconf>";
}

std::string AddDecoderResponseJson()
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

std::string DeleteDecoderResponseJson()
{
    return "{\n"
           "  \"huawei-vbussw-service:ub-memory-decoder-delete\": {\n"
           "    \"huawei-vbussw-service:output\": {\n"
           "      \"result\": \"success\"\n"
           "    }\n"
           "  }\n"
           "}";
}

std::string InvalidateDecoderResponseJson()
{
    return "{\n"
           "  \"huawei-vbussw-service:ub-memory-decoder-invalid\": {\n"
           "    \"huawei-vbussw-service:output\": {\n"
           "      \"result\": \"success\"\n"
           "    }\n"
           "  }\n"
           "}";
}

std::string DecoderHandleResponseJson()
{
    return "{\n"
           "  \"huawei-vbussw-service:ub-memory-handle\": {\n"
           "    \"huawei-vbussw-service:output\": {\n"
           "      \"ub-memory-handles\": {}\n"
           "    }\n"
           "  }\n"
           "}";
}

} // namespace ubse::it::infra::lcne_xml
