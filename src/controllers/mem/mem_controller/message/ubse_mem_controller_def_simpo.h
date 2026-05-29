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
#ifndef UBSE_MEM_CONTROLLER_DEF_SIMPO_H
#define UBSE_MEM_CONTROLLER_DEF_SIMPO_H

#include "ubse_base_message.h"
#include "ubse_mem_controller_def.h"

namespace ubse::mem::controller::message {
using ubse::message::UbseBaseMessage;
using ubse::utils::Ref;
class UbseMemFdDescSimpo : public UbseBaseMessage {
public:
    UbseMemFdDescSimpo() = default;
    explicit UbseMemFdDescSimpo(uint8_t* data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    void SetUbseMemFdDesc(def::UbseMemFdDesc desc)
    {
        data_ = std::move(desc);
    }

    def::UbseMemFdDesc GetUbseMemFdDesc()
    {
        return data_;
    }
    UbseResult Serialize() override;
    UbseResult Deserialize() override;

private:
    def::UbseMemFdDesc data_;
};

class UbseMemFdDescListSimpo : public UbseBaseMessage {
public:
    UbseMemFdDescListSimpo() = default;
    explicit UbseMemFdDescListSimpo(uint8_t* data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    void SetUbseMemFdDescList(std::vector<def::UbseMemFdDesc> desc)
    {
        data_ = std::move(desc);
    }

    std::vector<def::UbseMemFdDesc> GetUbseMemFdDescList()
    {
        return data_;
    }
    UbseResult Serialize() override;
    UbseResult Deserialize() override;

private:
    std::vector<def::UbseMemFdDesc> data_;
};

class DefUbseMemNumaDescSimpo : public UbseBaseMessage {
public:
    DefUbseMemNumaDescSimpo() = default;
    explicit DefUbseMemNumaDescSimpo(uint8_t* data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    void SetUbseMemNumaDesc(def::UbseMemNumaDesc desc)
    {
        data_ = std::move(desc);
    }

    def::UbseMemNumaDesc GetUbseMemNumaDesc()
    {
        return data_;
    }
    UbseResult Serialize() override;
    UbseResult Deserialize() override;

private:
    def::UbseMemNumaDesc data_;
};
class DefUbseMemNumaDescListSimpo : public UbseBaseMessage {
public:
    DefUbseMemNumaDescListSimpo() = default;
    explicit DefUbseMemNumaDescListSimpo(uint8_t* data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    void SetUbseMemNumaDescList(std::vector<def::UbseMemNumaDesc> desc)
    {
        data_ = std::move(desc);
    }

    std::vector<def::UbseMemNumaDesc> GetUbseMemNumaDescList()
    {
        return data_;
    }
    UbseResult Serialize() override;
    UbseResult Deserialize() override;

private:
    std::vector<def::UbseMemNumaDesc> data_;
};
class UbseMemNumaDescSimpo : public UbseBaseMessage {
public:
    UbseMemNumaDescSimpo() = default;
    explicit UbseMemNumaDescSimpo(uint8_t* data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    void SetUbseMemNumaDesc(UbseMemNumaDesc desc)
    {
        data_ = std::move(desc);
    }

    UbseMemNumaDesc GetUbseMemNumaDesc()
    {
        return data_;
    }
    UbseResult Serialize() override;
    UbseResult Deserialize() override;

private:
    UbseMemNumaDesc data_;
};
class UbseMemAddrDescSimpo : public UbseBaseMessage {
public:
    UbseMemAddrDescSimpo() = default;
    explicit UbseMemAddrDescSimpo(uint8_t* data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    void SetUbseMemAddrDesc(UbseMemAddrDesc desc)
    {
        data_ = std::move(desc);
    }

    UbseMemAddrDesc GetUbseMemAddrDesc()
    {
        return data_;
    }
    UbseResult Serialize() override;
    UbseResult Deserialize() override;

private:
    UbseMemAddrDesc data_;
};
class UbseMemShmDescSimpo : public UbseBaseMessage {
public:
    UbseMemShmDescSimpo() = default;
    explicit UbseMemShmDescSimpo(uint8_t* data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    void SetUbseMemShmDesc(def::UbseMemShmDesc desc)
    {
        data_ = std::move(desc);
    }

    def::UbseMemShmDesc GetUbseMemShmDesc()
    {
        return data_;
    }
    UbseResult Serialize() override;
    UbseResult Deserialize() override;

private:
    def::UbseMemShmDesc data_;
};
class UbseMemShmDescListSimpo : public UbseBaseMessage {
public:
    UbseMemShmDescListSimpo() = default;
    explicit UbseMemShmDescListSimpo(uint8_t* data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    void SetUbseMemShmDescList(std::vector<def::UbseMemShmDesc> desc)
    {
        data_ = std::move(desc);
    }

    std::vector<def::UbseMemShmDesc> GetUbseMemShmDescList()
    {
        return data_;
    }
    UbseResult Serialize() override;
    UbseResult Deserialize() override;

private:
    std::vector<def::UbseMemShmDesc> data_;
};
class UbseMemShmMemStatusDescSimpo : public UbseBaseMessage {
public:
    UbseMemShmMemStatusDescSimpo() = default;
    explicit UbseMemShmMemStatusDescSimpo(uint8_t* data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    void SetUbseMemShmMemStatusDesc(def::UbseMemShmMemStatusDesc desc)
    {
        data_ = std::move(desc);
    }

    def::UbseMemShmMemStatusDesc GetUbseMemShmMemStatusDesc()
    {
        return data_;
    }
    UbseResult Serialize() override;
    UbseResult Deserialize() override;

private:
    def::UbseMemShmMemStatusDesc data_;
};
class UbseMemDebtQueryRequestSimpo : public UbseBaseMessage {
public:
    UbseMemDebtQueryRequestSimpo() = default;
    explicit UbseMemDebtQueryRequestSimpo(uint8_t* data, uint32_t size)
    {
        SetInputRawData(data, size);
    }

    void SetUbseMemDebtQueryRequest(def::UbseMemDebtQueryRequest request)
    {
        debtQueryRequest_ = std::move(request);
    }

    def::UbseMemDebtQueryRequest GetUbseMemDebtQueryRequest()
    {
        return debtQueryRequest_;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    def::UbseMemDebtQueryRequest debtQueryRequest_;
};

class UbseMemIdQueryRequestSimpo : public UbseBaseMessage {
public:
    UbseMemIdQueryRequestSimpo() = default;
    explicit UbseMemIdQueryRequestSimpo(uint8_t* data, uint32_t size)
    {
        SetInputRawData(data, size);
    }

    void SetUbseMemIdQueryRequest(def::UbseMemIdQueryRequest request)
    {
        memIdQueryRequest_ = std::move(request);
    }

    def::UbseMemIdQueryRequest GetUbseMemIdQueryRequest()
    {
        return memIdQueryRequest_;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    def::UbseMemIdQueryRequest memIdQueryRequest_;
};

class UbseMemExportMemDescSimpo : public UbseBaseMessage {
public:
    UbseMemExportMemDescSimpo() = default;
    explicit UbseMemExportMemDescSimpo(uint8_t* data, uint32_t size)
    {
        SetInputRawData(data, size);
    }

    void SetUbseMemExportMemDesc(def::UbseExportMemDesc request)
    {
        exportMemDesc_ = std::move(request);
    }

    def::UbseExportMemDesc GetUbseMemExportMemDesc()
    {
        return exportMemDesc_;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    def::UbseExportMemDesc exportMemDesc_;
};

class UbseMemNodeBorrowInfoMessage : public UbseBaseMessage {
public:
    UbseMemNodeBorrowInfoMessage() = default;
    explicit UbseMemNodeBorrowInfoMessage(uint8_t* data, uint32_t size)
    {
        SetInputRawData(data, size);
    }
    inline void SetUbseNodeBorrowInfos(std::vector<def::UbseNodeBorrowInfo> info)
    {
        nodeBorrowInfos_ = std::move(info);
    }

    inline std::vector<def::UbseNodeBorrowInfo> GetUbseNodeBorrowInfos()
    {
        return nodeBorrowInfos_;
    }
    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    std::vector<def::UbseNodeBorrowInfo> nodeBorrowInfos_{};
};

class UbseMemNodeBorrowInfoReqMessage : public UbseBaseMessage {
public:
    UbseMemNodeBorrowInfoReqMessage() = default;

    UbseResult Serialize() override;

    UbseResult Deserialize() override;
};

using UbseMemFdDescSimpoPtr = Ref<UbseMemFdDescSimpo>;
using UbseMemFdDescListSimpoPtr = Ref<UbseMemFdDescListSimpo>;
using DefUbseMemNumaDescSimpoPtr = Ref<DefUbseMemNumaDescSimpo>;
using DefUbseMemNumaDescListSimpoPtr = Ref<DefUbseMemNumaDescListSimpo>;
using UbseMemNumaDescSimpoPtr = Ref<UbseMemNumaDescSimpo>;
using UbseMemAddrDescSimpoPtr = Ref<UbseMemAddrDescSimpo>;
using UbseMemShmDescSimpoPtr = Ref<UbseMemShmDescSimpo>;
using UbseMemShmDescListSimpoPtr = Ref<UbseMemShmDescListSimpo>;
using UbseMemShmMemStatusDescSimpoPtr = Ref<UbseMemShmMemStatusDescSimpo>;
using UbseMemDebtQueryRequestSimpoPtr = Ref<UbseMemDebtQueryRequestSimpo>;
using UbseMemIdQueryRequestSimpoPtr = Ref<UbseMemIdQueryRequestSimpo>;
using UbseMemExportMemDescSimpoPtr = Ref<UbseMemExportMemDescSimpo>;
using UbseMemNodeBorrowInfoMessagePtr = Ref<UbseMemNodeBorrowInfoMessage>;
using UbseMemNodeBorrowInfoReqMessagePtr = Ref<UbseMemNodeBorrowInfoReqMessage>;
} // namespace ubse::mem::controller::message
#endif // UBSE_MEM_CONTROLLER_DEF_SIMPO_H
