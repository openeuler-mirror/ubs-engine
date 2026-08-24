/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#ifndef TEST_UBSE_SSU_IPC_COMMON_H
#define TEST_UBSE_SSU_IPC_COMMON_H

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

#include <memory>
#include <string>
#include <vector>

#include "ubse_api_server_def.h"
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_os_util.h"
#include "ubse_service_registry.h"
#include "plugin_services/ssu/ubse_ssu_service.h"
#include "ubse_ssu_ipc_handler.h"

namespace ubse::ssu::ipc::ut {

using namespace ubse::plugin::service::ssu;
using namespace common::def;
using namespace api::server;

/**
 * @brief 手写的 UbseSsuService mock 实现
 *
 * 用成员变量记录调用参数与预设返回值，便于在测试中精确断言。
 */
class MockSsuService : public UbseSsuService {
public:
    MockSsuService() = default;
    ~MockSsuService() override = default;

    // ====== 预设返回值（默认成功） ======
    uint32_t allocRet{UBSE_OK};
    uint32_t freeRet{UBSE_OK};
    uint32_t listRet{UBSE_OK};
    uint32_t getByNameRet{UBSE_OK};
    uint32_t getNsStatsRet{UBSE_OK};
    uint32_t getConnectInfoRet{UBSE_OK};
    uint32_t addPermRet{UBSE_OK};
    uint32_t removePermRet{UBSE_OK};
    uint32_t attachRet{UBSE_OK};
    uint32_t detachRet{UBSE_OK};
    uint32_t attachLinearRet{UBSE_OK};
    uint32_t detachLinearRet{UBSE_OK};
    uint32_t attachStripedRet{UBSE_OK};
    uint32_t detachStripedRet{UBSE_OK};
    uint32_t getFeListRet{UBSE_OK};
    uint32_t feAllocRet{UBSE_OK};
    uint32_t feFreeRet{UBSE_OK};

    // ====== 预设输出值 ======
    UbseSsuAllocResult allocResult;
    std::vector<UbseSsuAllocResult> listResult;
    UbseSsuAllocResult getByNameResult;
    std::vector<UbseSsuNsStats> nsStatsList;
    std::vector<UbseSsuConnectInfo> connectInfoList;
    std::vector<std::string> attachNsDevPaths;
    std::string attachDevPath;
    std::vector<UbseSsuFe> feList;
    std::string feAllocBusInstanceGuid;

    // ====== 调用计数 ======
    int allocCount{0};
    int freeCount{0};
    int listCount{0};
    int getByNameCount{0};
    int getNsStatsCount{0};
    int getConnectInfoCount{0};
    int addPermCount{0};
    int removePermCount{0};
    int attachCount{0};
    int detachCount{0};
    int attachLinearCount{0};
    int detachLinearCount{0};
    int attachStripedCount{0};
    int detachStripedCount{0};
    int getFeListCount{0};
    int feAllocCount{0};
    int feFreeCount{0};

    // ====== 最近一次调用入参（用于断言） ======
    UbseSsuAllocSpaceReq lastAllocReq;
    UbseSsuAllocIdentityInfo lastIdentity;
    std::string lastName;
    std::string lastNqn;
    UbseSsuSpaceReq lastSpaceReq;
    UbseSsuLinearSpaceReq lastLinearReq;
    UbseSsuStripedSpaceReq lastStripedReq;
    UbseSsuVfe lastVfe;
    uint32_t lastUpi{0};
    const UbseSsuVfe *lastGetConnectInfoVfe{nullptr};
    std::string lastFeAllocBusInstanceGuid;

    // ====== 接口实现 ======
    uint32_t ListAllocInfo(std::vector<UbseSsuAllocResult> &result,
                           const UbseSsuAllocIdentityInfo &identity) override
    {
        listCount++;
        lastIdentity = identity;
        result = listResult;
        return listRet;
    }

    uint32_t GetAllocInfoByName(const std::string &name, UbseSsuAllocResult &result,
                                const UbseSsuAllocIdentityInfo &identity) override
    {
        getByNameCount++;
        lastName = name;
        lastIdentity = identity;
        result = getByNameResult;
        return getByNameRet;
    }

    uint32_t GetNsStats(const std::string &name, std::vector<UbseSsuNsStats> &statsList,
                        const UbseSsuAllocIdentityInfo &identity) override
    {
        getNsStatsCount++;
        lastName = name;
        lastIdentity = identity;
        statsList = nsStatsList;
        return getNsStatsRet;
    }

    uint32_t GetConnectInfo(const std::string &name, const UbseSsuVfe *vfe,
                            std::vector<UbseSsuConnectInfo> &connectInfoList,
                            const UbseSsuAllocIdentityInfo &identity) override
    {
        getConnectInfoCount++;
        lastName = name;
        lastGetConnectInfoVfe = vfe;
        lastIdentity = identity;
        if (vfe != nullptr) {
            lastVfe = *vfe;
        }
        connectInfoList = this->connectInfoList;
        return getConnectInfoRet;
    }

    uint32_t AllocSpace(const UbseSsuAllocSpaceReq &req, const UbseSsuAllocIdentityInfo &identity,
                        UbseSsuAllocResult &result) override
    {
        allocCount++;
        lastAllocReq = req;
        lastIdentity = identity;
        result = allocResult;
        return allocRet;
    }

    uint32_t FreeSpace(const std::string &name, const UbseSsuAllocIdentityInfo &identity) override
    {
        freeCount++;
        lastName = name;
        lastIdentity = identity;
        return freeRet;
    }

    uint32_t AddAccessPermission(const std::string &name, const std::string &nqn,
                                 const UbseSsuAllocIdentityInfo &identity) override
    {
        addPermCount++;
        lastName = name;
        lastNqn = nqn;
        lastIdentity = identity;
        return addPermRet;
    }

    uint32_t RemoveAccessPermission(const std::string &name, const std::string &nqn,
                                    const UbseSsuAllocIdentityInfo &identity) override
    {
        removePermCount++;
        lastName = name;
        lastNqn = nqn;
        lastIdentity = identity;
        return removePermRet;
    }

    uint32_t AttachSpace(const UbseSsuSpaceReq &req, std::vector<std::string> &nsDevPaths) override
    {
        attachCount++;
        lastSpaceReq = req;
        nsDevPaths = attachNsDevPaths;
        return attachRet;
    }

    uint32_t DetachSpace(const UbseSsuSpaceReq &req) override
    {
        detachCount++;
        lastSpaceReq = req;
        return detachRet;
    }

    uint32_t AttachLinearSpace(const UbseSsuLinearSpaceReq &req, std::vector<std::string> &nsDevPaths,
                               std::string &devPath) override
    {
        attachLinearCount++;
        lastLinearReq = req;
        nsDevPaths = attachNsDevPaths;
        devPath = attachDevPath;
        return attachLinearRet;
    }

    uint32_t DetachLinearSpace(const UbseSsuLinearSpaceReq &req) override
    {
        detachLinearCount++;
        lastLinearReq = req;
        return detachLinearRet;
    }

    uint32_t AttachStripedSpace(const UbseSsuStripedSpaceReq &req, std::vector<std::string> &nsDevPaths,
                                std::string &devPath) override
    {
        attachStripedCount++;
        lastStripedReq = req;
        nsDevPaths = attachNsDevPaths;
        devPath = attachDevPath;
        return attachStripedRet;
    }

    uint32_t DetachStripedSpace(const UbseSsuStripedSpaceReq &req) override
    {
        detachStripedCount++;
        lastStripedReq = req;
        return detachStripedRet;
    }

    uint32_t GetFeDeviceList(std::vector<UbseSsuFe> &feList) override
    {
        getFeListCount++;
        feList = this->feList;
        return getFeListRet;
    }

    uint32_t FeDeviceAlloc(uint32_t upi, const UbseSsuVfe &vfe, std::string &busInstanceGuid) override
    {
        feAllocCount++;
        lastUpi = upi;
        lastVfe = vfe;
        lastFeAllocBusInstanceGuid = busInstanceGuid; // 记录输入值
        busInstanceGuid = feAllocBusInstanceGuid;     // 设置输出值
        return feAllocRet;
    }

    uint32_t FeDeviceFree(uint32_t upi, const UbseSsuVfe &vfe) override
    {
        feFreeCount++;
        lastUpi = upi;
        lastVfe = vfe;
        return feFreeRet;
    }
};

/**
 * @brief IPC handler 测试通用 fixture
 *
 * SetUp 时将 MockSsuService 注册到 UbseServiceRegistry 并 mock GetUserNameById，
 * TearDown 时反注册并校验 mockcpp 全局 mock。
 */
class IpcTestFixture : public testing::Test {
public:
    void SetUp() override;
    void TearDown() override;

    static UbseRequestContext MakeContext(uint16_t opCode = 0);

    std::shared_ptr<MockSsuService> mockService;
};

/**
 * @brief handler 访问器模板
 *
 * UbseSsuHandler 的 Unpack/Handle/Pack/Init 是 protected 虚函数，
 * buffer_/context_/identity_ 是 protected 成员。
 * 通过此模板用 using 声明暴露，便于测试直接调用三段式而无需走 Execute 的 SendResponse 分支。
 */
template <typename HandlerT>
class HandlerAccessor : public HandlerT {
public:
    using HandlerT::buffer_;
    using HandlerT::context_;
    using HandlerT::identity_;
    using HandlerT::Init;
    using HandlerT::Unpack;
    using HandlerT::Handle;
    using HandlerT::Pack;
};

} // namespace ubse::ssu::ipc::ut

#endif // TEST_UBSE_SSU_IPC_COMMON_H
