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

#ifndef UBSE_MANAGER_UBSE_LOGGER_AUDIT_H
#define UBSE_MANAGER_UBSE_LOGGER_AUDIT_H

#include <iostream>
#include <sstream>
#include <unordered_map>
// UBSE_AUDIT_LOGGING_OFF用于需要关闭审计日志输出文件场景，如UT场景
#ifdef UBSE_AUDIT_LOGGING_OFF
#include <fstream>

#define UBSE_AUDIT_OPERATE(interface) std::ofstream("/dev/null")
#define UBSE_AUDIT_RUNTIME_ALLOC std::ofstream("/dev/null")
#define UBSE_AUDIT_RUNTIME_DEALLOC std::ofstream("/dev/null")
#define UBSE_AUDIT_SECURITY(interface) std::ofstream("/dev/null")

#else
namespace ubse::log {

/**
 * @brief 创建单条审计操作日志，记入系统审计日志
 * @param interface [in] 接口名，执行该操作的接口名称
 */
#define UBSE_AUDIT_OPERATE(interface) UbseAuditlog() == OperateLoggerEntry(interface, RecordType::AUDIT_OPERATE)

/**
 * @brief 创建单条审计运行日志（资源分配），记入系统审计日志
 */
#define UBSE_AUDIT_RUNTIME_ALLOC UbseAuditlog() == RuntimeLoggerEntry(RecordType::AUDIT_RUNTIME_ALLOC)

/**
 * @brief 创建单条审计运行日志（资源释放），记入系统审计日志
 */
#define UBSE_AUDIT_RUNTIME_DEALLOC UbseAuditlog() == RuntimeLoggerEntry(RecordType::AUDIT_RUNTIME_DEALLOC)

/**
 * @brief 创建单条审计安全日志，记入审计日志
 * @param interface [in] 接口名，执行该操作的接口名称
 */
#define UBSE_AUDIT_SECURITY(interface) UbseAuditlog() == SecurityLoggerEntry(interface, RecordType::AUDIT_SECURITY)

enum class RecordType : uint8_t {
    AUDIT_OPERATE = 0,     // 操作日志记录,0
    AUDIT_RUNTIME_ALLOC,   // 运行日志记录,1
    AUDIT_RUNTIME_DEALLOC, // 运行日志记录,2
    AUDIT_SECURITY         // 安全日志记录,3
};

void InitializeAuditFunctions();
void AuditDestroy();
class AuditLoggerEntry {
public:
    AuditLoggerEntry() = default;
    virtual ~AuditLoggerEntry();
    AuditLoggerEntry &operator << (const std::string &data)
    {
        os_ << data;
        return *this;
    }
    std::string Str() const
    {
        return os_.str();
    }

    virtual void Sendlog(AuditLoggerEntry &auditLoggerEntry) = 0;

protected:
    void SendAuditMessage(RecordType type, const std::string &logMessage, int result);
    std::string RecordToString(RecordType &recordType);
    int RecordToAudit(RecordType &recordType);
    // 审计事件类型映射表(对应audit—records.h中的定义)
    std::unordered_map<std::string, int> AuditType_ = {{"AUDIT_USER_CMD",                 1123 },
                                                       {"AUDIT_DEV_ALLOC",                2307 },
                                                       {"AUDIT_DEV_DEALLOC",              2308 },
                                                       {"AUDIT_CRYPTO_PARAM_CHANGE_USER", 2401 } };
    std::ostringstream os_;
};
class OperateLoggerEntry : public AuditLoggerEntry {
public:
    explicit OperateLoggerEntry(std::string interface, RecordType type);

    void Sendlog(AuditLoggerEntry &auditLoggerEntry) override;

private:
    std::string interface_;
    RecordType type_;
};
class RuntimeLoggerEntry : public AuditLoggerEntry {
public:
    explicit RuntimeLoggerEntry(RecordType type);

    void Sendlog(AuditLoggerEntry &auditLoggerEntry) override;

private:
    RecordType type_;
};
class SecurityLoggerEntry : public AuditLoggerEntry {
public:
    explicit SecurityLoggerEntry(std::string interface, RecordType type);

    void Sendlog(AuditLoggerEntry &auditLoggerEntry) override;

private:
    std::string interface_;
    RecordType type_;
};

struct UbseAuditlog {
    bool operator == (AuditLoggerEntry &auditloggerEntry);
};
}
#endif
#endif // UBSE_MANAGER_UBSE_LOGGER_AUDIT_H
