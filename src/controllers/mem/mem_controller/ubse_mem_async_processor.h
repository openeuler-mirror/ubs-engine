// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
#ifndef UBSE_MANAGER_UBSE_MEM_ASYNC_PROCESSOR_H
#define UBSE_MANAGER_UBSE_MEM_ASYNC_PROCESSOR_H
#include "ubse_com_def.h"
#include "ubse_mmi_interface.h"
#include "message/ubse_mem_simpo_types.h"

namespace ubse::mem::controller {
using ubse::adapter_plugins::mmi::UbseMemNumaBorrowReq;
using ubse::adapter_plugins::mmi::UbseMemOperationResp;
using ubse::adapter_plugins::mmi::UbseMemReturnReq;
using ubse::common::def::UbseResult;
UbseResult AsyncMemShmBorrowProcessor(message::UbseMemShareBorrowReqSimpoPtr request);
UbseResult AsyncMemShmBorrowRespProcessor(message::UbseMemOperationRespSimpoPtr request);
UbseResult AsyncMemShmAttachProcessor(message::UbseMemShareAttachReqSimpoPtr request);
UbseResult AsyncMemShmAttachRespProcessor(message::UbseMemOperationRespSimpoPtr request);
UbseResult AsyncMemShmDetachProcessor(message::UbseMemShareDetachReqSimpoPtr request,
                                      const std::string& realRequestNodeId);
UbseResult AsyncMemShmDetachRespProcessor(message::UbseMemOperationRespSimpoPtr request);
UbseResult AsyncMemShmReturnProcessor(message::UbseMemReturnReqSimpoPtr request, const std::string& realRequestNodeId);
UbseResult AsyncMemCommonReturnRespProcessor(message::UbseMemOperationRespSimpoPtr request);
UbseResult DoNumaBorrowAsync(const UbseMemNumaBorrowReq& request);
UbseResult DoNumaBorrowRespAsync(const UbseMemOperationResp& resp);
UbseResult DoReturnAsync(const UbseMemReturnReq& request, const std::string& realRequestNodeId);
UbseResult DoReturnRespAsync(const UbseMemOperationResp& resp);
} // namespace ubse::mem::controller

#endif // UBSE_MANAGER_UBSE_MEM_ASYNC_PROCESSOR_H
