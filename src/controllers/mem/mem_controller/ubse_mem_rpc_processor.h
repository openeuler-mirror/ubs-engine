// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

#ifndef UBSE_MEM_RPC_H
#define UBSE_MEM_RPC_H
#include <cstdint>
#include <string>
#include "ubse_com.h"
#include "ubse_com_base.h"
#include "ubse_com_module.h"
#include "ubse_error.h"
#include "ubse_ipc_server.h"
#include "ubse_mmi_interface.h"

namespace ubse::mem::controller {
using ubse::adapter_plugins::mmi::UbseMemFdPermissionReq;
using ubse::adapter_plugins::mmi::UbseMemFdPermissionResp;
using ubse::com::UbseComBaseMessageHandler;
using ubse::com::UbseComBaseMessageHandlerCtxPtr;
using ubse::com::UbseComModule;
using ubse::utils::Ref;

class UbseMemFdBorrowMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

class UbseMemNumaBorrowMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

class UbseMemAddrBorrowMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

class UbseMemShareBorrowMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

class UbseMemShareAttachMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

class UbseMemShareDetachMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

class UbseMemFdReturnHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

class UbseMemNumaReturnHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

class UbseMemShareReturnHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

class UbseMemAddrReturnHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

class UbseMemCallbackMessage : public UbseBaseMessage {
public:
    UbseMemCallbackMessage();

    explicit UbseMemCallbackMessage(std::string dataRaw);

    UbseResult Serialize() override;

    UbseResult Deserialize() override;

    std::string data;
};

using UbseMemCallbackMessagePtr = Ref<UbseMemCallbackMessage>;
class UbseMemFdBorrowExportObjCallbackMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

class UbseMemFdBorrowImportObjCallbackMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

class UbseMemFdBorrowImportObjForPermissionCallbackMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

class UbseMemNumaBorrowExportObjCallbackMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

class UbseMemNumaBorrowImportObjCallbackMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

class UbseMemShareBorrowExportObjCallbackMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

class UbseMemShareBorrowImportObjCallbackMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

class UbseMemAddrBorrowExportObjCallbackMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

class UbseMemAddrBorrowImportObjCallbackMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};
class UbseMemShmCreateRespMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};
class UbseMemShmAttachRespMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};
class UbseMemShmDetachRespMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};
class UbseMemReturnRespHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

class UbseMemFdPermissionReqMessage : public UbseBaseMessage {
public:
    UbseMemFdPermissionReqMessage() = default;

    explicit UbseMemFdPermissionReqMessage(ubse::adapter_plugins::mmi::UbseMemFdPermissionReq fdPermissionReq);

    UbseResult Serialize() override;

    UbseResult Deserialize() override;

    UbseMemFdPermissionReq fdPermissionReq{};
};

class UbseMemFdPermissionRespMessage : public UbseBaseMessage {
public:
    UbseMemFdPermissionRespMessage() = default;

    explicit UbseMemFdPermissionRespMessage(UbseMemFdPermissionResp fdPermissionResp);

    UbseResult Serialize() override;

    UbseResult Deserialize() override;

    UbseMemFdPermissionResp fdPermissionResp{};
};

class UbseMemFdPermissionHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

class UbseMemFdBorrowRespMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

class UbseMemFdReturnRespMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};
class UbseMemNumaBorrowRespMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};
class UbseMemNumaReturnRespMessageHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};

class MemScheduleHandler {
public:
    MemScheduleHandler() {}

    ~MemScheduleHandler() {}

    static UbseResult RegHandler();

private:
    static UbseResult RegisterFdBorrowHandlers(const std::shared_ptr<UbseComModule>& comModule);
    static UbseResult RegisterNumaBorrowHandlers(const std::shared_ptr<UbseComModule>& comModule);
    static UbseResult RegisterAddrBorrowHandlers(const std::shared_ptr<UbseComModule>& comModule);
    static UbseResult RegisterShareBorrowHandlers(const std::shared_ptr<UbseComModule>& comModule);
    static UbseResult RegisterReturnHandler(const std::shared_ptr<UbseComModule>& comModule);
    static UbseResult RegisterShmCreateRespHandlers(const std::shared_ptr<UbseComModule>& comModule);
    static UbseResult RegisterShmAttachRespHandlers(const std::shared_ptr<UbseComModule>& comModule);
    static UbseResult RegisterShmDetachRespHandlers(const std::shared_ptr<UbseComModule>& comModule);
};
} // namespace ubse::mem::controller
#endif // UBSE_MEM_RPC_H
