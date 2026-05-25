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

#include "ubse_security_manager.h"

#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <vector>

#include <linux/capability.h>

#include <securec.h>

#include <bitset>

#include "ubse_error.h"
#include "ubse_logger_module.h"

namespace ubse::security {
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");

constexpr unsigned long long CAP_FULL_BIT = 64;     // 总能力位数（V3+ 标准）
constexpr unsigned long long CAP_SEGMENT_BIT = 32;  // 单个 cap_data 结构处理的位数
constexpr unsigned long long CAP_SEGMENT_COUNT = 2; // cap_data 结构的数量

UbseResult UbseSecurityManager::GetCapabilities()
{
    // 初始化 cap_header_t
    __user_cap_header_struct capHeader{};
    capHeader.version = _LINUX_CAPABILITY_VERSION_3; // 使用版本 3
    capHeader.pid = 0;                               // 获取当前进程的能力

    // 初始化 cap_user_data_t
    const auto data = new (std::nothrow) __user_cap_data_struct[CAP_SEGMENT_COUNT];
    if (data == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }

    // 清空数据结构
    auto ret = memset_s(data, sizeof(__user_cap_data_struct) * CAP_SEGMENT_COUNT, 0,
                        sizeof(__user_cap_data_struct) * CAP_SEGMENT_COUNT);
    if (ret != EOK) {
        delete[] data;
        return ret;
    }

    // 调用 syscall(SYS_capget) 获取能力
    errno = 0;
    if (syscall(SYS_capget, &capHeader, data) < 0) {
        perror("Failed to get capabilities");
        delete[] data;
        return UBSE_ERROR;
    }

    // 清理资源
    delete[] data;
    return UBSE_OK;
}

void SetCapabilitiesData(__user_cap_data_struct* capData)
{
    const std::vector<__u32> pCapabilities = {
        CAP_SYS_NICE, // 允许进程改变进程或线程的优先级和调度策略
        CAP_FOWNER,   // 允许绕过 /dev/obmm_shmdev 的 owner 检查
        CAP_DAC_OVERRIDE,
        CAP_DAC_READ_SEARCH, // 允许读取其他进程的 /proc/* 文件
        CAP_CHOWN,       // 允许修改 /dev/obmm_shmdev 的 owner 和 group
        CAP_AUDIT_WRITE, // 允许写入审计日志 日志路径 /var/log/audit
        CAP_NET_ADMIN,   // 允许访问urma文件
        CAP_SYS_PTRACE,  // 允许访问其他进程的信息，如 /proc/pid/numa_maps
    };

    const std::vector<__u32> eCapabilities = {
        CAP_SYS_NICE,    // 允许进程改变进程或线程的优先级和调度策略
        CAP_DAC_READ_SEARCH, // 允许读取其他进程的 /proc/* 文件
        CAP_AUDIT_WRITE, // 允许写入审计日志 日志路径 /var/log/audit
        CAP_NET_ADMIN,   // 允许访问urma文件
        CAP_SYS_PTRACE,  // 允许访问其他进程的信息，如 /proc/pid/numa_maps
    };

    const std::vector<__u32> iCapabilities = {
        CAP_DAC_OVERRIDE,
    };

    // 设置能力
    for (const auto cap : pCapabilities) {
        if (cap >= CAP_SEGMENT_BIT) {
            capData[1].permitted |= (1ULL << (cap - CAP_SEGMENT_BIT));
        } else {
            capData[0].permitted |= (1ULL << cap);
        }
    }

    for (const auto cap : eCapabilities) {
        if (cap >= CAP_SEGMENT_BIT) {
            capData[1].effective |= (1ULL << (cap - CAP_SEGMENT_BIT));
        } else {
            capData[0].effective |= (1ULL << cap);
        }
    }

    for (const auto cap : iCapabilities) {
        if (cap >= CAP_SEGMENT_BIT) {
            capData[1].inheritable |= (1ULL << (cap - CAP_SEGMENT_BIT));
        } else {
            capData[0].inheritable |= (1ULL << cap);
        }
    }
}

UbseResult UbseSecurityManager::SetInitialCapabilities()
{
    // 初始化 cap_header_t 和 cap_user_data_t
    __user_cap_header_struct capHeader{};
    capHeader.version = _LINUX_CAPABILITY_VERSION_3; // 使用能力版本 3
    capHeader.pid = 0;                               // 设置当前进程的能力

    // 初始化 cap_user_data_t（支持 64 位权限位设置）
    const auto capData = new (std::nothrow) __user_cap_data_struct[CAP_SEGMENT_COUNT];
    if (capData == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }

    // 清空数据结构
    auto ret = memset_s(capData, sizeof(__user_cap_data_struct) * CAP_SEGMENT_COUNT, 0,
                        sizeof(__user_cap_data_struct) * CAP_SEGMENT_COUNT);
    if (ret != EOK) {
        delete[] capData;
        return ret;
    }

    SetCapabilitiesData(capData);

    // 调用 syscall 设置能力
    errno = 0;
    if (syscall(SYS_capset, &capHeader, capData) < 0) {
        perror("Failed to set capabilities");
        delete[] capData;
        return UBSE_ERROR;
    }
    delete[] capData;
    return UBSE_OK;
}

UbseResult UbseSecurityManager::ModifyEffectiveCapabilities(const std::vector<__u32>& caps, bool isAdd)
{
    __user_cap_header_struct capHeader;
    capHeader.version = _LINUX_CAPABILITY_VERSION_3; // 使用能力版本 3
    capHeader.pid = 0;

    __user_cap_data_struct capData[CAP_SEGMENT_COUNT];

    if (syscall(SYS_capget, &capHeader, capData) < 0) {
        UBSE_LOG_ERROR << "Failed to get capabilities, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    for (const auto cap : caps) {
        if (!(capData[CAP_TO_INDEX(cap)].permitted & CAP_TO_MASK(cap))) {
            UBSE_LOG_ERROR << "Capability not in permitted set, " << FormatRetCode(UBSE_ERROR_INVAL);
            return UBSE_ERROR_INVAL;
        }
    }

    if (isAdd) {
        for (const auto cap : caps) {
            capData[CAP_TO_INDEX(cap)].effective |= CAP_TO_MASK(cap);
        }
    } else {
        for (const auto cap : caps) {
            capData[CAP_TO_INDEX(cap)].effective &= ~CAP_TO_MASK(cap);
        }
    }

    if (syscall(SYS_capset, &capHeader, capData) < 0) {
        UBSE_LOG_ERROR << "Failed to set capabilities, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    return UBSE_OK;
}
} // namespace ubse::security