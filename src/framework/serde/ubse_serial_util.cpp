/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "ubse_serial_util.h"

#include "securec.h"

namespace ubse::serial {
int SafeCopy(void *dst, uint64_t des_len, const void *src, uint64_t src_len)
{
    return memcpy_s(dst, des_len, src, src_len);
}

UbseSerialization::UbseSerialization() : mBuf(nullptr), mCap(INIT_CAPACITY), mLen(sizeof(uint64_t)), mFlag(true)
{
    mBuf = new (std::nothrow) uint8_t[mCap];
    if (Unlikely(mBuf == nullptr)) {
        mFlag = false;
        return;
    }
    *reinterpret_cast<uint32_t *>(mBuf) = HEAD_CTRL_CODE;
};

UbseSerialization::~UbseSerialization()
{
    if (mBuf != nullptr) {
        delete[] mBuf;
        mBuf = nullptr;
    }
}

UbseSerialization::UbseSerialization(uint64_t cap)
    : mBuf(nullptr),
      mCap(AlignTo8(cap) + sizeof(uint64_t)),
      mLen(sizeof(uint64_t)),
      mFlag(true)
{
    if (mCap >= MAX_CAPACITY) {
        mFlag = false;
        return;
    }
    mBuf = new (std::nothrow) uint8_t[mCap];
    if (Unlikely(mBuf == nullptr)) {
        mFlag = false;

        return;
    }
    *reinterpret_cast<uint32_t *>(mBuf) = HEAD_CTRL_CODE;
};

UbseSerialization::UbseSerialization(UbseSerialization &&other) noexcept
    : mBuf(other.mBuf),
      mCap(other.mCap),
      mLen(other.mLen),
      mFlag(other.mFlag)
{
    other.mBuf = nullptr;
    other.mLen = 0;
    other.mFlag = true;
}

uint8_t *UbseSerialization::GetBuffer(bool bGetCtrl)
{
    *reinterpret_cast<uint32_t *>(mBuf + sizeof(uint32_t)) = mLen;
    auto tmp = mBuf;
    if (bGetCtrl) {
        mFlag = false;
        mBuf = nullptr;
    }
    return tmp;
}

UbseSerialization &UbseSerialization::operator=(UbseSerialization &&other) noexcept
{
    mBuf = other.mBuf;
    other.mBuf = nullptr;
    mCap = other.mCap;
    mLen = other.mLen;
    other.mLen = 0;
    mFlag = other.mFlag;
    other.mFlag = true;
    return *this;
}

UbseSerialization &UbseSerialization::operator<<(array_len_insert arrayLenInsert)
{
    if (Unlikely(!mFlag)) {
        return *this;
    }
    if (!expandCapacity(sizeof(uint64_t))) {
        return *this;
    }
    writeTypeAndLen(ARRAY_CTRL_CODE, arrayLenInsert.len);
    return *this;
}

UbseSerialization &UbseSerialization::operator<<(const std::string &str)
{
    add(reinterpret_cast<uint8_t *>(const_cast<char *>(str.c_str())), str.size() + 1, GetTypePointerId<char>());
    return *this;
}

UbseSerialization &UbseSerialization::operator<<(std::string &&str)
{
    add(reinterpret_cast<uint8_t *>(const_cast<char *>(str.c_str())), str.size() + 1, GetTypePointerId<char>());
    return *this;
}

UbseSerialization &UbseSerialization::operator<<(const char *str)
{
    if (Unlikely(!mFlag || str == nullptr)) {
        mFlag = false;
        return *this;
    }
    add(reinterpret_cast<uint8_t *>(const_cast<char *>(str)), strlen(str) + 1, GetTypePointerId<char>());
    return *this;
}

UbseSerialization &UbseSerialization::operator<<(UbseSerialization &kid)
{
    if (!kid.Check()) {
        mFlag = false;
        return *this;
    }
    auto len = kid.GetLength();
    if (!expandCapacity(sizeof(uint64_t) + len)) {
        return *this;
    }
    writeTypeAndLen(NEST_CTRL_CODE, len);
    if (Unlikely(SafeCopy(mBuf + mLen, len, kid.GetBuffer(), len) != 0)) {
        mFlag = false;
        return *this;
    }
    mLen += AlignTo8(len);
    return *this;
}

bool UbseSerialization::expandCapacity(uint64_t len)
{
    if (mLen + len >= MAX_CAPACITY) {
        mFlag = false;
        return false;
    }
    bool bNeedExpand = false;
    while (mCap < mLen + len) {
        (mCap << 1 >= MAX_CAPACITY) ? mCap = MAX_CAPACITY : mCap <<= 1;
        bNeedExpand = true;
    }
    if (!bNeedExpand) {
        return true;
    }
    auto newBuf = new (std::nothrow) uint8_t[mCap];
    if (Unlikely(newBuf == nullptr)) {
        mFlag = false;
        return false;
    }
    if (Unlikely(SafeCopy(newBuf, mLen, mBuf, mLen) != 0)) {
        mFlag = false;
        delete[] newBuf;
        return false;
    }
    delete[] mBuf;
    mBuf = newBuf;
    return true;
}

void UbseSerialization::add(uint8_t *addr, uint64_t len, uint32_t type)
{
    if (Unlikely(!mFlag)) {
        return;
    }
    if (Unlikely(addr == nullptr || len == 0 || AlignTo8(len) > LIMIT_LEN)) {
        mFlag = false;
        return;
    }
    if (!expandCapacity(sizeof(uint64_t) + AlignTo8(len))) {
        return;
    }
    writeTypeAndLen(type, len);
    if (Unlikely(SafeCopy(mBuf + mLen, len, addr, len) != 0)) {
        mFlag = false;
        return;
    }
    mLen += AlignTo8(len);
}

bool UbseDeSerialization::Set(uint8_t *buf, uint64_t len, bool bNew)
{
    if (mBuf != nullptr || buf == nullptr || len == 0 || len % sizeof(uint64_t) != 0 || len >= MAX_CAPACITY) {
        mFlag = false;
        return false;
    }
    if (*reinterpret_cast<uint32_t *>(buf) != HEAD_CTRL_CODE) { // 头部控制块校验
        mFlag = false;
        return false;
    }
    if (*reinterpret_cast<uint32_t *>(buf + sizeof(uint32_t)) != len) { // 长度校验
        mFlag = false;
        return false;
    }
    if (bNew) {
        mBuf = new (std::nothrow) uint8_t[len];
        if (Unlikely(mBuf == nullptr)) {
            mFlag = false;
            return false;
        }
        if (Unlikely(SafeCopy(mBuf, len, buf, len) != 0)) {
            mFlag = false;
            delete[] mBuf;
            mBuf = nullptr;
            return false;
        }
    } else {
        mBuf = buf;
    }
    mPos = mBuf + sizeof(uint64_t);
    mLen = len;
    mGetBufCtrl = bNew;
    return true;
}

UbseDeSerialization::UbseDeSerialization() : mBuf(nullptr), mPos(mBuf), mLen(0), mFlag(true), mGetBufCtrl(false) {}

UbseDeSerialization::UbseDeSerialization(const uint8_t *buf, uint64_t len, bool bNew)
    : mBuf(nullptr),
      mPos(mBuf),
      mLen(0),
      mFlag(true),
      mGetBufCtrl(false)
{
    Set(const_cast<uint8_t *>(buf), len, bNew);
}

UbseDeSerialization::~UbseDeSerialization()
{
    if (mGetBufCtrl) {
        UbseSerialFreeFunc(mBuf);
    }
}

uint8_t *UbseDeSerialization::GetBuffer(bool bGetCtrl)
{
    auto tmp = mBuf;
    if (bGetCtrl) {
        if (mGetBufCtrl) {
            mBuf = nullptr;
        } else {
            return nullptr;
        }
    }
    return tmp;
}

bool UbseDeSerialization::checkValid()
{
    if (Unlikely(!mFlag)) {
        return false;
    }
    if (Unlikely(mPos >= mBuf + mLen)) {
        mFlag = false;
        return false;
    }
    return true;
}

UbseDeSerialization &UbseDeSerialization::operator>>(std::string &str)
{
    if (!checkValid()) {
        return *this;
    }
    uint8_t *addr;
    uint64_t len;
    if (!get(addr, len, GetTypePointerId<char>())) {
        return *this;
    }
    if (len == 0) {
        mFlag = false;
        return *this;
    }
    str = std::string(reinterpret_cast<char *>(addr), len - 1);
    return *this;
}

UbseDeSerialization &UbseDeSerialization::operator>>(char *&str)
{
    if (!checkValid()) {
        return *this;
    }
    uint8_t *addr;
    uint64_t len;
    if (!get(addr, len, GetTypePointerId<char>())) {
        return *this;
    }
    if (len == 0) {
        mFlag = false;
        return *this;
    }
    str = new (std::nothrow) char[len];
    if (Unlikely(str == nullptr)) {
        mFlag = false;
        return *this;
    }
    if (Unlikely(SafeCopy(str, len, addr, len) != 0)) {
        mFlag = false;
        delete[] str;
        str = nullptr;
    }
    return *this;
}

UbseDeSerialization &UbseDeSerialization::operator>>(const char *&str)
{
    if (!checkValid()) {
        return *this;
    }
    uint8_t *addr;
    uint64_t len;
    if (!get(addr, len, GetTypePointerId<char>())) {
        return *this;
    }
    if (len == 0) {
        mFlag = false;
        return *this;
    }
    auto tmp = new (std::nothrow) char[len];
    if (Unlikely(tmp == nullptr)) {
        mFlag = false;
        return *this;
    }
    if (Unlikely(SafeCopy(tmp, len, addr, len) != 0)) {
        mFlag = false;
        delete[] tmp;
        return *this;
    }
    str = tmp;
    return *this;
}

UbseDeSerialization &UbseDeSerialization::operator>>(UbseDeSerialization &kid)
{
    if (!checkValid()) {
        return *this;
    }
    uint8_t *addr;
    uint64_t len;
    if (!get(addr, len, NEST_CTRL_CODE)) {
        return *this;
    }
    kid.Set(addr, len, true);
    return *this;
}

bool UbseDeSerialization::get(uint8_t *&addr, uint64_t &len, uint32_t type)
{
    auto t = *(reinterpret_cast<uint32_t *>(mPos));
    if (t != type) {
        mFlag = false;
        return false;
    }
    mPos += sizeof(uint32_t);
    len = *(reinterpret_cast<uint32_t *>(mPos));
    mPos += sizeof(uint32_t);
    addr = mPos;
    mPos += AlignTo8(len);
    if (mPos > mBuf + mLen) { // 防错误长度溢出场景
        mFlag = false;
        return false;
    }
    return true;
}
} // namespace ubse::serial
