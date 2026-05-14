/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MP_BORROW_CONF_UTIL_H
#define MP_BORROW_CONF_UTIL_H

#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include "ubse_logger.h"
#include "ubse_node_controller.h"
#include "mp_configuration.h"
#include "mp_error.h"

namespace mempooling {
using namespace ubse::log;
using namespace ubse::nodeController;

class MpParseGroupProviderConf {
public:
    static MpParseGroupProviderConf& Instance()
    {
        static MpParseGroupProviderConf instance;
        return instance;
    }
    // 获取输入节点Id的可借入节点Id集合
    MpResult GetBorrowableList(const std::string& curNid, std::unordered_set<std::string>& borrowableNidSet);

private:
    MpParseGroupProviderConf() = default;
    ~MpParseGroupProviderConf() = default;
    MpParseGroupProviderConf(const MpParseGroupProviderConf&) = delete;
    MpParseGroupProviderConf& operator=(const MpParseGroupProviderConf&) = delete;

    MpResult BuildBorrowMap();
    MpResult ParseBorrowConf(UbseMemGroupNodeList& groupList, UbseMemProviderNodeList& providerList);
    std::map<std::string, std::unordered_set<std::string>> borrowMap;
};

} // namespace mempooling

#endif // MP_BORROW_CONF_UTIL_H