/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "ubse_serial_util.h"

namespace ubse::serial {
enum class allowed_type {
    CHAR = 0,
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
    STRING,
    BIT_REFERENCE,
    MAX_BASE_TYPE_NUMS = 64,
};

template <>
serial_type GetTypeId<short>()
{
    return static_cast<serial_type>(allowed_type::SHORT);
}

template <>
serial_type GetTypeId<int>()
{
    return static_cast<serial_type>(allowed_type::INT);
}
template <>
serial_type GetTypeId<long>()
{
    return static_cast<serial_type>(allowed_type::LONG);
}
template <>
serial_type GetTypeId<long long>()
{
    return static_cast<serial_type>(allowed_type::LONG_LONG);
}
template <>
serial_type GetTypeId<unsigned short>()
{
    return static_cast<serial_type>(allowed_type::UNSIGNED_SHORT);
}
template <>
serial_type GetTypeId<unsigned int>()
{
    return static_cast<serial_type>(allowed_type::UNSIGNED_INT);
}
template <>
serial_type GetTypeId<unsigned long>()
{
    return static_cast<serial_type>(allowed_type::UNSIGNED_LONG);
}
template <>
serial_type GetTypeId<unsigned long long>()
{
    return static_cast<serial_type>(allowed_type::UNSIGNED_LONG_LONG);
}
template <>
serial_type GetTypeId<char>()
{
    return static_cast<serial_type>(allowed_type::CHAR);
}
template <>
serial_type GetTypeId<signed char>()
{
    return static_cast<serial_type>(allowed_type::SIGNED_CHAR);
}
template <>
serial_type GetTypeId<unsigned char>()
{
    return static_cast<serial_type>(allowed_type::UNSIGNED_CHAR);
}
template <>
serial_type GetTypeId<wchar_t>()
{
    return static_cast<serial_type>(allowed_type::WCHAR_T);
}
template <>
serial_type GetTypeId<char16_t>()
{
    return static_cast<serial_type>(allowed_type::CHAR16_T);
}
template <>
serial_type GetTypeId<char32_t>()
{
    return static_cast<serial_type>(allowed_type::CHAR32_T);
}
template <>
serial_type GetTypeId<bool>()
{
    return static_cast<serial_type>(allowed_type::BOOL);
}
template <>
serial_type GetTypeId<float>()
{
    return static_cast<serial_type>(allowed_type::FLOAT);
}
template <>
serial_type GetTypeId<double>()
{
    return static_cast<serial_type>(allowed_type::DOUBLE);
}

template <>
serial_type GetTypePointerId<short>()
{
    return static_cast<serial_type>(allowed_type::SHORT_PTR);
}

template <>
serial_type GetTypePointerId<int>()
{
    return static_cast<serial_type>(allowed_type::INT_PTR);
}
template <>
serial_type GetTypePointerId<long>()
{
    return static_cast<serial_type>(allowed_type::LONG_PTR);
}
template <>
serial_type GetTypePointerId<long long>()
{
    return static_cast<serial_type>(allowed_type::LONG_LONG_PTR);
}
template <>
serial_type GetTypePointerId<unsigned short>()
{
    return static_cast<serial_type>(allowed_type::UNSIGNED_SHORT_PTR);
}
template <>
serial_type GetTypePointerId<unsigned int>()
{
    return static_cast<serial_type>(allowed_type::UNSIGNED_INT_PTR);
}
template <>
serial_type GetTypePointerId<unsigned long>()
{
    return static_cast<serial_type>(allowed_type::UNSIGNED_LONG_PTR);
}
template <>
serial_type GetTypePointerId<unsigned long long>()
{
    return static_cast<serial_type>(allowed_type::UNSIGNED_LONG_LONG_PTR);
}
template <>
serial_type GetTypePointerId<char>()
{
    return static_cast<serial_type>(allowed_type::CHAR_PTR);
}
template <>
serial_type GetTypePointerId<signed char>()
{
    return static_cast<serial_type>(allowed_type::SIGNED_CHAR_PTR);
}
template <>
serial_type GetTypePointerId<unsigned char>()
{
    return static_cast<serial_type>(allowed_type::UNSIGNED_CHAR_PTR);
}
template <>
serial_type GetTypePointerId<wchar_t>()
{
    return static_cast<serial_type>(allowed_type::WCHAR_T_PTR);
}
template <>
serial_type GetTypePointerId<char16_t>()
{
    return static_cast<serial_type>(allowed_type::CHAR16_T_PTR);
}
template <>
serial_type GetTypePointerId<char32_t>()
{
    return static_cast<serial_type>(allowed_type::CHAR32_T_PTR);
}
template <>
serial_type GetTypePointerId<bool>()
{
    return static_cast<serial_type>(allowed_type::BOOL_PTR);
}
template <>
serial_type GetTypePointerId<float>()
{
    return static_cast<serial_type>(allowed_type::FLOAT_PTR);
}
template <>
serial_type GetTypePointerId<double>()
{
    return static_cast<serial_type>(allowed_type::DOUBLE_PTR);
}
template <>
serial_type GetTypePointerId<std::_Bit_reference>()
{
    return static_cast<serial_type>(allowed_type::BIT_REFERENCE);
}
template <>
serial_type GetTypePointerId<std::string>()
{
    return static_cast<serial_type>(allowed_type::STRING);
}
} // namespace ubse::serial