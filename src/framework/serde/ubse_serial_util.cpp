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

#include "ubse_serial_util.h"

#include "securec.h"

namespace ubse::serial {
enum class HeadBlockIndex {
    CTRL_CODE_IDX = 0,
    ALIGN_CODE_IDX,
    LEN_CODE_ID,
    MAX_CODE_NUMS
};
constexpr common_len HEAD_BLOCK_NUMS = static_cast<common_len>(HeadBlockIndex::MAX_CODE_NUMS);

// 由调用方控制安全性
void SetCode(base_ptr_type* buf, HeadBlockIndex idx, serial_head value)
{
    auto offset = static_cast<serial_head>(idx) * sizeof(serial_head);
    *reinterpret_cast<serial_head*>(buf + offset) = value;
}

// 由调用方控制安全性
serial_head GetCode(base_ptr_type* buf, HeadBlockIndex idx)
{
    auto offset = static_cast<serial_head>(idx) * sizeof(serial_head);
    return *reinterpret_cast<serial_head*>(buf + offset);
}

inline bool isValidAlign(ALIGN_BASE align)
{
    return align == ALIGN_BASE::OFFSET_BASE_1 || align == ALIGN_BASE::OFFSET_BASE_2 ||
           align == ALIGN_BASE::OFFSET_BASE_4 || align == ALIGN_BASE::OFFSET_BASE_8;
}

UbseSerialization::UbseSerialization()
    : mBuf_(nullptr),
      mAlignOffset_(sizeof(serial_head) - 1),
      mLen_(AlignTo(sizeof(serial_head) * HEAD_BLOCK_NUMS)),
      mCap_(std::max(AlignTo(INIT_CAPACITY), mLen_)),
      mFlag_(true)
{
    mBuf_ = new (std::nothrow) base_ptr_type[mCap_];
    if (Unlikely(mBuf_ == nullptr)) {
        mFlag_ = false;
        return;
    }
}

UbseSerialization::UbseSerialization(ALIGN_BASE align)
    : mBuf_(nullptr),
      mAlignOffset_(static_cast<common_len>(align) - 1),
      mLen_(AlignTo(sizeof(serial_head) * HEAD_BLOCK_NUMS)),
      mCap_(std::max(AlignTo(INIT_CAPACITY), mLen_)),
      mFlag_(true)
{
    if (!isValidAlign(align)) {
        mFlag_ = false;
        return;
    }
    mBuf_ = new (std::nothrow) base_ptr_type[mCap_]();
    if (Unlikely(mBuf_ == nullptr)) {
        mFlag_ = false;
        return;
    }
}

UbseSerialization::~UbseSerialization()
{
    if (mBuf_ != nullptr) {
        delete[] mBuf_;
        mBuf_ = nullptr;
    }
}

UbseSerialization::UbseSerialization(common_len cap, ALIGN_BASE align)
    : mBuf_(nullptr),
      mAlignOffset_(static_cast<common_len>(align) - 1),
      mLen_(AlignTo(sizeof(serial_head) * HEAD_BLOCK_NUMS)),
      mCap_(AlignTo(cap) + mLen_),
      mFlag_(true)
{
    if (!isValidAlign(align)) {
        mFlag_ = false;
        return;
    }
    if (mCap_ >= MAX_CAPACITY) {
        mFlag_ = false;
        return;
    }
    mBuf_ = new (std::nothrow) base_ptr_type[mCap_]();
    if (Unlikely(mBuf_ == nullptr)) {
        mFlag_ = false;
        return;
    }
}

UbseSerialization::UbseSerialization(UbseSerialization&& other) noexcept
    : mBuf_(other.mBuf_),
      mAlignOffset_(other.mAlignOffset_),
      mCap_(other.mCap_),
      mLen_(other.mLen_),
      mFlag_(other.mFlag_)
{
    other.mBuf_ = nullptr;
    other.mLen_ = 0;
    other.mFlag_ = true;
}

base_ptr_type* UbseSerialization::GetBuffer(bool bGetCtrl)
{
    if (Unlikely(mBuf_ == nullptr)) {
        return nullptr;
    }
    SetCode(mBuf_, HeadBlockIndex::CTRL_CODE_IDX, static_cast<serial_head>(CTRL_TYPE::HEAD_CTRL_CODE));
    SetCode(mBuf_, HeadBlockIndex::ALIGN_CODE_IDX, static_cast<serial_head>(mAlignOffset_));
    SetCode(mBuf_, HeadBlockIndex::LEN_CODE_ID, static_cast<serial_head>(mLen_));
    auto tmp = mBuf_;
    if (bGetCtrl) {
        mFlag_ = false;
        mBuf_ = nullptr;
    }
    return tmp;
}

UbseSerialization& UbseSerialization::operator=(UbseSerialization&& other) noexcept
{
    mBuf_ = other.mBuf_;
    other.mBuf_ = nullptr;
    mCap_ = other.mCap_;
    mLen_ = other.mLen_;
    other.mLen_ = 0;
    mFlag_ = other.mFlag_;
    other.mFlag_ = true;
    return *this;
}

UbseSerialization& UbseSerialization::operator<<(array_len_insert arrayLenInsert)
{
    if (Unlikely(!mFlag_)) {
        return *this;
    }
    if (Unlikely(arrayLenInsert.len >= ONCE_LIMIT_LEN)) {
        mFlag_ = false;
        return *this;
    }
    auto alignedHeadLen = AlignTo(sizeof(serial_head));
    if (!expandCapacity(alignedHeadLen)) {
        return *this;
    }
    writeTypeAndLen(mBuf_ + mLen_, static_cast<serial_type>(CTRL_TYPE::ARRAY_CTRL_CODE),
                    static_cast<serial_len>(arrayLenInsert.len));
    mLen_ += alignedHeadLen;
    return *this;
}

template <>
UbseSerialization& UbseSerialization::operator<< <bool>(const std::vector<bool>& vec)
{
    *this << array_len_insert(vec.size());
    if (vec.empty()) {
        return *this;
    }
    std::vector<base_ptr_type> bytes(vec.size());
    for (size_t i = 0; i < bytes.size(); i++) {
        bytes[i] = vec[i] ? 1 : 0;
    }
    add(reinterpret_cast<const base_ptr_type*>(bytes.data()), bytes.size(), GetTypePointerId<std::_Bit_reference>());
    return *this;
}

UbseSerialization& UbseSerialization::operator<<(const std::string& str)
{
    add(reinterpret_cast<const base_ptr_type*>(str.c_str()), str.size() + 1, GetTypePointerId<std::string>());
    return *this;
}

UbseSerialization& UbseSerialization::operator<<(std::string&& str)
{
    return *this << str;
}

UbseSerialization& UbseSerialization::operator<<(const char* str)
{
    if (str == nullptr) {
        mFlag_ = false;
        return *this;
    }
    add(reinterpret_cast<const base_ptr_type*>(str), strlen(str) + 1, GetTypePointerId<std::string>());
    return *this;
}

UbseSerialization& UbseSerialization::operator<<(UbseSerialization& kid)
{
    if (!kid.Check()) {
        mFlag_ = false;
        return *this;
    }
    auto len = kid.GetLength();
    if (Unlikely(len >= ONCE_LIMIT_LEN)) {
        mFlag_ = false;
        return *this;
    }
    auto alignedHeadLen = AlignTo(sizeof(serial_head));
    auto alignedParamLen = AlignTo(len);
    if (!expandCapacity(alignedHeadLen + alignedParamLen)) {
        return *this;
    }
    writeTypeAndLen(mBuf_ + mLen_, static_cast<serial_type>(CTRL_TYPE::NEST_CTRL_CODE), static_cast<serial_len>(len));
    mLen_ += alignedHeadLen;
    if (Unlikely(memcpy_s(mBuf_ + mLen_, len, kid.GetBuffer(), len) != EOK)) {
        mFlag_ = false;
        return *this;
    }
    mLen_ += alignedParamLen;
    return *this;
}

bool UbseSerialization::expandCapacity(common_len len)
{
    if (mLen_ + len >= MAX_CAPACITY) {
        mFlag_ = false;
        return false;
    }
    bool bNeedExpand = false;
    while (mCap_ < mLen_ + len) {
        ((mCap_ << 1) >= MAX_CAPACITY) ? mCap_ = MAX_CAPACITY - 1 : mCap_ <<= 1;
        bNeedExpand = true;
    }
    if (!bNeedExpand) {
        return true;
    }
    auto newBuf = new (std::nothrow) base_ptr_type[mCap_]();
    if (Unlikely(newBuf == nullptr)) {
        mFlag_ = false;
        return false;
    }
    if (Unlikely(memcpy_s(newBuf, mCap_, mBuf_, mLen_) != EOK)) {
        mFlag_ = false;
        delete[] newBuf;
        return false;
    }
    delete[] mBuf_;
    mBuf_ = newBuf;
    return true;
}

void UbseSerialization::add(const base_ptr_type* addr, common_len len, serial_type type)
{
    if (Unlikely(!mFlag_)) {
        return;
    }
    if (Unlikely(addr == nullptr || len == 0 || len >= ONCE_LIMIT_LEN - mAlignOffset_)) {
        mFlag_ = false;
        return;
    }
    auto alignedHeadLen = AlignTo(sizeof(serial_head));
    auto alignedParamLen = AlignTo(len);
    if (!expandCapacity(alignedHeadLen + alignedParamLen)) {
        return;
    }
    writeTypeAndLen(mBuf_ + mLen_, type, static_cast<serial_len>(len));
    mLen_ += alignedHeadLen;
    if (Unlikely(memcpy_s(mBuf_ + mLen_, len, addr, len) != EOK)) {
        mFlag_ = false;
        return;
    }
    mLen_ += alignedParamLen;
}

bool UbseDeSerialization::Set(base_ptr_type* buf, common_len len, bool bNew)
{
    if (mBuf_ != nullptr || buf == nullptr || len < sizeof(serial_head) * HEAD_BLOCK_NUMS || len >= MAX_CAPACITY) {
        mFlag_ = false;
        return false;
    }
    // 头部控制块校验
    if (GetCode(buf, HeadBlockIndex::CTRL_CODE_IDX) != static_cast<serial_head>(CTRL_TYPE::HEAD_CTRL_CODE)) {
        mFlag_ = false;
        return false;
    }
    mAlignOffset_ = GetCode(buf, HeadBlockIndex::ALIGN_CODE_IDX);
    auto alignBase = mAlignOffset_ + 1;
    if (!isValidAlign(static_cast<ALIGN_BASE>(alignBase))) {
        mFlag_ = false;
        return false;
    }
    auto l = GetCode(buf, HeadBlockIndex::LEN_CODE_ID);
    if (l != len || l % (alignBase) != 0) {
        mFlag_ = false;
        return false;
    }
    if (bNew) {
        mBuf_ = new (std::nothrow) base_ptr_type[len]();
        if (Unlikely(mBuf_ == nullptr)) {
            mFlag_ = false;
            return false;
        }
        if (Unlikely(memcpy_s(mBuf_, len, buf, len) != EOK)) {
            mFlag_ = false;
            delete[] mBuf_;
            mBuf_ = nullptr;
            return false;
        }
    } else {
        mBuf_ = buf;
    }
    mPos_ = mBuf_ + AlignTo(sizeof(serial_head) * HEAD_BLOCK_NUMS);
    mLen_ = len;
    mGetBufCtrl_ = bNew;
    return true;
}

UbseDeSerialization::UbseDeSerialization()
    : mBuf_(nullptr),
      mPos_(mBuf_),
      mLen_(0),
      mFlag_(true),
      mGetBufCtrl_(false),
      mAlignOffset_(sizeof(serial_len) - 1)
{
}

UbseDeSerialization::UbseDeSerialization(const base_ptr_type* buf, common_len len, bool bNew)
    : mBuf_(nullptr),
      mPos_(mBuf_),
      mLen_(0),
      mFlag_(true),
      mGetBufCtrl_(false),
      mAlignOffset_(sizeof(serial_len) - 1)
{
    Set(const_cast<base_ptr_type*>(buf), len, bNew);
}

UbseDeSerialization::~UbseDeSerialization()
{
    if (mGetBufCtrl_) {
        UbseSerialFreeFunc(mBuf_);
    }
}

uint8_t* UbseDeSerialization::GetBuffer(bool bGetCtrl)
{
    auto tmp = mBuf_;
    if (bGetCtrl) {
        if (mGetBufCtrl_) {
            mBuf_ = nullptr;
        } else {
            return nullptr;
        }
    }
    return tmp;
}

bool UbseDeSerialization::checkValid(common_len expectLen)
{
    if (Unlikely(!mFlag_)) {
        return false;
    }
    if (Unlikely(expectLen == 0 || mPos_ + expectLen > mBuf_ + mLen_)) {
        mFlag_ = false;
        return false;
    }
    return true;
}

template <>
UbseDeSerialization& UbseDeSerialization::operator>> <bool>(std::vector<bool>& vec)
{
    common_len bit_len;
    *this >> array_len_capture(bit_len);
    if (bit_len == 0) {
        vec.clear();
        return *this;
    }
    base_ptr_type* addr;
    common_len len;
    if (!get(addr, len, GetTypePointerId<std::_Bit_reference>())) {
        return *this;
    }
    if (len == 0 || bit_len != len) {
        mFlag_ = false;
        return *this;
    }
    vec.resize(bit_len);
    for (size_t i = 0; i < vec.size(); i++) {
        vec[i] = (addr[i] != 0);
    }
    return *this;
}

UbseDeSerialization& UbseDeSerialization::operator>>(std::string& str)
{
    base_ptr_type* addr;
    common_len len;
    if (!get(addr, len, GetTypePointerId<std::string>())) {
        return *this;
    }
    if (Unlikely(len == 0)) {
        mFlag_ = false;
        return *this;
    }
    str = std::string(reinterpret_cast<char*>(addr), len - 1);
    return *this;
}

UbseDeSerialization& UbseDeSerialization::operator>>(char*& str)
{
    base_ptr_type* addr;
    common_len len;
    if (!get(addr, len, GetTypePointerId<std::string>())) {
        return *this;
    }
    if (Unlikely(len == 0)) {
        mFlag_ = false;
        return *this;
    }
    str = new (std::nothrow) char[len];
    if (Unlikely(str == nullptr)) {
        mFlag_ = false;
        return *this;
    }
    if (Unlikely(memcpy_s(str, len, addr, len) != EOK)) {
        mFlag_ = false;
        delete[] str;
        str = nullptr;
    }
    return *this;
}

UbseDeSerialization& UbseDeSerialization::operator>>(UbseDeSerialization& kid)
{
    base_ptr_type* addr;
    common_len len;
    if (!get(addr, len, static_cast<uint32_t>(CTRL_TYPE::NEST_CTRL_CODE))) {
        return *this;
    }
    kid.Set(addr, len, true);
    if (!kid.Check()) {
        mFlag_ = false;
        return *this;
    }
    return *this;
}

bool UbseDeSerialization::get(base_ptr_type*& addr, common_len& len, serial_type type)
{
    auto expectReadLen = AlignTo(sizeof(serial_head));
    if (!checkValid(expectReadLen)) {
        mFlag_ = false;
        return false;
    }
    auto o = readLenByType(mPos_, type);
    if (o.has_value()) {
        len = o.value();
        mPos_ += expectReadLen;
    } else {
        mFlag_ = false;
        return false;
    }
    addr = mPos_;
    mPos_ += AlignTo(len);
    if (mPos_ > mBuf_ + mLen_) { // 防错误长度溢出场景
        mFlag_ = false;
        return false;
    }
    return true;
}
} // namespace ubse::serial
