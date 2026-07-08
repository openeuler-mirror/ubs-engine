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

#ifndef MOCK_LCNE_XML_H
#define MOCK_LCNE_XML_H

#include <cstdint>
#include <string>
#include <vector>

namespace ubse::it::infra::lcne_xml {

std::string BusInstanceXml(uint32_t slotId);
std::string HostInfoXml(uint32_t slotId);
std::string IoDieInfoXml(uint32_t slotId);
std::string TopologyNodesXml(uint32_t slotId, const std::vector<uint32_t>& clusterSlotIds = {});
std::string TopologyCnaXml(uint32_t slotId);
std::string UrmaEidXml();
std::string FeEidXml(uint32_t slotId);
std::string FeBindingXml(uint32_t slotId);
std::string SubscriptionResponseXml();
std::string AddDecoderResponseJson();
std::string DeleteDecoderResponseJson();
std::string InvalidateDecoderResponseJson();
std::string DecoderHandleResponseJson();

} // namespace ubse::it::infra::lcne_xml

#endif // MOCK_LCNE_XML_H
