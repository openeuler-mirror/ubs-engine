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

#include "ubse_ssu_attach_record.h"

#include <array>
#include <limits>
#include <set>
#include <type_traits>
#include "ubse_logger.h"
#include "ubse_ssu_utils.h"

namespace ubse::ssu::persist {

using namespace ubse::log;
using namespace ubse::ssu::utils;
using ubse::adapter_plugins::ssu::def::UBSE_SSU_MAX_NAME_LENGTH;
using ubse::adapter_plugins::ssu::def::UBSE_SSU_MAX_USER_NAME_LENGTH;
using ubse::adapter_plugins::ssu::def::UbseSsuAddressingType;
using ubse::adapter_plugins::ssu::def::UbseSsuRaidLevel;
using ubse::plugin::service::ssu::UbseSsuLBAFormat;

UBSE_DEFINE_THIS_MODULE("ubse");

namespace {

// 单条记录内 nsList/nsVerifyList 条目数上限（与 verify 消息 MAX_VERIFY_NS_NUM 对齐）
constexpr uint32_t MAX_NS_PER_RECORD = 1024;

// allowHostNqnList 条目数上限（与硬件 HOST_NUM 上限对齐）
constexpr uint32_t MAX_HOST_NQN_PER_NS = 128;

// nsDevPath 前缀白名单：仅允许 NVMe 命名空间持久化路径，防篡改引入任意块设备（如 /dev/sda）
constexpr std::array<const char *, 2> NS_DEV_PATH_PREFIX = {"/dev/disk/by-id/nvme-uuid.", "/dev/nvme"};

// STRIPED chunkSize 合法值（单位KB，与 UbseSsuChunkSize 枚举一致）
const std::set<uint32_t> &GetValidChunkSizes()
{
    static const std::set<uint32_t> valid = {4, 16, 32, 64, 128, 256, 512};
    return valid;
}

bool IsNsDevPathAllowed(const std::string &path)
{
    if (path.find("..") != std::string::npos) { // 拒绝路径穿越序列（如 /dev/nvme../../etc/x）
        return false;
    }
    for (const auto *prefix : NS_DEV_PATH_PREFIX) {
        if (path.rfind(prefix, 0) == 0) {
            return true;
        }
    }
    return false;
}

bool IsLbaFormatValid(uint32_t lbaFormat)
{
    return lbaFormat == static_cast<uint32_t>(UbseSsuLBAFormat::LBA_FORMAT_512) ||
           lbaFormat == static_cast<uint32_t>(UbseSsuLBAFormat::LBA_FORMAT_4K);
}

bool IsBlockDeviceOptionsValid(const UbseSsuAttachRecord &record)
{
    if (!record.blockDeviceOptions.has_value()) {
        return record.strategy == UbseSsuAllocStrategy::NORMAL; // NORMAL 无聚合设备
    }
    const auto &opts = *record.blockDeviceOptions;
    const auto addrType = static_cast<uint32_t>(opts.addressingType);
    if (addrType > static_cast<uint32_t>(UbseSsuAddressingType::LINEAR)) {
        return false;
    }
    // 编址类型须与分配策略一致：STRIPED→STRIPED，LINEAR→LINEAR
    if (record.strategy == UbseSsuAllocStrategy::STRIPED &&
        opts.addressingType != UbseSsuAddressingType::STRIPED) {
        return false;
    }
    if (record.strategy == UbseSsuAllocStrategy::LINEAR &&
        opts.addressingType != UbseSsuAddressingType::LINEAR) {
        return false;
    }
    const auto raidLevel = static_cast<uint32_t>(opts.raidLevel);
    if (raidLevel != static_cast<uint32_t>(UbseSsuRaidLevel::RAID0) &&
        raidLevel != static_cast<uint32_t>(UbseSsuRaidLevel::RAID5)) {
        return false;
    }
    return GetValidChunkSizes().count(opts.chunkSize) > 0;
}

bool IsRecordFieldsValid(const UbseSsuAttachRecord &record)
{
    if (record.name.empty() || record.name.size() >= UBSE_SSU_MAX_NAME_LENGTH) {
        return false;
    }
    if (static_cast<uint32_t>(record.strategy) > static_cast<uint32_t>(UbseSsuAllocStrategy::NORMAL)) {
        return false;
    }
    if (static_cast<uint32_t>(record.recordState) >
        static_cast<uint32_t>(UbseSsuAttachRecordState::SUSPECT)) { // 上界与状态机定义对齐，允许 SUSPECT 落盘记录重载
        return false;
    }
    if (!record.devName.empty() && !IsValidDevName(record.devName)) { // 白名单，防路径穿越
        return false;
    }
    if (!IsOptionalNqnValid(record.nqn)) { // 空串表示 defaultNqn，非空须为合法 HostNqn
        return false;
    }
    if (record.identity.userName.empty() ||
        record.identity.userName.size() >= UBSE_SSU_MAX_USER_NAME_LENGTH) {
        return false;
    }
    if (!IsBlockDeviceOptionsValid(record)) {
        return false;
    }
    if (record.nsList.empty() || record.nsList.size() != record.nsVerifyList.size()) {
        return false; // nsList 与 nsVerifyList 必须一一对应
    }
    if (record.nsList.size() > MAX_NS_PER_RECORD) {
        return false;
    }
    if (record.attachedAt < 0) {
        return false;
    }
    for (const auto &ns : record.nsList) {
        if (!IsNsDevPathAllowed(ns.nsDevPath)) {
            return false;
        }
        if (ns.namespaceId == 0 || ns.nsSize == 0) {
            return false;
        }
        if (!IsLbaFormatValid(static_cast<uint32_t>(ns.lbaFormat))) {
            return false;
        }
        if (ns.allowHostNqnList.size() > MAX_HOST_NQN_PER_NS) {
            return false;
        }
    }
    for (const auto &verify : record.nsVerifyList) {
        if (verify.guid.empty() || verify.defaultNqn.empty()) { // AgentAttachNs 回放必需
            return false;
        }
    }
    return true;
}

rapidjson::Value SerializeIdentity(const UbseSsuAllocIdentityInfo &identity,
                                   rapidjson::Document::AllocatorType &allocator)
{
    rapidjson::Value value(rapidjson::kObjectType);
    value.AddMember("uid", identity.uid, allocator);
    value.AddMember("userName", rapidjson::Value(identity.userName.c_str(), allocator), allocator);
    return value;
}

rapidjson::Value SerializeNs(const UbseSsuNameSpaceInfo &ns, rapidjson::Document::AllocatorType &allocator)
{
    rapidjson::Value value(rapidjson::kObjectType);
    value.AddMember("tgtEid", rapidjson::Value(ns.tgtEid.c_str(), allocator), allocator);
    value.AddMember("tgtNqn", rapidjson::Value(ns.tgtNqn.c_str(), allocator), allocator);
    value.AddMember("nsUuid", rapidjson::Value(ns.nsUuid.c_str(), allocator), allocator);
    value.AddMember("namespaceId", ns.namespaceId, allocator);
    value.AddMember("nsDevPath", rapidjson::Value(ns.nsDevPath.c_str(), allocator), allocator);
    value.AddMember("nsSize", ns.nsSize, allocator);
    value.AddMember("lbaFormat", static_cast<uint32_t>(ns.lbaFormat), allocator);
    rapidjson::Value hostNqnArr(rapidjson::kArrayType);
    for (const auto &nqn : ns.allowHostNqnList) {
        hostNqnArr.PushBack(rapidjson::Value(nqn.c_str(), allocator), allocator);
    }
    value.AddMember("allowHostNqnList", hostNqnArr, allocator);
    return value;
}

rapidjson::Value SerializeNsVerify(const UbseSsuNsVerifyInfo &verify,
                                   rapidjson::Document::AllocatorType &allocator)
{
    rapidjson::Value value(rapidjson::kObjectType);
    value.AddMember("defaultNqn", rapidjson::Value(verify.defaultNqn.c_str(), allocator), allocator);
    value.AddMember("jettyId", verify.jettyId, allocator);
    value.AddMember("guid", rapidjson::Value(verify.guid.c_str(), allocator), allocator);
    return value;
}

bool GetJsonString(const rapidjson::Value &parent, const char *key, std::string &value)
{
    if (!parent.IsObject() || !parent.HasMember(key) || !parent[key].IsString()) {
        return false;
    }
    value = parent[key].GetString();
    return true;
}

template <typename T>
bool GetJsonUint(const rapidjson::Value &parent, const char *key, T &value)
{
    static_assert(std::is_unsigned_v<T>, "T must be unsigned");
    if (!parent.IsObject() || !parent.HasMember(key) || !parent[key].IsUint64()) {
        return false;
    }
    const uint64_t raw = parent[key].GetUint64();
    if (raw > std::numeric_limits<T>::max()) {
        return false;
    }
    value = static_cast<T>(raw);
    return true;
}

bool DeserializeIdentity(const rapidjson::Value &value, UbseSsuAllocIdentityInfo &identity)
{
    if (!value.IsObject() || !GetJsonUint(value, "uid", identity.uid) ||
        !GetJsonString(value, "userName", identity.userName)) {
        return false;
    }
    return true;
}

bool DeserializeBlockDeviceOptions(const rapidjson::Value &value,
                                   std::optional<UbseCreateBlockDeviceOptions> &options)
{
    if (value.IsNull()) { // null 表示 NORMAL 无聚合设备
        options.reset();
        return true;
    }
    if (!value.IsObject()) {
        return false;
    }
    uint32_t addressingType = 0;
    uint32_t raidLevel = 0;
    UbseCreateBlockDeviceOptions opts{};
    if (!GetJsonUint(value, "addressingType", addressingType) ||
        !GetJsonUint(value, "raidLevel", raidLevel) || !GetJsonUint(value, "chunkSize", opts.chunkSize)) {
        return false;
    }
    opts.addressingType = static_cast<UbseSsuAddressingType>(addressingType);
    opts.raidLevel = static_cast<UbseSsuRaidLevel>(raidLevel);
    options = opts;
    return true;
}

bool DeserializeNs(const rapidjson::Value &value, UbseSsuNameSpaceInfo &ns)
{
    uint32_t lbaFormat = 0;
    if (!value.IsObject() || !GetJsonString(value, "tgtEid", ns.tgtEid) ||
        !GetJsonString(value, "tgtNqn", ns.tgtNqn) || !GetJsonString(value, "nsUuid", ns.nsUuid) ||
        !GetJsonUint(value, "namespaceId", ns.namespaceId) ||
        !GetJsonString(value, "nsDevPath", ns.nsDevPath) || !GetJsonUint(value, "nsSize", ns.nsSize) ||
        !GetJsonUint(value, "lbaFormat", lbaFormat)) {
        return false;
    }
    ns.lbaFormat = static_cast<UbseSsuLBAFormat>(lbaFormat);
    if (!value.HasMember("allowHostNqnList") || !value["allowHostNqnList"].IsArray()) {
        return false;
    }
    const auto &hostNqnArr = value["allowHostNqnList"];
    if (hostNqnArr.Size() > MAX_HOST_NQN_PER_NS) { // 先 cap 再 reserve，防内存耗尽
        return false;
    }
    ns.allowHostNqnList.reserve(hostNqnArr.Size());
    for (rapidjson::SizeType j = 0; j < hostNqnArr.Size(); ++j) {
        if (!hostNqnArr[j].IsString()) {
            return false;
        }
        ns.allowHostNqnList.emplace_back(hostNqnArr[j].GetString());
    }
    return true;
}

bool DeserializeNsVerify(const rapidjson::Value &value, UbseSsuNsVerifyInfo &verify)
{
    if (!value.IsObject() || !GetJsonString(value, "defaultNqn", verify.defaultNqn) ||
        !GetJsonUint(value, "jettyId", verify.jettyId) || !GetJsonString(value, "guid", verify.guid)) {
        return false;
    }
    return true;
}

} // namespace

void SerializeRecord(const UbseSsuAttachRecord &record, rapidjson::Value &value,
                     rapidjson::Document::AllocatorType &allocator)
{
    value.SetObject();
    value.AddMember("name", rapidjson::Value(record.name.c_str(), allocator), allocator);
    value.AddMember("strategy", static_cast<uint32_t>(record.strategy), allocator);
    value.AddMember("recordState", static_cast<uint32_t>(record.recordState), allocator);
    value.AddMember("devName", rapidjson::Value(record.devName.c_str(), allocator), allocator);
    value.AddMember("nqn", rapidjson::Value(record.nqn.c_str(), allocator), allocator);
    value.AddMember("identity", SerializeIdentity(record.identity, allocator), allocator);
    if (record.blockDeviceOptions.has_value()) {
        rapidjson::Value opts(rapidjson::kObjectType);
        opts.AddMember("addressingType", static_cast<uint32_t>(record.blockDeviceOptions->addressingType),
                       allocator);
        opts.AddMember("raidLevel", static_cast<uint32_t>(record.blockDeviceOptions->raidLevel), allocator);
        opts.AddMember("chunkSize", record.blockDeviceOptions->chunkSize, allocator);
        value.AddMember("blockDeviceOptions", opts, allocator);
    } else {
        value.AddMember("blockDeviceOptions", rapidjson::Value(rapidjson::kNullType), allocator);
    }
    rapidjson::Value nsArr(rapidjson::kArrayType);
    for (const auto &ns : record.nsList) {
        nsArr.PushBack(SerializeNs(ns, allocator), allocator);
    }
    value.AddMember("nsList", nsArr, allocator);
    rapidjson::Value verifyArr(rapidjson::kArrayType);
    for (const auto &verify : record.nsVerifyList) {
        verifyArr.PushBack(SerializeNsVerify(verify, allocator), allocator);
    }
    value.AddMember("nsVerifyList", verifyArr, allocator);
    value.AddMember("attachedAt", record.attachedAt, allocator);
}

bool DeserializeRecord(const rapidjson::Value &value, UbseSsuAttachRecord &record)
{
    if (!value.IsObject()) {
        return false;
    }
    uint32_t strategy = 0;
    uint32_t recordState = 0;
    if (!GetJsonString(value, "name", record.name) || !GetJsonUint(value, "strategy", strategy) ||
        !GetJsonUint(value, "recordState", recordState) ||
        !GetJsonString(value, "devName", record.devName) || !GetJsonString(value, "nqn", record.nqn) ||
        !value.HasMember("identity") || !value.HasMember("blockDeviceOptions") ||
        !value.HasMember("nsList") || !value["nsList"].IsArray() ||
        !value.HasMember("nsVerifyList") || !value["nsVerifyList"].IsArray()) {
        return false;
    }
    record.strategy = static_cast<UbseSsuAllocStrategy>(strategy);
    record.recordState = static_cast<UbseSsuAttachRecordState>(recordState);

    if (!DeserializeIdentity(value["identity"], record.identity) ||
        !DeserializeBlockDeviceOptions(value["blockDeviceOptions"], record.blockDeviceOptions)) {
        return false;
    }

    // nsList（先查条目数上限再 reserve，防篡改 count 内存耗尽）
    const auto &nsArr = value["nsList"];
    if (nsArr.Size() > MAX_NS_PER_RECORD) {
        return false;
    }
    record.nsList.clear();
    record.nsList.reserve(nsArr.Size());
    for (rapidjson::SizeType i = 0; i < nsArr.Size(); ++i) {
        UbseSsuNameSpaceInfo ns{};
        if (!DeserializeNs(nsArr[i], ns)) {
            return false;
        }
        record.nsList.emplace_back(std::move(ns));
    }

    // nsVerifyList（与 nsList 一一对应）
    const auto &verifyArr = value["nsVerifyList"];
    if (verifyArr.Size() != nsArr.Size()) {
        return false;
    }
    record.nsVerifyList.clear();
    record.nsVerifyList.reserve(verifyArr.Size());
    for (rapidjson::SizeType i = 0; i < verifyArr.Size(); ++i) {
        UbseSsuNsVerifyInfo verify{};
        if (!DeserializeNsVerify(verifyArr[i], verify)) {
            return false;
        }
        record.nsVerifyList.emplace_back(std::move(verify));
    }

    // attachedAt（缺省按0处理，向后兼容）
    if (value.HasMember("attachedAt")) {
        if (!value["attachedAt"].IsInt64()) {
            return false;
        }
        record.attachedAt = value["attachedAt"].GetInt64();
    } else {
        record.attachedAt = 0;
    }

    if (!IsRecordFieldsValid(record)) {
        UBSE_LOG_WARN << "SSU attach record validation failed, name=" << record.name;
        return false;
    }
    return true;
}

} // namespace ubse::ssu::persist
