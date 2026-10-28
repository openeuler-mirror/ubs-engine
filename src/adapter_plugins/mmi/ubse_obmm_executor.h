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

#ifndef UBSE_MANAGER_RM_OBMM_EXECUTOR_H
#define UBSE_MANAGER_RM_OBMM_EXECUTOR_H
#include <array>
#include <csignal>
#include <cstddef>
#include <vector>

#include <dlfcn.h>
#include <linux/capability.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ubse_common_def.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_types.h"
#include "ubse_mmi_def.h"
#include "ubse_mmi_interface.h"
#include "ubse_mmi_obmm_def.h"
#include "ubse_mmi_timeout_manager.h"
#include "ubse_security_module.h"

namespace ubse::mmi {
#define MODULE_LOG_NAME "ubse"
using ubse::adapter_plugins::mmi::UbseMemImportStatus;
using ubse::adapter_plugins::mmi::UbseMemObmmInfo;
using ubse::common::def::UbseResult;
using ubse::security::UbseSecurityModule;

constexpr int INVALID_MEM_ID = 0;
constexpr auto OBMM_OFFLINE_TIMEOUT_CONFIG_KEY = "obmm.memory.offline.timeout";
constexpr uint64_t MS_PER_SECOND = 1000;
constexpr uint64_t DEFAULT_UNIMPORT_TIMEOUT_MS = 100 * MS_PER_SECOND;
using ObmmExportPtr = mem_id (*)(const size_t length[OBMM_MAX_LOCAL_NUMA_NODES], unsigned long flags,
                                 struct obmm_mem_desc* desc);
using ObmmUnexportPtr = int (*)(mem_id id, unsigned long flags);
using ObmmImportPtr = mem_id (*)(const struct obmm_mem_desc* desc, unsigned long flags, int base_dist, int* numa);
using ObmmUnimportPtr = int (*)(mem_id id, unsigned long flags);
using ObmmExportByPidPtr = mem_id (*)(int pid, void* va, size_t size, unsigned long flags, struct obmm_mem_desc* desc);
using ObmmQueryPaByMemIdPtr = int (*)(mem_id id, unsigned long offset, unsigned long* pa);
using ObmmPreImportPtr = int (*)(struct obmm_preimport_info* preimport_info, unsigned long flags);
using ObmmUnPreImportPtr = int (*)(const struct obmm_preimport_info* preimport_info, unsigned long flags);

using ObmmSwitchBeginImportPtr = int (*)(mem_id id, unsigned long flags, const struct obmm_switch_import_target* target,
                                         const void* priv, uint16_t privLen);
using ObmmSwitchBeginExportPtr = int (*)(mem_id id, unsigned long flags, const struct obmm_switch_export_target* target,
                                         struct obmm_mem_desc* desc, const void* priv, uint16_t privLen);
using ObmmSwitchEndPtr = int (*)(mem_id id, int success);

/* ObmmSwitchBeginImport 的私有语义入参（对接层对内暴露，Step1 设计 §3.3）。
 * EID 按 128bit 字节携带，禁止把既有 desc 的 uint32 seid/deid 截断表示直接充当 EID（Step1 设计 §8 O3）。 */
struct ObmmSwitchParam {
    mem_id oldImportMemId{INVALID_MEM_ID};          /* 待切换的导入 memId */
    uint64_t newPa{0};                              /* 新链路物理地址 */
    uint32_t newScna{0};                            /* 使用方侧 CNA（导入节点侧） */
    uint32_t newDcna{0};                            /* 提供方侧 CNA（新导出节点侧） */
    uint8_t baseDist{0};                            /* 新链路 NUMA 距离 */
    std::array<uint8_t, UBSE_EID_LENGTH> newSeid{}; /* 使用方侧 EID，128bit 小端 */
    std::array<uint8_t, UBSE_EID_LENGTH> newDeid{}; /* 提供方侧 EID，128bit 小端 */
    std::vector<uint8_t> priv{};                    /* 待生效私有数据（可为空），switch_end(1) 后替换 */
};

struct ObmmSwitchExportParam {
    mem_id importMemId{INVALID_MEM_ID};
    obmm_switch_export_target target{};
    std::array<uint8_t, UBSE_EID_LENGTH> seid{};
    std::array<uint8_t, UBSE_EID_LENGTH> deid{};
    std::vector<uint8_t> priv{};
};

struct ObmmPidExportParam {
    int pid;
    void* va;
    size_t size;
    unsigned long flags;
    uint64_t exportNuma;
    uint64_t memid;
    ObmmPidExportParam(const int pid, void* const va, const size_t size, const unsigned long flags,
                       const uint64_t exportNuma, const uint64_t memid)
        : pid(pid),
          va(va),
          size(size),
          flags(flags),
          exportNuma(exportNuma),
          memid(memid)
    {
    }
};

class RmObmmExecutor {
public:
    UbseResult MemcpyMt(void* dst, const void* src, size_t size);
    UbseResult Init();
    UbseResult Exit();

    // 封装的obmm的基本单操作接口
    mem_id ObmmExport(size_t size[MAX_NUMA_NODES], int arraySize, const ObmmOpParam& opParam,
                      ubse_mem_obmm_mem_desc& desc);
    UbseResult ObmmUnExport(mem_id id);
    mem_id ObmmImport(const ubse_mem_obmm_mem_desc& desc, const ObmmOpParam& opParam, int* numa);
    UbseResult ObmmUnImport(mem_id id);
    UbseResult ObmmUnImport(mem_id id, uint64_t timeoutMs);

    // 批量导出导入
    std::vector<mem_id> ObmmExport(size_t size[MAX_NUMA_NODES], int arraySize, ObmmOpParam& opParam,
                                   std::vector<ubse_mem_obmm_mem_desc>& desc, uint64_t blockSize);
    UbseResult ObmmUnExport(const std::vector<mem_id>& id);
    std::vector<mem_id> ObmmImport(const std::vector<UbseMemObmmInfo>& desc, ObmmOpParam& opParam,
                                   UbseMemImportStatus& status, int* numa);
    UbseResult ObmmUnImport(const std::vector<mem_id>& id);
    UbseResult ObmmUnImport(const std::vector<mem_id>& id, uint64_t timeoutMs);
    UbseResult ObmmExportPid(ObmmPidExportParam& param, struct ubse_mem_obmm_mem_desc& desc,
                             const UbseMemLocalObmmCustomMeta& customMeta, const UbMemPrivData& ubMemPrivData);
    UbseResult ObmmQueryUBPaByMemId(uint64_t handle, unsigned long offset, unsigned long* pa);

    UbseResult ObmmPreImport(struct obmm_preimport_info* preimport_info, unsigned long flags);
    UbseResult ObmmUnPreImport(struct obmm_preimport_info* preimport_info, unsigned long flags);

    // 故障迁移：两阶段切换封装（必选符号，加载失败阻断初始化；调用前空指针检查）
    UbseResult ObmmSwitchBeginImport(const ObmmSwitchParam& param);
    UbseResult ObmmSwitchBeginExport(const ObmmSwitchExportParam& param, struct obmm_mem_desc& desc);
    UbseResult ObmmSwitchEnd(mem_id oldImportMemId, bool success);

    UbseResult DlOpenLib(const std::string& obmmPath);
    mem_id ObmmDevChangeUidGid(uint64_t memId, bool importMem, const ObmmOpParam& opParam);

    uint64_t GetObmmBlockSize();

    static uint64_t CalculateUnImportTimeout(uint64_t blockSizeMb);

    template <typename T>
    UbseResult DlOpenFunName(T& funPtr, const std::string& funName)
    {
        funPtr =
            reinterpret_cast<T>(dlsym(handle, funName.c_str())); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        if (funPtr == nullptr) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Get obmmFunc from libobmm.so failed, fun name is " << funName
                           << ", error is " << dlerror();
            dlclose(handle);
            handle = nullptr;
            return UBSE_ERROR;
        }
        return UBSE_OK;
    }

    virtual ~RmObmmExecutor() = default;

    static std::string ObmmMemIdToFilePath(const uint64_t memId)
    {
        if (memId == 0) {
            return "";
        }
        static std::string obmmPath = "/dev/obmm_shmdev";
        return obmmPath + std::to_string(memId);
    }

    // 修改obmm文件的用户权限，让用户程序有权限可以mmap
    static bool BaseObmmDevChangeUidGid(const uint64_t memId, uid_t uid, gid_t gid, mode_t mode)
    {
        const std::string basicString = ObmmMemIdToFilePath(memId);
        if (basicString.empty()) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "ObmmMemIdToFilePath is empty.";
            return false;
        }
        [[maybe_unused]] mode_t ownerAllowedMode = mode; // owner allowed;
        mode_t groupAllowedMode = mode;                  // group allowed;
        std::vector<__u32> caps = {
            CAP_FOWNER,
            CAP_CHOWN,
        };
        ubse::security::CapabilityGuard capsGuard(caps);
        if (!capsGuard.IsEnabled()) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Add capabilities failed.";
            return false;
        }
        errno = 0;
        // mode为0不修改权限
        if (groupAllowedMode != 0 && chmod(basicString.c_str(), groupAllowedMode) != 0) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "chmod error, errno is " << errno;
            return false;
        }
        if (chown(basicString.c_str(), uid, gid) != 0) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "chown error, errno is " << errno;
            return false;
        }
        return true;
    }

    static RmObmmExecutor& GetInstance()
    {
        static RmObmmExecutor instance;
        return instance;
    }
    RmObmmExecutor(const RmObmmExecutor& other) = delete;
    RmObmmExecutor(RmObmmExecutor&& other) = delete;
    RmObmmExecutor& operator=(const RmObmmExecutor& other) = delete;
    RmObmmExecutor& operator=(RmObmmExecutor&& other) noexcept = delete;

private:
    RmObmmExecutor() = default;
    static constexpr auto OBMM_PATH = "libobmm.so.1";
    bool preOnlineSwitch{false};
    void* handle{nullptr};
    void* accelHandle{nullptr};
    UbseResult LoadMemcpyMt();
    static uint64_t offlineTimeoutMs_;     // 从配置文件obmm.memory.offline.timeout获取
    static bool offlineTimeoutConfigured_; // 是否显式配置了obmm.memory.offline.timeout

    void RegisterSigusr1Handler();

    ObmmExportPtr obmmExportFunc{};
    ObmmUnexportPtr obmmUnexportFunc{};
    ObmmImportPtr obmmImportFunc{};
    ObmmUnimportPtr obmmUnimportFunc{};
    ObmmExportByPidPtr obmmExportByPidFunc{};
    ObmmQueryPaByMemIdPtr obmmQueryPaByMemIdFunc{};
    ObmmPreImportPtr obmmPreImportFunc{};
    ObmmUnPreImportPtr obmmUnPreImportFunc{};
    ObmmSwitchBeginImportPtr obmmSwitchBeginImportFunc{};
    ObmmSwitchBeginExportPtr obmmSwitchBeginExportFunc{};
    ObmmSwitchEndPtr obmmSwitchEndFunc{};
    // ABI 对齐 obmm-accel/libobmm-accel.h，从 libobmm-accel.so.1 动态加载。
    using MemcpyMtPtr = void* (*)(void*, const void*, size_t, unsigned int);
    MemcpyMtPtr memcpyMtFunc{};
};
#undef MODULE_LOG_NAME
} // namespace ubse::mmi
#endif // UBSE_MANAGER_RM_OBMM_EXECUTOR_H
