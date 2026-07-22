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

#include "it_console_log.h"
#include "mock_lcne_xml.h"

namespace ubse::it::infra {

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

void MockLcneServer::RegisterHandlers()
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
                    res.set_content(lcne_xml::TopologyNodesXml(slotId_, clusterSlotIds_), "application/yang-data+xml");
                });

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

    server_.Get("/restconf/data/huawei-lingqu-topology:lingqu-topology/addresses",
                [this](const httplib::Request&, httplib::Response& res) {
                    res.status = httplib::OK_200;
                    res.set_header("Content-Type", "application/yang-data+xml");
                    res.set_content(lcne_xml::TopologyCnaXml(slotId_), "application/yang-data+xml");
                });

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
