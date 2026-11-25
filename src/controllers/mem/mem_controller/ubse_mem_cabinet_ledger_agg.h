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

#ifndef UBS_ENGINE_UBSE_MEM_CABINET_LEDGER_AGG_H
#define UBS_ENGINE_UBSE_MEM_CABINET_LEDGER_AGG_H

#include "ubse_def.h"
#include "ubse_common_def.h"

namespace ubse::mem::controller {
using ubse::common::def::UbseResult;

 /**
 * @brief B 侧柜级聚合账本查询应答（0x001A，柜主 -> 柜主）
 *        柜内串行采集、三态打标、按请求柜过滤、单飞保护；详见设计文档 §5.2
 * @param req [in] UbseCabinetLedgerQueryReq 序列化报文
 * @param resp [out] UbseCabinetLedgerQueryResp 序列化报文
 * @return UBSE_OK 表示报文已正常回填（逐节点失败体现在 items 内），其它为构造失败
 */
UbseResult CabinetLedgerQueryHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);
} // namespace ubse::mem::controller

#endif // UBS_ENGINE_UBSE_MEM_CABINET_LEDGER_AGG_H
