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

#ifndef MOCK_LCNE_SERVER_H
#define MOCK_LCNE_SERVER_H

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "httplib.h"

namespace ubse::it::infra {

using ubse::common::def::UbseResult;

class MockLcneServer {
public:
    // UDS mode: listen on Unix domain socket at udsPath, with slotId for multi-node scenarios
    MockLcneServer(const std::string& udsPath, uint32_t slotId, const std::vector<uint32_t>& clusterSlotIds = {});
    ~MockLcneServer();

    UbseResult Start();
    UbseResult Stop();
    bool IsReady();
    UbseResult WaitForReady(uint32_t timeoutMs);

    uint32_t GetSlotId() const
    {
        return slotId_;
    }

private:
    void RegisterHandlers();
    std::string GenerateBusInstanceXml() const;
    std::string GenerateIoDieInfoXml() const;
    std::string GenerateTopologyNodesXml() const;
    std::string GenerateTopologyCnaXml() const;
    std::string GenerateUrmaEidXml() const;
    std::string GenerateFeEidXml() const;
    std::string GenerateFeBindingXml() const;
    std::string GenerateAddDecoderResponseJson() const;
    std::string GenerateDeleteDecoderResponseJson() const;
    std::string GenerateInvalidateDecoderResponseJson() const;
    std::string GenerateDecoderHandleResponseJson() const;

    httplib::Server server_;
    std::string udsPath_;
    uint32_t slotId_ = 1;
    std::vector<uint32_t> clusterSlotIds_;
    std::thread serverThread_;
    std::atomic_bool running_;
};

} // namespace ubse::it::infra

#endif // MOCK_LCNE_SERVER_H
