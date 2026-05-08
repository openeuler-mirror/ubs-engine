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

#ifndef TEST_UBSE_URMA_UVS_H
#define TEST_UBSE_URMA_UVS_H

#include <gtest/gtest.h>
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_node_controller.h"
#include "ubse_urma_uvs_module.h"

namespace ubse::urma {
extern UbseResult FillNodeComInfo(const std::vector<PhysicalLink> &allLinkInfo,
                           const std::vector<UbseUrmaUvsNodeInfo> &bondingInfo, std::vector<UbcoreTopoNode> &nodes);
extern UbseResult ConvertEidStrToHexCharList(const std::string &input, char outBytes[IPV6_BYTE_COUNT]);
}

namespace ubse::urma::ut {
class TestUrmaUvs : public testing::Test {
public:
    TestUrmaUvs() = default;

    void SetUp() override;

    void TearDown() override;
};
}
#endif //TEST_UBSE_URMA_UVS_H
