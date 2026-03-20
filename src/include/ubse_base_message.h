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

#ifndef UBSE_MANAGER_UBSE_BASE_MESSAGE_H
#define UBSE_MANAGER_UBSE_BASE_MESSAGE_H
#include <referable/ubse_ref.h>
#include <cstdint>
#include <memory>
#include <string>

#include "ubse_common_def.h"

namespace ubse::message {
using namespace ubse::utils;
using namespace ubse::common::def;

class UbseBaseMessage : public Referable {
public:
    ~UbseBaseMessage() override = default;

    UbseResult SetInputRawDataFromShared(std::shared_ptr<uint8_t[]> data, uint32_t size);
    UbseResult SetInputRawData(uint8_t *rawData, uint32_t size, bool copy = false);

    inline uint8_t *SerializedData() const
    {
        return mOutputRawData ? mOutputRawData.get() : nullptr;
    }

    inline uint32_t SerializedDataSize() const
    {
        return mOutputRawDataSize;
    }

    inline uint8_t *InputRawData() const
    {
        return mInputRawData ? mInputRawData.get() : nullptr;
    }

    inline uint32_t InputRawDataSize() const
    {
        return mInputRawDataSize;
    }

    virtual UbseResult Serialize() = 0;

    virtual UbseResult Deserialize() = 0;

    inline UbseResult CheckMsgBody() const
    {
        return mErrCode;
    }

    // 输出消息
    virtual std::string ToString() const
    {
        return "";
    }

    template <class Child>
    static Ref<UbseBaseMessage> Convert(const Ref<Child> &child)
    {
        if (child.Get() != nullptr) {
            return Ref<UbseBaseMessage>(static_cast<UbseBaseMessage *>(child.Get()));
        }
        return gNullPtr;
    }

    template <class Child>
    static Ref<UbseBaseMessage> Convert(const Child &child)
    {
        if (child.Get() != nullptr) {
            return Ref<UbseBaseMessage>(static_cast<UbseBaseMessage *>(child.Get()));
        }
        return gNullPtr;
    }

    template <class Parent>
    static Ref<Parent> DeConvert(const Ref<UbseBaseMessage> &child)
    {
        if (child.Get() != nullptr) {
            return Ref<Parent>(static_cast<Parent *>(child.Get()));
        }
        return Ref<Parent>(nullptr);
    }

    // 获取共享所有权版本
    std::shared_ptr<uint8_t[]> GetSharedOutputData()
    {
        if (!mOutputRawData)
            return nullptr;
        return {mOutputRawData.release(), std::default_delete<uint8_t[]>()};
    }

public:
    static Ref<UbseBaseMessage> gNullPtr;

    void SetErrCode(uint32_t errCode)
    {
        mErrCode = errCode;
    }

    uint32_t GetErrCode() const
    {
        return mErrCode;
    }

protected:
    std::shared_ptr<uint8_t[]> mInputRawData;
    std::unique_ptr<uint8_t[]> mOutputRawData;
    uint32_t mInputRawDataSize = 0;
    uint32_t mOutputRawDataSize = 0;
    uint32_t mErrCode = 0;
};

using UbseBaseMessagePtr = Ref<UbseBaseMessage>;
} // namespace ubse::message
#endif // UBSE_MANAGER_UBSE_BASE_MESSAGE_H
