#include <cstring>
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

#include "test_ubse_obmm_executor.h"

#include <sys/mman.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <future>
#include <thread>

#include <ubse_mem_instance_inner.h>
#include <ubse_node_controller.h>
#include <ubse_obmm_executor.h>
#include <ubse_obmm_meta_restore.h>
#include <ubse_obmm_utils.h>
#include <ubse_security_module.h>

namespace ubse::ut::mmi {
using namespace ubse::mmi;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::nodeController;
using namespace ubse::security;

class TestUbseObmmUnimport : public testing::Test {
    void SetUp() override
    {
        Test::SetUp();
        oldUnimportFunc_ = RmObmmExecutor::GetInstance().obmmUnimportFunc;
        RmObmmExecutor::GetInstance().obmmUnimportFunc = [](unsigned long id, unsigned long flags) {
            return 0;
        };

        struct sigaction sa {
        };
        sa.sa_handler = [](int) {
        };
        sa.sa_flags = 0;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGUSR1, &sa, &oldSa_);
    }

    void TearDown() override
    {
        RmObmmExecutor::GetInstance().obmmUnimportFunc = oldUnimportFunc_;
        sigaction(SIGUSR1, &oldSa_, nullptr);
        Test::TearDown();
        GlobalMockObject::verify();
    }

private:
    ObmmUnimportPtr oldUnimportFunc_{nullptr};
    struct sigaction oldSa_ {
    };
};

TEST_F(TestUbseObmmUnimport, UnImportWithTimeout_Success)
{
    auto ret = RmObmmExecutor::GetInstance().ObmmUnImport(1, 5000);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseObmmUnimport, UnImportWithTimeout_NullptrFunc)
{
    RmObmmExecutor::GetInstance().obmmUnimportFunc = nullptr;
    auto ret = RmObmmExecutor::GetInstance().ObmmUnImport(1, 5000);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseObmmUnimport, UnImportWithTimeout_ObmmError)
{
    RmObmmExecutor::GetInstance().obmmUnimportFunc = [](unsigned long id, unsigned long flags) {
        errno = EIO;
        return -1;
    };
    auto ret = RmObmmExecutor::GetInstance().ObmmUnImport(1, 5000);
    EXPECT_EQ(ret, UBSE_MMI_OBMM_OP_FAILED);
}

TEST_F(TestUbseObmmUnimport, UnImportWithTimeout_InterruptedBySignal)
{
    RmObmmExecutor::GetInstance().obmmUnimportFunc = [](unsigned long id, unsigned long flags) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        errno = EINTR;
        return -1;
    };

    auto ret = RmObmmExecutor::GetInstance().ObmmUnImport(42, 100);
    EXPECT_EQ(ret, UBSE_MMI_OBMM_OP_TIMEOUT);
}

TEST_F(TestUbseObmmUnimport, UnImportWithTimeout_ENOENT_ReturnsOk)
{
    RmObmmExecutor::GetInstance().obmmUnimportFunc = [](unsigned long id, unsigned long flags) {
        errno = ENOENT;
        return -1;
    };
    auto ret = RmObmmExecutor::GetInstance().ObmmUnImport(1, 5000);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseObmmUnimport, BatchUnImportWithTimeout_Success)
{
    std::vector<mem_id> memIds = {1, 2, 3};
    auto ret = RmObmmExecutor::GetInstance().ObmmUnImport(memIds, 5000);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseObmmUnimport, BatchUnImportWithTimeout_PartialTimeout)
{
    static int callCount = 0;
    callCount = 0;
    RmObmmExecutor::GetInstance().obmmUnimportFunc = [](unsigned long id, unsigned long flags) {
        callCount++;
        if (id == 2) {
            errno = EINTR;
            return -1;
        }
        return 0;
    };
    std::vector<mem_id> memIds = {1, 2, 3};
    auto ret = RmObmmExecutor::GetInstance().ObmmUnImport(memIds, 5000);
    EXPECT_EQ(ret, UBSE_MMI_OBMM_OP_TIMEOUT);
    EXPECT_EQ(callCount, 2);
}

TEST_F(TestUbseObmmUnimport, CalculateUnImportTimeoutDefault)
{
    RmObmmExecutor::offlineTimeoutConfigured_ = false;
    EXPECT_EQ(RmObmmExecutor::CalculateUnImportTimeout(128), 5128);
    EXPECT_EQ(RmObmmExecutor::CalculateUnImportTimeout(4096), 9096);
    EXPECT_EQ(RmObmmExecutor::CalculateUnImportTimeout(0), 5000);
}

TEST_F(TestUbseObmmUnimport, CalculateUnImportTimeoutConfigured)
{
    RmObmmExecutor::offlineTimeoutConfigured_ = true;
    RmObmmExecutor::offlineTimeoutMs_ = 100000;
    EXPECT_EQ(RmObmmExecutor::CalculateUnImportTimeout(128), 100000);
    EXPECT_EQ(RmObmmExecutor::CalculateUnImportTimeout(4096), 100000);
    EXPECT_EQ(RmObmmExecutor::CalculateUnImportTimeout(0), 100000);

    RmObmmExecutor::offlineTimeoutMs_ = 120000;
    EXPECT_EQ(RmObmmExecutor::CalculateUnImportTimeout(999), 120000);

    RmObmmExecutor::offlineTimeoutConfigured_ = false;
}

TEST_F(TestUbseObmmExecutor, Init_Success)
{
    auto ret = RmObmmExecutor::GetInstance().Init();
    EXPECT_NE(ret, UBSE_OK);

    ret = RmObmmExecutor::GetInstance().Init();
    EXPECT_NE(ret, UBSE_OK);

    MOCKER(&RmObmmUtils::GetPreOnlineSwitch).stubs().will(returnValue(UBSE_OK));
    MOCKER(&RmObmmExecutor::DlOpenLib).stubs().will(returnValue(UBSE_ERROR));
    ret = RmObmmExecutor::GetInstance().Init();
    EXPECT_NE(ret, UBSE_OK);

    MOCKER(&RmObmmExecutor::DlOpenLib).reset();
    MOCKER(&RmObmmExecutor::DlOpenLib).stubs().will(returnValue(UBSE_OK));
    ret = RmObmmExecutor::GetInstance().Init();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseObmmExecutor, Exit_Success)
{
    auto ret = RmObmmExecutor::GetInstance().Exit();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseObmmExecutor, ObmmExportPid_NullptrFunc)
{
    auto& executor = RmObmmExecutor::GetInstance();
    auto oldExportByPidFunc = executor.obmmExportByPidFunc;
    executor.obmmExportByPidFunc = nullptr;
    ObmmPidExportParam param(0, nullptr, 0, 0, 0, 0);
    ubse_mem_obmm_mem_desc desc{};
    UbseMemLocalObmmCustomMeta customMeta{};
    UbMemPrivData privData{};

    EXPECT_EQ(executor.ObmmExportPid(param, desc, customMeta, privData), UBSE_ERROR_NULLPTR);
    executor.obmmExportByPidFunc = oldExportByPidFunc;
}

namespace {
constexpr mem_id TEST_SWITCH_MEM_ID = 42;
constexpr uint64_t TEST_NEW_PA = 0x1234567800ULL;

ObmmSwitchParam BuildSwitchParam(size_t privLen)
{
    ObmmSwitchParam param;
    param.oldImportMemId = TEST_SWITCH_MEM_ID;
    param.newPa = TEST_NEW_PA;
    param.newScna = 0x1111;
    param.newDcna = 0x2222;
    param.baseDist = 40;
    for (uint32_t i = 0; i < UBSE_EID_LENGTH; i++) {
        param.newSeid[i] = static_cast<uint8_t>(i + 1);
        param.newDeid[i] = static_cast<uint8_t>(0x80 + i);
    }
    for (size_t i = 0; i < privLen; i++) {
        param.priv.push_back(static_cast<uint8_t>(i + 0x10));
    }
    return param;
}
} // namespace

class TestUbseObmmSwitch : public testing::Test {
    void SetUp() override
    {
        Test::SetUp();
        MOCKER(&UbseSecurityModule::ModifyEffectiveCapabilities).stubs().will(returnValue(UBSE_OK));
        auto& executor = RmObmmExecutor::GetInstance();
        oldSwitchBeginImportFunc_ = executor.obmmSwitchBeginImportFunc;
        oldSwitchExportFunc_ = executor.obmmSwitchBeginExportFunc;
        oldSwitchEndFunc_ = executor.obmmSwitchEndFunc;
        oldExportFunc_ = executor.obmmExportFunc;
        oldImportFunc_ = executor.obmmImportFunc;
    }

    void TearDown() override
    {
        auto& executor = RmObmmExecutor::GetInstance();
        executor.obmmSwitchBeginImportFunc = oldSwitchBeginImportFunc_;
        executor.obmmSwitchBeginExportFunc = oldSwitchExportFunc_;
        executor.obmmSwitchEndFunc = oldSwitchEndFunc_;
        executor.obmmExportFunc = oldExportFunc_;
        executor.obmmImportFunc = oldImportFunc_;
        Test::TearDown();
        GlobalMockObject::verify();
    }

private:
    ObmmSwitchBeginImportPtr oldSwitchBeginImportFunc_{nullptr};
    ObmmSwitchBeginExportPtr oldSwitchExportFunc_{nullptr};
    ObmmSwitchEndPtr oldSwitchEndFunc_{nullptr};
    ObmmExportPtr oldExportFunc_{nullptr};
    ObmmImportPtr oldImportFunc_{nullptr};
};

TEST_F(TestUbseObmmSwitch, SharedExportPersistsIdentityForEveryBlock)
{
    auto& executor = RmObmmExecutor::GetInstance();
    MOCKER(&UbseNodeController::GetLocalEidBySocket).stubs().with(_, outBound(uint32_t{2})).will(returnValue(UBSE_OK));
    MOCKER(&RmObmmExecutor::ObmmDevChangeUidGid).stubs().will(returnValue(mem_id{123}));
    static uint32_t exportedBlocks = 0;
    exportedBlocks = 0;
    executor.obmmExportFunc = [](const size_t*, unsigned long, obmm_mem_desc* desc) -> mem_id {
        UbseMemLocalObmmCustomMeta meta{};
        UbMemPrivData priv{};
        EXPECT_EQ(RmObmmMetaRestore::RestoreAgentLocalObmmMetaDataFromBuffer(desc->priv, desc->priv_len, meta, priv),
                  UBSE_OK);
        EXPECT_EQ(meta.type, static_cast<uint8_t>(UbseBorrowType::SHARE_BORROW));
        EXPECT_EQ(meta.migrateInfo, 0x80000000U | exportedBlocks);
        EXPECT_TRUE(ShmMigrateIsExport(meta.migrateInfo));
        EXPECT_EQ(ShmMigrateOrigin(meta.migrateInfo), 0U);
        ++exportedBlocks;
        return 123;
    };
    size_t sizes[MAX_NUMA_NODES]{};
    sizes[0] = 256ULL << 20;
    ObmmOpParam param{};
    param.borrowType = UbseBorrowType::SHARE_BORROW;
    param.customMeta.type = static_cast<uint8_t>(UbseBorrowType::SHARE_BORROW);
    std::vector<ubse_mem_obmm_mem_desc> desc(2);
    EXPECT_EQ(executor.ObmmExport(sizes, MAX_NUMA_NODES, param, desc, 128ULL << 20).size(), 2);
    EXPECT_EQ(exportedBlocks, 2);
    EXPECT_EQ(param.migrateInfos, (std::vector<uint32_t>{0x80000000U, 0x80000001U}));

    // 导出请求不能携带导入标记继续写priv。
    param.migrateInfos[0] = 0;
    EXPECT_TRUE(executor.ObmmExport(sizes, MAX_NUMA_NODES, param, desc, 128ULL << 20).empty());
    EXPECT_EQ(exportedBlocks, 2);
}

TEST_F(TestUbseObmmSwitch, SharedImportPrivClearsExportIdentityAndOrigin)
{
    auto& executor = RmObmmExecutor::GetInstance();
    MOCKER(&RmObmmExecutor::ObmmDevChangeUidGid).stubs().will(returnValue(mem_id{123}));
    executor.obmmImportFunc = [](const obmm_mem_desc* desc, unsigned long, int, int*) -> mem_id {
        UbseMemLocalObmmCustomMeta meta{};
        UbMemPrivData priv{};
        EXPECT_EQ(RmObmmMetaRestore::RestoreAgentLocalObmmMetaDataFromBuffer(desc->priv, desc->priv_len, meta, priv),
                  UBSE_OK);
        EXPECT_EQ(meta.migrateInfo, 0xffffU); // 保留version/index的最大值，清除整个高16位。
        EXPECT_FALSE(ShmMigrateIsExport(meta.migrateInfo));
        EXPECT_EQ(ShmMigrateOrigin(meta.migrateInfo), 0U);
        return 123;
    };
    ObmmOpParam param{};
    param.borrowType = UbseBorrowType::SHARE_BORROW;
    param.customMeta.type = static_cast<uint8_t>(UbseBorrowType::SHARE_BORROW);
    param.migrateInfos = {MakeShmMigrateInfo(15, 4095, 32767, true)};
    std::vector<UbseMemObmmInfo> desc(1);
    UbseMemImportStatus status{};
    status.decoderResult.resize(1);
    EXPECT_EQ(executor.ObmmImport(desc, param, status, nullptr).size(), 1);
}

// 结构体布局锁定（与 ubse_mem_types.h 中 static_assert 双保险）
TEST_F(TestUbseObmmSwitch, SwitchBeginImportTargetLayout)
{
    EXPECT_EQ(sizeof(struct obmm_switch_import_target), 56u);
    EXPECT_EQ(offsetof(struct obmm_switch_import_target, seid), 16u);
    EXPECT_EQ(offsetof(struct obmm_switch_import_target, base_dist), 48u);
    EXPECT_EQ(offsetof(struct obmm_switch_export_target, node_count), 16u * sizeof(size_t));
}

// 空指针防护：switch 符号为必选符号（缺失时 DlOpenLib 整体失败），此处仅验证封装方法的空指针路径
TEST_F(TestUbseObmmSwitch, SwitchFuncsNullptr_WrappersReturnNullptrError)
{
    auto& executor = RmObmmExecutor::GetInstance();
    executor.obmmSwitchBeginImportFunc = nullptr;
    executor.obmmSwitchBeginExportFunc = nullptr;
    executor.obmmSwitchEndFunc = nullptr;

    auto param = BuildSwitchParam(0);
    EXPECT_EQ(executor.ObmmSwitchBeginImport(param), UBSE_ERROR_NULLPTR);
    obmm_mem_desc desc{};
    ObmmSwitchExportParam exportParam{};
    EXPECT_EQ(executor.ObmmSwitchBeginExport(exportParam, desc), UBSE_ERROR_NULLPTR);
    EXPECT_EQ(executor.ObmmSwitchEnd(TEST_SWITCH_MEM_ID, true), UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseObmmSwitch, CapabilityFailureDoesNotCallSwitch)
{
    MOCKER(&UbseSecurityModule::ModifyEffectiveCapabilities).reset();
    MOCKER(&UbseSecurityModule::ModifyEffectiveCapabilities).stubs().will(returnValue(UBSE_ERROR));
    auto& executor = RmObmmExecutor::GetInstance();
    executor.obmmSwitchBeginImportFunc = [](mem_id, unsigned long, const obmm_switch_import_target*, const void*,
                                            uint16_t) {
        ADD_FAILURE() << "begin_import called without capability";
        return 0;
    };
    executor.obmmSwitchBeginExportFunc = [](mem_id, unsigned long, const obmm_switch_export_target*, obmm_mem_desc*,
                                            const void*, uint16_t) {
        ADD_FAILURE() << "begin_export called without capability";
        return 0;
    };
    executor.obmmSwitchEndFunc = [](mem_id, int) {
        ADD_FAILURE() << "end called without capability";
        return 0;
    };
    EXPECT_EQ(executor.ObmmSwitchBeginImport(BuildSwitchParam(0)), UBSE_ERR_PERMISSION_DENIED);
    ObmmSwitchExportParam param{};
    param.importMemId = TEST_SWITCH_MEM_ID;
    param.target.node_count = 1;
    param.target.size[0] = 128ULL << 20;
    obmm_mem_desc desc{};
    EXPECT_EQ(executor.ObmmSwitchBeginExport(param, desc), UBSE_ERR_PERMISSION_DENIED);
    EXPECT_EQ(executor.ObmmSwitchEnd(TEST_SWITCH_MEM_ID, true), UBSE_ERR_PERMISSION_DENIED);
    EXPECT_EQ(executor.ObmmSwitchEnd(TEST_SWITCH_MEM_ID, false), UBSE_ERR_PERMISSION_DENIED);
}

// 正常路径：字段转换逐字段断言 + flags 恒为 KEEP_OLD_ALIVE + priv 落点为结构体尾部连续内存
TEST_F(TestUbseObmmSwitch, SwitchBeginImport_Success_FieldRoundTrip)
{
    static struct obmm_switch_import_target captured {
    };
    static uint8_t capturedPriv[OBMM_MAX_PRIV_LEN] = {};
    static unsigned long capturedFlags = 0;
    static mem_id capturedId = 0;
    static uint16_t capturedLen = 0;
    auto& executor = RmObmmExecutor::GetInstance();
    executor.obmmSwitchBeginImportFunc = [](mem_id id, unsigned long flags,
                                            const struct obmm_switch_import_target* target, const void* priv,
                                            uint16_t privLen) {
        captured = *target;
        if (privLen > 0) {
            memcpy(capturedPriv, priv, privLen);
        }
        capturedId = id;
        capturedLen = privLen;
        capturedFlags = flags;
        return 0;
    };

    auto param = BuildSwitchParam(4);
    EXPECT_EQ(executor.ObmmSwitchBeginImport(param), UBSE_OK);
    EXPECT_EQ(capturedFlags, OBMM_SWITCH_FLAG_KEEP_OLD_ALIVE);
    EXPECT_EQ(capturedId, TEST_SWITCH_MEM_ID);
    EXPECT_EQ(captured.pa, TEST_NEW_PA);
    EXPECT_EQ(captured.scna, 0x1111);
    EXPECT_EQ(captured.dcna, 0x2222);
    EXPECT_EQ(captured.base_dist, 40);
    EXPECT_EQ(std::vector<uint8_t>(captured.seid, captured.seid + UBSE_EID_LENGTH),
              std::vector<uint8_t>(param.newSeid.begin(), param.newSeid.end()));
    EXPECT_EQ(std::vector<uint8_t>(captured.deid, captured.deid + UBSE_EID_LENGTH),
              std::vector<uint8_t>(param.newDeid.begin(), param.newDeid.end()));
    EXPECT_EQ(capturedLen, 4);
    EXPECT_EQ(std::vector<uint8_t>(capturedPriv, capturedPriv + capturedLen), param.priv);
}

TEST_F(TestUbseObmmSwitch, SwitchBeginImport_EmptyPriv)
{
    static const void* capturedPriv = reinterpret_cast<void*>(1);
    static uint16_t capturedLen = 1;
    auto& executor = RmObmmExecutor::GetInstance();
    executor.obmmSwitchBeginImportFunc = [](mem_id, unsigned long, const struct obmm_switch_import_target*,
                                            const void* priv, uint16_t len) {
        capturedPriv = priv;
        capturedLen = len;
        return 0;
    };

    auto param = BuildSwitchParam(0);
    EXPECT_EQ(executor.ObmmSwitchBeginImport(param), UBSE_OK);
    EXPECT_EQ(capturedPriv, nullptr);
    EXPECT_EQ(capturedLen, 0);
}

TEST_F(TestUbseObmmSwitch, SwitchBeginImport_PrivExceedsMaxLen)
{
    static int callCount = 0;
    callCount = 0;
    auto& executor = RmObmmExecutor::GetInstance();
    executor.obmmSwitchBeginImportFunc = [](mem_id, unsigned long, const struct obmm_switch_import_target*, const void*,
                                            uint16_t) {
        callCount++;
        return 0;
    };

    auto param = BuildSwitchParam(OBMM_MAX_PRIV_LEN + 1);
    EXPECT_EQ(executor.ObmmSwitchBeginImport(param), UBSE_ERROR_INVAL);
    EXPECT_EQ(callCount, 0); // 参数错误直接返回，不调用 obmm
}

// 错误码翻译：EBUSY 专用码，其余一般失败
TEST_F(TestUbseObmmSwitch, SwitchBeginImport_ErrnoTranslation)
{
    static int errToSet = 0;
    auto& executor = RmObmmExecutor::GetInstance();
    executor.obmmSwitchBeginImportFunc = [](mem_id, unsigned long, const struct obmm_switch_import_target*, const void*,
                                            uint16_t) {
        errno = errToSet;
        return -1;
    };

    auto param = BuildSwitchParam(0);

    errToSet = EBUSY;
    EXPECT_EQ(executor.ObmmSwitchBeginImport(param), UBSE_ERROR_BUSY);

    errToSet = EINVAL;
    EXPECT_EQ(executor.ObmmSwitchBeginImport(param), UBSE_MMI_OBMM_OP_FAILED);

    errToSet = ENOENT;
    EXPECT_EQ(executor.ObmmSwitchBeginImport(param), UBSE_MMI_OBMM_OP_FAILED);

    errToSet = EOPNOTSUPP;
    EXPECT_EQ(executor.ObmmSwitchBeginImport(param), UBSE_MMI_OBMM_OP_FAILED);
}

TEST_F(TestUbseObmmSwitch, SwitchBeginExport_DescAndPriv)
{
    auto& executor = RmObmmExecutor::GetInstance();
    executor.obmmSwitchBeginExportFunc = [](mem_id id, unsigned long flags,
                                            const struct obmm_switch_export_target* target, struct obmm_mem_desc* desc,
                                            const void* priv, uint16_t privLen) {
        EXPECT_EQ(id, TEST_SWITCH_MEM_ID);
        EXPECT_EQ(flags, OBMM_SWITCH_FLAG_KEEP_OLD_ALIVE);
        EXPECT_EQ(target->node_count, 1);
        EXPECT_EQ(target->size[0], 2u << 20);
        EXPECT_EQ(desc->seid[0], 1);
        EXPECT_EQ(desc->deid[0], 2);
        EXPECT_EQ(desc->length, 0);
        EXPECT_EQ(desc->addr, 0);
        EXPECT_EQ(privLen, 1);
        EXPECT_EQ(*static_cast<const uint8_t*>(priv), 3);
        desc->addr = 0x200000;
        desc->tokenid = 7;
        return 0;
    };
    ObmmSwitchExportParam param{};
    param.importMemId = TEST_SWITCH_MEM_ID;
    param.target.node_count = 1;
    param.target.size[0] = 2u << 20;
    param.seid[0] = 1;
    param.deid[0] = 2;
    param.priv = {3};
    obmm_mem_desc desc{};
    desc.length = 123;
    desc.addr = 456;
    EXPECT_EQ(executor.ObmmSwitchBeginExport(param, desc), UBSE_OK);
    EXPECT_EQ(desc.addr, 0x200000u);
    EXPECT_EQ(desc.tokenid, 7u);
}

TEST_F(TestUbseObmmSwitch, SwitchBeginExport_LastBlockUsesSameNumaDecision)
{
    auto& executor = RmObmmExecutor::GetInstance();
    executor.obmmSwitchBeginExportFunc = [](mem_id, unsigned long, const struct obmm_switch_export_target* target,
                                            struct obmm_mem_desc* desc, const void* priv, uint16_t privLen) {
        EXPECT_EQ(target->node_count, 3);
        EXPECT_EQ(target->size[0], 0U);
        EXPECT_EQ(target->size[1], 0U);
        EXPECT_EQ(target->size[2], 128ULL << 20);
        EXPECT_EQ(privLen, sizeof(UbMemPrivData) + sizeof(ObmmCustomMetaV2));
        UbseMemLocalObmmCustomMeta meta{};
        UbMemPrivData prefix{};
        EXPECT_EQ(DecodeObmmCustomMeta(static_cast<const uint8_t*>(priv), privLen, meta, prefix), UBSE_OK);
        EXPECT_EQ(meta.numaSizes[0], 128ULL << 20);
        EXPECT_EQ(meta.exportNumaIds[0], 2);
        EXPECT_EQ(meta.memidCount, 8);
        EXPECT_EQ(meta.migrateInfo, 0x800b1007U);
        EXPECT_TRUE(ShmMigrateIsExport(meta.migrateInfo));
        EXPECT_EQ(ShmMigrateOrigin(meta.migrateInfo), 11U);
        desc->addr = 0x200000;
        return 0;
    };
    ubse::adapter_plugins::mmi::UbseMemShmSwitchExportObj request{};
    request.importMemId = TEST_SWITCH_MEM_ID;
    request.blockIndex = 7;
    request.numaSizes = {0, 0, 128ULL << 20};
    request.exportObj.req.name = "mapped_eight_blocks";
    request.exportObj.req.requestNodeId = "2";
    request.exportObj.algoResult.exportNumaInfos = {{"2", 0, 2, 1024ULL << 20}};
    // 尚无导出结果，总设备数由完整迁移标记提供。
    request.exportObj.migrateInfos.resize(8);
    request.exportObj.migrateInfos[7] = MakeShmMigrateInfo(1, 7, 11, true);
    ubse_mem_obmm_mem_desc desc{};
    EXPECT_EQ(MemInstanceInnerShm::GetInstance().MemShmSwitchBeginExportExecutor(request, desc), UBSE_OK);
    EXPECT_EQ(desc.length, 128ULL << 20);
    ASSERT_EQ(request.exportObj.algoResult.exportNumaInfos.size(), 1);
    EXPECT_EQ(request.exportObj.algoResult.exportNumaInfos.front().size, 1024ULL << 20);
    request.exportObj.migrateInfos[7] = ShmMigrateImportInfo(request.exportObj.migrateInfos[7]);
    EXPECT_EQ(MemInstanceInnerShm::GetInstance().MemShmSwitchBeginExportExecutor(request, desc), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseObmmSwitch, SwitchBeginExport_UsesOnlyReturnedCredentials)
{
    auto& executor = RmObmmExecutor::GetInstance();
    executor.obmmSwitchBeginExportFunc = [](mem_id, unsigned long, const struct obmm_switch_export_target* target,
                                            struct obmm_mem_desc* desc, const void*, uint16_t) {
        EXPECT_EQ(target->node_count, 2);
        for (size_t i = 2; i < OBMM_MAX_LOCAL_NUMA_NODES; ++i) {
            EXPECT_EQ(target->size[i], 0);
        }
        // 未约定的返回字段不能作为账本来源，包括未回填的 length。
        desc->length = 0;
        desc->scna = 123;
        desc->dcna = 456;
        std::memset(desc->seid, 0xff, sizeof(desc->seid));
        std::memset(desc->deid, 0xff, sizeof(desc->deid));
        desc->addr = 0x200000;
        desc->tokenid = 7;
        return 0;
    };
    ubse::adapter_plugins::mmi::UbseMemShmSwitchExportObj request{};
    request.importMemId = TEST_SWITCH_MEM_ID;
    request.numaSizes = {2u << 20, 4u << 20};
    request.priv = {3};
    request.seid[0] = 0x12;
    request.seid[1] = 0x34;
    request.deid[0] = 0x4a;
    request.deid[2] = 0x04;
    ubse_mem_obmm_mem_desc desc{};
    desc.marId = 99;
    EXPECT_EQ(MemInstanceInnerShm::GetInstance().MemShmSwitchBeginExportExecutor(request, desc), UBSE_OK);
    EXPECT_EQ(desc.addr, 0x200000u);
    EXPECT_EQ(desc.tokenid, 7u);
    EXPECT_EQ(desc.length, 6u << 20);
    EXPECT_EQ(desc.seid, 0x3412u);
    EXPECT_EQ(desc.deid, 0x4004au);
    EXPECT_EQ(desc.scna, 0u);
    EXPECT_EQ(desc.dcna, 0u);
    EXPECT_EQ(desc.marId, 0u);
}

TEST_F(TestUbseObmmSwitch, SwitchBeginExport_InvalidInputDoesNotReachObmm)
{
    auto& executor = RmObmmExecutor::GetInstance();
    executor.obmmSwitchBeginExportFunc = [](mem_id, unsigned long, const struct obmm_switch_export_target*,
                                            struct obmm_mem_desc*, const void*, uint16_t) {
        ADD_FAILURE() << "Invalid MMI input must not reach obmm";
        return 0;
    };
    ubse::adapter_plugins::mmi::UbseMemShmSwitchExportObj request{};
    request.importMemId = TEST_SWITCH_MEM_ID;
    request.priv = {3};
    ubse_mem_obmm_mem_desc desc{};
    auto& inner = MemInstanceInnerShm::GetInstance();
    EXPECT_EQ(inner.MemShmSwitchBeginExportExecutor(request, desc), UBSE_ERROR_INVAL);
    request.numaSizes.assign(OBMM_MAX_LOCAL_NUMA_NODES + 1, 2u << 20);
    EXPECT_EQ(inner.MemShmSwitchBeginExportExecutor(request, desc), UBSE_ERROR_INVAL);
    request.numaSizes = {2u << 20};
    request.importMemId = 0;
    EXPECT_EQ(inner.MemShmSwitchBeginExportExecutor(request, desc), UBSE_ERROR_INVAL);
    request.importMemId = TEST_SWITCH_MEM_ID;
    request.priv.resize(OBMM_MAX_PRIV_LEN + 1);
    EXPECT_EQ(inner.MemShmSwitchBeginExportExecutor(request, desc), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseObmmSwitch, SwitchEnd_SuccessAndSuccessParamMapping)
{
    static int capturedSuccess = -1;
    auto& executor = RmObmmExecutor::GetInstance();
    executor.obmmSwitchEndFunc = [](mem_id id, int success) {
        (void)id;
        capturedSuccess = success;
        return 0;
    };

    EXPECT_EQ(executor.ObmmSwitchEnd(TEST_SWITCH_MEM_ID, true), UBSE_OK);
    EXPECT_EQ(capturedSuccess, 1);
    EXPECT_EQ(executor.ObmmSwitchEnd(TEST_SWITCH_MEM_ID, false), UBSE_OK);
    EXPECT_EQ(capturedSuccess, 0);
}

// switch_end 三态：EAGAIN 可重试 / ESTALE 切换已不存在，不得混同一般失败
TEST_F(TestUbseObmmSwitch, SwitchEnd_ErrnoTranslation)
{
    static int errToSet = 0;
    auto& executor = RmObmmExecutor::GetInstance();
    executor.obmmSwitchEndFunc = [](mem_id id, int success) {
        (void)id;
        (void)success;
        errno = errToSet;
        return -1;
    };

    errToSet = EAGAIN;
    EXPECT_EQ(executor.ObmmSwitchEnd(TEST_SWITCH_MEM_ID, true), UBSE_ERROR_AGAIN);

    errToSet = ESTALE;
    EXPECT_EQ(executor.ObmmSwitchEnd(TEST_SWITCH_MEM_ID, true), UBSE_MMI_OBMM_SWITCH_NOT_EXIST);

    errToSet = EINVAL;
    EXPECT_EQ(executor.ObmmSwitchEnd(TEST_SWITCH_MEM_ID, true), UBSE_MMI_OBMM_OP_FAILED);
}

TEST_F(TestUbseObmmSwitch, MemcpyMtCopiesWithEightThreads)
{
    auto& executor = RmObmmExecutor::GetInstance();
    auto saved = executor.memcpyMtFunc;
    executor.memcpyMtFunc = [](void* dst, const void* src, size_t size, unsigned int threads) -> void* {
        EXPECT_EQ(8U, threads);
        return std::memcpy(dst, src, size);
    };
    const char src[] = "migration";
    char dst[sizeof(src)]{};
    EXPECT_EQ(UBSE_OK, executor.MemcpyMt(dst, src, sizeof(src)));
    EXPECT_EQ(0, std::memcmp(dst, src, sizeof(src)));
    executor.memcpyMtFunc = nullptr;
    EXPECT_EQ(UBSE_ERROR_INVAL, executor.MemcpyMt(dst, src, sizeof(src)));
    executor.memcpyMtFunc = saved;
}
} // namespace ubse::ut::mmi
