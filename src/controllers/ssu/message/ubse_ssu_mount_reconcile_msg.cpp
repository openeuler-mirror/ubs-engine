/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_ssu_mount_reconcile_msg.h"

#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_serial_util.h"

namespace ubse::ssu::message {

using namespace ubse::log;
using namespace ubse::serial;

using ubse::adapter_plugins::ssu::def::UbseCreateBlockDeviceOptions;
using ubse::adapter_plugins::ssu::def::UbseSsuAddressingType;
using ubse::adapter_plugins::ssu::def::UbseSsuRaidLevel;
using ubse::ssu::persist::IsRecordFieldsValid;
using ubse::ssu::persist::MAX_ATTACH_RECORDS;

UBSE_DEFINE_THIS_MODULE("ubse");

// 既有serde声明（定义在ubse_ssu_alloc_msg.cpp / ubse_ssu_attach_detach_verify_msg.cpp）
UbseSerialization &operator<<(UbseSerialization &serializer, const UbseSsuNameSpaceInfo &info);
UbseDeSerialization &operator>>(UbseDeSerialization &deserializer, UbseSsuNameSpaceInfo &info);
UbseSerialization &operator<<(UbseSerialization &serializer, const UbseSsuNsVerifyInfo &info);
UbseDeSerialization &operator>>(UbseDeSerialization &deserializer, UbseSsuNsVerifyInfo &info);

// 挂载记录RPC序列化
// 与JSON持久化格式（ubse_ssu_attach_record.cpp）相互独立，仅走master-agent内部通道
UbseSerialization &operator<<(UbseSerialization &serializer, const UbseSsuAttachRecord &record)
{
    // serde框架对右值operator<<按需删除，枚举经具名局部变量序列化（同alloc msg惯例）
    const uint32_t strategy = static_cast<uint32_t>(record.strategy);
    const uint32_t recordState = static_cast<uint32_t>(record.recordState);
    serializer << record.name << strategy << recordState << record.devName << record.nqn << record.identity.uid
               << record.identity.userName;
    const uint32_t hasBlockDevice = static_cast<uint32_t>(record.blockDeviceOptions.has_value());
    serializer << hasBlockDevice;
    if (record.blockDeviceOptions.has_value()) {
        const uint32_t addressingType = static_cast<uint32_t>(record.blockDeviceOptions->addressingType);
        const uint32_t raidLevel = static_cast<uint32_t>(record.blockDeviceOptions->raidLevel);
        serializer << addressingType << raidLevel << record.blockDeviceOptions->chunkSize;
    }
    const auto nsCnt = static_cast<uint32_t>(record.nsList.size());
    serializer << nsCnt;
    for (const auto &ns : record.nsList) {
        serializer << ns;
    }
    const auto verifyCnt = static_cast<uint32_t>(record.nsVerifyList.size());
    serializer << verifyCnt;
    for (const auto &verify : record.nsVerifyList) {
        serializer << verify;
    }
    serializer << record.attachedAt;
    return serializer;
}

UbseDeSerialization &operator>>(UbseDeSerialization &deserializer, UbseSsuAttachRecord &record)
{
    uint32_t strategy = 0;
    uint32_t recordState = 0;
    deserializer >> record.name >> strategy >> recordState >> record.devName >> record.nqn
        >> record.identity.uid >> record.identity.userName;
    if (strategy > static_cast<uint32_t>(UbseSsuAllocStrategy::NORMAL)) {
        UBSE_LOG_ERROR << "Attach record rpc serde: invalid strategy=" << strategy;
        deserializer.SetFail();
        return deserializer;
    }
    record.strategy = static_cast<UbseSsuAllocStrategy>(strategy);
    if (recordState > static_cast<uint32_t>(UbseSsuAttachRecordState::SUSPECT)) {
        UBSE_LOG_ERROR << "Attach record rpc serde: invalid recordState=" << recordState;
        deserializer.SetFail();
        return deserializer;
    }
    record.recordState = static_cast<UbseSsuAttachRecordState>(recordState);
    uint32_t hasBlockDevice = 0;
    deserializer >> hasBlockDevice;
    record.blockDeviceOptions.reset();
    if (hasBlockDevice != 0) {
        UbseCreateBlockDeviceOptions options;
        uint32_t addressingType = 0;
        uint32_t raidLevel = 0;
        deserializer >> addressingType >> raidLevel >> options.chunkSize;
        options.addressingType = static_cast<UbseSsuAddressingType>(addressingType);
        options.raidLevel = static_cast<UbseSsuRaidLevel>(raidLevel);
        record.blockDeviceOptions = options;
    }
    uint32_t nsCnt = 0;
    deserializer >> nsCnt;
    record.nsList.clear();
    // 数量先cap再reserve/循环（防篡改报文内存耗尽，同alloc result模式）
    constexpr uint32_t MAX_RECORD_NS_NUM = 1024;
    if (nsCnt > MAX_RECORD_NS_NUM) {
        UBSE_LOG_ERROR << "Attach record rpc serde: ns num exceeds max limit, size=" << nsCnt;
        deserializer.SetFail();
        return deserializer;
    }
    for (uint32_t i = 0; i < nsCnt; ++i) {
        UbseSsuNameSpaceInfo ns;
        deserializer >> ns;
        if (!deserializer.Check()) {
            break;
        }
        record.nsList.emplace_back(std::move(ns));
    }
    uint32_t verifyCnt = 0;
    deserializer >> verifyCnt;
    record.nsVerifyList.clear();
    if (verifyCnt > MAX_RECORD_NS_NUM) {
        UBSE_LOG_ERROR << "Attach record rpc serde: verify num exceeds max limit, size=" << verifyCnt;
        deserializer.SetFail();
        return deserializer;
    }
    for (uint32_t i = 0; i < verifyCnt; ++i) {
        UbseSsuNsVerifyInfo verify;
        deserializer >> verify;
        if (!deserializer.Check()) {
            break;
        }
        record.nsVerifyList.emplace_back(std::move(verify));
    }
    deserializer >> record.attachedAt;
    // 字段级校验与JSON持久化路径对齐（devName白名单/nsDevPath前缀白名单等），
    // 该记录将驱动agent本地挂载回放，网络报文不得绕过白名单
    if (deserializer.Check() && !IsRecordFieldsValid(record)) {
        UBSE_LOG_ERROR << "Attach record rpc serde: field validation failed, name=" << record.name;
        deserializer.SetFail();
    }
    return deserializer;
}

// 摘要项序列化
UbseSerialization &operator<<(UbseSerialization &serializer, const UbseSsuMountSummaryItem &item)
{
    const uint32_t recordState = static_cast<uint32_t>(item.recordState);
    serializer << item.name << recordState << item.devName;
    return serializer;
}

UbseDeSerialization &operator>>(UbseDeSerialization &deserializer, UbseSsuMountSummaryItem &item)
{
    uint32_t recordState = 0;
    deserializer >> item.name >> recordState >> item.devName;
    if (recordState > static_cast<uint32_t>(UbseSsuAttachRecordState::SUSPECT)) {
        UBSE_LOG_ERROR << "Mount summary serde: invalid recordState=" << recordState;
        deserializer.SetFail();
        return deserializer;
    }
    item.recordState = static_cast<UbseSsuAttachRecordState>(recordState);
    return deserializer;
}

// 平账指令项序列化
UbseSerialization &operator<<(UbseSerialization &serializer, const UbseSsuReconcileItem &item)
{
    const uint32_t action = static_cast<uint32_t>(item.action);
    const uint32_t hasRecord = static_cast<uint32_t>(item.hasRecord);
    serializer << item.name << action << hasRecord;
    if (item.hasRecord) {
        serializer << item.record;
    }
    serializer << item.result;
    return serializer;
}

UbseDeSerialization &operator>>(UbseDeSerialization &deserializer, UbseSsuReconcileItem &item)
{
    uint32_t action = 0;
    uint32_t hasRecord = 0;
    deserializer >> item.name >> action >> hasRecord;
    if (action > static_cast<uint32_t>(UbseSsuReconcileAction::REPORT)) {
        UBSE_LOG_ERROR << "Reconcile item serde: invalid action=" << action;
        deserializer.SetFail();
        return deserializer;
    }
    item.action = static_cast<UbseSsuReconcileAction>(action);
    item.hasRecord = (hasRecord != 0);
    if (item.hasRecord) {
        deserializer >> item.record;
    }
    deserializer >> item.result;
    return deserializer;
}

uint32_t UbseSsuMountSummaryReqMsg::Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const
{
    // 空请求体：序列化仅输出长度占位（保持与框架序列化协议一致的空报文）
    UbseSerialization out;
    uint32_t dummy = 0;
    out << dummy;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "SSU mount summary req serialize failed.";
        return UBSE_ERROR;
    }
    bufferSize = out.GetLength();
    buffer.reset(out.GetBuffer(true));
    return UBSE_OK;
}

uint32_t UbseSsuMountSummaryReqMsg::Deserialize(const uint8_t *data, uint32_t size)
{
    (void)data;
    (void)size;
    return UBSE_OK;
}

UbseSsuMountSummaryRespMsg::UbseSsuMountSummaryRespMsg(const UbseSsuMountSummaryResp &resp) : resp_(resp)
{
}

const UbseSsuMountSummaryResp &UbseSsuMountSummaryRespMsg::GetMountSummaryResp() const
{
    return resp_;
}

uint32_t UbseSsuMountSummaryRespMsg::Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const
{
    UbseSerialization out;
    out << resp_.errorCode;
    const auto cnt = static_cast<uint32_t>(resp_.items.size());
    out << cnt;
    for (const auto &item : resp_.items) {
        out << item;
    }
    if (!out.Check()) {
        UBSE_LOG_ERROR << "SSU mount summary resp serialize failed.";
        return UBSE_ERROR;
    }
    bufferSize = out.GetLength();
    buffer.reset(out.GetBuffer(true));
    return UBSE_OK;
}

uint32_t UbseSsuMountSummaryRespMsg::Deserialize(const uint8_t *data, uint32_t size)
{
    UbseDeSerialization in(data, size);
    in >> resp_.errorCode;
    uint32_t cnt = 0;
    in >> cnt;
    resp_.items.clear();
    // 数量先cap（与持久化侧MAX_ATTACH_RECORDS对齐，防篡改报文耗尽内存）
    if (cnt > MAX_ATTACH_RECORDS) {
        UBSE_LOG_ERROR << "SSU mount summary resp items exceed max limit, size=" << cnt;
        in.SetFail();
        return UBSE_ERROR;
    }
    for (uint32_t i = 0; i < cnt; ++i) {
        UbseSsuMountSummaryItem item;
        in >> item;
        if (!in.Check()) {
            break;
        }
        resp_.items.emplace_back(std::move(item));
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "SSU mount summary resp deserialize failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseSsuReconcileReqMsg::UbseSsuReconcileReqMsg(const UbseSsuReconcileReq &req) : req_(req)
{
}

const UbseSsuReconcileReq &UbseSsuReconcileReqMsg::GetReconcileReq() const
{
    return req_;
}

uint32_t UbseSsuReconcileReqMsg::Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const
{
    UbseSerialization out;
    const auto cnt = static_cast<uint32_t>(req_.items.size());
    out << cnt;
    for (const auto &item : req_.items) {
        out << item;
    }
    if (!out.Check()) {
        UBSE_LOG_ERROR << "SSU reconcile req serialize failed.";
        return UBSE_ERROR;
    }
    bufferSize = out.GetLength();
    buffer.reset(out.GetBuffer(true));
    return UBSE_OK;
}

uint32_t UbseSsuReconcileReqMsg::Deserialize(const uint8_t *data, uint32_t size)
{
    UbseDeSerialization in(data, size);
    uint32_t cnt = 0;
    in >> cnt;
    req_.items.clear();
    // 数量先cap（与持久化侧MAX_ATTACH_RECORDS对齐，防篡改报文耗尽内存）
    if (cnt > MAX_ATTACH_RECORDS) {
        UBSE_LOG_ERROR << "SSU reconcile req items exceed max limit, size=" << cnt;
        in.SetFail();
        return UBSE_ERROR;
    }
    for (uint32_t i = 0; i < cnt; ++i) {
        UbseSsuReconcileItem item;
        in >> item;
        if (!in.Check()) {
            break;
        }
        req_.items.emplace_back(std::move(item));
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "SSU reconcile req deserialize failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseSsuReconcileRespMsg::UbseSsuReconcileRespMsg(const UbseSsuReconcileResp &resp) : resp_(resp)
{
}

const UbseSsuReconcileResp &UbseSsuReconcileRespMsg::GetReconcileResp() const
{
    return resp_;
}

uint32_t UbseSsuReconcileRespMsg::Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const
{
    UbseSerialization out;
    out << resp_.errorCode;
    const auto cnt = static_cast<uint32_t>(resp_.items.size());
    out << cnt;
    for (const auto &item : resp_.items) {
        out << item;
    }
    if (!out.Check()) {
        UBSE_LOG_ERROR << "SSU reconcile resp serialize failed.";
        return UBSE_ERROR;
    }
    bufferSize = out.GetLength();
    buffer.reset(out.GetBuffer(true));
    return UBSE_OK;
}

uint32_t UbseSsuReconcileRespMsg::Deserialize(const uint8_t *data, uint32_t size)
{
    UbseDeSerialization in(data, size);
    in >> resp_.errorCode;
    uint32_t cnt = 0;
    in >> cnt;
    resp_.items.clear();
    // 数量先cap（与持久化侧MAX_ATTACH_RECORDS对齐，防篡改报文耗尽内存）
    if (cnt > MAX_ATTACH_RECORDS) {
        UBSE_LOG_ERROR << "SSU reconcile resp items exceed max limit, size=" << cnt;
        in.SetFail();
        return UBSE_ERROR;
    }
    for (uint32_t i = 0; i < cnt; ++i) {
        UbseSsuReconcileItem item;
        in >> item;
        if (!in.Check()) {
            break;
        }
        resp_.items.emplace_back(std::move(item));
    }
    if (!in.Check()) {
        UBSE_LOG_ERROR << "SSU reconcile resp deserialize failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

} // namespace ubse::ssu::message
