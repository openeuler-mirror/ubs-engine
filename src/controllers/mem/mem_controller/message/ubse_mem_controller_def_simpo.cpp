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

#include "ubse_mem_controller_def_simpo.h"

#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_mem_controller_def_serial.h"
#include "ubse_serial_util.h"

namespace ubse::mem::controller::message {
UBSE_DEFINE_THIS_MODULE("ubse");

UbseResult UbseMemFdDescSimpo::Serialize()
{
    UbseSerialization out;
    UbseErrCodeSerialization(out, mErrCode);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize ErrCode failed.";
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    UbseMemFdDescSerialization(out, data_);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize failed.";
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseMemFdDescSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    serial::UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    if (!UbseErrCodeDeserialization(in, mErrCode)) {
        UBSE_LOG_ERROR << "Failed to deserialize ErrCode";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    if (!UbseMemFdDescDeserialization(in, data_)) {
        UBSE_LOG_ERROR << "Failed to deserialize UbseMemFdDesc";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    return UBSE_OK;
}

UbseResult UbseMemFdDescListSimpo::Serialize()
{
    serial::UbseSerialization out;
    UbseErrCodeSerialization(out, mErrCode);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize ErrCode failed.";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    UbseMemFdDescListSerialization(out, data_);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize failed.";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseMemFdDescListSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    serial::UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    if (!UbseErrCodeDeserialization(in, mErrCode)) {
        UBSE_LOG_ERROR << "Failed to deserialize ErrCode";
        return UBSE_ERROR;
    }
    if (!UbseMemFdDescListDeserialization(in, data_)) {
        UBSE_LOG_ERROR << "Failed to deserialize UbseMemFdDescList";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
UbseResult DefUbseMemNumaDescSimpo::Serialize()
{
    serial::UbseSerialization out;
    UbseErrCodeSerialization(out, mErrCode);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize ErrCode failed.";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    UbseDefMemNumaDescSerialization(out, data_);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize failed.";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult DefUbseMemNumaDescSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    serial::UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    if (!UbseErrCodeDeserialization(in, mErrCode)) {
        UBSE_LOG_ERROR << "Failed to deserialize ErrCode";
        return UBSE_ERROR;
    }
    if (!UbseDefMemNumaDescDeSerialization(in, data_)) {
        UBSE_LOG_ERROR << "Failed to deserialize DefUbseMemNumaDesc";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult DefUbseMemNumaDescListSimpo::Serialize()
{
    serial::UbseSerialization out;
    UbseErrCodeSerialization(out, mErrCode);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize ErrCode failed.";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    UbseDefMemNumaDescListSerialization(out, data_);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize failed.";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult DefUbseMemNumaDescListSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    serial::UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    if (!UbseErrCodeDeserialization(in, mErrCode)) {
        UBSE_LOG_ERROR << "Failed to deserialize ErrCode";
        return UBSE_ERROR;
    }
    if (!UbseDefMemNumaDescListDeSerialization(in, data_)) {
        UBSE_LOG_ERROR << "Failed to deserialize DefUbseMemNumaDescList";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
UbseResult UbseMemNumaDescSimpo::Serialize()
{
    serial::UbseSerialization out;
    UbseErrCodeSerialization(out, mErrCode);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize ErrCode failed.";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    UbseMemNumaDescSerialization(out, data_);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize failed.";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseMemNumaDescSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    serial::UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    if (!UbseErrCodeDeserialization(in, mErrCode)) {
        UBSE_LOG_ERROR << "Failed to deserialize ErrCode";
        return UBSE_ERROR;
    }
    if (!UbseMemNumaDescDeSerialization(in, data_)) {
        UBSE_LOG_ERROR << "Failed to deserialize UbseMemNumaDesc";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseMemAddrDescSimpo::Serialize()
{
    serial::UbseSerialization out;
    UbseErrCodeSerialization(out, mErrCode);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize ErrCode failed.";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    UbseMemAddrDescSerialization(out, data_);

    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize failed.";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseMemAddrDescSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    serial::UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    if (!UbseErrCodeDeserialization(in, mErrCode)) {
        UBSE_LOG_ERROR << "Failed to deserialize ErrCode";
        return UBSE_ERROR;
    }
    if (!UbseMemAddrDescDeSerialization(in, data_)) {
        UBSE_LOG_ERROR << "Failed to deserialize UbseMemAddrDesc";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseMemShmDescSimpo::Serialize()
{
    serial::UbseSerialization out;
    UbseErrCodeSerialization(out, mErrCode);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize ErrCode failed.";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    UbseMemShmDescSerialization(out, data_);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize failed.";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseMemShmDescSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    serial::UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    if (!UbseErrCodeDeserialization(in, mErrCode)) {
        UBSE_LOG_ERROR << "Failed to deserialize ErrCode";
        return UBSE_ERROR;
    }
    if (!UbseMemShmDescDeSerialization(in, data_)) {
        UBSE_LOG_ERROR << "Failed to deserialize UbseMemShmDesc";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseMemShmDescListSimpo::Serialize()
{
    serial::UbseSerialization out;
    UbseErrCodeSerialization(out, mErrCode);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize ErrCode failed.";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    UbseMemShmDescListSerialization(out, data_);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize failed.";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseMemShmDescListSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    serial::UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    if (!UbseErrCodeDeserialization(in, mErrCode)) {
        UBSE_LOG_ERROR << "Failed to deserialize ErrCode";
        return UBSE_ERROR;
    }
    if (!UbseMemShmDescListDeSerialization(in, data_)) {
        UBSE_LOG_ERROR << "Failed to deserialize UbseMemShmDescList";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
UbseResult UbseMemShmMemStatusDescSimpo::Serialize()
{
    serial::UbseSerialization out;
    UbseErrCodeSerialization(out, mErrCode);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize ErrCode failed.";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    UbseMemShmMemStatusDescSerialization(out, data_);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize failed.";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseMemShmMemStatusDescSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    serial::UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    if (!UbseErrCodeDeserialization(in, mErrCode)) {
        UBSE_LOG_ERROR << "Failed to deserialize ErrCode";
        return UBSE_ERROR;
    }
    if (!UbseMemShmMemStatusDescDeSerialization(in, data_)) {
        UBSE_LOG_ERROR << "Failed to deserialize UbseMemShmMemStatusDesc";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseMemDebtQueryRequestSimpo::Serialize()
{
    serial::UbseSerialization out;
    out << debtQueryRequest_.name << debtQueryRequest_.importNodeId << debtQueryRequest_.exportNodeId;
    UbseUdsInfoSerialization(out, debtQueryRequest_.udsInfo);
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Serialize failed.";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseMemDebtQueryRequestSimpo::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    serial::UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    in >> debtQueryRequest_.name >> debtQueryRequest_.importNodeId >> debtQueryRequest_.exportNodeId;
    if (!UbseUdsInfoDeserialization(in, debtQueryRequest_.udsInfo)) {
        UBSE_LOG_ERROR << "Failed to deserialize UbseUdsInfo";
        return UBSE_ERROR;
    }

    return UBSE_OK;
}

UbseResult UbseMemNodeBorrowInfoMessage::Serialize()
{
    UbseSerialization serialization{};
    if (!UbseNodeBorrowInfosSerialize(serialization, nodeBorrowInfos_)) {
        UBSE_LOG_ERROR << "Serialize UbseNodeBorrowInfos failed";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = serialization.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(serialization.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseMemNodeBorrowInfoMessage::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null";
        return UBSE_ERROR_NULLPTR;
    }
    UbseDeSerialization deSerialization(mInputRawData.get(), mInputRawDataSize);
    if (!UbseNodeBorrowInfosDeserialize(deSerialization, nodeBorrowInfos_)) {
        UBSE_LOG_ERROR << "Deserialize UbseNodeBorrowInfos failed";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseMemNodeBorrowInfoReqMessage::Serialize()
{
    mOutputRawDataSize = 1; // 创建一个size为1的数组
    mOutputRawData = std::make_unique<uint8_t[]>(mOutputRawDataSize);
    return UBSE_OK;
}

UbseResult UbseMemNodeBorrowInfoReqMessage::Deserialize()
{
    return UBSE_OK;
}
} // namespace ubse::mem::controller::message