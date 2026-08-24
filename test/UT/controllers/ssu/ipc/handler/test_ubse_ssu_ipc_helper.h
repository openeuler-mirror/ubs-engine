/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#ifndef TEST_UBSE_SSU_IPC_HELPER_H
#define TEST_UBSE_SSU_IPC_HELPER_H

#include <string>

#include "../test_ubse_ssu_ipc_common.h"

namespace ubse::ssu::ipc::ut {
constexpr uint32_t UBS_SSU_GUID_LENGTH = 32;
// RAII 包装请求 buffer，避免在每个测试中手动 delete[]
struct RequestGuard {
    api::server::UbseIpcMessage msg{nullptr, 0};
    explicit RequestGuard(api::server::UbseIpcMessage m) : msg(m) {}
    ~RequestGuard()
    {
        if (msg.buffer != nullptr) {
            delete[] msg.buffer;
            msg.buffer = nullptr;
        }
    }
    const api::server::UbseIpcMessage &Ref() const { return msg; }
};

// RAII 包装响应 buffer
struct ResponseGuard {
    api::server::UbseIpcMessage msg{nullptr, 0};
    ~ResponseGuard()
    {
        if (msg.buffer != nullptr) {
            delete[] msg.buffer;
            msg.buffer = nullptr;
        }
    }
};

// 通用辅助：用 UbsePackUtil 构造一个精确大小的请求 buffer
api::server::UbseIpcMessage BuildReqFromBuffer(const uint8_t *src, uint32_t len);

// ===== 请求 buffer 构造器 =====

api::server::UbseIpcMessage MakeAllocSpaceReq(const std::string &name = "alloc_test");
api::server::UbseIpcMessage MakeFreeSpaceReq(const std::string &name = "free_test");
api::server::UbseIpcMessage MakeGetAllocInfoByNameReq(const std::string &name = "by_name_test");
api::server::UbseIpcMessage MakeGetNsStatsReq(const std::string &name = "ns_stats_test");
api::server::UbseIpcMessage MakeGetConnectInfoReq(bool withVfe, const std::string &name = "conn_test");
api::server::UbseIpcMessage MakeAccessPermissionReq(const std::string &name = "perm_test",
                                                     const std::string &nqn = "nqn.perm");
api::server::UbseIpcMessage MakeSpaceReq(const std::string &name = "space_test",
                                          const std::string &nqn = "nqn.space",
                                          const std::string &srcEid = "eid_src");
api::server::UbseIpcMessage MakeLinearSpaceReq(const std::string &name = "linear_test",
                                                const std::string &devName = "linear_dev");
api::server::UbseIpcMessage MakeStripedSpaceReq(const std::string &name = "striped_test",
                                                 const std::string &devName = "striped_dev",
                                                 uint8_t level = 0, uint32_t chunkSize = 4);
api::server::UbseIpcMessage MakeFeDeviceAllocReq(uint32_t upi = 1,
                                                  const std::string &busGuid = std::string(32, 'c'));
api::server::UbseIpcMessage MakeFeDeviceFreeReq(uint32_t upi = 1);
api::server::UbseIpcMessage MakeEmptyReq();
api::server::UbseIpcMessage MakeInvalidReq_NameTooLong();
api::server::UbseIpcMessage MakeFeDeviceReq_TruncatedAfterUpi(uint32_t upi);
api::server::UbseIpcMessage MakeFeDeviceReq_TruncatedInVfeGuid(uint32_t upi);
api::server::UbseIpcMessage MakeFeDeviceReq_TruncatedAfterVfe(uint32_t upi);

} // namespace ubse::ssu::ipc::ut

#endif // TEST_UBSE_SSU_IPC_HELPER_H
