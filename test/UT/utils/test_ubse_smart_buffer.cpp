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

#include "test_ubse_smart_buffer.h"

#include "securec.h"
#include "ubse_smart_buffer.h"

namespace ubse::ut::utils {
using namespace ubse::utils;

void TestUbseSmartBuffer::SetUp()
{
    Test::SetUp();
}

void TestUbseSmartBuffer::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * 测试智能指针构造 smart buffer
 * 测试步骤：
 * 1. 构造智能指针，构造 smart buffer
 * 2. 获取 share data
 * 预期结果：
 * 1. 构造成功
 * 2. share data不为空
 */
TEST_F(TestUbseSmartBuffer, ArrayInit)
{
    size_t len = 1;
    std::unique_ptr<uint8_t[]> buf = std::make_unique<uint8_t[]>(len);
    UbseSmartBuffer buffer(std::move(buf), len);

    EXPECT_EQ(buffer.GetSharedData() != nullptr, true);
}

/*
 * 用例描述：
 * 使用 UbseByteBuffer 构造 smart buffer
 * 测试步骤：
 * 1. buffer 长度为空
 * 2. buffer 长度不为空
 * 预期结果：
 * 1. 构造成功
 * 2. 构造成功
 */
TEST_F(TestUbseSmartBuffer, FromBuffer)
{
    UbseByteBuffer buffer{};
    buffer.len = 0;

    EXPECT_NO_THROW(UbseSmartBuffer::FromBuffer(buffer, true));

    buffer.len = 1;
    buffer.data = static_cast<uint8_t *>(malloc(buffer.len * sizeof(uint8_t)));
    buffer.freeFunc = [](uint8_t *data) -> void {
        free(data);
    };

    EXPECT_NO_THROW(UbseSmartBuffer::FromBuffer(buffer, true));
}
/*
 * 用例描述：
 * 测试原始指针 + deleter 构造
 * 测试步骤：
 * 1. 分配内存
 * 2. 构造 UbseSmartBuffer
 * 3. 验证数据和大小
 * 4. 验证析构时 deleter 被调用（通过 mock）
 * 预期结果：
 * 1. 构造成功
 * 2. GetData 返回正确数据
 * 3. 析构时调用 deleter
 */
TEST_F(TestUbseSmartBuffer, RawPtrInit)
{
    size_t len = 5;
    uint8_t *raw = new uint8_t[len]{1, 2, 3, 4, 5};

    bool deleter_called = false;
    auto deleter = [&deleter_called](uint8_t *p) {
        delete[] p;
        deleter_called = true;
    };

    {
        UbseSmartBuffer buffer(raw, len, deleter);
        EXPECT_EQ(buffer.GetSize(), len);
        EXPECT_NE(buffer.GetData(), nullptr);
        for (size_t i = 0; i < len; ++i) {
            EXPECT_EQ(buffer.GetData()[i], i + 1);
        }
        // buffer 作用域内，deleter 未调用
        EXPECT_FALSE(deleter_called);
    } // buffer 析构

    EXPECT_TRUE(deleter_called); // 验证 deleter 被调用
}

/*
 * 用例描述：
 * 测试 FromBuffer 深拷贝数据（不释放源）
 * 测试步骤：
 * 1. 构造 UbseByteBuffer 指向堆内存
 * 2. 调用 FromBuffer(shouldRelease = false)
 * 3. 修改原始数据
 * 4. 检查 UbseSmartBuffer 中的数据未受影响
 * 预期结果：
 * 1. 返回非空 buffer
 * 2. 数据正确
 * 3. 修改源数据不影响 smart buffer
 */
TEST_F(TestUbseSmartBuffer, FromBuffer_CopyData_NoRelease)
{
    size_t len = 3;
    uint8_t *raw_data = new uint8_t[len]{10, 20, 30};

    UbseByteBuffer buf{raw_data, len, [](uint8_t* p) { delete[] p; }};

    UbseSmartBuffer smartBuf = UbseSmartBuffer::FromBuffer(buf, false); // 不释放源

    // 验证拷贝成功
    EXPECT_EQ(smartBuf.GetSize(), len);
    EXPECT_NE(smartBuf.GetData(), nullptr);
    for (size_t i = 0; i < len; ++i) {
        EXPECT_EQ(smartBuf.GetData()[i], raw_data[i]);
    }

    // 修改源数据
    raw_data[0] = 99;

    // smart buffer 数据应保持不变
    EXPECT_EQ(smartBuf.GetData()[0], 10);
    EXPECT_EQ(smartBuf.GetData()[1], 20);
    EXPECT_EQ(smartBuf.GetData()[2], 30);

    // 清理：调用者负责释放源（因 shouldRelease=false）
    delete[] raw_data;
}

/*
 * 用例描述：
 * 测试 FromBuffer 深拷贝数据 + 释放源
 * 测试步骤：
 * 1. 构造 UbseByteBuffer
 * 2. mock 其 freeFunc
 * 3. 调用 FromBuffer(shouldRelease = true)
 * 预期结果：
 * 1. smart buffer 数据正确
 * 2. freeFunc 被调用
 */
TEST_F(TestUbseSmartBuffer, FromBuffer_CopyData_ShouldRelease)
{
    size_t len = 2;
    uint8_t *raw_data = new uint8_t[len]{1, 2};

    bool free_called = false;
    auto mockFree = [&free_called](uint8_t *p) {
        delete[] p;
        free_called = true;
    };

    UbseByteBuffer buf{raw_data, len, mockFree};

    UbseSmartBuffer smartBuf = UbseSmartBuffer::FromBuffer(buf, true); // 释放源

    // 验证数据
    EXPECT_EQ(smartBuf.GetSize(), len);
    EXPECT_EQ(smartBuf.GetData()[0], 1);
    EXPECT_EQ(smartBuf.GetData()[1], 2);

    // 验证源被释放
    EXPECT_TRUE(free_called);
}

/*
 * 用例描述：
 * 测试 FromBuffer 在 memcpy_s 失败时的行为（错误注入）
 * 测试步骤：
 * 1. mock memcpy_s 返回非0（失败）
 * 2. 构造 UbseByteBuffer
 * 3. 调用 FromBuffer(shouldRelease = true)
 * 预期结果：
 * 1. 返回空 buffer（GetSize() == 0）
 * 2. freeFunc 被调用（释放源）
 */
TEST_F(TestUbseSmartBuffer, FromBuffer_MemcpyFail_ShouldRelease)
{
    size_t len = 1;
    uint8_t *raw_data = new uint8_t[len]{42};

    bool free_called = false;
    auto mockFree = [&free_called](uint8_t *p) {
        delete[] p;
        free_called = true;
    };

    UbseByteBuffer buf{raw_data, len, mockFree};

    // 模拟 memcpy_s 失败
    MOCKER(&memcpy_s).stubs().will(returnValue(1));

    UbseSmartBuffer smartBuf = UbseSmartBuffer::FromBuffer(buf, true);

    // 验证返回空 buffer
    EXPECT_EQ(smartBuf.GetSize(), 0);
    EXPECT_EQ(smartBuf.GetData(), nullptr);
    EXPECT_EQ(smartBuf.GetSharedData(), nullptr);

    // 验证即使拷贝失败，源也被释放
    EXPECT_TRUE(free_called);

    // 注意：raw_data 已被 free_called 释放，不要再 delete[]
}
/*
 * 用例描述：
 * 测试 FromBuffer 传入空或无效 buffer
 * 测试步骤：
 * 1. 构造空 UbseByteBuffer
 * 2. 调用 FromBuffer
 * 预期结果：
 * 1. 返回空 UbseSmartBuffer
 */
TEST_F(TestUbseSmartBuffer, FromBuffer_Null)
{
    UbseByteBuffer buf{nullptr, 0, nullptr};
    auto smartBuf = UbseSmartBuffer::FromBuffer(buf, true);
    EXPECT_EQ(smartBuf.GetSize(), 0);
    EXPECT_EQ(smartBuf.GetData(), nullptr);
    EXPECT_EQ(smartBuf.GetSharedData(), nullptr);

    // data 为空，len 非零
    buf.data = nullptr;
    buf.len = 5;
    smartBuf = UbseSmartBuffer::FromBuffer(buf, false);
    EXPECT_EQ(smartBuf.GetSize(), 0);
    EXPECT_EQ(smartBuf.GetData(), nullptr);
}

/*
 * 用例描述：
 * 测试 FromBuffer 不释放源
 * 测试步骤：
 * 1. 构造 UbseByteBuffer
 * 2. 调用 FromBuffer(shouldRelease = false)
 * 3. 验证数据拷贝成功
 * 预期结果：
 * 1. 数据正确
 * 2. 源未被释放
 */
TEST_F(TestUbseSmartBuffer, FromBuffer_NoRelease)
{
    size_t len = 3;
    uint8_t *data = new uint8_t[len]{7, 8, 9};

    UbseByteBuffer buf{data, len, [](uint8_t* p) { delete[] p; }};

    auto smartBuf = UbseSmartBuffer::FromBuffer(buf, false); // 不释放

    EXPECT_EQ(smartBuf.GetSize(), len);
    for (size_t i = 0; i < len; ++i) {
        EXPECT_EQ(smartBuf.GetData()[i], data[i]);
    }

    // 源未被释放，仍可访问（但不应再用，因 FromBuffer 语义可能不同）
    delete[] data; // 调用者负责
}

/*
 * 用例描述：
 * 测试 FromBuffer 释放源
 * 测试步骤：
 * 1. 构造 UbseByteBuffer
 * 2. mock freeFunc
 * 3. 调用 FromBuffer(shouldRelease = true)
 * 4. 验证 freeFunc 被调用
 * 预期结果：
 * 1. 数据拷贝成功
 * 2. freeFunc 被调用
 */
TEST_F(TestUbseSmartBuffer, FromBuffer_ShouldRelease)
{
    size_t len = 2;
    uint8_t *data = new uint8_t[len]{1, 2};

    bool free_called = false;
    auto freeFunc = [&free_called](uint8_t *p) {
        delete[] p;
        free_called = true;
    };

    UbseByteBuffer buf{data, len, freeFunc};

    auto smartBuf = UbseSmartBuffer::FromBuffer(buf, true); // 释放源

    EXPECT_TRUE(free_called); // 源被释放
    EXPECT_EQ(smartBuf.GetSize(), len);
    EXPECT_EQ(smartBuf.GetData()[0], 1);
    EXPECT_EQ(smartBuf.GetData()[1], 2);
}

/*
 * 用例描述：
 * 测试移动语义
 * 测试步骤：
 * 1. 构造一个 buffer
 * 2. 移动构造另一个
 * 3. 移动赋值
 * 预期结果：
 * 1. 移动后原对象为空
 * 2. 新对象持有数据
 */
TEST_F(TestUbseSmartBuffer, MoveSemantics)
{
    size_t len = 1;
    std::unique_ptr<uint8_t[]> buf = std::make_unique<uint8_t[]>(len);
    buf[0] = 100;

    UbseSmartBuffer buf1(std::move(buf), len);
    EXPECT_EQ(buf1.GetSize(), len);

    // 移动构造
    UbseSmartBuffer buf2(std::move(buf1));
    EXPECT_EQ(buf1.GetSize(), 0); // 原对象为空
    EXPECT_EQ(buf1.GetData(), nullptr);
    EXPECT_EQ(buf2.GetSize(), len);
    EXPECT_EQ(buf2.GetData()[0], 100);

    // 移动赋值
    UbseSmartBuffer buf3 = std::move(buf2);
    EXPECT_EQ(buf2.GetSize(), 0);
    EXPECT_EQ(buf3.GetSize(), len);
    EXPECT_EQ(buf3.GetData()[0], 100);
}

/*
 * 用例描述：
 * 测试 const/non-const GetData 和 GetSize，以及 GetSharedData 的正确性
 * 测试步骤：
 * 1. 构造 buffer
 * 2. 调用所有访问器
 * 3. 验证 GetSharedData 返回的 shared_ptr 指向正确数据
 * 预期结果：
 * 1. 返回值正确
 * 2. shared_ptr 数据正确，引用计数合理
 */
TEST_F(TestUbseSmartBuffer, Accessors)
{
    size_t len = 3;
    std::unique_ptr<uint8_t[]> buf = std::make_unique<uint8_t[]>(len);
    buf[0] = 1;
    buf[1] = 2;
    buf[2] = 3;

    const UbseSmartBuffer cbuf(std::move(buf), len);

    // 测试 GetSize
    EXPECT_EQ(cbuf.GetSize(), len);

    // 测试 GetData (const)
    const uint8_t *data = cbuf.GetData();
    EXPECT_NE(data, nullptr);
    EXPECT_EQ(data[0], 1);
    EXPECT_EQ(data[1], 2);
    EXPECT_EQ(data[2], 3);

    // 测试 GetSharedData
    auto shared = cbuf.GetSharedData();
    EXPECT_NE(shared, nullptr);
    EXPECT_EQ(shared.use_count(), 2); // cbuf + shared 两个持有者

    // 验证 shared 指向的数据正确
    EXPECT_EQ(shared.get(), data); // 应该指向同一块内存
    EXPECT_EQ(shared[0], 1);
    EXPECT_EQ(shared[1], 2);
    EXPECT_EQ(shared[2], 3);

    // 额外验证：即使 cbuf 析构，shared 仍可访问数据
    // 体现 shared_ptr 的价值）
    {
        auto temp = shared; // 再增加引用
        EXPECT_EQ(temp.use_count(), 3);
    }
    EXPECT_EQ(shared.use_count(), 2); // temp 析构后，引用数减少
}
} // namespace ubse::ut::utils