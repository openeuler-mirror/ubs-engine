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

#ifndef UBSE_MANAGER_UBSE_SERIAL_UTIL_H
#define UBSE_MANAGER_UBSE_SERIAL_UTIL_H

#include <vector>
#include <map>
#include <string>
#include <cstring>
#include <limits>
#include <type_traits>

namespace ubse::serial {
constexpr uint64_t INIT_CAPACITY = 256;
constexpr uint64_t MAX_CAPACITY = 1 << 20; // 1MB容量上限
constexpr uint64_t LIMIT_LEN = 1 << 16; // 单次添加序列化内容长度64k上限，超过会导致序列化失败

constexpr uint32_t ARRAY_CTRL_CODE = 1 << 16;
constexpr uint32_t NEST_CTRL_CODE = 1 << 17;
constexpr uint32_t HEAD_CTRL_CODE = UINT32_MAX;

inline bool Likely(bool condition)
{
    return __builtin_expect(static_cast<int>(condition), 1) != 0;
}

inline bool Unlikely(bool condition)
{
    return __builtin_expect(static_cast<int>(condition), 0) != 0;
}

inline uint64_t AlignTo8(uint64_t value)
{
    return ((value) + 7) & ~7; // 加 7 后与 1111...1000 进行按位与运算，结果为 8 的倍数
}

/**
 * @brief 序列化相关内存释放函数
 * @warning 使用之前确保该内存的所有权
 * @tparam T 类型
 * @param ptr 地址
 */
template <typename T> void UbseSerialFreeFunc(T *&ptr)
{
    if (ptr != nullptr) {
        delete[] ptr;
        ptr = nullptr;
    }
}

/**
 * 内存安全拷贝函数
 * @param dst 目标地址
 * @param des_len 目标接收长度
 * @param src 源地址
 * @param src_len 源长度
 * @return 0位成功，非0失败
 */
int SafeCopy(void *dst, uint64_t des_len, const void *src, uint64_t src_len);

/**
 * @brief 指定地址+长度组合工具，用于定长数组的序列化和反序列化
 * @tparam T 基础类型的模板
 * @details 序列化时，保证ptr指向内存中有内容且len长度正确
 * @details 反序列化时，保证ptr已经分配足够内存空间，如果空间不够，会反序列化失败
 */
template <typename T, typename = std::enable_if_t<std::is_convertible_v<T, uint8_t>>> struct addr_len {
    T *ptr;
    uint64_t len;
    addr_len() : ptr(nullptr), len(0){};
    addr_len(T *p, uint64_t l) : ptr(p), len(l * sizeof(T)){};
};

/**
 * @brief 数组长度容器
 * 用法 UbseSerialization se << array_len_insert(len);
 */
struct array_len_insert {
    uint64_t len;
    array_len_insert() = delete;
    explicit array_len_insert(uint64_t l) : len(l){};
};

/**
 * @brief 右值套件，用于控制传入右值的类型。
 * @tparam T 期望右值的类型
 */
template <typename T, typename = std::enable_if_t<std::is_convertible_v<T, uint8_t>>> struct right_v {
    T val;
    right_v() = delete;
    template <typename U> right_v(U &) = delete;
    template <typename U> explicit right_v(U &&v) : val(static_cast<T>(v)){};
};

/**
 * @brief 枚举值套件，用于控制传入枚举值的类型。
 * @tparam T 枚举类型
 */
template <typename T> struct enum_v {
    T *l_val = nullptr;
    T r_val;
    enum_v() = delete;
    explicit enum_v(T &v) : l_val(&v), r_val(v){};
    explicit enum_v(T &&v) : r_val(v){};
};

template <typename T> uint16_t GetTypeId();

template <typename T> uint16_t GetTypePointerId();

class UbseSerialization {
public:
    UbseSerialization();
    explicit UbseSerialization(uint64_t cap);

    UbseSerialization(const UbseSerialization &other) = delete;
    UbseSerialization operator = (const UbseSerialization &other) = delete;

    UbseSerialization(UbseSerialization &&other) noexcept;
    UbseSerialization &operator = (UbseSerialization &&other) noexcept;

    ~UbseSerialization();

    /* *
     * @brief 检验该函数调用前的序列化是否成功
     * @details 如果有重要的序列化数据，建议一次反序列化完直接调用判断
     * @details 其余情况可多次序列化后再进行判断
     * @return true成功， false失败
     */
    inline bool Check() const
    {
        return mFlag;
    }

    /* *
     * @brief 用于插入一个数组长度
     * @param arrayLenInsert 数组长度容器
     * @return UbseSerialization对象
     */
    UbseSerialization &operator << (array_len_insert arrayLenInsert);

    // 禁止基础类型的右值传入，防止反序列化时无法确定类型
    template <typename T, typename = std::enable_if_t<std::is_convertible_v<T, uint64_t>>>
    UbseSerialization &operator << (T &&) = delete;

    template <typename T, typename = std::enable_if_t<std::is_convertible_v<T, uint64_t>>>
    UbseSerialization &operator << (const T &param)
    {
        if (Unlikely(!mFlag)) {
            return *this;
        }
        if (!expandCapacity(sizeof(uint64_t) + AlignTo8(sizeof(T)))) {
            return *this;
        }
        // 八字节，低四字节类型，高四字节长度
        writeTypeAndLen(GetTypeId<T>(), sizeof(T));
        *(reinterpret_cast<T *>(mBuf + mLen)) = param;
        mLen += AlignTo8(sizeof(T));
        return *this;
    }

    template <typename T, typename = std::enable_if_t<std::is_convertible_v<T, uint64_t>>>
    UbseSerialization &operator << (T &param)
    {
        if (Unlikely(!mFlag)) {
            return *this;
        }
        if (!expandCapacity(sizeof(uint64_t) + AlignTo8(sizeof(T)))) {
            return *this;
        }
        // 八字节，低四字节类型，高四字节长度
        writeTypeAndLen(GetTypeId<T>(), sizeof(T));
        *(reinterpret_cast<T *>(mBuf + mLen)) = param;
        mLen += AlignTo8(sizeof(T));
        return *this;
    }

    template <typename T> UbseSerialization &operator << (right_v<T> &&rv)
    {
        return *this << rv.val;
    }

    template <typename T> UbseSerialization &operator << (enum_v<T> &&e)
    {
        int e_v = static_cast<int>(e.r_val);
        return *this << e_v;
    }

    template <typename T> UbseSerialization &operator << (const std::vector<T> &vec)
    {
        *this << array_len_insert(vec.size());
        for (auto &element : vec) {
            *this << element;
        }
        return *this;
    }

    template <typename T_KEY, typename T_VAL> UbseSerialization &operator << (const std::map<T_KEY, T_VAL> &map)
    {
        *this << array_len_insert(map.size());
        for (auto &element : map) {
            *this << element.first << element.second;
        }
        return *this;
    }

    template <typename T_KEY, typename T_VAL, typename T_CMP>
    UbseSerialization &operator << (const std::map<T_KEY, T_VAL, T_CMP> &map)
    {
        *this << array_len_insert(map.size());
        for (auto &element : map) {
            *this << element.first << element.second;
        }
        return *this;
    }

    UbseSerialization &operator << (const std::string &str);

    UbseSerialization &operator << (std::string &&str);

    UbseSerialization &operator << (const char *str);

    UbseSerialization &operator << (UbseSerialization &kid);

    template <typename T> UbseSerialization &operator << (addr_len<T> addrLen)
    {
        add(reinterpret_cast<uint8_t *>(addrLen.ptr), addrLen.len, GetTypePointerId<T>());
        return *this;
    }

    uint8_t *GetBuffer(bool bGetCtrl = false);

    inline uint64_t GetLength() const
    {
        return mLen;
    }

private:
    bool expandCapacity(uint64_t len);

    inline void writeTypeAndLen(uint32_t type, uint32_t len)
    {
        *(reinterpret_cast<uint32_t *>(mBuf + mLen)) = type;
        mLen += sizeof(uint32_t);
        *(reinterpret_cast<uint32_t *>(mBuf + mLen)) = len;
        mLen += sizeof(uint32_t);
    }

    void add(uint8_t *addr, uint64_t len, uint32_t type);

private:
    uint8_t *mBuf;
    uint64_t mCap;
    uint64_t mLen;
    bool mFlag;
};

/**
 * @brief 获取数组长度的容器
 * @tparam T 模板类型，建议使用uint32、uint64
 * @details 使用方法：
 * UbseDeSerialization de;
 * uint64_t len;
 * de >> arrayLenCapture(len); 即可获取数组长度，若反序列化失败返回0
 *
 */
template <typename T> struct array_len_capture {
    T &len;
    array_len_capture() = delete;
    explicit array_len_capture(T &l) : len(l)
    {
        len = 0;
    }
};

/**
 * @brief 动态分配地址+长度组合工具，用于接收反序列化内容
 * @tparam T 基础类型的模板
 * @details 仅用于反序列化，ptr为动态分配的地址，需要用freeFunc手动释放
 */
template <typename T, typename = std::enable_if_t<std::is_convertible_v<T, uint8_t>>> struct alloc_addr_len_ {
    T *ptr;
    uint64_t len;
    void (*freeFunc)(T *&ptr);
    alloc_addr_len_() : ptr(nullptr), len(0), freeFunc(UbseSerialFreeFunc){};
};

class UbseDeSerialization {
public:
    UbseDeSerialization();

    UbseDeSerialization(const uint8_t *buf, uint64_t len, bool bNew = false);

    UbseDeSerialization(const UbseDeSerialization &) = delete;
    UbseDeSerialization operator = (const UbseDeSerialization &) = delete;
    UbseDeSerialization(UbseDeSerialization &&) = delete;
    UbseDeSerialization operator = (UbseDeSerialization &&) = delete;

    ~UbseDeSerialization();

    bool Set(uint8_t *buf, uint64_t len, bool bNew = false);

    /* *
     * @brief 获取用于反序列的内存的地址
     * @param bGetCtrl 是否获取内存的控制权
     * @return nullptr为失败，非nullptr为成功
     * @warning 当bGetCtrl为true时，只有mGetBufCtrl也为true
     * 即该内存为此对象独自持有，才可转移内存的控制权
     */
    uint8_t *GetBuffer(bool bGetCtrl = false);

    inline uint64_t GetLength() const
    {
        return mLen;
    }

    /* *
     * @brief 检验该函数调用前的反序列化是否成功
     * @details 如果有重要的反序列化数据，建议一次反序列化完直接调用判断
     * @details 其余情况可多次反序列化后再进行判断
     * @return true成功， false失败
     */
    inline bool Check() const
    {
        return mFlag;
    }

    template <typename T, typename = std::enable_if_t<std::is_convertible_v<T, uint64_t>>>
    UbseDeSerialization &operator >> (T &param)
    {
        if (!checkValid()) {
            return *this;
        }
        // 八字节，低四字节类型，高四字节长度
        auto t = *(reinterpret_cast<uint32_t *>(mPos));
        mPos += sizeof(uint32_t);
        auto len = *(reinterpret_cast<uint32_t *>(mPos));
        mPos += sizeof(uint32_t);
        if (t != GetTypeId<T>() || len != sizeof(T)) {
            mFlag = false;
            return *this;
        }
        if (mPos + AlignTo8(sizeof(T)) > mBuf + mLen) { // 防错误长度溢出场景
            mFlag = false;
            return *this;
        }
        param = *(reinterpret_cast<T *>(mPos));
        mPos += AlignTo8(sizeof(T));
        return *this;
    }

    template <typename T> UbseDeSerialization &operator >> (std::vector<T> &vec)
    {
        uint64_t len;
        *this >> array_len_capture(len);
        vec.resize(len);
        for (uint64_t i = 0; i < len; i++) {
            *this >> vec[i];
        }
        return *this;
    }

    template <typename T_KEY, typename T_VAL> UbseDeSerialization &operator >> (std::map<T_KEY, T_VAL> &map)
    {
        uint64_t len;
        *this >> array_len_capture(len);
        for (uint64_t i = 0; i < len; i++) {
            T_KEY key;
            *this >> key;
            *this >> map[key];
        }
        return *this;
    }

    template <typename T_KEY, typename T_VAL, typename T_CMP>
    UbseDeSerialization &operator >> (std::map<T_KEY, T_VAL, T_CMP> &map)
    {
        uint64_t len;
        *this >> array_len_capture(len);
        for (uint64_t i = 0; i < len; i++) {
            T_KEY key;
            *this >> key;
            *this >> map[key];
        }
        return *this;
    }

    template <typename T> UbseDeSerialization &operator >> (alloc_addr_len_<T> &allocAddrLen)
    {
        if (!checkValid()) {
            return *this;
        }
        uint8_t *addr;
        uint64_t len;
        if (!get(addr, len, GetTypePointerId<T>())) {
            return *this;
        }
        allocAddrLen.ptr = new (std::nothrow) T[len];
        if (Unlikely(allocAddrLen.ptr == nullptr)) {
            mFlag = false;
            return *this;
        }
        allocAddrLen.len = len / sizeof(T);
        if (Unlikely(SafeCopy(allocAddrLen.ptr, len, addr, len) != 0)) {
            mFlag = false;
        }
        return *this;
    }

    UbseDeSerialization &operator >> (std::string &str);

    UbseDeSerialization &operator >> (char *&str);

    UbseDeSerialization &operator >> (const char *&str);

    UbseDeSerialization &operator >> (UbseDeSerialization &kid);

    template <typename T> UbseDeSerialization &operator >> (addr_len<T> addrLen)
    {
        if (!checkValid()) {
            return *this;
        }
        uint8_t *addr;
        uint64_t len;
        if (!get(addr, len, GetTypePointerId<T>())) {
            return *this;
        }
        if (addrLen.len < len) {
            mFlag = false;
            return *this;
        }
        if (Unlikely(SafeCopy(addrLen.ptr, addrLen.len, addr, len) != 0)) {
            mFlag = false;
        }
        return *this;
    }

    template <typename T> UbseDeSerialization &operator >> (enum_v<T> &&e)
    {
        if (e.l_val == nullptr) {
            mFlag = false;
            return *this;
        }
        if (!checkValid()) {
            return *this;
        }
        int e_v;
        *this >> e_v;
        *e.l_val = static_cast<T>(e_v);
        return *this;
    }

    /* *
     * @brief 通过容器获得数组长度
     * @tparam T 模板类型
     * @param arrayLenCapture 获取长度的容器
     * @return UbseDeSerialization对象
     */
    template <typename T> UbseDeSerialization &operator >> (array_len_capture<T> &&arrayLenCapture)
    {
        if (!checkValid()) {
            return *this;
        }
        if (*reinterpret_cast<uint32_t *>(mPos) == ARRAY_CTRL_CODE) {
            mPos += sizeof(uint32_t);
            arrayLenCapture.len = static_cast<T>(*reinterpret_cast<uint32_t *>(mPos));
            mPos += sizeof(uint32_t);
        } else {
            mFlag = false;
        }
        return *this;
    }

private:
    bool checkValid();

    bool get(uint8_t *&addr, uint64_t &len, uint32_t type);

private:
    uint8_t *mBuf;
    uint8_t *mPos;
    uint64_t mLen;
    bool mFlag;
    bool mGetBufCtrl;
};

template <> uint16_t GetTypeId<short>();
template <> uint16_t GetTypeId<int>();
template <> uint16_t GetTypeId<long>();
template <> uint16_t GetTypeId<long long>();
template <> uint16_t GetTypeId<unsigned short>();
template <> uint16_t GetTypeId<unsigned int>();
template <> uint16_t GetTypeId<unsigned long>();
template <> uint16_t GetTypeId<unsigned long long>();
template <> uint16_t GetTypeId<char>();
template <> uint16_t GetTypeId<signed char>();
template <> uint16_t GetTypeId<unsigned char>();
template <> uint16_t GetTypeId<wchar_t>();
template <> uint16_t GetTypeId<char16_t>();
template <> uint16_t GetTypeId<char32_t>();
template <> uint16_t GetTypeId<bool>();
template <> uint16_t GetTypeId<float>();
template <> uint16_t GetTypeId<double>();

template <> uint16_t GetTypePointerId<short>();
template <> uint16_t GetTypePointerId<int>();
template <> uint16_t GetTypePointerId<long>();
template <> uint16_t GetTypePointerId<long long>();
template <> uint16_t GetTypePointerId<unsigned short>();
template <> uint16_t GetTypePointerId<unsigned int>();
template <> uint16_t GetTypePointerId<unsigned long>();
template <> uint16_t GetTypePointerId<unsigned long long>();
template <> uint16_t GetTypePointerId<char>();
template <> uint16_t GetTypePointerId<signed char>();
template <> uint16_t GetTypePointerId<unsigned char>();
template <> uint16_t GetTypePointerId<wchar_t>();
template <> uint16_t GetTypePointerId<char16_t>();
template <> uint16_t GetTypePointerId<char32_t>();
template <> uint16_t GetTypePointerId<bool>();
template <> uint16_t GetTypePointerId<float>();
template <> uint16_t GetTypePointerId<double>();
} // namespace ubse::utils
#endif // UBSE_MANAGER_UBSE_SERIAL_UTIL_H