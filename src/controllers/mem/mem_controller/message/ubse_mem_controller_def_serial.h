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
#ifndef UBSE_MEM_CONTROLLER_DEF_SERIAL_H
#define UBSE_MEM_CONTROLLER_DEF_SERIAL_H

#include "ubse_mem_controller_def.h"
#include "ubse_serial_util.h"
namespace ubse::mem::controller::message {
using namespace ubse::serial;
using namespace controller;

void UbseErrCodeSerialization(UbseSerialization& out, const uint32_t& mErrCode);
bool UbseErrCodeDeserialization(UbseDeSerialization& in, uint32_t& mErrCode);
void UbseUdsInfoSerialization(UbseSerialization& out, const def::UbseUdsInfo& udsInfo);
bool UbseUdsInfoDeserialization(UbseDeSerialization& in, def::UbseUdsInfo& udsInfo);
void UbseMemFdDescSerialization(UbseSerialization& out, const def::UbseMemFdDesc& desc);
bool UbseMemFdDescDeserialization(UbseDeSerialization& in, def::UbseMemFdDesc& desc);

void UbseMemFdDescListSerialization(UbseSerialization& out, const std::vector<def::UbseMemFdDesc>& descList);
bool UbseMemFdDescListDeserialization(UbseDeSerialization& in, std::vector<def::UbseMemFdDesc>& descList);

void UbseDefMemNumaDescSerialization(UbseSerialization& out, const def::UbseMemNumaDesc& desc);
bool UbseDefMemNumaDescDeSerialization(UbseDeSerialization& in, def::UbseMemNumaDesc& desc);

void UbseDefMemNumaDescListSerialization(UbseSerialization& out, const std::vector<def::UbseMemNumaDesc>& descList);
bool UbseDefMemNumaDescListDeSerialization(UbseDeSerialization& in, std::vector<def::UbseMemNumaDesc>& descList);

void UbseMemNumaDescSerialization(UbseSerialization& out, const UbseMemNumaDesc& desc);
bool UbseMemNumaDescDeSerialization(UbseDeSerialization& in, UbseMemNumaDesc& desc);

void UbseMemAddrDescSerialization(UbseSerialization& out, const UbseMemAddrDesc& desc);
bool UbseMemAddrDescDeSerialization(UbseDeSerialization& in, UbseMemAddrDesc& desc);

void UbseMemShmDescSerialization(UbseSerialization& out, const def::UbseMemShmDesc& desc);
bool UbseMemShmDescDeSerialization(UbseDeSerialization& in, def::UbseMemShmDesc& desc);
void UbseMemShmDescListSerialization(UbseSerialization& out, const std::vector<def::UbseMemShmDesc>& descList);
bool UbseMemShmDescListDeSerialization(UbseDeSerialization& in, std::vector<def::UbseMemShmDesc>& descList);
void UbseMemShmMemStatusDescSerialization(UbseSerialization& out, const def::UbseMemShmMemStatusDesc& desc);
bool UbseMemShmMemStatusDescDeSerialization(UbseDeSerialization& in, def::UbseMemShmMemStatusDesc& desc);

bool UbseNodeBorrowInfosSerialize(UbseSerialization &serialization,
                                  const std::vector<def::UbseNodeBorrowInfo> &nodeBorrowInfos);

bool UbseNodeBorrowInfosDeserialize(UbseDeSerialization &deSerialization,
                                    std::vector<def::UbseNodeBorrowInfo> &nodeBorrowInfos);

bool UbseCliShmDescSerialize(const ubse::mem::def::UbseMemShmDesc &shmDesc, const std::string &importNodeId,
                             UbseSerialization &serialization);
} // namespace ubse::mem::controller::message
#endif // UBSE_MEM_CONTROLLER_DEF_SERIAL_H
