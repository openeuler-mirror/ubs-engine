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

#include "ubse_logger_audit.h"

#include <dlfcn.h>
#include <iostream>
#include <sstream>

#include <securec.h>

#include "ubse_common_def.h"
#include "sys/syslog.h"

namespace ubse::log {
using namespace ubse::common::def;
static bool g_isOpenAudit{false};
constexpr int AUDIT_RESULT_SUCCESS = 1; // success
static bool g_loaded{false};            // 标识 libaudit 是否已成功加载
static void* g_auditLibHandle{nullptr}; // 动态加载的 libaudit 库的句柄
int g_auditfd = -1;

template <typename T>
inline int AuditDlsym(void* handle, T& ptr, std::string sym)
{
    auto ptr1 = dlsym(handle, sym.c_str());
    if (ptr1 == nullptr) {
        return -1;
    }
    ptr = reinterpret_cast<T>(ptr1);
    return 0;
}

// 定义 audit 函数指针类
using FuncAuditOpen = int (*)();
using FuncAuditClose = void (*)(int);
using FuncAuditLogUserMsg = int (*)(int, int, const char*, const char*, const char*, const char*, int);

// 静态成员变量保存 audit 库句柄和函数指针
static FuncAuditOpen g_auditOpenFunc = nullptr;
static FuncAuditClose g_auditCloseFunc = nullptr;
static FuncAuditLogUserMsg g_auditLogUserMessageFunc = nullptr;
// 动态加载 libaudit 库
void InitializeAuditFunctions()
{
    // audit打开,动态库已加载
    if (g_loaded && g_isOpenAudit) {
        return; // 已经初始化完成，快速返回
    }

    // 尝试加载 libaudit.so
    g_auditLibHandle = dlopen("libaudit.so", RTLD_LAZY);
    if (!g_auditLibHandle) {
        const char* error = dlerror();
        std::cerr << "dlopen unable to load libaudit: " << error << std::endl;
        return; // 无法加载，跳过初始化
    }

    // 加载 audit_open 函数
    auto ret = AuditDlsym<FuncAuditOpen>(g_auditLibHandle, g_auditOpenFunc, "audit_open");
    const char* error = dlerror();
    if (ret != 0) {
        dlclose(g_auditLibHandle);
        g_auditLibHandle = nullptr;
        std::cerr << "Unable to find symbol 'audit_open': " << error << std::endl;
    }

    // 加载 audit_close 函数
    ret = AuditDlsym<FuncAuditClose>(g_auditLibHandle, g_auditCloseFunc, "audit_close");
    error = dlerror();
    if (ret != 0) {
        dlclose(g_auditLibHandle);
        g_auditLibHandle = nullptr;
        std::cerr << "Unable to find symbol 'audit_close': " << error << std::endl;
    }

    // 加载 audit_log_user_message 函数
    ret = AuditDlsym<FuncAuditLogUserMsg>(g_auditLibHandle, g_auditLogUserMessageFunc, "audit_log_user_message");
    error = dlerror();
    if (ret != 0) {
        dlclose(g_auditLibHandle);
        g_auditLibHandle = nullptr;
        std::cerr << "Unable to find symbol 'audit_log_user_message': " << error << std::endl;
    }
    if (g_auditOpenFunc) {
        g_auditfd = g_auditOpenFunc();
        if (g_auditfd >= 0) {
            g_isOpenAudit = true;
            g_loaded = true;
        }
    }
}
// 释放相关句柄
void AuditDestroy()
{
    if (g_isOpenAudit && g_loaded) {
        if (g_auditCloseFunc && g_auditfd >= 0) {
            g_auditCloseFunc(g_auditfd); // 调用动态加载的 audit_close_func
        }
        // 清空函数指针，避免野指针访问
        g_auditCloseFunc = nullptr;
        g_auditOpenFunc = nullptr;
        g_auditLogUserMessageFunc = nullptr;
        if (g_auditLibHandle != nullptr) {
            dlclose(g_auditLibHandle); // 关闭共享库句柄
        }
        g_isOpenAudit = false;
        g_auditLibHandle = nullptr;
        g_auditfd = -1;
    }
}
OperateLoggerEntry::OperateLoggerEntry(std::string interface, RecordType type) : interface_(interface), type_(type) {}
RuntimeLoggerEntry::RuntimeLoggerEntry(RecordType type) : type_(type) {}
SecurityLoggerEntry::SecurityLoggerEntry(std::string interface, RecordType type) : interface_(interface), type_(type) {}
AuditLoggerEntry::~AuditLoggerEntry() {}
int AuditLoggerEntry::RecordToAudit(RecordType& recordType)
{
    if (recordType == RecordType::AUDIT_OPERATE) {
        return AuditType_["AUDIT_USER_CMD"];
    }
    if (recordType == RecordType::AUDIT_RUNTIME_ALLOC) {
        return AuditType_["AUDIT_DEV_ALLOC"];
    }
    if (recordType == RecordType::AUDIT_RUNTIME_DEALLOC) {
        return AuditType_["AUDIT_DEV_DEALLOC"];
    }
    if (recordType == RecordType::AUDIT_SECURITY) {
        return AuditType_["AUDIT_CRYPTO_PARAM_CHANGE_USER"];
    }
    return AuditType_["AUDIT_USER_CMD"];
}
std::string AuditLoggerEntry::RecordToString(RecordType& recordType)
{
    if (recordType == RecordType::AUDIT_OPERATE) {
        return "OperateLog";
    }
    if (recordType == RecordType::AUDIT_RUNTIME_ALLOC) {
        return "RuntimeLog";
    }
    if (recordType == RecordType::AUDIT_RUNTIME_DEALLOC) {
        return "RuntimeLog";
    }
    if (recordType == RecordType::AUDIT_SECURITY) {
        return "SecurityLog";
    }
    return "OperateLog";
}
void AuditLoggerEntry::SendAuditMessage(RecordType type, const std::string& logMessage, int result)
{
    // libaudit.so加载失败
    if (!g_loaded) {
        // audit功能退化为syslog
        syslog(LOG_INFO, "%s", logMessage.c_str());
        return;
    }
    if (!logMessage.empty() && g_auditfd >= 0) {
        auto ret = g_auditLogUserMessageFunc(g_auditfd, RecordToAudit(type), logMessage.c_str(), nullptr, nullptr,
                                             nullptr, result);
        if (ret < 0) {
            std::cerr << "Unable to send audit message " << logMessage.c_str() << ": " << strerror(errno) << std::endl;
        }
    }
}
void OperateLoggerEntry::Sendlog(AuditLoggerEntry& auditLoggerEntry)
{
    std::ostringstream oss;
    oss << "[" << interface_ << "]"
        << "[type: " << RecordToString(type_) << "]"
        << " " << auditLoggerEntry.Str() << std::endl;
    std::string logMessage = oss.str();
    SendAuditMessage(type_, logMessage, AUDIT_RESULT_SUCCESS);
}
void RuntimeLoggerEntry::Sendlog(AuditLoggerEntry& auditLoggerEntry)
{
    std::ostringstream oss;
    oss << " [type: " << RecordToString(type_) << "]"
        << " " << auditLoggerEntry.Str() << std::endl;
    std::string logMessage = oss.str();
    SendAuditMessage(type_, logMessage, AUDIT_RESULT_SUCCESS);
}
void SecurityLoggerEntry::Sendlog(AuditLoggerEntry& auditLoggerEntry)
{
    std::ostringstream oss;
    oss << " [type: " << RecordToString(type_) << "]"
        << " " << auditLoggerEntry.Str() << std::endl;
    std::string logMessage = oss.str();
    SendAuditMessage(type_, logMessage, AUDIT_RESULT_SUCCESS);
}

bool UbseAuditlog::operator==(AuditLoggerEntry& auditloggerEntry)
{
    auditloggerEntry.Sendlog(auditloggerEntry);
    return true;
}
} // namespace ubse::log