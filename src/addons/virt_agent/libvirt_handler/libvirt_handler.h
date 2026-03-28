/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
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

#ifndef VM_LIBVIRT_HANDLER_H
#define VM_LIBVIRT_HANDLER_H

#include <dynamic_priority_queue.h>
#include <ubse_ras.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <vector>
#include <rapidjson/document.h>
#include <ubse_api_server_def.h>
#include "ham_migrate_vm_info.h"
#include "vm_error.h"
#include "vm_http_util.h"
#include "vm_info.h"
#include "vm_strategy_struct.h"
#include "ham_migrate_dst_info_message.h"

namespace vm {
using namespace ubse::ras;
using namespace rapidjson;
using namespace api::server;
struct RespInfo {
    unsigned int code{};
    std::string message = "{}";

    std::string ToJson() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << R"("code": )" << code << ", ";
        oss << R"("message": )" << message;
        oss << "}";
        return oss.str();
    }
};

enum class ClearType {
    NODE = 0,                    // Current node
    MIGRATED_CLEAR = 1,          // Migration successful cleanup
    NOMIGRATE_CLEAR = 2,         // Migration failed cleanup
};

enum class ExportAccessMode : uint8_t {
    HAM_MIGRATE_MODE = 0,
    FAST_RECOVERY_MODE = 1,
};

struct ClearInfo {
    std::string borrowName{};
    ClearType state = ClearType::NODE;
    std::string srcNodeId{};
    int srcPid{};
    std::string dstNodeId{};
    int dstPid{};
};

class LibvirtHandler {
public:
    static VmResult Start();
    static uint32_t HamMigrateNorth(const UbseIpcMessage &req, const UbseRequestContext &context);
    static VmResult ConvertToVaList(const Value &msgJson, std::vector<VirtualAddress> &valist);
    static VmResult ProcessResponse(const RespInfo& respInfo, UbseIpcMessage& resp, uint64_t requestId);
private:
    static VmResult ValidateAndParseRequest(const UbseIpcMessage& req, Document& msgJson,
                                            std::string& scene, std::string& action);
};
} // namespace vm

#endif // VM_LIBVIRT_HANDLER_H
