// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
#ifndef UBSE_MANAGER_UBSE_MEM_ASYNC_PROCESSOR_H
#define UBSE_MANAGER_UBSE_MEM_ASYNC_PROCESSOR_H
#include "ubse_com_def.h"
#include "ubse_mmi_interface.h"
#include "message/ubse_mem_numa_borrow_req_simpo.h"
#include "message/ubse_mem_operation_resp_simpo.h"
#include "message/ubse_mem_return_req_simpo.h"
#include "message/ubse_mem_share_attach_req_simpo.h"
#include "message/ubse_mem_share_borrow_req_simpo.h"
#include "message/ubse_mem_share_detach_req_simpo.h"

namespace ubse::mem::controller {
using namespace ubse::common::def;
using namespace ubse::adapter_plugins::mmi;
message::UbseResult AsyncMemShmBorrowProcessor(message::UbseMemShareBorrowReqSimpoPtr request);
message::UbseResult AsyncMemShmBorrowRespProcessor(message::UbseMemOperationRespSimpoPtr request);
message::UbseResult AsyncMemShmAttachProcessor(message::UbseMemShareAttachReqSimpoPtr request);
message::UbseResult AsyncMemShmAttachRespProcessor(message::UbseMemOperationRespSimpoPtr request);
message::UbseResult AsyncMemShmDetachProcessor(message::UbseMemShareDetachReqSimpoPtr request,
                                               const std::string& realRequestNodeId);
message::UbseResult AsyncMemShmDetachRespProcessor(message::UbseMemOperationRespSimpoPtr request);
message::UbseResult AsyncMemShmReturnProcessor(message::UbseMemReturnReqSimpoPtr request,
                                               const std::string& realRequestNodeId);
message::UbseResult AsyncMemCommonReturnRespProcessor(message::UbseMemOperationRespSimpoPtr request);
UbseResult DoNumaBorrowAsync(const UbseMemNumaBorrowReq& request);
UbseResult DoNumaBorrowRespAsync(const UbseMemOperationResp& resp);
UbseResult DoReturnAsync(const UbseMemReturnReq& request, const std::string& realRequestNodeId);
UbseResult DoReturnRespAsync(const UbseMemOperationResp& resp);
} // namespace ubse::mem::controller

#endif // UBSE_MANAGER_UBSE_MEM_ASYNC_PROCESSOR_H
