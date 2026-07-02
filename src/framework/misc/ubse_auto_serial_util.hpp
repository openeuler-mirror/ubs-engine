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
#ifndef UBSE_AUTO_SERIAL_UTIL_H
#define UBSE_AUTO_SERIAL_UTIL_H
#include <cstddef>
#include <string>
#include <unordered_set>
#include <type_traits>
#include <vector>

#include "src/framework/serde/ubse_serial_util.h"
namespace ubse::serial::util {
using namespace ubse::serial;
// (1) 声明可序列化成员（使用成员指针）
template <class T>
struct SerializableMembers;

// (2) 声明直接基类（用于继承）
template <class T>
struct SerializationTraits {
    using base_type = void; // 默认无基类
};

template <class T>
using base_type_t = typename SerializationTraits<T>::base_type;

template <class T>
constexpr auto GetMembers()
{
    return SerializableMembers<T>::value();
}

// === 可反射类 ===
template <typename T, typename = void>
struct is_reflectable : std::false_type {};

template <typename T>
struct is_reflectable<T, std::void_t<decltype(SerializableMembers<T>::value())>> : std::true_type {};

template <typename T>
constexpr bool is_reflectable_v = is_reflectable<T>::value;

// === 6. 基本类型支持（int 、string 、enum、 bool）===
template <class T>
constexpr bool is_basic_v = std::is_arithmetic_v<T> || std::is_same_v<std::decay_t<T>, std::string>;

template <class T>
std::enable_if_t<is_basic_v<T>, bool> SerializeField(UbseSerialization &out, const T &v)
{
    out << v;
    return out.Check();
}

template <class T>
std::enable_if_t<is_basic_v<T>, bool> DeSerializeField(UbseDeSerialization &in, T &v)
{
    in >> v;
    return in.Check();
}

template <class T>
std::enable_if_t<std::is_enum_v<T>, bool> SerializeField(UbseSerialization &out, const T &v)
{
    out << enum_v(v);
    return out.Check();
}

template <class T>
std::enable_if_t<std::is_enum_v<T>, bool> DeSerializeField(UbseDeSerialization &in, T &v)
{
    in >> enum_v(v);
    return in.Check();
}

// ============================================================
// Raw memory copy — opt-in trait for types that serialize as
// raw byte blobs via addr_len<uint8_t>.
// Specializations for concrete types are declared in per-module
// auto_serial headers (e.g. ubse_mem_auto_serial.h).
// ============================================================
template <typename T>
struct is_raw_memcopy : std::false_type {};

template <typename T>
inline std::enable_if_t<is_raw_memcopy<T>::value, bool> SerializeField(UbseSerialization& out, const T& v)
{
    out << addr_len<uint8_t>{const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(&v)), sizeof(v)};
    return out.Check();
}

template <typename T>
inline std::enable_if_t<is_raw_memcopy<T>::value, bool> DeSerializeField(UbseDeSerialization& in, T& v)
{
    addr_len<uint8_t> addrLen{reinterpret_cast<unsigned char*>(&v), sizeof(v)};
    in >> addrLen;
    return in.Check();
}

// ============================================================
// Bitfield types — opt-in trait for structs that pack into a
// single uint16_t for wire transfer.
// Specialize is_bitfield<T> and bitfield_traits<T> in per-module
// auto_serial headers (e.g. ubse_mem_auto_serial.h).
// ============================================================
template <typename T>
struct is_bitfield : std::false_type {};

template <typename T>
struct bitfield_traits {};

template <typename T>
inline std::enable_if_t<is_bitfield<T>::value, bool> SerializeField(UbseSerialization& out, const T& v)
{
    uint16_t val = bitfield_traits<T>::pack(v);
    out << val;
    return out.Check();
}

template <typename T>
inline std::enable_if_t<is_bitfield<T>::value, bool> DeSerializeField(UbseDeSerialization& in, T& v)
{
    uint16_t val;
    in >> val;
    if (!in.Check())
        return false;
    bitfield_traits<T>::unpack(val, v);
    return true;
}

// === 反射 + 容器前置声明（容器重载互相嵌套需要 Phase 1 可见）===
template <class T>
std::enable_if_t<is_reflectable_v<T>, bool> SerializeField(UbseSerialization &out, const T &obj);

template <class T>
std::enable_if_t<is_reflectable_v<T>, bool> DeSerializeField(UbseDeSerialization &in, T &obj);

template <class K, class V>
bool SerializeField(UbseSerialization &out, const std::unordered_map<K, V> &v);

template <class K, class V>
bool DeSerializeField(UbseDeSerialization &in, std::unordered_map<K, V> &v);

template <typename... Args>
bool SerializeField(UbseSerialization &out, const std::tuple<Args...> &tup);

template <typename... Args>
bool DeSerializeField(UbseDeSerialization &in, std::tuple<Args...> &tup);

template <typename T, size_t N>
bool SerializeField(UbseSerialization &out, const T (&arr)[N]);

template <typename T, size_t N>
bool DeSerializeField(UbseDeSerialization &in, T (&arr)[N]);

template <class T>
bool SerializeField(UbseSerialization &out, const std::unordered_set<T> &v);

template <class T>
bool DeSerializeField(UbseDeSerialization &in, std::unordered_set<T> &v);

// === 5. 容器支持 vector ===
template <class T>
bool SerializeField(UbseSerialization &out, const std::vector<T> &v)
{
    out << array_len_insert(v.size());
    for (const auto &item : v) {
        if (!SerializeField(out, item)) {
            return false;
        }
    }
    return out.Check();
}

template <class T>
bool DeSerializeField(UbseDeSerialization &in, std::vector<T> &v)
{
    uint64_t n;
    in >> array_len_capture(n);
    if (!in.Check() || n > ONCE_LIMIT_LEN) {
        return false;
    }
    v.resize(n);
    for (auto &item : v) {
        if (!DeSerializeField(in, item)) {
            return false;
        }
    }
    return true;
}

// === 6. 容器支持（示例：unordered_map）===
template <class K, class V>
bool SerializeField(UbseSerialization &out, const std::unordered_map<K, V> &v)
{
    if (!SerializeField(out, v.size())) {
        return false;
    }
    for (const auto &[serialKey, serialValue] : v) {
        if (!SerializeField(out, serialKey) || !SerializeField(out, serialValue)) {
            return false;
        }
    }
    return true;
}

template <class K, class V>
bool DeSerializeField(UbseDeSerialization &in, std::unordered_map<K, V> &v)
{
    size_t n;
    if (!DeSerializeField(in, n) || n > ONCE_LIMIT_LEN) {
        return false;
    }
    v.clear();
    v.reserve(n);

    for (size_t i = 0; i < n; ++i) {
        K key{};
        V value{};
        if (!DeSerializeField(in, key) || !DeSerializeField(in, value)) {
            return false;
        }
        v.emplace(std::move(key), std::move(value));
    }
    return true;
}

template <typename... Args>
bool SerializeField(UbseSerialization &out, const std::tuple<Args...> &tup)
{
    return std::apply(
        [&out](const auto &...args) {
            return (SerializeField(out, args) && ...); // C++17 折叠表达式
        },
        tup);
}

template <typename... Args>
bool DeSerializeField(UbseDeSerialization &in, std::tuple<Args...> &tup)
{
    return std::apply([&in](auto &...args) { return (DeSerializeField(in, args) && ...); }, tup);
}

// === C-style array support (must be BEFORE reflectable templates for Phase-1 lookup) ===
template <typename T, size_t N>
bool SerializeField(UbseSerialization &out, const T (&arr)[N])
{
    for (size_t i = 0; i < N; ++i) {
        if (!SerializeField(out, arr[i])) return false;
    }
    return true;
}

template <typename T, size_t N>
bool DeSerializeField(UbseDeSerialization &in, T (&arr)[N])
{
    for (size_t i = 0; i < N; ++i) {
        if (!DeSerializeField(in, arr[i])) return false;
    }
    return true;
}

// === unordered_set support ===
template <class T>
bool SerializeField(UbseSerialization &out, const std::unordered_set<T> &v)
{
    if (!SerializeField(out, v.size())) return false;
    for (const auto &item : v) {
        if (!SerializeField(out, item)) return false;
    }
    return true;
}

template <class T>
bool DeSerializeField(UbseDeSerialization &in, std::unordered_set<T> &v)
{
    size_t n;
    if (!DeSerializeField(in, n) || n > ONCE_LIMIT_LEN) {
        return false;
    }
    v.clear();
    v.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        T item{};
        if (!DeSerializeField(in, item)) {
            return false;
        }
        v.insert(std::move(item));
    }
    return true;
}

// === 反射类型（定义在末尾，确保所有重载对泛型 lambda 可见）===
template <class T>
std::enable_if_t<is_reflectable_v<T>, bool> SerializeField(UbseSerialization &out, const T &obj)
{
    if constexpr (!std::is_same_v<base_type_t<T>, void>) {
        if (!SerializeField(out, static_cast<const base_type_t<T> &>(obj))) {
            return false;
        }
    }
    return std::apply(
        [&](auto... ptrs) { return (SerializeField(out, obj.*ptrs) && ...); },
        SerializableMembers<T>::value());
}

template <class T>
std::enable_if_t<is_reflectable_v<T>, bool> DeSerializeField(UbseDeSerialization &in, T &obj)
{
    if constexpr (!std::is_same_v<base_type_t<T>, void>) {
        if (!DeSerializeField(in, static_cast<base_type_t<T> &>(obj))) {
            return false;
        }
    }
    return std::apply(
        [&](auto... ptrs) { return (DeSerializeField(in, obj.*ptrs) && ...); },
        SerializableMembers<T>::value());
}
} // namespace ubse::serial::util

#define UBSE_SERIALIZE(Class, Base, ...)         \
    template <>                                  \
    struct SerializableMembers<Class> {          \
        static constexpr auto value()            \
        {                                        \
            return std::make_tuple(__VA_ARGS__); \
        }                                        \
    };                                           \
    template <>                                  \
    struct SerializationTraits<Class> {          \
        using base_type = Base;                  \
    };
#endif
