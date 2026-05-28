/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
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

#ifndef VM_MEM_MANAGER_H
#define VM_MEM_MANAGER_H

#include <memory>

#include "mempooling_module.h"
#include "vm_mem.h"

namespace vm::overcommit {
using namespace vm::mempooling;

enum class MemOpType {
    BORROW,
    MIGRATE,
    RETURN,
    OTHER,
};

struct UserInfo {
    uid_t uid;
    std::string username;
};

struct MemOpMetadata {
    std::vector<VMPresetParam> vmPresetParam{};
    std::vector<pid_t> pids{};
    std::vector<uint64_t> borrowSizes{};
    std::vector<std::string> borrowIds{};
    std::vector<uint16_t> presentNumaIds{};
    WaterMark waterMark{};
    UserInfo userInfo{};
};

struct MemOpStruct {
    NodeLocInfo nodeLoc{};
    MemOpType type = MemOpType::OTHER;
    MemOpMetadata memOpMetadata{};
};

class Manager {
public:
    virtual ~Manager() = default;
    virtual void Run() = 0;
};

class VmMemManager : public Manager {
public:
    explicit VmMemManager(MemOpStruct memOpStruct);
    VmMemManager() = default;
    ~VmMemManager() override = default;

protected:
    MemOpStruct memOpStruct{};
    [[nodiscard]] bool RunPreflight(MemOpType type) const;
    VmResult RunMemBorrow(MemBorrowExecuteResult& borrowResult);
    VmResult OutMemMigrate();
    VmResult OutMemReturn();

private:
    void CleanEmptyBorrowRes(MemBorrowExecuteResult& result);
};
} // namespace vm::overcommit
#endif // VM_MEM_MANAGER_H
