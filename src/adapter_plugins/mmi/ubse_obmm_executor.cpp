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
#include "ubse_obmm_executor.h"
#include <fcntl.h>
#include <securec.h>
#include <sys/mman.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <csignal>
#include <cstring>
#include "ubse_conf_module.h"
#include "ubse_mem_common_utils.h"
#include "ubse_mem_def.h"
#include "ubse_obmm_meta_restore.h"
#include "ubse_obmm_utils.h"
#include "src/controllers/mem/mem_decoder_utils/ubse_mem_prehandle_manager.h"

namespace ubse::mmi {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::security;
using namespace ubse::context;
using ubse::adapter_plugins::mmi::MakeShmMigrateInfo;
using ubse::adapter_plugins::mmi::SHM_MIGRATE_MAX_BLOCKS;
using ubse::adapter_plugins::mmi::ShmMigrateImportInfo;
using ubse::adapter_plugins::mmi::ShmMigrateIndex;
using ubse::adapter_plugins::mmi::ShmMigrateIsExport;
using ubse::adapter_plugins::mmi::ShmMigrateVersion;

std::vector<__u32> overrideCap = {CAP_DAC_OVERRIDE};
static std::vector<__u32> switchCap = {CAP_SYS_ADMIN};
static const std::string OBMM_LOG_INFO = "#######UB[OBMM]########";
static constexpr uint64_t OFFLINE_TIMEOUT_MIN_S = 10;
static constexpr uint64_t OFFLINE_TIMEOUT_MAX_S = 1800;
static constexpr int OBMM_IMPORT_BASE_DIST = 40;

uint64_t RmObmmExecutor::offlineTimeoutMs_ = DEFAULT_UNIMPORT_TIMEOUT_MS;
bool RmObmmExecutor::offlineTimeoutConfigured_ = false;

UbseResult RmObmmExecutor::MemcpyMt(void* dst, const void* src, size_t size)
{
    constexpr unsigned int COPY_THREADS = 8;
    if (memcpyMtFunc == nullptr || dst == nullptr || src == nullptr || size == 0) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "memcpy_mt unavailable or invalid arguments, size=" << size;
        return UBSE_ERROR_INVAL;
    }
    if (memcpyMtFunc(dst, src, size, COPY_THREADS) != dst) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "memcpy_mt returned unexpected destination, size=" << size;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult RmObmmExecutor::Init()
{
    auto ret = RmObmmUtils::GetInstance().GetPreOnlineSwitch(preOnlineSwitch);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "GetPreOnlineSwitch failed.";
        return ret;
    }
    ret = DlOpenLib(OBMM_PATH);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get obmm funcs failed from libobmm.so.";
        return ret;
    }
    uint64_t timeoutMs = DEFAULT_UNIMPORT_TIMEOUT_MS;
    auto configModule = UbseContext::GetInstance().GetModule<ubse::config::UbseConfModule>();
    if (configModule != nullptr &&
        configModule->GetConf("ubse.memory", OBMM_OFFLINE_TIMEOUT_CONFIG_KEY, timeoutMs) == UBSE_OK) {
        if (timeoutMs >= OFFLINE_TIMEOUT_MIN_S && timeoutMs <= OFFLINE_TIMEOUT_MAX_S) {
            offlineTimeoutConfigured_ = true;
            offlineTimeoutMs_ = timeoutMs * MS_PER_SECOND;
            UBSE_LOG_INFO << MMI_LOG_INFO << "obmm.memory.offline.timeout configured, value=" << timeoutMs << "s";
        } else {
            UBSE_LOG_WARN << MMI_LOG_INFO << "obmm.memory.offline.timeout=" << timeoutMs << " out of range ["
                          << OFFLINE_TIMEOUT_MIN_S << ", " << OFFLINE_TIMEOUT_MAX_S << "], use original calculation";
        }
    } else {
        UBSE_LOG_INFO << MMI_LOG_INFO << "obmm.memory.offline.timeout not configured, use original calculation";
    }
    RegisterSigusr1Handler();
    return UBSE_OK;
}

void RmObmmExecutor::RegisterSigusr1Handler()
{
    struct sigaction sa {
    };
    sa.sa_handler = [](int) {
        const char msg[] = "SIGUSR1 received, obmm_unimport interrupted\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
    };
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, nullptr);
}

UbseResult RmObmmExecutor::Exit()
{
    UBSE_LOG_INFO << MMI_LOG_INFO << "Close obmm executor.";
    memcpyMtFunc = nullptr;
    if (accelHandle != nullptr) {
        if (dlclose(accelHandle) != 0) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Close libobmm-accel.so.1 failed, error is " << dlerror();
            return UBSE_ERROR;
        }
        accelHandle = nullptr;
    }
    return UBSE_OK;
}

UbseResult RmObmmExecutor::DlOpenLib(const std::string& obmmPath)
{
    handle = dlopen(obmmPath.c_str(), RTLD_NOW);
    if (handle == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Dlopen obmm.so failed, error is " << dlerror();
        return UBSE_ERROR_INVAL;
    }
    auto ret = DlOpenFunName(obmmExportFunc, "obmm_export");
    if (UBSE_RESULT_FAIL(ret)) {
        return ret;
    }
    ret = DlOpenFunName(obmmUnexportFunc, "obmm_unexport");
    if (UBSE_RESULT_FAIL(ret)) {
        return ret;
    }
    ret = DlOpenFunName(obmmImportFunc, "obmm_import");
    if (UBSE_RESULT_FAIL(ret)) {
        return ret;
    }
    ret = DlOpenFunName(obmmUnimportFunc, "obmm_unimport");
    if (UBSE_RESULT_FAIL(ret)) {
        return ret;
    }
    ret = DlOpenFunName(obmmExportByPidFunc, "obmm_export_useraddr");
    if (UBSE_RESULT_FAIL(ret)) {
        return ret;
    }
    ret = DlOpenFunName(obmmQueryPaByMemIdFunc, "obmm_query_pa_by_memid");
    if (UBSE_RESULT_FAIL(ret)) {
        return ret;
    }
    ret = DlOpenFunName(obmmPreImportFunc, "obmm_preimport");
    if (UBSE_RESULT_FAIL(ret)) {
        return ret;
    }
    ret = DlOpenFunName(obmmUnPreImportFunc, "obmm_unpreimport");
    if (UBSE_RESULT_FAIL(ret)) {
        return ret;
    }
    ret = DlOpenFunName(obmmSwitchBeginImportFunc, "obmm_switch_begin_import");
    if (UBSE_RESULT_FAIL(ret)) {
        return ret;
    }
    ret = DlOpenFunName(obmmSwitchBeginExportFunc, "obmm_switch_begin_export");
    if (UBSE_RESULT_FAIL(ret)) {
        return ret;
    }
    ret = DlOpenFunName(obmmSwitchEndFunc, "obmm_switch_end");
    if (UBSE_RESULT_FAIL(ret)) {
        return ret;
    }
    // memcpy_mt 本次暂不启用，避免 libobmm-accel 加载失败影响其他 OBMM 功能启动。
    // ret = LoadMemcpyMt();
    return UBSE_OK;
}

UbseResult RmObmmExecutor::LoadMemcpyMt()
{
    if (memcpyMtFunc != nullptr) {
        return UBSE_OK;
    }
    constexpr auto ACCEL_PATH = "libobmm-accel.so.1";
    accelHandle = dlopen(ACCEL_PATH, RTLD_NOW | RTLD_LOCAL);
    if (accelHandle == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Dlopen " << ACCEL_PATH << " failed, error is " << dlerror();
        return UBSE_ERROR;
    }
    dlerror();
    memcpyMtFunc = reinterpret_cast<MemcpyMtPtr>(dlsym(accelHandle, "memcpy_mt"));
    const auto error = dlerror();
    if (error != nullptr || memcpyMtFunc == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get memcpy_mt from " << ACCEL_PATH << " failed, error is "
                       << (error != nullptr ? error : "null symbol");
        memcpyMtFunc = nullptr;
        dlclose(accelHandle);
        accelHandle = nullptr;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

static std::string eid_bytes_to_hex_dwords(const uint8_t* data, size_t len)
{
    std::ostringstream oss;
    oss << std::hex;
    const uint32_t constant_8 = 8; // 8字节

    uint64_t high = 0;
    for (int i = 0; i < constant_8; ++i) {                          // 8位
        high |= static_cast<uint64_t>(data[i]) << (i * constant_8); // 小端
    }

    uint64_t low = 0;
    for (int i = constant_8; i < len; ++i) {
        low |= static_cast<uint64_t>(data[i]) << ((i - constant_8) * constant_8); // 小端
    }

    oss << "0x" << low << ":0x" << high;
    return oss.str();
}

static void PrintObmmMemDesc(const obmm_mem_desc& obmmMemDesc)
{
    auto deid = eid_bytes_to_hex_dwords(obmmMemDesc.deid, UBSE_EID_LENGTH);
    auto seid = eid_bytes_to_hex_dwords(obmmMemDesc.seid, UBSE_EID_LENGTH);
    UBSE_LOG_DEBUG << MMI_LOG_INFO << "obmmMemDesc.scna=" << obmmMemDesc.scna;
    UBSE_LOG_DEBUG << MMI_LOG_INFO << "obmmMemDesc.length=" << obmmMemDesc.length;
    UBSE_LOG_DEBUG << MMI_LOG_INFO << "obmmMemDesc.dcna=" << obmmMemDesc.dcna;
    UBSE_LOG_DEBUG << MMI_LOG_INFO << "obmmMemDesc.tokenid=" << obmmMemDesc.tokenid;
    UBSE_LOG_DEBUG << MMI_LOG_INFO << "obmmMemDesc.addr=" << obmmMemDesc.addr;
    UBSE_LOG_DEBUG << MMI_LOG_INFO << "obmmMemDesc.deid=" << deid;
    UBSE_LOG_DEBUG << MMI_LOG_INFO << "obmmMemDesc.seid=" << seid;
    UBSE_LOG_DEBUG << MMI_LOG_INFO << "obmmMemDesc.scna=" << obmmMemDesc.scna;
    UBSE_LOG_DEBUG << MMI_LOG_INFO << "obmmMemDesc.priv_len=" << obmmMemDesc.priv_len;
}

/**
 * 导出内存。
 * 根据提供的大小和标志，使用OBMM（内存管理模块）导出内存，并返回内存描述符。
 * @param size 每个NUMA节点的内存大小数组。
 * @param opParam
 * @param flags 导出内存的标志。
 * @param desc 导出内存后的描述符。
 * @return 成功时返回导出的内存ID，失败时返回INVALID_MEM_ID。
 */
mem_id RmObmmExecutor::ObmmExport(size_t size[MAX_NUMA_NODES], int arraySize, const ObmmOpParam& opParam,
                                  ubse_mem_obmm_mem_desc& desc)
{
    UBSE_LOG_DEBUG << MMI_LOG_INFO << OBMM_LOG_INFO << "Start to use Obmm Export interface, opParam is "
                   << opParam.toString() << ", size list is: ";
    for (int i = 0; i < MAX_NUMA_NODES; ++i) {
        if (size[i] != 0) {
            UBSE_LOG_DEBUG << MMI_LOG_INFO << OBMM_LOG_INFO << "numaid=" << i << ", size=" << size[i];
        }
    }
    auto obmmFlags = 0;
    if (opParam.borrowType == UbseBorrowType::FD_BORROW || opParam.borrowType == UbseBorrowType::SHARE_BORROW) {
        obmmFlags = obmmFlags | OBMM_EXPORT_FLAG_ALLOW_MMAP;
    }
    for (int i = 0; i < MAX_NUMA_NODES; ++i) {
        if (size[i] > 0) {
            UBSE_LOG_INFO << MMI_LOG_INFO << OBMM_LOG_INFO << "obmm_export numa=" << i << ", size=" << size[i]
                          << ", flag=" << obmmFlags;
        }
    }
    if (obmmExportFunc == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ObmmExportFunc is nullptr, please check.";
        return INVALID_MEM_ID;
    }
    auto obmmMemDesc = ConstructExportMemDesc(opParam.customMeta, opParam.privData);
    if (obmmMemDesc == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ConstructExportMemDesc return null.";
        return INVALID_MEM_ID;
    }
    CapabilityGuard overrideCapGuard(overrideCap);
    mem_id memId = obmmExportFunc(size, obmmFlags, obmmMemDesc);
    overrideCapGuard.Disable();
    UBSE_LOG_INFO << MMI_LOG_INFO << OBMM_LOG_INFO << "ObmmExport returned! memid=" << memId
                  << ", obmmFlags=" << obmmFlags;
    PrintObmmMemDesc(*obmmMemDesc);
    if (memId == INVALID_MEM_ID) {
        char buf[STR_ERROR_BUF_SIZE] = {0};
        UBSE_LOG_ERROR << MMI_LOG_INFO << OBMM_LOG_INFO << "ObmmExport error! memid=" << memId << ", flag=" << obmmFlags
                       << ", errno=" << errno
                       << ", errMsg=" << RmCommonUtils::GetInstance().GetStrError(errno, buf, STR_ERROR_BUF_SIZE);
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        return memId;
    }
    CopyObmmMemDescValue(obmmMemDesc, desc);
    RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
    return ObmmDevChangeUidGid(memId, false, opParam);
}

/**
 * 取消导出内存。
 * 根据提供的内存ID，使用OBMM取消导出内存。
 * @param id 要取消导出的内存ID。
 * @return 成功时返回成功标志，失败时返回错误代码。
 */
UbseResult RmObmmExecutor::ObmmUnExport(mem_id id)
{
    UBSE_LOG_DEBUG << MMI_LOG_INFO << OBMM_LOG_INFO << "Start to use Obmm Unexport interface, memid=" << id;
    unsigned long flags = 0;
    if (obmmUnexportFunc == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ObmmUnexportFunc is nullptr, please check.";
        return UBSE_ERROR_NULLPTR;
    }
    CapabilityGuard overrideCapGuard(overrideCap);
    auto ret = obmmUnexportFunc(id, flags);
    overrideCapGuard.Disable();
    if (ret && errno != ENOENT) {
        char buf[STR_ERROR_BUF_SIZE] = {0};
        UBSE_LOG_ERROR << MMI_LOG_INFO << OBMM_LOG_INFO << "ObmmUnExport error! memid=" << id << ", ret=" << ret
                       << ", flag=" << flags << ", errno=" << errno
                       << ", errMsg=" << RmCommonUtils::GetInstance().GetStrError(errno, buf, STR_ERROR_BUF_SIZE);
        return UBSE_MMI_OBMM_OP_FAILED;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << OBMM_LOG_INFO << "obmm_unexport ok memid=" << id << ", flag=" << flags;
    return UBSE_OK;
}

/**
 * 导入内存。
 * 根据提供的内存描述符、标志和NUMA节点ID，使用OBMM导入内存。
 * @param desc 内存描述符。
 * @param opParam obmm
 * @param flags 导入内存的标志。
 * @param numa 导入的NUMA节点ID。
 * @return 成功时返回导入的内存ID，失败时返回INVALID_MEM_ID。
 */
mem_id RmObmmExecutor::ObmmImport(const ubse_mem_obmm_mem_desc& desc, const ObmmOpParam& opParam, int* numa)
{
    if (numa != nullptr) {
        UBSE_LOG_INFO << MMI_LOG_INFO << "numa is" << *numa;
        if (*numa == 0) {
            return INVALID_MEM_ID;
        }
    }
    UBSE_LOG_DEBUG << MMI_LOG_INFO << OBMM_LOG_INFO << "Start to use Obmm Import interface, opParam is "
                   << opParam.toString();
    auto obmmFlags = 0;
    if (opParam.borrowType == UbseBorrowType::NUMA_BORROW || opParam.borrowType == UbseBorrowType::ADDR_BORROW) {
        obmmFlags = obmmFlags | OBMM_IMPORT_FLAG_NUMA_REMOTE;
    }
    if (opParam.borrowType == UbseBorrowType::FD_BORROW || opParam.borrowType == UbseBorrowType::SHARE_BORROW) {
        obmmFlags = obmmFlags | OBMM_IMPORT_FLAG_ALLOW_MMAP;
    }
    if (opParam.borrowType == UbseBorrowType::NUMA_BORROW && opParam.customMeta.decoderResult.staticHandle != 0) {
        obmmFlags = obmmFlags | OBMM_IMPORT_FLAG_PREIMPORT;
    }
    auto obmmMemDesc = ConstructImportMemDesc(opParam, desc);
    if (obmmMemDesc == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ConstructImportMemDesc return null.";
        return INVALID_MEM_ID;
    }
    PrintObmmMemDesc(*obmmMemDesc);
    mem_id memid = INVALID_MEM_ID;
    if (obmmImportFunc == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ObmmImportFunc is nullptr, please check.";
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        return INVALID_MEM_ID;
    }
    CapabilityGuard overrideCapGuard(overrideCap);
    memid = obmmImportFunc(obmmMemDesc, obmmFlags, OBMM_IMPORT_BASE_DIST, numa);
    overrideCapGuard.Disable();
    if (memid == INVALID_MEM_ID) {
        char buf[STR_ERROR_BUF_SIZE] = {0};
        UBSE_LOG_ERROR << MMI_LOG_INFO << OBMM_LOG_INFO << "ObmmImport error! memid=" << memid << ", flag=" << obmmFlags
                       << ", errno=" << errno
                       << ", errMsg=" << RmCommonUtils::GetInstance().GetStrError(errno, buf, STR_ERROR_BUF_SIZE);
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        return memid;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << OBMM_LOG_INFO << " name=" << std::string(opParam.customMeta.name)
                  << ", opParam=" << opParam.toString() << ", obmm importMemid=" << memid
                  << ", obmm exportMemid=" << opParam.customMeta.exportMemid
                  << ", exportNodeId=" << opParam.customMeta.exportNodeId;
    RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
    return ObmmDevChangeUidGid(memid, true, opParam);
}

/**
 * 取消导入内存。
 * 根据提供的内存ID和标志，使用OBMM取消导入内存。
 * @param id 要取消导入的内存ID。
 * @param flags 取消导入的标志。
 * @return 成功时返回成功标志，失败时返回错误代码。
 */
UbseResult RmObmmExecutor::ObmmUnImport(mem_id id)
{
    UBSE_LOG_DEBUG << MMI_LOG_INFO << OBMM_LOG_INFO << "Start to use Obmm Unimport interface, mem_id=" << id;
    auto obmmFlags = 0;
    int ret;
    errno = 0;
    if (obmmUnimportFunc == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ObmmUnimportFunc is nullptr, please check.";
        return UBSE_ERROR_NULLPTR;
    }
    CapabilityGuard overrideCapGuard(overrideCap);
    ret = obmmUnimportFunc(id, obmmFlags);
    overrideCapGuard.Disable();
    if (ret && errno != ENOENT) {
        char buf[STR_ERROR_BUF_SIZE] = {0};
        UBSE_LOG_ERROR << MMI_LOG_INFO << OBMM_LOG_INFO << "obmm_unimport error! memid=" << id << ", ret=" << ret
                       << ", flag=" << obmmFlags << ", errno=" << errno
                       << ", errMsg=" << RmCommonUtils::GetInstance().GetStrError(errno, buf, STR_ERROR_BUF_SIZE);
        return UBSE_MMI_OBMM_OP_FAILED;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << OBMM_LOG_INFO << "obmm_unimport memid=" << id << ", ret=" << ret;
    return UBSE_OK;
}

UbseResult RmObmmExecutor::ObmmUnImport(mem_id id, uint64_t timeoutMs)
{
    UBSE_LOG_DEBUG << MMI_LOG_INFO << OBMM_LOG_INFO
                   << "Start to use Obmm Unimport with timeout interface, mem_id=" << id << ", timeoutMs=" << timeoutMs;
    auto obmmFlags = 0;
    int ret;
    errno = 0;
    if (obmmUnimportFunc == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ObmmUnimportFunc is nullptr, please check.";
        return UBSE_ERROR_NULLPTR;
    }

    pthread_t targetTid = pthread_self();
    UbseMmiTimeoutGuard guard(timeoutMs, [targetTid]() {
        UBSE_LOG_WARN << MMI_LOG_INFO << "obmm_unimport timeout, sending SIGUSR1 to interrupt target thread.";
        pthread_kill(targetTid, SIGUSR1);
    });

    CapabilityGuard overrideCapGuard(overrideCap);
    ret = obmmUnimportFunc(id, obmmFlags);
    overrideCapGuard.Disable();

    guard.Cancel();

    if (ret && errno != ENOENT) {
        char buf[STR_ERROR_BUF_SIZE] = {0};
        if (errno == EINTR) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << OBMM_LOG_INFO << "obmm_unimport interrupted by timeout, memid=" << id;
            return UBSE_MMI_OBMM_OP_TIMEOUT;
        }
        UBSE_LOG_ERROR << MMI_LOG_INFO << OBMM_LOG_INFO << "obmm_unimport error! memid=" << id << ", ret=" << ret
                       << ", flag=" << obmmFlags << ", errno=" << errno
                       << ", errMsg=" << RmCommonUtils::GetInstance().GetStrError(errno, buf, STR_ERROR_BUF_SIZE);
        return UBSE_MMI_OBMM_OP_FAILED;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << OBMM_LOG_INFO << "obmm_unimport memid=" << id << ", ret=" << ret;
    return UBSE_OK;
}

/**
 * 开始切换引入内存的物理后端（两阶段切换第一阶段，契约见 obmm_switch.md）。
 * flags 固定置 KEEP_OLD_ALIVE：迁移场景旧链路预期仍有效，中止时可回退 pa0（Step1 设计 §5.3）。
 */
UbseResult RmObmmExecutor::ObmmSwitchBeginImport(const ObmmSwitchParam& param)
{
    UBSE_LOG_DEBUG << MMI_LOG_INFO << OBMM_LOG_INFO
                   << "Start to use Obmm SwitchBeginImport interface, memId=" << param.oldImportMemId;
    if (obmmSwitchBeginImportFunc == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ObmmSwitchBeginImportFunc is nullptr, please check.";
        return UBSE_ERROR_NULLPTR;
    }
    if (param.priv.size() > OBMM_MAX_PRIV_LEN) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << OBMM_LOG_INFO << "ObmmSwitchBeginImport priv len " << param.priv.size()
                       << " exceeds OBMM_MAX_PRIV_LEN " << OBMM_MAX_PRIV_LEN << ", memId=" << param.oldImportMemId;
        return UBSE_ERROR_INVAL;
    }
    obmm_switch_import_target target{};
    const uint16_t privLen = static_cast<uint16_t>(param.priv.size());
    target.pa = param.newPa;
    target.scna = param.newScna;
    target.dcna = param.newDcna;
    target.base_dist = param.baseDist;
    std::copy(param.newSeid.begin(), param.newSeid.end(), target.seid);
    std::copy(param.newDeid.begin(), param.newDeid.end(), target.deid);
    const void* priv = param.priv.empty() ? nullptr : param.priv.data();
    errno = 0;
    UbseSecurityModule::ModifyEffectiveCapabilities(switchCap, true);
    int ret = obmmSwitchBeginImportFunc(param.oldImportMemId, OBMM_SWITCH_FLAG_KEEP_OLD_ALIVE, &target, priv, privLen);
    const int switchErrno = errno;
    UbseSecurityModule::ModifyEffectiveCapabilities(switchCap, false);
    if (ret != 0) {
        char buf[STR_ERROR_BUF_SIZE] = {0};
        UBSE_LOG_ERROR << MMI_LOG_INFO << OBMM_LOG_INFO
                       << "obmm_switch_begin_import error! memId=" << param.oldImportMemId << ", ret=" << ret
                       << ", errno=" << switchErrno
                       << ", errMsg=" << RmCommonUtils::GetInstance().GetStrError(switchErrno, buf, STR_ERROR_BUF_SIZE);
        if (switchErrno == EBUSY) {
            // 该内存已有切换进行中：不是失败，说明上一轮 begin 已生效，编排层按"begin 已完成"处理
            return UBSE_ERROR_BUSY;
        }
        return UBSE_MMI_OBMM_OP_FAILED;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << OBMM_LOG_INFO << "obmm_switch_begin_import success, memId=" << param.oldImportMemId
                  << ", privLen=" << privLen;
    return UBSE_OK;
}

UbseResult RmObmmExecutor::ObmmSwitchBeginExport(const ObmmSwitchExportParam& param, struct obmm_mem_desc& desc)
{
    if (obmmSwitchBeginExportFunc == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << OBMM_LOG_INFO
                       << "ObmmSwitchBeginExportFunc is nullptr, memId=" << param.importMemId;
        return UBSE_ERROR_NULLPTR;
    }
    if (param.importMemId == INVALID_MEM_ID || param.priv.size() > OBMM_MAX_PRIV_LEN || param.target.node_count < 1 ||
        param.target.node_count > static_cast<int>(OBMM_MAX_LOCAL_NUMA_NODES)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << OBMM_LOG_INFO
                       << "ObmmSwitchBeginExport invalid parameters, memId=" << param.importMemId
                       << ", nodeCount=" << param.target.node_count << ", privLen=" << param.priv.size();
        return UBSE_ERROR_INVAL;
    }
    for (int i = param.target.node_count; i < static_cast<int>(OBMM_MAX_LOCAL_NUMA_NODES); ++i) {
        if (param.target.size[i] != 0) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << OBMM_LOG_INFO
                           << "ObmmSwitchBeginExport nonzero unused NUMA size, memId=" << param.importMemId
                           << ", numaIndex=" << i << ", size=" << param.target.size[i];
            return UBSE_ERROR_INVAL;
        }
    }
    auto memsetRet = memset_s(&desc, sizeof(desc), 0, sizeof(desc));
    if (memsetRet != EOK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << OBMM_LOG_INFO << "ObmmSwitchBeginExport memset desc failed.";
        return UBSE_MMI_OBMM_OP_FAILED;
    }
    std::copy(param.seid.begin(), param.seid.end(), desc.seid);
    std::copy(param.deid.begin(), param.deid.end(), desc.deid);
    const void* priv = param.priv.empty() ? nullptr : param.priv.data();
    errno = 0;
    UbseSecurityModule::ModifyEffectiveCapabilities(switchCap, true);
    const int ret = obmmSwitchBeginExportFunc(param.importMemId, OBMM_SWITCH_FLAG_KEEP_OLD_ALIVE, &param.target, &desc,
                                              priv, static_cast<uint16_t>(param.priv.size()));
    const int switchErrno = errno;
    UbseSecurityModule::ModifyEffectiveCapabilities(switchCap, false);
    if (ret == 0) {
        UBSE_LOG_INFO << MMI_LOG_INFO << OBMM_LOG_INFO << "obmm_switch_begin_export end, memId=" << param.importMemId
                      << ", ret=" << ret << ", hasAddress=" << (desc.addr != 0);
        return UBSE_OK;
    }
    char buf[STR_ERROR_BUF_SIZE] = {0};
    UBSE_LOG_ERROR << MMI_LOG_INFO << OBMM_LOG_INFO << "obmm_switch_begin_export error, memId=" << param.importMemId
                   << ", ret=" << ret << ", errno=" << switchErrno
                   << ", errMsg=" << RmCommonUtils::GetInstance().GetStrError(switchErrno, buf, STR_ERROR_BUF_SIZE);
    if (switchErrno == EBUSY) {
        return UBSE_ERROR_BUSY;
    }
    return UBSE_MMI_OBMM_OP_FAILED;
}

UbseResult RmObmmExecutor::ObmmSwitchEnd(mem_id oldImportMemId, bool success)
{
    UBSE_LOG_DEBUG << MMI_LOG_INFO << OBMM_LOG_INFO << "Start to use Obmm SwitchEnd interface, memId=" << oldImportMemId
                   << ", success=" << success;
    if (obmmSwitchEndFunc == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ObmmSwitchEndFunc is nullptr, please check.";
        return UBSE_ERROR_NULLPTR;
    }
    errno = 0;
    UbseSecurityModule::ModifyEffectiveCapabilities(switchCap, true);
    int ret = obmmSwitchEndFunc(oldImportMemId, success ? 1 : 0);
    const int switchErrno = errno;
    UbseSecurityModule::ModifyEffectiveCapabilities(switchCap, false);
    if (ret != 0) {
        char buf[STR_ERROR_BUF_SIZE] = {0};
        UBSE_LOG_ERROR << MMI_LOG_INFO << OBMM_LOG_INFO << "obmm_switch_end error! memId=" << oldImportMemId
                       << ", ret=" << ret << ", success=" << success << ", errno=" << switchErrno
                       << ", errMsg=" << RmCommonUtils::GetInstance().GetStrError(switchErrno, buf, STR_ERROR_BUF_SIZE);
        if (switchErrno == EAGAIN) {
            // 保留内核错误分类；迁移层按单次失败策略处理，不重试业务。
            return UBSE_ERROR_AGAIN;
        }
        if (switchErrno == ESTALE) {
            // 无进行中的切换（典型：内核 30s 兜底已自动中止，KEEP_OLD_ALIVE 下大概率已回退 pa0），
            // 不推断已提交；迁移层保留未知资源，不执行probe。
            return UBSE_MMI_OBMM_SWITCH_NOT_EXIST;
        }
        return UBSE_MMI_OBMM_OP_FAILED;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << OBMM_LOG_INFO << "obmm_switch_end memId=" << oldImportMemId
                  << ", success=" << success;
    return UBSE_OK;
}

mem_id RmObmmExecutor::ObmmDevChangeUidGid(uint64_t memId, bool importMem, const ObmmOpParam& opParam)
{
    if (opParam.borrowType == UbseBorrowType::NUMA_BORROW || opParam.borrowType == UbseBorrowType::ADDR_BORROW) {
        return memId;
    }
    if (opParam.borrowType == UbseBorrowType::FD_BORROW && !importMem) {
        return memId;
    }
    if (!BaseObmmDevChangeUidGid(memId, opParam.uid, opParam.gid, opParam.mode)) {
        if (importMem) {
            ObmmUnImport(memId);
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Change uid error, unimport memid=" << memId;
        } else {
            ObmmUnExport(memId);
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Change uid error, unexport memid=" << memId;
        }
        return INVALID_MEM_ID;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << "memid=" << memId << ", isimport=" << importMem
                  << ", opParam=" << opParam.toString();
    return memId;
}

std::vector<mem_id> RmObmmExecutor::ObmmImport(const std::vector<UbseMemObmmInfo>& desc, ObmmOpParam& opParam,
                                               UbseMemImportStatus& status, int* numa)
{
    if (desc.empty() || desc.size() != status.decoderResult.size()) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "The mem desc is invalid.";
        return {};
    }
    if (opParam.borrowType == UbseBorrowType::SHARE_BORROW &&
        (desc.size() > SHM_MIGRATE_MAX_BLOCKS ||
         (!opParam.migrateInfos.empty() && opParam.migrateInfos.size() != desc.size()))) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ObmmImport migrate marker check failed, descSize=" << desc.size()
                       << ", maxBlocks=" << SHM_MIGRATE_MAX_BLOCKS
                       << ", migrateInfoSize=" << opParam.migrateInfos.size();
        return {};
    }
    std::vector<mem_id> result;
    result.reserve(desc.size());
    for (int i = 0; i < desc.size(); i++) {
        opParam.customMeta.memidCount = desc.size();
        opParam.customMeta.exportMemid = desc[i].memId;
        if (opParam.borrowType == UbseBorrowType::SHARE_BORROW) {
            opParam.customMeta.migrateInfo = opParam.migrateInfos.empty() ?
                                                 MakeShmMigrateInfo(0, i, 0, false) :
                                                 ShmMigrateImportInfo(opParam.migrateInfos[i]);
        }
        opParam.customMeta.decoderResult = status.decoderResult[i];
        auto importMemid = ObmmImport(desc[i].desc, opParam, numa);
        if (importMemid == 0) {
            ObmmUnImport(result);
            UBSE_LOG_ERROR << MMI_LOG_INFO
                           << "OBMM import error, memIds=" << RmCommonUtils::GetInstance().MemToStr(result);
            return {};
        }
        result.push_back(importMemid);
    }
    return result;
}

UbseResult RmObmmExecutor::ObmmUnImport(const std::vector<mem_id>& id)
{
    std::vector<uint64_t> successfulList;
    for (mem_id memId : id) {
        const auto ret = ObmmUnImport(memId);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "All memIds=" << RmCommonUtils::GetInstance().MemToStr(id)
                           << ", successfulList=" << RmCommonUtils::GetInstance().MemToStr(successfulList)
                           << ", errCode=" << ret;
            return ret;
        }
        successfulList.push_back(memId);
    }
    return UBSE_OK;
}

UbseResult RmObmmExecutor::ObmmUnImport(const std::vector<mem_id>& id, uint64_t timeoutMs)
{
    std::vector<uint64_t> successfulList;
    for (mem_id memId : id) {
        const auto ret = ObmmUnImport(memId, timeoutMs);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "All memIds=" << RmCommonUtils::GetInstance().MemToStr(id)
                           << ", successfulList=" << RmCommonUtils::GetInstance().MemToStr(successfulList)
                           << ", errCode=" << ret;
            return ret;
        }
        successfulList.push_back(memId);
    }
    return UBSE_OK;
}

bool SplitObmmExportSize(const size_t size[MAX_NUMA_NODES], const size_t blockSize,
                         std::vector<std::array<size_t, MAX_NUMA_NODES>>& result)
{
    size_t blockNum[MAX_NUMA_NODES]{};
    size_t blockAll{0};
    if (blockSize == 0u) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Block size=0 is invalid.";
        return false;
    }
    for (int i = 0; i < MAX_NUMA_NODES; ++i) {
        bool isOverflow = false;
        blockNum[i] = (RmCommonUtils::GetInstance().SafeAdd(size[i], blockSize, isOverflow) - 1) / blockSize;
        if (isOverflow) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Overflow occurred during addition. size[" << i << "]=" << size[i]
                           << ", blockSize=" << blockSize;
            return false;
        }
        blockAll += blockNum[i];
    }
    result.clear();
    result.reserve(blockAll);
    for (int i = 0; i < blockAll; ++i) {
        result.push_back({});
    }
    int index = 0;
    for (int i = 0; i < MAX_NUMA_NODES; ++i) {
        for (int j = 0; j < blockNum[i]; ++j) {
            result[index++][i] = blockSize;
        }
    }
    return !result.empty();
}

std::vector<mem_id> RmObmmExecutor::ObmmExport(size_t size[MAX_NUMA_NODES], int arraySize, ObmmOpParam& opParam,
                                               std::vector<ubse_mem_obmm_mem_desc>& desc, uint64_t blockSize)
{
    std::vector<std::array<size_t, MAX_NUMA_NODES>> resultTemp;
    if (!SplitObmmExportSize(size, blockSize, resultTemp)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO
                       << "SplitObmmExportSize error, borrowTypeCode=" << static_cast<uint32_t>(opParam.borrowType)
                       << ", blockSize=" << blockSize;
        return {};
    }

    if (desc.empty()) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "The desc.size=" << desc.size();
        return {};
    }

    if (desc.size() != resultTemp.size()) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "The desc.size=" << desc.size()
                       << ", The resultTemp.size=" << resultTemp.size();
        return {};
    }
    if (opParam.borrowType == UbseBorrowType::SHARE_BORROW) {
        if (resultTemp.size() > SHM_MIGRATE_MAX_BLOCKS ||
            (!opParam.migrateInfos.empty() && opParam.migrateInfos.size() != resultTemp.size())) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "ObmmExport migrate marker check failed, blockCount=" << resultTemp.size()
                           << ", maxBlocks=" << SHM_MIGRATE_MAX_BLOCKS
                           << ", migrateInfoSize=" << opParam.migrateInfos.size();
            return {};
        }
        if (opParam.migrateInfos.empty()) {
            for (size_t i = 0; i < resultTemp.size(); ++i) {
                opParam.migrateInfos.push_back(MakeShmMigrateInfo(0, i, 0, true));
            }
        }
    }
    for (size_t i = 0; i < opParam.migrateInfos.size(); ++i) {
        if ((opParam.borrowType == UbseBorrowType::SHARE_BORROW && !ShmMigrateIsExport(opParam.migrateInfos[i])) ||
            ShmMigrateIndex(opParam.migrateInfos[i]) != i ||
            ShmMigrateVersion(opParam.migrateInfos[i]) != ShmMigrateVersion(opParam.migrateInfos.front())) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "ObmmExport migrate marker invariant broken, index=" << i
                           << ", marker=" << opParam.migrateInfos[i] << ", expectIndex=" << i
                           << ", expectVersion=" << ShmMigrateVersion(opParam.migrateInfos.front());
            return {};
        }
    }
    std::vector<mem_id> result;
    result.reserve(resultTemp.size());
    for (int i = 0; i < resultTemp.size(); ++i) {
        opParam.customMeta.memidCount = resultTemp.size();
        opParam.customMeta.migrateInfo = opParam.migrateInfos.empty() ? 0 : opParam.migrateInfos[i];
        auto id = ObmmExport(resultTemp[i].data(), MAX_NUMA_NODES, opParam, desc[i]);
        if (id == 0) {
            ObmmUnExport(result);
            UBSE_LOG_ERROR << MMI_LOG_INFO
                           << "OBMM export error, memIds=" << RmCommonUtils::GetInstance().MemToStr(result);
            return {};
        }
        result.push_back(id);
    }
    return result;
}

UbseResult RmObmmExecutor::ObmmUnExport(const std::vector<mem_id>& id)
{
    std::vector<uint64_t> successfulList;
    UbseResult errCode = UBSE_OK;
    for (mem_id memId : id) {
        const auto ret = ObmmUnExport(memId);
        if (ret != UBSE_OK) {
            // 单个memId释放失败时继续释放剩余memId，避免批量释放场景下剩余memId漏释放
            UBSE_LOG_ERROR << MMI_LOG_INFO << "UnExport memid failed, errCode=" << ret
                           << ", memids=" << RmCommonUtils::GetInstance().MemToStr(id);
            if (errCode == UBSE_OK) {
                errCode = ret;
            }
            continue;
        }
        successfulList.push_back(memId);
    }
    if (errCode != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "All memids=" << RmCommonUtils::GetInstance().MemToStr(id)
                       << ", successfulList=" << RmCommonUtils::GetInstance().MemToStr(successfulList)
                       << ", errCode=" << errCode;
    }
    return errCode;
}

UbseResult RmObmmExecutor::ObmmExportPid(ObmmPidExportParam& param, ubse_mem_obmm_mem_desc& desc,
                                         const UbseMemLocalObmmCustomMeta& customMeta, const UbMemPrivData& privData)
{
    UBSE_LOG_DEBUG << MMI_LOG_INFO << "ObmmExportPid start";
    if (obmmExportByPidFunc == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ObmmExportByPidFunc is nullptr, please check.";
        return UBSE_ERROR_NULLPTR;
    }
    auto obmmMemDesc = ConstructExportMemDesc(customMeta, privData);
    if (obmmMemDesc == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    auto flag = 0;
    UBSE_LOG_INFO << MMI_LOG_INFO << "The pid=" << param.pid << ", va="
                  << reinterpret_cast<uint64_t>(param.va) // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                  << ", size=" << param.size;
    CapabilityGuard overrideCapGuard(overrideCap);
    param.memid = obmmExportByPidFunc(param.pid, param.va, param.size, flag, obmmMemDesc);
    overrideCapGuard.Disable();
    if (param.memid == INVALID_MEM_ID) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << OBMM_LOG_INFO << "Export failed, memid=0. errno=" << errno
                       << ", errMsg: " << strerror(errno);
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        return UBSE_MMI_OBMM_OP_FAILED;
    }
    CopyObmmMemDescValue(obmmMemDesc, desc);
    PrintObmmMemDesc(*obmmMemDesc);
    if (UBSE_RESULT_FAIL(ubse::mmi::RmObmmDevRead::GetNuma(param.memid, param.exportNuma))) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "GetNuma by memid failed.";
        RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
        return UBSE_MMI_OBMM_OP_FAILED;
    }

    UBSE_LOG_INFO << MMI_LOG_INFO << "obmm_export_useraddr success.";
    RmCommonUtils::GetInstance().SafeFree(obmmMemDesc);
    return UBSE_OK;
}

UbseResult RmObmmExecutor::ObmmQueryUBPaByMemId(uint64_t handle, unsigned long offset, unsigned long* pa)
{
    if (!pa) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << OBMM_LOG_INFO << " pa is null.";
        return UBSE_ERROR_NULLPTR;
    }
    if (!obmmQueryPaByMemIdFunc) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << OBMM_LOG_INFO << "obmmQueryPaByMemIdFunc is null.";
        return UBSE_ERROR_NULLPTR;
    }
    CapabilityGuard overrideCapGuard(overrideCap);
    auto ret = obmmQueryPaByMemIdFunc(handle, offset, pa);
    overrideCapGuard.Disable();
    if (ret != 0) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << OBMM_LOG_INFO << "obmmQueryPaByMemIdFunc error handle is " << handle << ".";
        return UBSE_MMI_OBMM_OP_FAILED;
    }
    return UBSE_OK;
}

UbseResult RmObmmExecutor::ObmmPreImport(struct obmm_preimport_info* preimport_info, unsigned long flags)
{
    if (preimport_info == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "The obmm_preimport_info is nullptr, error_code=" << UBSE_ERROR_INVAL;
        return UBSE_ERROR_INVAL;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO
                  << "Start to use Obmm PreImport interface, preimport_info is scna=" << preimport_info->scna
                  << ", dcna=" << preimport_info->dcna << ", length=" << preimport_info->length;
    if (obmmPreImportFunc == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "The obmmPreImportFunc is nullptr, error_code=" << UBSE_ERROR_NULLPTR;
        return UBSE_ERROR_NULLPTR;
    }

    CapabilityGuard overrideCapGuard(overrideCap);
    auto ret = obmmPreImportFunc(preimport_info, flags);
    overrideCapGuard.Disable();

    if (ret == -1 && errno == EEXIST) {
        UBSE_LOG_INFO << MMI_LOG_INFO << "The cna connect has been preOnline, scna=" << preimport_info->scna
                      << ", dcna=" << preimport_info->dcna;
        return UBSE_OK;
    }
    if (ret != 0) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "obmmPreImportFailed, ret=" << ret << ", scna= " << preimport_info->scna
                       << ", dcna=" << preimport_info->dcna << ", errno=" << errno;
        return UBSE_MMI_OBMM_OP_FAILED;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << "PreImport success, remote numa_id=" << preimport_info->numa_id
                  << ", scna=" << preimport_info->scna;
    return UBSE_OK;
}
UbseResult RmObmmExecutor::ObmmUnPreImport(struct obmm_preimport_info* preimport_info, unsigned long flags)
{
    if (preimport_info == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "The obmm_preimport_info is nullptr, error_code=" << UBSE_ERROR_INVAL;
        return UBSE_ERROR_INVAL;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO
                  << "Start to use Obmm UnPreImport interface, preimport_info is scna=" << preimport_info->scna
                  << ", dcna=" << preimport_info->dcna << ", length=" << preimport_info->length;
    if (obmmUnPreImportFunc == nullptr) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "The obmmPreImportFunc is nullptr, error_code=" << UBSE_ERROR_NULLPTR;
        return UBSE_ERROR_NULLPTR;
    }
    CapabilityGuard overrideCapGuard(overrideCap);
    auto ret = obmmUnPreImportFunc(preimport_info, flags);
    overrideCapGuard.Disable();
    if (ret != 0) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "obmmUnPreImportFailed, ret=" << ret << ", scna=" << preimport_info->scna
                       << ", dcna=" << preimport_info->dcna;
        return UBSE_MMI_OBMM_OP_FAILED;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << "UnPreImport success, remote numa_id=" << preimport_info->numa_id
                  << ", scna=" << preimport_info->scna;
    return UBSE_OK;
}

uint64_t RmObmmExecutor::CalculateUnImportTimeout(uint64_t blockSizeMb)
{
    if (offlineTimeoutConfigured_) {
        return offlineTimeoutMs_;
    }
    static uint64_t minTimeOutMs = 5000;
    return minTimeOutMs + blockSizeMb;
}

} // namespace ubse::mmi
