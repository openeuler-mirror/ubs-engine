/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef VM_HTTP_UTIL_H
#define VM_HTTP_UTIL_H

#include <sstream>
#include <string>
#include <rapidjson/document.h>

#include <ubse_def.h>
#include "vm_error.h"

namespace vm {
using namespace rapidjson;

struct VirtualAddress {
    uint64_t start{};
    uint64_t length{};

    std::string ToJson() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << R"("start": )" << start << ", ";
        oss << R"("length": )" << length;
        oss << "}";
        return oss.str();
    }
};

struct BorrowInfo {
    std::string srcNodeId{};
    int srcPid{};
    std::string dstNodeId{};
    int dstPid{};
    std::string type = "APP_PRI_BORROW";
    std::string name{};
    int srcSocket{};
    int srcNuma{};
    int dstSocket{};
    int dstNuma{};
    std::vector<VirtualAddress> valist{};

    std::string ToJson() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << R"("srcNodeId": ")" << srcNodeId << R"(", )";
        oss << R"("srcProcessId": )" << srcPid << ", ";
        oss << R"("dstNodeId": ")" << dstNodeId << R"(", )";
        oss << R"("dstProcessId": )" << dstPid << ", ";
        oss << R"("type": ")" << type << R"(", )";
        oss << R"("srcSocket": ")" << srcSocket << R"(", )";
        oss << R"("srcNuma": ")" << srcNuma << R"(", )";
        oss << R"("dstSocket": ")" << dstSocket << R"(", )";
        oss << R"("dstNuma": ")" << dstNuma << R"(", )";
        oss << R"("name": ")" << name << R"(", )";
        oss << R"("valist": )";
        oss << "[";
        for (size_t i = 0; i < valist.size(); ++i) {
            oss << valist[i].ToJson();
            if (i != valist.size() - 1) {
                oss << ", ";
            }
        }
        oss << "]";
        oss << "}";
        return oss.str();
    }
};

struct BorrowResponse {
    std::string name{};
    uint32_t scna{};
    std::vector<int16_t> numaIds{};
    std::string ToJson() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << R"("name": ")" << name << R"(", )";
        oss << R"("scna": )" << scna << ", ";
        oss << R"("numaIds": )";
        oss << "[";
        for (size_t i = 0; i < numaIds.size(); ++i) {
            oss << numaIds[i];
            if (i != numaIds.size() - 1) {
                oss << ", ";
            }
        }
        oss << "]";
        oss << "}";
        return oss.str();
    }
};

class HttpUtil {
public:
    static VmResult AddProcessTracking(const int &pid, const int &scanTime, const int &type);
    static VmResult RemoveProcessTracking(const int &pid);
    static VmResult EnableProcessMigrate(const int &pid, bool enable);
    static VmResult ToUbseByteBuffer(const std::string &bodyString, UbseByteBuffer &resp);
    static VmResult SetResp(UbseByteBuffer &resp, const VmResult &code, const std::string &msg);
};
} // namespace vm
#endif // VM_HTTP_UTIL_H
