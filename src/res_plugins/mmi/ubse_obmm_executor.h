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

#include <linux/capability.h>
#include <sys/stat.h>
#include <vector>
#include "ubse_common_def.h"
#include "ubse_mem_types.h"
#include "ubse_security_module.h"
#include "dlfcn.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger_inner.h"
#include "ubse_mem_obj.h"
#include "ubse_mem_obmm_def.h"
#include "unistd.h"

namespace ubse::mmi {
using namespace ubse::common::def;
using namespace ubse::mem::obj;
using namespace ubse::context;
using namespace ubse::security;

#define INVALID_MEM_ID 0
using ObmmExportPtr = mem_id (*)(const size_t length[OBMM_MAX_LOCAL_NUMA_NODES], unsigned long flags,
                                 struct obmm_mem_desc *desc);
using ObmmUnexportPtr = int (*)(mem_id id, unsigned long flags);
using ObmmImportPtr = mem_id (*)(const struct obmm_mem_desc *desc, unsigned long flags, int base_dist, int *numa);
using ObmmUnimportPtr = int (*)(mem_id id, unsigned long flags);

class RmObmmExecutor {
public:
    UbseResult Init();
    UbseResult Exit();

    // 封装的obmm的基本单操作接口
    mem_id ObmmExport(size_t size[MAX_NUMA_NODES], const ObmmOpParam &opParam, ubse_mem_obmm_mem_desc &desc);
    UbseResult ObmmUnExport(mem_id id);
    mem_id ObmmImport(const ubse_mem_obmm_mem_desc &desc, const ObmmOpParam &opParam, int *numa);
    UbseResult ObmmUnImport(mem_id id);

    // 批量导出导入
    std::vector<mem_id> ObmmExport(size_t size[MAX_NUMA_NODES], ObmmOpParam &opParam,
                                   std::vector<ubse_mem_obmm_mem_desc> &desc);
    UbseResult ObmmUnExport(const std::vector<mem_id> &id);
    std::vector<mem_id> ObmmImport(const std::vector<UbseMemObmmInfo> &desc, ObmmOpParam &opParam, int *numa);
    UbseResult ObmmUnImport(const std::vector<mem_id> &id);

    UbseResult DlOpenLib(const std::string &obmmPath);
    mem_id ObmmDevChangeUidGid(uint64_t memId, bool importMem, const ObmmOpParam &opParam);

    template <typename T>
    UbseResult DlOpenFunName(T &funPtr, const std::string &funName)
    {
        funPtr = reinterpret_cast<T>(dlsym(handle, funName.c_str()));
        if (funPtr == nullptr) {
            UBSE_LOG_ERROR << "Get obmmFunc from libobmm.so failed, fun name is " << funName << ", error is "
                         << dlerror();
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
            UBSE_LOG_ERROR << "ObmmMemIdToFilePath is empty.";
            return false;
        }
        [[maybe_unused]] mode_t ownerAllowedMode = mode; // owner allowed;
        mode_t groupAllowedMode = mode;                  // group allowed;
        std::vector<__u32> caps = {
            CAP_FOWNER,
            CAP_CHOWN,
        };
        UbseContext &ubseContext = UbseContext::GetInstance();
        auto ubseSecurityModule = ubseContext.GetModule<UbseSecurityModule>();
        if (ubseSecurityModule == nullptr) {
            UBSE_LOG_ERROR << "Get security module failed.";
            return UBSE_ERROR_NULLPTR;
        }
        auto ret = ubseSecurityModule->ModifyEffectiveCapabilities(caps, UbseCapOperateType::CAP_ADD);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Add capabilities failed.";
            return false;
        }
        errno = 0;
        // mode为0不修改权限
        if (groupAllowedMode != 0) {
            if (chmod(basicString.c_str(), groupAllowedMode) != 0) {
                UBSE_LOG_ERROR << "chmod error, errno is " << errno;
                ret = ubseSecurityModule->ModifyEffectiveCapabilities(caps, UbseCapOperateType::CAP_DELETE);
                if (ret != UBSE_OK) {
                    UBSE_LOG_ERROR << "Delete capabilities failed.";
                }
                return false;
            }
        }
        if (chown(basicString.c_str(), uid, gid) != 0) {
            UBSE_LOG_ERROR << "chown error, errno is " << errno;
            ret = ubseSecurityModule->ModifyEffectiveCapabilities(caps, UbseCapOperateType::CAP_DELETE);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "Delete capabilities failed.";
            }
            return false;
        }
        ret = ubseSecurityModule->ModifyEffectiveCapabilities(caps, UbseCapOperateType::CAP_DELETE);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Delete capabilities failed.";
            return false;
        }
        return true;
    }

    static RmObmmExecutor &GetInstance()
    {
        static RmObmmExecutor instance;
        return instance;
    }
    RmObmmExecutor(const RmObmmExecutor &other) = delete;
    RmObmmExecutor(RmObmmExecutor &&other) = delete;
    RmObmmExecutor &operator=(const RmObmmExecutor &other) = delete;
    RmObmmExecutor &operator=(RmObmmExecutor &&other) noexcept = delete;

private:
    RmObmmExecutor() = default;
    static constexpr auto OBMM_PATH = "libobmm.so";
    uint64_t blockSize{DEFAULT_BLOCK_SIZE};
    void *handle{nullptr};

    ObmmExportPtr obmmExportFunc{};
    ObmmUnexportPtr obmmUnexportFunc{};
    ObmmImportPtr obmmImportFunc{};
    ObmmUnimportPtr obmmUnimportFunc{};
};
} // namespace ubse::mmi
#endif // UBSE_MANAGER_RM_OBMM_EXECUTOR_H
