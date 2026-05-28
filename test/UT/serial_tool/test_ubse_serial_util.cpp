/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "test_ubse_serial_util.h"

#include <ctime>
#include <random>

#include "ubse_common_def.h"
#include "ubse_type_util.cpp"

namespace ubse::ut::serial {
using namespace ubse::common::def;
// 生成[min, max]范围内的随机整数
int getRandomInt(int min = INT32_MIN, int max = INT32_MAX)
{
    static std::mt19937 generator(time(nullptr)); // 使用Mersenne Twister引擎
    std::uniform_int_distribution<int> distribution(min, max);
    return distribution(generator);
}

// 生成[min, max]范围内的随机浮点数
double getRandomDouble(double min = FLT_MIN_10_EXP, double max = FLT_MAX_10_EXP)
{
    static std::mt19937 generator(time(nullptr));
    std::uniform_real_distribution<double> distribution(min, max);
    return distribution(generator);
}

void TestUbseSerialUtil::SetUp()
{
    Test::SetUp();
}

void TestUbseSerialUtil::TearDown()
{
    Test::TearDown();
}

/*
 * 用例描述
 * 扩展后的容量不超过MAX_CAPACITY
 */
TEST_F(TestUbseSerialUtil, expandCapacity_less_than_max_cap)
{
    uint64_t expandSize = NO_2048; // 远远小于MAX_CAPACITY
    UbseSerialization serialization;
    serialization.expandCapacity(expandSize);
    EXPECT_TRUE(serialization.Check());
    auto curCap = serialization.mCap_;
    EXPECT_TRUE(curCap < MAX_CAPACITY);
}

/*
 * 用例描述
 * 扩展后的容量翻倍后超过MAX_CAPACITY，则设定为MAX_CAPACITY - 1
 */
TEST_F(TestUbseSerialUtil, expandCapacity_greater_than_max_cap)
{
    GTEST_SKIP();
    uint64_t expandSize = (MAX_CAPACITY >> 1) + 1; // 大于MAX_CAPACITY的一半
    UbseSerialization serialization;
    serialization.expandCapacity(expandSize);
    EXPECT_TRUE(serialization.Check());
    auto curCap = serialization.mCap_;
    EXPECT_EQ(curCap, MAX_CAPACITY - 1);
}

/*
 * 用例描述
 * 扩展失败
 */
TEST_F(TestUbseSerialUtil, expandCapacity_failed)
{
    uint64_t expandSize = MAX_CAPACITY + 1; // 大于MAX_CAPACITY
    UbseSerialization serialization;
    serialization.expandCapacity(expandSize);
    EXPECT_FALSE(serialization.Check());
}

/*
 * 用例描述
 * 基础类型转换
 */
TEST_F(TestUbseSerialUtil, basic_type)
{
    for (uint8_t i = 0; i < 16; i++) { // 循环16次检测
        short short_s = getRandomInt();
        short short_d;
        int int_s = getRandomInt();
        int int_d;
        long long_s = getRandomInt();
        long long_d;
        long long ll_s = getRandomInt();
        long long ll_d;
        unsigned int uint_s = getRandomInt();
        unsigned int uint_d;
        unsigned long ul_s = getRandomInt();
        unsigned long ul_d;
        unsigned long long ull_s = getRandomInt();
        unsigned long long ull_d;

        UbseSerialization serialization;
        EXPECT_EQ(serialization.Check(), true);
        serialization << short_s << int_s << long_s << ll_s << uint_s << ul_s << ull_s;
        EXPECT_EQ(serialization.Check(), true);

        UbseDeSerialization deSerialization(serialization.GetBuffer(), serialization.GetLength());
        deSerialization >> short_d >> int_d >> long_d >> ll_d >> uint_d >> ul_d >> ull_d;
        EXPECT_EQ(deSerialization.Check(), true);
        EXPECT_EQ(short_s, short_d);
        EXPECT_EQ(int_s, int_d);
        EXPECT_EQ(long_s, long_d);
        EXPECT_EQ(ll_s, ll_d);
        EXPECT_EQ(uint_s, uint_d);
        EXPECT_EQ(ul_s, ul_d);
        EXPECT_EQ(ull_s, ull_d);
    }
}

/*
 * 用例描述
 * 序列化输入右值和枚举
 */
TEST_F(TestUbseSerialUtil, serial_right_val_type)
{
    enum class type
    {
        AA,
        BB,
    };
    UbseSerialization serialization;
    type t_s = type::BB;
    serialization << right_v<uint64_t>(1) << right_v<double>(2.3) << right_v<char>('a') << std::string("123")
                  << enum_v(type::AA) << enum_v(t_s);
    EXPECT_EQ(serialization.Check(), true);
    uint64_t a;
    double b;
    char c;
    std::string s;
    type t;
    type t_d;
    UbseDeSerialization deSerialization(serialization.GetBuffer(), serialization.GetLength());
    deSerialization >> a >> b >> c >> s >> enum_v(t) >> enum_v(t_d);
    EXPECT_EQ(deSerialization.Check(), true);
    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 2.3);
    EXPECT_EQ(c, 'a');
    EXPECT_EQ(s, std::string("123"));
    EXPECT_EQ(t, type::AA);
    EXPECT_EQ(t_s, t_d);
}

/*
 * 用例描述
 * 错误场景，类型未对齐
 */
TEST_F(TestUbseSerialUtil, fail_scene)
{
    short short_s = getRandomInt();
    short short_d;
    int int_s = getRandomInt();
    int int_d;
    long long_s = getRandomInt();
    long long_d;
    long long ll_s = getRandomInt();
    long long ll_d;
    float f_s = getRandomDouble();
    float f_d;
    auto typeId = GetTypeId<short>();
    UbseSerialization serialization;
    serialization << short_s << int_s << long_s << ll_s << f_s;
    EXPECT_EQ(serialization.Check(), true);

    UbseDeSerialization deSerialization(serialization.GetBuffer(), serialization.GetLength());
    deSerialization >> f_d >> ll_d >> long_d >> int_d >> short_d;
    EXPECT_EQ(deSerialization.Check(), false);
}

/*
 * 用例描述
 * 数组场景——成功
 */
TEST_F(TestUbseSerialUtil, array_secne)
{
    UbseSerialization serialization;
    uint64_t arr[NO_8][NO_4];
    serialization << array_len_insert(NO_8);
    for (uint16_t i = 0; i < NO_8; i++) {
        serialization << array_len_insert(NO_4);
        for (uint16_t j = 0; j < NO_4; j++) {
            arr[i][j] = i * NO_8 + j;
            serialization << arr[i][j];
        }
    }
    EXPECT_EQ(serialization.Check(), true);

    UbseDeSerialization deSerialization(serialization.GetBuffer(), serialization.GetLength());
    uint32_t x;
    uint32_t y;
    deSerialization >> array_len_capture(x);
    EXPECT_EQ(x, NO_8);
    for (uint16_t i = 0; i < x; i++) {
        deSerialization >> array_len_capture(y);
        EXPECT_EQ(y, NO_4);
        for (uint16_t j = 0; j < y; j++) {
            uint64_t v;
            deSerialization >> v;
            EXPECT_EQ(v, arr[i][j]);
        }
    }
    EXPECT_EQ(deSerialization.Check(), true);
}

/*
 * 用例描述
 * 容器场景——成功
 */
TEST_F(TestUbseSerialUtil, vector_map_scene)
{
    {
        UbseSerialization serialization;
        std::vector<int> vec_s = {1, 2, 3, 4};
        std::vector<int> vec_d;
        serialization << vec_s;
        EXPECT_EQ(serialization.Check(), true);
        UbseDeSerialization deSerialization(serialization.GetBuffer(), serialization.GetLength());
        deSerialization >> vec_d;
        EXPECT_EQ(deSerialization.Check(), true);
        for (uint8_t i = 0; i < vec_d.size(); i++) {
            EXPECT_EQ(vec_s[i], vec_d[i]);
        }
    }
    {
        UbseSerialization serialization;
        std::map<int, std::string> map_s;
        std::map<int, std::string> map_d;
        map_s[NO_1] = "111";
        map_s[NO_2] = "222";
        serialization << map_s;
        EXPECT_EQ(serialization.Check(), true);
        UbseDeSerialization deSerialization(serialization.GetBuffer(), serialization.GetLength());
        deSerialization >> map_d;
        EXPECT_EQ(deSerialization.Check(), true);
        for (auto& kv : map_d) {
            EXPECT_EQ(map_d[kv.first], map_s[kv.first]);
        }
    }
    {
        UbseSerialization serialization;
        std::map<std::string, std::vector<int>> map_s, map_d;
        map_s["111"] = {1, 2, 3, 4};
        map_s["222"] = {1, 2, 3, 4};
        serialization << map_s;
        EXPECT_EQ(serialization.Check(), true);
        UbseDeSerialization deSerialization(serialization.GetBuffer(), serialization.GetLength());
        deSerialization >> map_d;
        EXPECT_EQ(deSerialization.Check(), true);
        for (auto& kv : map_d) {
            auto& v_d = kv.second;
            auto& v_s = map_s[kv.first];
            EXPECT_EQ(v_s.size(), v_d.size());
            for (uint8_t i = 0; i < v_d.size(); i++) {
                EXPECT_EQ(v_s[i], v_d[i]);
            }
        }
    }
}
/*
 * 用例描述
 * 地址长度套件场景——成功
 */
TEST_F(TestUbseSerialUtil, addr_len)
{
    {
        UbseSerialization serialization;
        int arr[4] = {1, 2, 3, 4};
        serialization << addr_len{arr, 4};
        EXPECT_EQ(serialization.Check(), true);

        UbseDeSerialization deSerialization(serialization.GetBuffer(), serialization.GetLength());
        alloc_addr_len_<int> allocAddrLen;
        deSerialization >> allocAddrLen;
        EXPECT_EQ(deSerialization.Check(), true);
        EXPECT_EQ(allocAddrLen.len, 4);
        for (uint8_t i = 0; i < allocAddrLen.len; i++) {
            EXPECT_EQ(arr[i], allocAddrLen.ptr[i]);
        }
        allocAddrLen.freeFunc(allocAddrLen.ptr);
    }
    {
        UbseSerialization serialization;
        addr_len<uint8_t> addrLen;
        addrLen.len = 1 << 20;
        uint8_t tmp = 0;
        addrLen.ptr = &tmp;
        serialization << addrLen;
        EXPECT_EQ(serialization.Check(), false); // 超长
    }
}

/*
 * 用例描述
 * 字符串场景——成功
 */
TEST_F(TestUbseSerialUtil, string_scene)
{
    {
        UbseSerialization serialization;
        std::string str_s = "1234";
        std::string str_d;
        serialization << str_s;
        EXPECT_EQ(serialization.Check(), true);

        UbseDeSerialization deSerialization(serialization.GetBuffer(), serialization.GetLength());
        deSerialization >> str_d;
        EXPECT_EQ(deSerialization.Check(), true);
        EXPECT_EQ(str_s, str_d);
    }

    {
        UbseSerialization serialization;
        char* str_s = "hahaha";
        std::string str_d;
        serialization << str_s;
        EXPECT_EQ(serialization.Check(), true);

        UbseDeSerialization deSerialization(serialization.GetBuffer(), serialization.GetLength());
        deSerialization >> str_d;
        EXPECT_EQ(deSerialization.Check(), true);
        EXPECT_EQ(std::string(str_s), str_d);
    }

    {
        UbseSerialization serialization;
        std::string str_s = "hahaha"; // char* str_s = "hahaha"
        char* str_d = nullptr;
        serialization << str_s;
        EXPECT_EQ(serialization.Check(), true);

        UbseDeSerialization deSerialization(serialization.GetBuffer(), serialization.GetLength());
        deSerialization >> str_d;
        EXPECT_EQ(deSerialization.Check(), true);
        EXPECT_EQ(std::string(str_d), str_s);
        delete[] str_d;
    }
}

/*
 * 用例描述
 * 套娃场景——成功
 */
TEST_F(TestUbseSerialUtil, nesting)
{
    UbseSerialization se_a;
    std::vector<int> vec_s = {1, 2, 3, 4};
    std::vector<int> vec_d;
    se_a << vec_s;
    EXPECT_EQ(se_a.Check(), true);
    UbseSerialization se_b;
    std::string str_s = "1234";
    std::string str_d;
    se_b << str_s;
    EXPECT_EQ(se_b.Check(), true);
    UbseSerialization se_c;
    std::map<std::string, std::vector<int>> map_s;
    std::map<std::string, std::vector<int>> map_d;
    map_s["111"] = {1, 2, 3, 4};
    map_s["222"] = {1, 2, 3, 4};
    map_s["333"] = {1, 2, 3, 4};
    se_c << map_s;
    EXPECT_EQ(se_c.Check(), true);
    UbseSerialization serialization;
    serialization << se_a << se_b << se_c;
    EXPECT_EQ(serialization.Check(), true);
    UbseDeSerialization deSerialization(serialization.GetBuffer(), serialization.GetLength());
    UbseDeSerialization de_a;
    UbseDeSerialization de_b;
    UbseDeSerialization de_c;
    deSerialization >> de_a >> de_b >> de_c;
    EXPECT_EQ(deSerialization.Check(), true);
    de_a >> vec_d;
    EXPECT_EQ(de_a.Check(), true);
    for (uint8_t i = 0; i < vec_d.size(); i++) {
        EXPECT_EQ(vec_s[i], vec_d[i]);
    }
    de_b >> str_d;
    EXPECT_EQ(de_b.Check(), true);
    EXPECT_EQ(str_s, str_d);
    de_c >> map_d;
    EXPECT_EQ(de_c.Check(), true);
    for (auto& kv : map_d) {
        auto& v_d = kv.second;
        auto& v_s = map_s[kv.first];
        EXPECT_EQ(v_s.size(), v_d.size());
        for (uint8_t i = 0; i < v_d.size(); i++) {
            EXPECT_EQ(v_s[i], v_d[i]);
        }
    }
}

/*
 * 用例描述
 * 截断场景——防护成功
 */
TEST_F(TestUbseSerialUtil, cut)
{
    {
        UbseSerialization serialization;
        short a_s = 1;
        short a_d;
        serialization << a_s;

        UbseDeSerialization deNoCut(serialization.GetBuffer(), serialization.GetLength());
        deNoCut >> a_d;
        EXPECT_EQ(deNoCut.Check(), true);
        EXPECT_EQ(a_s, a_d);

        UbseDeSerialization deCut1(serialization.GetBuffer(), sizeof(uint32_t));
        EXPECT_EQ(deCut1.Check(), false);

        UbseDeSerialization deCut2(serialization.GetBuffer(), sizeof(uint64_t));
        deCut2 >> a_d;
        EXPECT_EQ(deCut2.Check(), false);
    }
    {
        UbseSerialization serialization;
        std::string str_s = "cut_scene12345", str_d;
        serialization << str_s;
        UbseDeSerialization deNoCut(serialization.GetBuffer(), serialization.GetLength());
        deNoCut >> str_d;
        EXPECT_EQ(deNoCut.Check(), true);
        EXPECT_EQ(str_s, str_d);

        UbseDeSerialization deCut1(serialization.GetBuffer(), sizeof(uint64_t) << 1);
        deCut1 >> str_d;
        EXPECT_EQ(deCut1.Check(), false);
    }
}

TEST_F(TestUbseSerialUtil, operator_right_arrow_const_char_ptr)
{
    // Case 1: 正常反序列化非空字符串
    {
        UbseSerialization ser;
        const char* src = "hello";
        ser << src;

        UbseDeSerialization deSer(ser.GetBuffer(), ser.GetLength());
        char* dst = nullptr;
        deSer >> dst;
        ASSERT_TRUE(deSer.Check());
        EXPECT_STREQ(dst, "hello");
        delete[] dst;
    }

    // Case 2: 触发 checkValid() 失败（如 pos 越界）
    {
        UbseDeSerialization deSer;
        deSer.mPos_ = deSer.mBuf_ + deSer.mLen_ + 1; // 强制越界
        char* str = nullptr;
        deSer >> str;
        EXPECT_FALSE(deSer.Check());
    }

    // Case 3: get() 失败（类型不匹配）
    {
        UbseSerialization ser;
        int x = 123;
        ser << x;

        UbseDeSerialization deSer(ser.GetBuffer(), ser.GetLength());
        char* str = nullptr;
        deSer >> str;
        EXPECT_FALSE(deSer.Check());
    }

    // Case 4: len == 0（非法字符串长度）
    {
        UbseSerialization ser;
        char empty = '\0';
        ser.add(reinterpret_cast<uint8_t*>(&empty), 0, GetTypePointerId<char>()); // 手动写入 len=0

        UbseDeSerialization deSer(ser.GetBuffer(), ser.GetLength());
        char* str = nullptr;
        deSer >> str;
        EXPECT_FALSE(deSer.Check());
    }
}

TEST_F(TestUbseSerialUtil, GetBuffer_deSer_with_ctrl)
{
    UbseSerialization ser;
    ser << "test";

    // bNew=true，表示接管 buffer 控制权
    UbseDeSerialization deSer(ser.GetBuffer(), ser.GetLength(), true);
    EXPECT_TRUE(deSer.Check());

    // Case 1: bGetCtrl = false -> 返回 buffer，不置空
    uint8_t* buf1 = deSer.GetBuffer(false);
    EXPECT_NE(buf1, nullptr);
    EXPECT_NE(deSer.mBuf_, nullptr); // mBuf 仍存在

    // Case 2: bGetCtrl = true 且 mGetBufCtrl = true -> 返回原指针，并置 mBuf=nullptr
    uint8_t* buf2 = deSer.GetBuffer(true);
    EXPECT_EQ(buf2, buf1);
    EXPECT_EQ(deSer.mBuf_, nullptr); // 已释放控制权

    // Case 3: 再次调用 GetBuffer(true) -> 因 mGetBufCtrl 仍为 true，但 mBuf=nullptr，应返回 nullptr
    uint8_t* buf3 = deSer.GetBuffer(true);
    EXPECT_EQ(buf3, nullptr);

    // Case 4: 构造时 bNew=false，则 mGetBufCtrl=false，此时 GetBuffer(true) 应返回 nullptr
    UbseDeSerialization deSer2(ser.GetBuffer(), ser.GetLength(), false);
    uint8_t* buf4 = deSer2.GetBuffer(true);
    EXPECT_EQ(buf4, nullptr);
}

TEST_F(TestUbseSerialUtil, GetBuffer_ControlTransfer)
{
    UbseSerialization ser;
    ser << "test_data";
    {
        UbseDeSerialization deSer(ser.GetBuffer(), ser.GetLength(), true);
        EXPECT_TRUE(deSer.Check());

        // bGetCtrl = false → 返回 buffer，mBuf 不变
        uint8_t* buf1 = deSer.GetBuffer(false);
        EXPECT_NE(buf1, nullptr);
        EXPECT_NE(deSer.mBuf_, nullptr);

        // bGetCtrl = true → 返回原指针，mBuf 置空
        uint8_t* buf2 = deSer.GetBuffer(true);
        EXPECT_EQ(buf2, buf1);
        EXPECT_EQ(deSer.mBuf_, nullptr);

        // 再次调用 → 返回 nullptr
        uint8_t* buf3 = deSer.GetBuffer(true);
        EXPECT_EQ(buf3, nullptr);
    }
    {
        UbseDeSerialization deSer(ser.GetBuffer(), ser.GetLength(), false);
        EXPECT_TRUE(deSer.Check());

        // 即使 bGetCtrl=true，也因 mGetBufCtrl=false 而拒绝
        uint8_t* buf = deSer.GetBuffer(true);
        EXPECT_EQ(buf, nullptr);

        // 但只读访问仍可
        uint8_t* readOnly = deSer.GetBuffer(false);
        EXPECT_NE(readOnly, nullptr);
    }
}

// 测试基本类型的 TypeId
TEST(SerialUtilTest, GetTypeId_BasicTypes)
{
    EXPECT_EQ(GetTypeId<char>(), static_cast<uint16_t>(allowed_type::CHAR));
    EXPECT_EQ(GetTypeId<signed char>(), static_cast<uint16_t>(allowed_type::SIGNED_CHAR));
    EXPECT_EQ(GetTypeId<unsigned char>(), static_cast<uint16_t>(allowed_type::UNSIGNED_CHAR));

    EXPECT_EQ(GetTypeId<short>(), static_cast<uint16_t>(allowed_type::SHORT));
    EXPECT_EQ(GetTypeId<unsigned short>(), static_cast<uint16_t>(allowed_type::UNSIGNED_SHORT));

    EXPECT_EQ(GetTypeId<int>(), static_cast<uint16_t>(allowed_type::INT));
    EXPECT_EQ(GetTypeId<unsigned int>(), static_cast<uint16_t>(allowed_type::UNSIGNED_INT));

    EXPECT_EQ(GetTypeId<long>(), static_cast<uint16_t>(allowed_type::LONG));
    EXPECT_EQ(GetTypeId<unsigned long>(), static_cast<uint16_t>(allowed_type::UNSIGNED_LONG));

    EXPECT_EQ(GetTypeId<long long>(), static_cast<uint16_t>(allowed_type::LONG_LONG));
    EXPECT_EQ(GetTypeId<unsigned long long>(), static_cast<uint16_t>(allowed_type::UNSIGNED_LONG_LONG));

    EXPECT_EQ(GetTypeId<bool>(), static_cast<uint16_t>(allowed_type::BOOL));

    EXPECT_EQ(GetTypeId<float>(), static_cast<uint16_t>(allowed_type::FLOAT));
    EXPECT_EQ(GetTypeId<double>(), static_cast<uint16_t>(allowed_type::DOUBLE));

    EXPECT_EQ(GetTypeId<wchar_t>(), static_cast<uint16_t>(allowed_type::WCHAR_T));
    EXPECT_EQ(GetTypeId<char16_t>(), static_cast<uint16_t>(allowed_type::CHAR16_T));
    EXPECT_EQ(GetTypeId<char32_t>(), static_cast<uint16_t>(allowed_type::CHAR32_T));
}

// 测试指针类型的 TypeId
TEST(SerialUtilTest, GetTypePointerId_PointerTypes)
{
    EXPECT_EQ(GetTypePointerId<char>(), static_cast<uint16_t>(allowed_type::CHAR_PTR));
    EXPECT_EQ(GetTypePointerId<short>(), static_cast<uint16_t>(allowed_type::SHORT_PTR));
    EXPECT_EQ(GetTypePointerId<int>(), static_cast<uint16_t>(allowed_type::INT_PTR));
    EXPECT_EQ(GetTypePointerId<long>(), static_cast<uint16_t>(allowed_type::LONG_PTR));
    EXPECT_EQ(GetTypePointerId<long long>(), static_cast<uint16_t>(allowed_type::LONG_LONG_PTR));

    EXPECT_EQ(GetTypePointerId<unsigned char>(), static_cast<uint16_t>(allowed_type::UNSIGNED_CHAR_PTR));
    EXPECT_EQ(GetTypePointerId<unsigned short>(), static_cast<uint16_t>(allowed_type::UNSIGNED_SHORT_PTR));
    EXPECT_EQ(GetTypePointerId<unsigned int>(), static_cast<uint16_t>(allowed_type::UNSIGNED_INT_PTR));
    EXPECT_EQ(GetTypePointerId<unsigned long>(), static_cast<uint16_t>(allowed_type::UNSIGNED_LONG_PTR));
    EXPECT_EQ(GetTypePointerId<unsigned long long>(), static_cast<uint16_t>(allowed_type::UNSIGNED_LONG_LONG_PTR));

    EXPECT_EQ(GetTypePointerId<bool>(), static_cast<uint16_t>(allowed_type::BOOL_PTR));
    EXPECT_EQ(GetTypePointerId<float>(), static_cast<uint16_t>(allowed_type::FLOAT_PTR));
    EXPECT_EQ(GetTypePointerId<double>(), static_cast<uint16_t>(allowed_type::DOUBLE_PTR));

    EXPECT_EQ(GetTypePointerId<wchar_t>(), static_cast<uint16_t>(allowed_type::WCHAR_T_PTR));
    EXPECT_EQ(GetTypePointerId<char16_t>(), static_cast<uint16_t>(allowed_type::CHAR16_T_PTR));
    EXPECT_EQ(GetTypePointerId<char32_t>(), static_cast<uint16_t>(allowed_type::CHAR32_T_PTR));
}

// 测试vector<bool>的序列化反序列化
TEST(SerialUtilTest, vector_bool_serial)
{
    std::vector<bool> vectors{};
    vectors.emplace_back(false);
    vectors.emplace_back(true);
    UbseSerialization ser{ALIGN_BASE::OFFSET_BASE_8};
    ser << vectors;
    EXPECT_TRUE(ser.Check());
    std::vector<bool> vectors1{};
    UbseDeSerialization de_ser{ser.GetBuffer(), ser.GetLength()};
    de_ser >> vectors1;
    EXPECT_TRUE(de_ser.Check());
    EXPECT_EQ(vectors, vectors1);

    std::vector<bool> vectors2{};
    UbseSerialization ser2{std::move(ser)};
    UbseDeSerialization de_ser2{ser2.GetBuffer(), ser2.GetLength()};
    de_ser2 >> vectors2;
    EXPECT_TRUE(de_ser2.Check());
    EXPECT_EQ(vectors, vectors2);
}

TEST(SerialUtilTest, unordered_set_int_serial)
{
    std::unordered_set<int> set_s{1, 2, 3, 4, 5};
    UbseSerialization ser;
    ser << set_s;
    EXPECT_TRUE(ser.Check());

    std::unordered_set<int> set_d;
    UbseDeSerialization deSer(ser.GetBuffer(), ser.GetLength());
    deSer >> set_d;
    EXPECT_TRUE(deSer.Check());
    EXPECT_EQ(set_s.size(), set_d.size());
    for (const auto& val : set_s) {
        EXPECT_TRUE(set_d.count(val) > 0);
    }
}

TEST(SerialUtilTest, unordered_set_uint64_t_serial)
{
    std::unordered_set<uint64_t> set_s{100, 200, 300, 400};
    UbseSerialization ser;
    ser << set_s;
    EXPECT_TRUE(ser.Check());

    std::unordered_set<uint64_t> set_d;
    UbseDeSerialization deSer(ser.GetBuffer(), ser.GetLength());
    deSer >> set_d;
    EXPECT_TRUE(deSer.Check());
    EXPECT_EQ(set_s.size(), set_d.size());
    for (const auto& val : set_s) {
        EXPECT_TRUE(set_d.count(val) > 0);
    }
}

TEST(SerialUtilTest, unordered_set_string_serial)
{
    std::unordered_set<std::string> set_s{"hello", "world", "test"};
    UbseSerialization ser;
    ser << set_s;
    EXPECT_TRUE(ser.Check());

    std::unordered_set<std::string> set_d;
    UbseDeSerialization deSer(ser.GetBuffer(), ser.GetLength());
    deSer >> set_d;
    EXPECT_TRUE(deSer.Check());
    EXPECT_EQ(set_s.size(), set_d.size());
    for (const auto& val : set_s) {
        EXPECT_TRUE(set_d.count(val) > 0);
    }
}

TEST(SerialUtilTest, unordered_set_empty_serial)
{
    std::unordered_set<int> set_s;
    UbseSerialization ser;
    ser << set_s;
    EXPECT_TRUE(ser.Check());

    std::unordered_set<int> set_d;
    set_d.insert(999);
    UbseDeSerialization deSer(ser.GetBuffer(), ser.GetLength());
    deSer >> set_d;
    EXPECT_TRUE(deSer.Check());
    EXPECT_TRUE(set_d.empty());
}

TEST(SerialUtilTest, unordered_set_single_element_serial)
{
    std::unordered_set<int> set_s{42};
    UbseSerialization ser;
    ser << set_s;
    EXPECT_TRUE(ser.Check());

    std::unordered_set<int> set_d;
    UbseDeSerialization deSer(ser.GetBuffer(), ser.GetLength());
    deSer >> set_d;
    EXPECT_TRUE(deSer.Check());
    EXPECT_EQ(set_d.size(), 1);
    EXPECT_TRUE(set_d.count(42) > 0);
}

} // namespace ubse::ut::serial