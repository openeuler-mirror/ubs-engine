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

#include "test_ubse_obmm_utils.h"

#include <fstream>

#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_obmm_utils.h"

namespace ubse::ut::mmi {
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::mmi;

TEST_F(TestUbseObmmUtils, GetBasicPreImportInfos_Success)
{
    std::vector<BasicPreImportInfo> basicPreImportInfos{};
    std::vector<std::string> tokens = {"0x01", "0x01", "0x01", "0x01", "0x01", "0x01", "0:0x01", "0:0x02", "1"};
    std::ifstream file{"mockfile"};
    auto ret = RmObmmUtils::GetBasicPreImportInfos(basicPreImportInfos, file, tokens);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(basicPreImportInfos[0].deid, 1);
    EXPECT_EQ(basicPreImportInfos[0].seid, 2);
}
}  // namespace ubse::ut::mmi
