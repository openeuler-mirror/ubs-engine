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

#include "mock_lcne_client.h"

#include <sys/socket.h>

#include "httplib.h"
#include "it_console_log.h"
#include "mock_lcne_xml.h"

namespace ubse::it::infra {

MockLcneClient::MockLcneClient(const std::string& ubseHttpUdsPath) : ubseHttpUdsPath_(ubseHttpUdsPath) {}

UbseResult MockLcneClient::NotifyLinkUpDown(bool isPortDown, const std::string& interfaceName)
{
    auto linkUpDown = isPortDown ? "link-down" : "link-up";
    if (ubseHttpUdsPath_.empty()) {
        IT_LOG_ERROR << "NotifyLinkUpDown: UBSE HTTP UDS path is empty, cannot push topology change.";
        return UBSE_ERROR;
    }

    httplib::Client cli(ubseHttpUdsPath_);
    cli.set_address_family(AF_UNIX);
    cli.set_connection_timeout(2, 0);
    cli.set_read_timeout(5, 0);
    cli.set_write_timeout(5, 0);

    auto res = cli.Post("/topolink/change/", lcne_xml::LinkUpDownNotificationXml(linkUpDown, interfaceName),
                        "application/yang-data+xml");
    if (!res) {
        IT_LOG_ERROR << "NotifyLinkUpDown: no response from UBSE " << ubseHttpUdsPath_;
        return UBSE_ERROR;
    }
    if (res->status != httplib::OK_200) {
        IT_LOG_ERROR << "NotifyLinkUpDown: UBSE returned status " << res->status;
        return UBSE_ERROR;
    }

    IT_LOG_INFO << "Pushed topology change to UBSE: " << linkUpDown << ", interface=" << interfaceName;
    return UBSE_OK;
}

} // namespace ubse::it::infra
