/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_MEM_RESIDUAL_DECODER_H
#define UBSE_MEM_RESIDUAL_DECODER_H

#include <cstdint>

namespace ubse::mem::controller {

void AddToResidualDecoderSet(uint32_t ubpuId, uint32_t iouId, uint32_t marId, uint8_t decoderIdx, uint64_t handle);
uint32_t TryCleanResidualDecoderEntry(uint32_t ubpuId, uint32_t iouId, uint32_t marId, uint8_t decoderIdx);
void RemoveFromResidualDecoderSet(uint32_t ubpuId, uint32_t iouId, uint32_t marId, uint8_t decoderIdx);

} // namespace ubse::mem::controller

#endif
