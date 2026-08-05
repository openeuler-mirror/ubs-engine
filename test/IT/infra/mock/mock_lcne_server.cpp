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

#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <chrono>
#include <filesystem>

#include "it_console_log.h"
#include "mock_lcne_xml.h"

namespace ubse::it::infra {

MockLcneServer::MockLcneServer(const std::string& udsPath, uint32_t slotId,
                               const std::vector<uint32_t>& clusterSlotIds, bool isClos, uint32_t nodeId)
    : udsPath_(udsPath),
      slotId_(slotId),
      nodeId_(isClos ? nodeId : slotId),
      isClos_(isClos),
      clusterSlotIds_(clusterSlotIds),
      running_(false)
{
    RegisterHandlers();
}

MockLcneServer::~MockLcneServer()
{
    Stop();
}

void MockLcneServer::RegisterHandlers()
{
    if (isClos_) {
        RegisterClosHandlers();
    } else {
        RegisterFullMeshHandlers();
    }

    // Common handlers shared by both modes
    server_.Post("/restconf/operations/notifications:create-subscription",
                 [](const httplib::Request&, httplib::Response& res) {
                     res.status = httplib::OK_200;
                     res.set_header("Content-Type", "application/yang-data+xml");
                     res.set_content(lcne_xml::SubscriptionResponseXml(), "application/yang-data+xml");
                 });

    server_.Post("/restconf/operations/huawei-vbussw-service:ub-memory-decoder",
                 [](const httplib::Request&, httplib::Response& res) {
                     res.status = httplib::OK_200;
                     res.set_header("Content-Type", "application/json");
                     res.set_content(lcne_xml::AddDecoderResponseJson(), "application/json");
                 });

    server_.Post("/restconf/operations/huawei-vbussw-service:ub-memory-decoder-delete",
                 [](const httplib::Request&, httplib::Response& res) {
                     res.status = httplib::OK_200;
                     res.set_header("Content-Type", "application/json");
                     res.set_content(lcne_xml::DeleteDecoderResponseJson(), "application/json");
                 });

    server_.Post("/restconf/operations/huawei-vbussw-service:ub-memory-decoder-invalid",
                 [](const httplib::Request&, httplib::Response& res) {
                     res.status = httplib::OK_200;
                     res.set_header("Content-Type", "application/json");
                     res.set_content(lcne_xml::InvalidateDecoderResponseJson(), "application/json");
                 });

    server_.Post("/restconf/operations/huawei-vbussw-service:ub-memory-handle",
                 [](const httplib::Request&, httplib::Response& res) {
                     res.status = httplib::OK_200;
                     res.set_header("Content-Type", "application/json");
                     res.set_content(lcne_xml::DecoderHandleResponseJson(), "application/json");
                 });
}

void MockLcneServer::RegisterFullMeshHandlers()
{
    server_.Get("/restconf/data/huawei-vbussw-inventory:vbussw-inventory/logic-entity-mappings",
                [this](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(lcne_xml::BusInstanceXml(slotId_), "application/yang-data+xml");
                });

    server_.Get("/restconf/data/huawei-vbussw-service:vbussw-service/iou-infos",
                [this](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(lcne_xml::IoDieInfoXml(slotId_), "application/yang-data+xml");
                });

    server_.Get("/restconf/data/huawei-vbussw-inventory:vbussw-inventory/logic-entities",
                [this](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(lcne_xml::HostInfoXml(slotId_), "application/yang-data+xml");
                });

    server_.Get("/restconf/data/huawei-lingqu-topology:lingqu-topology/nodes",
                [this](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(lcne_xml::TopologyNodesXml(slotId_, clusterSlotIds_),
                                    "application/yang-data+xml");
                });

    server_.Get("/restconf/data/huawei-lingqu-topology:lingqu-topology/addresses",
                [this](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(lcne_xml::TopologyCnaXml(slotId_), "application/yang-data+xml");
                });

    // FULL_MESH only: static-urma-eids (CLOS skips this, uses ⑦ keyed query instead)
    server_.Get("/restconf/data/huawei-vbussw-service:vbussw-service/static-urma-eids",
                [](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(lcne_xml::UrmaEidXml(), "application/yang-data+xml");
                });

    server_.Get("/restconf/data/huawei-vbussw-service:vbussw-service/entity-urma-communication-infos",
                [this](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(lcne_xml::FeEidXml(slotId_), "application/yang-data+xml");
                });

    server_.Get(R"(/restconf/data/huawei-vbussw-service:vbussw-service/entity-urma-communication-infos/.+)",
                [this](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(lcne_xml::FeEidXml(slotId_), "application/yang-data+xml");
                });

    server_.Get("/restconf/data/huawei-vbussw-service:vbussw-service/mue-ue-binding-infos",
                [this](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(lcne_xml::FeBindingXml(slotId_), "application/yang-data+xml");
                });

    server_.Get(R"(/restconf/data/huawei-vbussw-service:vbussw-service/mue-ue-binding-infos/.+)",
                [this](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(lcne_xml::FeBindingXml(slotId_), "application/yang-data+xml");
                });
}

void MockLcneServer::RegisterClosHandlers()
{
    // CLOS ① logic-entity-mappings (nodeId-based EID)
    server_.Get("/restconf/data/huawei-vbussw-inventory:vbussw-inventory/logic-entity-mappings",
                [this](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(lcne_xml::ClosBusInstanceXml(nodeId_), "application/yang-data+xml");
                });

    // CLOS ② iou-infos (2 ubpu, upi=0x7FFF)
    server_.Get("/restconf/data/huawei-vbussw-service:vbussw-service/iou-infos",
                [this](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(lcne_xml::ClosIoDieInfoXml(nodeId_), "application/yang-data+xml");
                });

    // CLOS ③ logic-entities (host, upi=0x7FFF)
    server_.Get("/restconf/data/huawei-vbussw-inventory:vbussw-inventory/logic-entities",
                [this](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(lcne_xml::ClosHostInfoXml(nodeId_), "application/yang-data+xml");
                });

    // CLOS ④ topology/nodes (2 ubpu × 8 ports, port-4 UP, all remote-* = "-")
    server_.Get("/restconf/data/huawei-lingqu-topology:lingqu-topology/nodes",
                [](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(lcne_xml::ClosTopologyNodesXml(), "application/yang-data+xml");
                });

    // CLOS ⑤ topology/addresses (bus-primary-cna/bus-port-cna with 0x80*(nodeId-1) offset)
    server_.Get("/restconf/data/huawei-lingqu-topology:lingqu-topology/addresses",
                [this](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(lcne_xml::ClosTopologyCnaXml(nodeId_), "application/yang-data+xml");
                });

    // CLOS ⑦ entity-urma-communication-infos (keyed single query: /entity-urma-communication-info=1,1,1)
    // CLOS does NOT call the list path, but register it for completeness
    server_.Get("/restconf/data/huawei-vbussw-service:vbussw-service/entity-urma-communication-infos",
                [this](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(lcne_xml::ClosFeEidXml(nodeId_), "application/yang-data+xml");
                });
    server_.Get(R"(/restconf/data/huawei-vbussw-service:vbussw-service/entity-urma-communication-infos/.+)",
                [this](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(lcne_xml::ClosFeEidXml(nodeId_), "application/yang-data+xml");
                });

    // CLOS ⑧ mue-ue-binding-infos (keyed single query: /mue-ue-binding-info=1,1,1)
    // Same XML for all nodes (mue-id 2..9, all PHYSICAL_TYPE)
    server_.Get("/restconf/data/huawei-vbussw-service:vbussw-service/mue-ue-binding-infos",
                [](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(lcne_xml::ClosFeBindingXml(), "application/yang-data+xml");
                });
    server_.Get(R"(/restconf/data/huawei-vbussw-service:vbussw-service/mue-ue-binding-infos/.+)",
                [](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(lcne_xml::ClosFeBindingXml(), "application/yang-data+xml");
                });
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

    // UDS mode: listen on Unix domain socket
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
        IT_LOG_INFO << "MockLcneServer (UDS) stopped";
    });
    IT_LOG_INFO << "MockLcneServer started on " << udsPath_
                << (isClos_ ? " (CLOS mode, nodeId=" + std::to_string(nodeId_) + ")"
                            : " (FULL_MESH mode, slotId=" + std::to_string(slotId_) + ")");
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

    if (!udsPath_.empty()) {
        unlink(udsPath_.c_str());
    }

    IT_LOG_INFO << "MockLcneServer stopped";
    return UBSE_OK;
}

bool MockLcneServer::IsReady()
{
    // UDS mode: check if socket file exists
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
