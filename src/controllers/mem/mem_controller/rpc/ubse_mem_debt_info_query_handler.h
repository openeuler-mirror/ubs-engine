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

#ifndef UBSE_MEM_DEBT_INFO_QUERY_HANDLER_H
#define UBSE_MEM_DEBT_INFO_QUERY_HANDLER_H
#include <ubse_com_module.h>

#include "ubse_com_base.h"
namespace ubse::mem::controller::rpc {
using ubse::com::UbseComBaseMessageHandler;
using ubse::com::UbseComBaseMessageHandlerCtxPtr;
using ubse::com::UbseMemQueryOpCode;
using ubse::com::UbseModuleCode;
class UbseMemDebtInfoQueryHandler : public UbseComBaseMessageHandler {
public:
    UbseMemDebtInfoQueryHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBINFO_QUERY);
    }

    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY);
    }

    bool NeedReply() override
    {
        return true;
    }

    static UbseResult RegUbseMemDebtInfoQueryHandler();
};

class UbseMemDebtInfoFdGetHandler : public UbseComBaseMessageHandler {
public:
    UbseMemDebtInfoFdGetHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_FD_GET);
    }

    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY);
    }

    bool NeedReply() override
    {
        return true;
    }
};
class UbseMemDebtInfoFdListHandler : public UbseComBaseMessageHandler {
public:
    UbseMemDebtInfoFdListHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_FD_LIST);
    }

    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY);
    }

    bool NeedReply() override
    {
        return true;
    }
};
class UbseMemDebtInfoNumaGetHandler : public UbseComBaseMessageHandler {
public:
    UbseMemDebtInfoNumaGetHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_NUMA_GET);
    }

    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY);
    }

    bool NeedReply() override
    {
        return true;
    }
};
class UbseMemDebtInfoNumaGetWithImportNodeHandler : public UbseComBaseMessageHandler {
public:
    UbseMemDebtInfoNumaGetWithImportNodeHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_NUMA_GET_WITH_IMPORT_NODE);
    }

    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY);
    }

    bool NeedReply() override
    {
        return true;
    }
};
class UbseMemDebtInfoNumaListHandler : public UbseComBaseMessageHandler {
public:
    UbseMemDebtInfoNumaListHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_NUMA_LIST);
    }

    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY);
    }

    bool NeedReply() override
    {
        return true;
    }
};
class UbseMemDebtInfoShmGetHandler : public UbseComBaseMessageHandler {
public:
    UbseMemDebtInfoShmGetHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_SHM_GET);
    }

    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY);
    }

    bool NeedReply() override
    {
        return true;
    }
};
class UbseMemDebtInfoShmListHandler : public UbseComBaseMessageHandler {
public:
    UbseMemDebtInfoShmListHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_SHM_LIST);
    }

    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY);
    }

    bool NeedReply() override
    {
        return true;
    }
};
class UbseMemDebtInfoShmStatusGetHandler : public UbseComBaseMessageHandler {
public:
    UbseMemDebtInfoShmStatusGetHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_SHM_STATUS_GET);
    }

    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY);
    }

    bool NeedReply() override
    {
        return true;
    }
};
class UbseMemDebtInfoAddrGetHandler : public UbseComBaseMessageHandler {
public:
    UbseMemDebtInfoAddrGetHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_ADDR_GET);
    }

    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY);
    }

    bool NeedReply() override
    {
        return true;
    }
};

class UbseMemDebtInfoPartialFetchHandler : public UbseComBaseMessageHandler {
public:
    UbseMemDebtInfoPartialFetchHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_DEBT_INFO_PARTIAL_FETCH);
    }

    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY);
    }
    bool NeedReply() override
    {
        return true;
    }
};

class UbseMemNodeBorrowQueryHandler : public UbseComBaseMessageHandler {
public:
    UbseMemNodeBorrowQueryHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_NODE_BORROW_INFO_QUERY);
    }

    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY);
    }

    bool NeedReply() override
    {
        return true;
    }
};

class UbseMemIdInfoGetHandler : public UbseComBaseMessageHandler {
public:
    UbseMemIdInfoGetHandler() = default;

    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override
    {
        return static_cast<uint16_t>(UbseMemQueryOpCode::UBSE_MEM_ID_DEBINFO_QUERY);
    }

    uint16_t GetModuleCode() override
    {
        return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_QUERY);
    }

    bool NeedReply() override
    {
        return true;
    }
};
}; // namespace ubse::mem::controller::rpc

#endif // UBSE_MEM_DEBT_INFO_QUERY_HANDLER_H