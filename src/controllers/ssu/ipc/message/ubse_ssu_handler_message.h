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

#ifndef UBS_ENGINE_UBSE_SSU_HANDLER_MESSAGE_H
#define UBS_ENGINE_UBSE_SSU_HANDLER_MESSAGE_H

#include <optional>
#include "plugin_services/ssu/ubse_ssu_service.h"
#include "ubse_common_def.h"
#include "ubse_api_server_def.h"

namespace ubse::ssu::ipc::message {

common::def::UbseResult SsuAllocResultListPack(
    const std::vector<ubse::plugin::service::ssu::UbseSsuAllocResult> &resultList,
    api::server::UbseIpcMessage &response);

common::def::UbseResult SsuGetAllocInfoByNameUnpack(const api::server::UbseIpcMessage &buffer, std::string &name);
common::def::UbseResult SsuGetAllocInfoByNamePack(const plugin::service::ssu::UbseSsuAllocResult &result,
                                                  api::server::UbseIpcMessage &response);

common::def::UbseResult SsuGetNsStatsUnpack(const api::server::UbseIpcMessage &buffer, std::string &name);
common::def::UbseResult SsuGetNsStatsPack(const std::vector<plugin::service::ssu::UbseSsuNsStats> &statsList,
                                          api::server::UbseIpcMessage &response);

common::def::UbseResult SsuGetConnectInfoUnpack(const api::server::UbseIpcMessage &buffer, std::string &name,
                                                std::optional<plugin::service::ssu::UbseSsuVfe> &vfe);
common::def::UbseResult SsuGetConnectInfoPack(
    const std::vector<ubse::plugin::service::ssu::UbseSsuConnectInfo> &connectInfoList,
    api::server::UbseIpcMessage &response);

common::def::UbseResult SsuAllocSpaceUnpack(const api::server::UbseIpcMessage &buffer,
                                            plugin::service::ssu::UbseSsuAllocSpaceReq &req);
common::def::UbseResult SsuAllocSpacePack(const plugin::service::ssu::UbseSsuAllocResult &result,
                                          api::server::UbseIpcMessage &response);

common::def::UbseResult SsuFreeSpaceUnpack(const api::server::UbseIpcMessage &buffer, std::string &name);

common::def::UbseResult SsuAddAccessPermissionUnpack(const api::server::UbseIpcMessage &buffer, std::string &name,
                                                     std::string &nqn);

common::def::UbseResult SsuRemoveAccessPermissionUnpack(const api::server::UbseIpcMessage &buffer, std::string &name,
                                                        std::string &nqn);

common::def::UbseResult SsuAttachSpaceUnpack(const api::server::UbseIpcMessage &buffer,
                                             plugin::service::ssu::UbseSsuSpaceReq &req);
common::def::UbseResult SsuAttachSpacePack(const std::string &devPath, api::server::UbseIpcMessage &response);

common::def::UbseResult SsuDetachSpaceUnpack(const api::server::UbseIpcMessage &buffer,
                                             plugin::service::ssu::UbseSsuSpaceReq &req);

common::def::UbseResult SsuAttachLinearSpaceUnpack(const api::server::UbseIpcMessage &buffer,
                                                   plugin::service::ssu::UbseSsuLinearSpaceReq &req);
common::def::UbseResult SsuAttachLinearSpacePack(const std::string &devPath, api::server::UbseIpcMessage &response);

common::def::UbseResult SsuDetachLinearSpaceUnpack(const api::server::UbseIpcMessage &buffer,
                                                   plugin::service::ssu::UbseSsuLinearSpaceReq &req);

common::def::UbseResult SsuAttachStripedSpaceUnpack(const api::server::UbseIpcMessage &buffer,
                                                    plugin::service::ssu::UbseSsuStripedSpaceReq &req);
common::def::UbseResult SsuAttachStripedSpacePack(const std::string &devPath, api::server::UbseIpcMessage &response);

common::def::UbseResult SsuDetachStripedSpaceUnpack(const api::server::UbseIpcMessage &buffer,
                                                    plugin::service::ssu::UbseSsuStripedSpaceReq &req);

common::def::UbseResult SsuGetFeDeviceListPack(const std::vector<plugin::service::ssu::UbseSsuFe> &feList,
                                               api::server::UbseIpcMessage &response);

common::def::UbseResult SsuFeDeviceAllocUnpack(const api::server::UbseIpcMessage &buffer, uint32_t &upi,
                                               plugin::service::ssu::UbseSsuVfe &vfe, std::string &busInstanceGuid);
common::def::UbseResult SsuFeDeviceAllocPack(const std::string &busInstanceGuid, api::server::UbseIpcMessage &response);

common::def::UbseResult SsuFeDeviceFreeUnpack(const api::server::UbseIpcMessage &buffer, uint32_t &upi,
                                              plugin::service::ssu::UbseSsuVfe &vfe, std::string &busInstanceGuid);

} // namespace ubse::ssu::ipc::message

#endif // UBS_ENGINE_UBSE_SSU_HANDLER_MESSAGE_H