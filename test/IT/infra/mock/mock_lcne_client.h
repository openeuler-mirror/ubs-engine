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

#ifndef MOCK_LCNE_CLIENT_H
#define MOCK_LCNE_CLIENT_H

#include <string>

#include "ubse_common_def.h"
#include "ubse_error.h"

namespace ubse::it::infra {

using ubse::common::def::UbseResult;

// LCNE 作为客户端, 通过 UBSE HTTP 服务(UDS)上报端口 link-up/link-down 拓扑变更告警
class MockLcneClient {
public:
    // ubseHttpUdsPath: UBSE 侧 HTTP 服务的 UDS socket 路径(如 <workDir>/run/ubse_ubm.socket)
    explicit MockLcneClient(const std::string& ubseHttpUdsPath);

    /**
     * @brief 向 UBSE 上报端口拓扑变更.
     * @param isPortDown    true: 端口 down; false: 端口 up
     * @param interfaceName 拓扑中的接口名, 如 "400GUB1/1/1"
     */
    UbseResult NotifyLinkUpDown(bool isPortDown, const std::string& interfaceName);

private:
    std::string ubseHttpUdsPath_;
};

} // namespace ubse::it::infra

#endif // MOCK_LCNE_CLIENT_H
