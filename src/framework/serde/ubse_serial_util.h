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

#include <cstring>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include "securec.h"

namespace ubse::serial {
inline bool Likely(bool condition)
{
    return __builtin_expect(static_cast<int>(condition), 1) != 0;
}

inline bool Unlikely(bool condition)
{
    return __builtin_expect(static_cast<int>(condition), 0) != 0;
}

using common_len = uint64_t;    // 外部传入长度类型，用于兼容各种长度
using base_type = uint64_t;     // 基础类型比较类型，通过能否与该类型强转来判断是否是基础类型
using base_ptr_type = uint8_t;  // 基础指针类型比较类型，通过能否与该类型强转来判断是否是基础指针类型，必须是uint8

using serial_head = uint32_t;   // 头块[类型块:长度块]，长度块高位替换为类型块即为头块
using serial_type = uint8_t;    // 类型块
using serial_len = serial_head; // 长度块，实际为头块的bit_len(serial_head)-bit_len(serial_type)位
constexpr serial_head LEN_CTRL_OFFSET = (sizeof(serial_len) - sizeof(serial_type)) * 8; // 长度块bit长度
constexpr serial_head LEN_CTRL_MASK = (serial_head(1) << LEN_CTRL_OFFSET) - 1;

constexpr serial_type MAX_BASE_TYPE_NUMS = 64; // 基础类型上限
constexpr common_len INIT_CAPACITY = 256;       // 【可变】
constexpr common_len MAX_CAPACITY = serial_head(1) << LEN_CTRL_OFFSET;  // 容量上限【可变，尽量为2^n】
constexpr common_len ONCE_LIMIT_LEN = MAX_CAPACITY >> 8;  // 单次添加序列化内容长度上限，超过会导致序列化失败【可变，尽量为2^n】

enum class CTRL_TYPE { // 控制类型
    CTRL_CODE_START = MAX_BASE_TYPE_NUMS,
    ARRAY_CTRL_CODE,
    NEST_CTRL_CODE,
    HEAD_CTRL_CODE = std::numeric_limits<serial_type>::max()
};

enum class ALIGN_BASE : serial_head {
    OFFSET_BASE_1 = 0x1,
    OFFSET_BASE_2 = 0x2,
    OFFSET_BASE_4 = 0x4,
    OFFSET_BASE_8 = 0x8,
    OFFSET_BASE_MASK = 0xf
};

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
 * @brief 指定地址+长度组合工具，用于定长数组的序列化和反序列化
 * @tparam T 基础类型的模板
 * @details 序列化时，保证ptr指向内存中有内容且len长度正确
 * @details 反序列化时，保证ptr已经分配足够内存空间，如果空间不够，会反序列化失败
 */
template <typename T, typename = std::enable_if_t<std::is_convertible_v<T, base_ptr_type>>> struct addr_len {
    T *ptr;
    common_len len;
    addr_len() : ptr(nullptr), len(0){};
    addr_len(T *p, common_len l) : ptr(p), len(l * sizeof(T)){};
};

/**
 * @brief 数组长度容器
 * 用法 UbseSerialization se << array_len_insert(len);
 */
struct array_len_insert {
    common_len len;
    array_len_insert() = delete;
    explicit array_len_insert(common_len l) : len(l){};
};

/**
 * @brief 右值套件，用于控制传入右值的类型。
 * @tparam T 期望右值的类型
 */
template <typename T, typename = std::enable_if_t<std::is_convertible_v<T, base_type>>> struct right_v {
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

template <typename T> serial_type GetTypeId();

template <typename T> serial_type GetTypePointerId();

class UbseSerialization {
public:
    UbseSerialization();
    explicit UbseSerialization(ALIGN_BASE align);
    explicit UbseSerialization(common_len cap,
                              ALIGN_BASE align = static_cast<ALIGN_BASE>(sizeof(serial_head)));

    UbseSerialization(const UbseSerialization &other) = delete;
    UbseSerialization operator = (const UbseSerialization &other) = delete;

    UbseSerialization(UbseSerialization &&other) noexcept;
    UbseSerialization &operator = (UbseSerialization &&other) noexcept;

    ~UbseSerialization();

    /**
     * @brief 检验该函数调用前的序列化是否成功
     * @details 如果有重要的序列化数据，建议一次反序列化完直接调用判断
     * @details 其余情况可多次序列化后再进行判断
     * @return true成功， false失败
     */
    inline bool Check() const
    {
        return mFlag_;
    }

    // 禁止基础类型的右值传入，防止反序列化时无法确定类型
    template <typename T, typename = std::enable_if_t<std::is_convertible_v<T, base_type>>>
    UbseSerialization &operator << (T &&) = delete;

    template <typename T, typename = std::enable_if_t<std::is_convertible_v<T, base_type>>>
    UbseSerialization &operator << (const T &param)
    {
        if (Unlikely(!mFlag_)) {
            return *this;
        }
        auto alignedHeadLen = AlignTo(sizeof(serial_head));
        auto alignedParamLen = AlignTo(sizeof(T));
        if (!expandCapacity(alignedHeadLen + alignedParamLen)) {
            return *this;
        }
        writeTypeAndLen(mBuf_ + mLen_, GetTypeId<T>(), sizeof(T));
        mLen_ += alignedHeadLen;
        *(reinterpret_cast<T *>(mBuf_ + mLen_)) = param;
        mLen_ += alignedParamLen;
        return *this;
    }

    template <typename T, typename = std::enable_if_t<std::is_convertible_v<T, base_type>>>
    UbseSerialization &operator << (T &param)
    {
        const T &constParam = param;
        return *this << constParam;
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
        if (vec.empty()) {
            return *this;
        }
        if constexpr (std::is_convertible_v<T, base_type>) {
            *this << addr_len<T>(const_cast<T*>(vec.data()), vec.size());
        } else {
            for (auto& element : vec) {
                *this << element;
            }
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

    UbseSerialization &operator << (array_len_insert arrayLenInsert);

    UbseSerialization &operator << (const std::string &str);

    UbseSerialization &operator << (std::string &&str);

    UbseSerialization &operator << (const char *str);

    UbseSerialization &operator << (UbseSerialization &kid);

    template <typename T> UbseSerialization &operator << (addr_len<T> addrLen)
    {
        add(reinterpret_cast<base_ptr_type *>(addrLen.ptr), addrLen.len, GetTypePointerId<T>());
        return *this;
    }

    /**
     * @brief 获取序列化结果
     * @param bGetCtrl 是否获得序列化结果的控制权。false: 获取一份此刻之前的序列化结果快照；true：获取一份此刻之前的序列化结果所有权。
     * @warning 当bGetCtrl为false时，仅获得此刻之前的序列化结果快照。不能长期持有返回的指针，只能短暂使用，除非手动拷贝一份。
     *          并且若再执行序列化很有可能导致前一刻获取的快照失效。
     *          当bGetCtrl为false时，还可以继续使用当前序列化对象继续序列化数据；
     *          当bGetCtrl为true时，该序列化对象就无法再继续序列化；
     * @return 序列化结果的起始地址
     */
    base_ptr_type *GetBuffer(bool bGetCtrl = false);

    inline common_len GetLength() const
    {
        return mLen_;
    }

private:
    bool expandCapacity(common_len len);

    /**
      * @brief 向指定地址写入类型和长度，安全性由调用者控制
      * @param buf 地址
      * @param type 类型
      * @param len 长度
      */
    inline void writeTypeAndLen(base_ptr_type* buf, serial_type type, serial_len len)
    {
        *(reinterpret_cast<serial_head*>(buf)) = (static_cast<serial_head>(type) << LEN_CTRL_OFFSET) | len;
    }

    void add(const base_ptr_type *addr, common_len len, serial_type type);

    inline common_len AlignTo(common_len l) const
    {
        return (((l) + mAlignOffset_) & ~mAlignOffset_);
    }

private:
    base_ptr_type *mBuf_;
    common_len mAlignOffset_;
    common_len mLen_;
    common_len mCap_;
    bool mFlag_;
};

template <> UbseSerialization& UbseSerialization::operator << <bool>(const std::vector<bool>& vec);

/**
 * @brief 获取数组长度的容器
 * @tparam T 模板类型，建议使用uint32、uint64
 * @details 使用方法：
 * UbseDeSerialization de;
 * T len;
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
template <typename T, typename = std::enable_if_t<std::is_convertible_v<T, base_ptr_type>>> struct alloc_addr_len_ {
    T *ptr;
    common_len len;
    void (*freeFunc)(T *&ptr);
    alloc_addr_len_() : ptr(nullptr), len(0), freeFunc(UbseSerialFreeFunc){};
};

class UbseDeSerialization {
public:
    UbseDeSerialization();

    UbseDeSerialization(const base_ptr_type *buf, common_len len, bool bNew = false);

    UbseDeSerialization(const UbseDeSerialization &) = delete;
    UbseDeSerialization operator = (const UbseDeSerialization &) = delete;
    UbseDeSerialization(UbseDeSerialization &&) = delete;
    UbseDeSerialization operator = (UbseDeSerialization &&) = delete;

    ~UbseDeSerialization();

    bool Set(base_ptr_type *buf, common_len len, bool bNew = false);

    /**
     * @brief 获取用于反序列的内存的地址
     * @param bGetCtrl 是否获取内存的控制权
     * @return nullptr为失败，非nullptr为成功
     * @warning 当bGetCtrl为true时，只有mGetBufCtrl也为true
     * 即该内存为此对象独自持有，才可转移内存的控制权
     */
    base_ptr_type *GetBuffer(bool bGetCtrl = false);

    inline common_len GetLength() const
    {
        return mLen_;
    }

    /**
     * @brief 检验该函数调用前的反序列化是否成功
     * @details 如果有重要的反序列化数据，建议一次反序列化完直接调用判断
     * @details 其余情况可多次反序列化后再进行判断
     * @return true成功， false失败
     */
    inline bool Check() const
    {
        return mFlag_;
    }

    /**
     * @brief 主动标记反序列化失败
     * @details 用于业务层在反序列化过程中检测到语义非法（如数组长度超限）时，
     *          让随后的 Check() 返回 false，使上层 Deserialize 统一返回错误码
     */
    inline void SetFail()
    {
        mFlag_ = false;
    }

    template <typename T, typename = std::enable_if_t<std::is_convertible_v<T, base_type>>>
    UbseDeSerialization &operator >> (T &param)
    {
        auto alignedHeadLen = AlignTo(sizeof(serial_head));
        auto alignedParamLen = AlignTo(sizeof(T));
        if (!checkValid(alignedHeadLen + alignedParamLen)) {
            return *this;
        }
        auto o = readLenByType(mPos_, GetTypeId<T>());
        if (!o.has_value() || o.value() != sizeof(T)) {
            mFlag_ = false;
            return *this;
        }
        mPos_ += alignedHeadLen;
        param = *(reinterpret_cast<T *>(mPos_));
        mPos_ += alignedParamLen;
        return *this;
    }

    template <typename T> UbseDeSerialization &operator >> (std::vector<T> &vec)
    {
        common_len len;
        *this >> array_len_capture(len);
        if (len == 0) {
            vec.clear();
            return *this;
        }
        if constexpr (std::is_convertible_v<T, base_type>) {
            base_ptr_type* addr;
            common_len base_len;
            if (!get(addr, base_len, GetTypePointerId<T>())) {
                return *this;
            }
            if (base_len == 0 || base_len % sizeof(T) != 0 || base_len / sizeof(T) != len) {
                mFlag_ = false;
                return *this;
            }
            vec.resize(len);
            T* p = reinterpret_cast<T*>(addr);
            for (size_t i = 0; i < vec.size(); i++) {
                vec[i] = *(p + i);
            }
        } else {
            vec.resize(len);
            for (common_len i = 0; i < len; i++) {
                *this >> vec[i];
            }
        }
        return *this;
    }

    template <typename T_KEY, typename T_VAL> UbseDeSerialization &operator >> (std::map<T_KEY, T_VAL> &map)
    {
        common_len len;
        *this >> array_len_capture(len);
        for (common_len i = 0; i < len; i++) {
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
        base_ptr_type* addr;
        common_len len;
        if (!get(addr, len, GetTypePointerId<T>())) {
            return *this;
        }
        if (Unlikely(len % sizeof(T) != 0)) {
            mFlag_ = false;
            return *this;
        }
        allocAddrLen.len = len / sizeof(T);
        allocAddrLen.ptr = new (std::nothrow) T[allocAddrLen.len];
        if (Unlikely(allocAddrLen.ptr == nullptr)) {
            mFlag_ = false;
            return *this;
        }
        if (Unlikely(memcpy_s(allocAddrLen.ptr, len, addr, len) != EOK)) {
            delete[] allocAddrLen.ptr;
            allocAddrLen.ptr = nullptr;
            allocAddrLen.len = 0;
            mFlag_ = false;
        }
        return *this;
    }

    UbseDeSerialization &operator >> (std::string &str);

    UbseDeSerialization &operator >> (char *&str);

    UbseDeSerialization &operator >> (UbseDeSerialization &kid);

    template <typename T> UbseDeSerialization &operator >> (addr_len<T> addrLen)
    {
        base_ptr_type *addr;
        common_len len;
        if (!get(addr, len, GetTypePointerId<T>())) {
            return *this;
        }
        if (len % sizeof(T) != 0 || addrLen.len < len) {
            mFlag_ = false;
            return *this;
        }
        if (Unlikely(memcpy_s(addrLen.ptr, addrLen.len, addr, len) != EOK)) {
            mFlag_ = false;
        }
        return *this;
    }

    template <typename T> UbseDeSerialization &operator >> (enum_v<T> &&e)
    {
        if (Unlikely(e.l_val == nullptr)) {
            mFlag_ = false;
            return *this;
        }
        int e_v;
        *this >> e_v;
        if (mFlag_) {
            *e.l_val = static_cast<T>(e_v);
        }
        return *this;
    }

    /**
     * @brief 通过容器获得数组长度
     * @tparam T 模板类型
     * @param arrayLenCapture 获取长度的容器
     * @return UbseDeSerialization对象
     */
    template <typename T> UbseDeSerialization &operator >> (array_len_capture<T> &&arrayLenCapture)
    {
        auto expectReadLen = AlignTo(sizeof(serial_head));
        if (!checkValid(expectReadLen)) {
            return *this;
        }
        auto o = readLenByType(mPos_, static_cast<serial_type>(CTRL_TYPE::ARRAY_CTRL_CODE));
        if (o.has_value()) {
            arrayLenCapture.len = static_cast<T>(o.value());
            mPos_ += expectReadLen;
        } else {
            mFlag_ = false;
        }
        return *this;
    }

private:
    // 默认必须能读到控制头
    bool checkValid(common_len expectLen);

    bool get(base_ptr_type *&addr, common_len &len, serial_type type);

    /**
     * @brief 从指定地址根据类型读取长度，安全性由调用者保证
     * @param buf 地址
     * @param type 类型
     * @return std::optional<serial_len>，若类型不匹配则为空，反之则读取到长度
     */
    inline std::optional<serial_len> readLenByType(base_ptr_type *buf, const serial_type &type)
    {
        std::optional<serial_len> result;
        auto head = *reinterpret_cast<serial_head*>(buf);
        auto t = static_cast<serial_type>(head >> LEN_CTRL_OFFSET);
        if (t == type) {
            result = head & LEN_CTRL_MASK;
        }
        return result;
    }

    inline common_len AlignTo(common_len l) const
    {
        return (((l) + mAlignOffset_) & ~mAlignOffset_);
    }

private:
    base_ptr_type *mBuf_;
    base_ptr_type *mPos_;
    common_len mLen_;
    bool mFlag_;
    bool mGetBufCtrl_;
    common_len mAlignOffset_;
};

template <> UbseDeSerialization& UbseDeSerialization::operator >> <bool>(std::vector<bool>& vec);

template <> serial_type GetTypeId<short>();
template <> serial_type GetTypeId<int>();
template <> serial_type GetTypeId<long>();
template <> serial_type GetTypeId<long long>();
template <> serial_type GetTypeId<unsigned short>();
template <> serial_type GetTypeId<unsigned int>();
template <> serial_type GetTypeId<unsigned long>();
template <> serial_type GetTypeId<unsigned long long>();
template <> serial_type GetTypeId<char>();
template <> serial_type GetTypeId<signed char>();
template <> serial_type GetTypeId<unsigned char>();
template <> serial_type GetTypeId<wchar_t>();
template <> serial_type GetTypeId<char16_t>();
template <> serial_type GetTypeId<char32_t>();
template <> serial_type GetTypeId<bool>();
template <> serial_type GetTypeId<float>();
template <> serial_type GetTypeId<double>();

template <> serial_type GetTypePointerId<short>();
template <> serial_type GetTypePointerId<int>();
template <> serial_type GetTypePointerId<long>();
template <> serial_type GetTypePointerId<long long>();
template <> serial_type GetTypePointerId<unsigned short>();
template <> serial_type GetTypePointerId<unsigned int>();
template <> serial_type GetTypePointerId<unsigned long>();
template <> serial_type GetTypePointerId<unsigned long long>();
template <> serial_type GetTypePointerId<char>();
template <> serial_type GetTypePointerId<signed char>();
template <> serial_type GetTypePointerId<unsigned char>();
template <> serial_type GetTypePointerId<wchar_t>();
template <> serial_type GetTypePointerId<char16_t>();
template <> serial_type GetTypePointerId<char32_t>();
template <> serial_type GetTypePointerId<bool>();
template <> serial_type GetTypePointerId<float>();
template <> serial_type GetTypePointerId<double>();
template <> serial_type GetTypePointerId<std::_Bit_reference>();
template <> serial_type GetTypePointerId<std::string>();
} // namespace ubse::utils
#endif // UBSE_MANAGER_UBSE_SERIAL_UTIL_H