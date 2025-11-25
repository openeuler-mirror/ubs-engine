/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "ubse_serial_util.h"

namespace ubse::serial {
enum class allowed_type {
    CHAR,
    BOOL,
    SHORT,
    INT,
    LONG,
    LONG_LONG,
    SIGNED_CHAR,

    FLOAT,
    DOUBLE,

    UNSIGNED_CHAR,
    UNSIGNED_SHORT,
    UNSIGNED_INT,
    UNSIGNED_LONG,
    UNSIGNED_LONG_LONG,

    WCHAR_T,
    CHAR16_T,
    CHAR32_T,

    SHORT_PTR,
    INT_PTR,
    LONG_PTR,
    LONG_LONG_PTR,
    UNSIGNED_SHORT_PTR,
    UNSIGNED_INT_PTR,
    UNSIGNED_LONG_PTR,
    UNSIGNED_LONG_LONG_PTR,
    CHAR_PTR,
    SIGNED_CHAR_PTR,
    UNSIGNED_CHAR_PTR,
    WCHAR_T_PTR,
    CHAR16_T_PTR,
    CHAR32_T_PTR,
    BOOL_PTR,
    FLOAT_PTR,
    DOUBLE_PTR,
};

template <>
uint16_t GetTypeId<short>()
{
    return static_cast<uint16_t>(allowed_type::SHORT);
}

template <>
uint16_t GetTypeId<int>()
{
    return static_cast<uint16_t>(allowed_type::INT);
}
template <>
uint16_t GetTypeId<long>()
{
    return static_cast<uint16_t>(allowed_type::LONG);
}
template <>
uint16_t GetTypeId<long long>()
{
    return static_cast<uint16_t>(allowed_type::LONG_LONG);
}
template <>
uint16_t GetTypeId<unsigned short>()
{
    return static_cast<uint16_t>(allowed_type::UNSIGNED_SHORT);
}
template <>
uint16_t GetTypeId<unsigned int>()
{
    return static_cast<uint16_t>(allowed_type::UNSIGNED_INT);
}
template <>
uint16_t GetTypeId<unsigned long>()
{
    return static_cast<uint16_t>(allowed_type::UNSIGNED_LONG);
}
template <>
uint16_t GetTypeId<unsigned long long>()
{
    return static_cast<uint16_t>(allowed_type::UNSIGNED_LONG_LONG);
}
template <>
uint16_t GetTypeId<char>()
{
    return static_cast<uint16_t>(allowed_type::CHAR);
}
template <>
uint16_t GetTypeId<signed char>()
{
    return static_cast<uint16_t>(allowed_type::SIGNED_CHAR);
}
template <>
uint16_t GetTypeId<unsigned char>()
{
    return static_cast<uint16_t>(allowed_type::UNSIGNED_CHAR);
}
template <>
uint16_t GetTypeId<wchar_t>()
{
    return static_cast<uint16_t>(allowed_type::WCHAR_T);
}
template <>
uint16_t GetTypeId<char16_t>()
{
    return static_cast<uint16_t>(allowed_type::CHAR16_T);
}
template <>
uint16_t GetTypeId<char32_t>()
{
    return static_cast<uint16_t>(allowed_type::CHAR32_T);
}
template <>
uint16_t GetTypeId<bool>()
{
    return static_cast<uint16_t>(allowed_type::BOOL);
}
template <>
uint16_t GetTypeId<float>()
{
    return static_cast<uint16_t>(allowed_type::FLOAT);
}
template <>
uint16_t GetTypeId<double>()
{
    return static_cast<uint16_t>(allowed_type::DOUBLE);
}

template <>
uint16_t GetTypePointerId<short>()
{
    return static_cast<uint16_t>(allowed_type::SHORT_PTR);
}

template <>
uint16_t GetTypePointerId<int>()
{
    return static_cast<uint16_t>(allowed_type::INT_PTR);
}
template <>
uint16_t GetTypePointerId<long>()
{
    return static_cast<uint16_t>(allowed_type::LONG_PTR);
}
template <>
uint16_t GetTypePointerId<long long>()
{
    return static_cast<uint16_t>(allowed_type::LONG_LONG_PTR);
}
template <>
uint16_t GetTypePointerId<unsigned short>()
{
    return static_cast<uint16_t>(allowed_type::UNSIGNED_SHORT_PTR);
}
template <>
uint16_t GetTypePointerId<unsigned int>()
{
    return static_cast<uint16_t>(allowed_type::UNSIGNED_INT_PTR);
}
template <>
uint16_t GetTypePointerId<unsigned long>()
{
    return static_cast<uint16_t>(allowed_type::UNSIGNED_LONG_PTR);
}
template <>
uint16_t GetTypePointerId<unsigned long long>()
{
    return static_cast<uint16_t>(allowed_type::UNSIGNED_LONG_LONG_PTR);
}
template <>
uint16_t GetTypePointerId<char>()
{
    return static_cast<uint16_t>(allowed_type::CHAR_PTR);
}
template <>
uint16_t GetTypePointerId<signed char>()
{
    return static_cast<uint16_t>(allowed_type::SIGNED_CHAR_PTR);
}
template <>
uint16_t GetTypePointerId<unsigned char>()
{
    return static_cast<uint16_t>(allowed_type::UNSIGNED_CHAR_PTR);
}
template <>
uint16_t GetTypePointerId<wchar_t>()
{
    return static_cast<uint16_t>(allowed_type::WCHAR_T_PTR);
}
template <>
uint16_t GetTypePointerId<char16_t>()
{
    return static_cast<uint16_t>(allowed_type::CHAR16_T_PTR);
}
template <>
uint16_t GetTypePointerId<char32_t>()
{
    return static_cast<uint16_t>(allowed_type::CHAR32_T_PTR);
}
template <>
uint16_t GetTypePointerId<bool>()
{
    return static_cast<uint16_t>(allowed_type::BOOL_PTR);
}
template <>
uint16_t GetTypePointerId<float>()
{
    return static_cast<uint16_t>(allowed_type::FLOAT_PTR);
}
template <>
uint16_t GetTypePointerId<double>()
{
    return static_cast<uint16_t>(allowed_type::DOUBLE_PTR);
}
}  // namespace ubse::utils