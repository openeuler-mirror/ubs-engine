/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef BASE_MESSAGE_H
#define BASE_MESSAGE_H

#include <cstdint>
#include <vector>
#include <string>
#include <securec.h>

#include "pointer_process.h"
#include "vm_error.h"
#include "vm_ref.h"

namespace vm {

class BaseMessage : public Referable {
public:
    ~BaseMessage() override
    {
        if (mInputRawDataOwned) {
            SafeDeleteArray(mInputRawData);
        }
        if (mOutputRawDataOwned) {
            SafeDeleteArray(mOutputRawData);
        }
    }

    VmResult SetInputRawData(uint8_t *rawData, uint32_t size, bool copy = false);

    inline uint8_t *SerializedData() const
    {
        return mOutputRawData;
    }

    inline uint32_t SerializedDataSize() const
    {
        return mOutputRawDataSize;
    }

    inline uint8_t *InputRawData() const
    {
        return mInputRawData;
    }

    inline uint32_t InputRawDataSize() const
    {
        return mInputRawDataSize;
    }

    virtual VmResult Serialize() = 0;

    virtual VmResult Deserialize() = 0;

    inline VmResult CheckMsgBody() const
    {
        return mErrCode;
    }

    virtual std::string ToString() const
    {
        return "";
    }

    template <class Child> static Ref<BaseMessage> Convert(const Ref<Child> &child)
    {
        if (child.Get() != nullptr) {
            return Ref<BaseMessage>(static_cast<BaseMessage *>(child.Get()));
        }
        return gNullPtr;
    }

    template <class Child> static Ref<BaseMessage> Convert(const Child &child)
    {
        if (child.Get() != nullptr) {
            return Ref<BaseMessage>(static_cast<BaseMessage *>(child.Get()));
        }
        return gNullPtr;
    }

    template <class Parent> static Ref<Parent> DeConvert(const Ref<BaseMessage> &child)
    {
        if (child.Get() != nullptr) {
            return Ref<Parent>(static_cast<Parent *>(child.Get()));
        }
        return Ref<Parent>(nullptr); // Return a Ref smart pointer to an object of type Parent.
    }

public:
    static Ref<BaseMessage> gNullPtr;

    void SetErrCode(uint32_t errCode)
    {
        mErrCode = errCode;
    }

    uint32_t GetErrCode() const
    {
        return mErrCode;
    }

    void SetOutputRawDataOwned(bool owned)
    {
        mOutputRawDataOwned = owned;
    }

protected:
    uint8_t *mInputRawData = nullptr;
    uint32_t mInputRawDataSize = 0;
    bool mInputRawDataOwned = false;
    bool mOutputRawDataOwned = false;
    uint8_t *mOutputRawData = nullptr;
    uint32_t mOutputRawDataSize = 0;
    uint32_t mErrCode = 0;
};

using BaseMessagePtr = Ref<BaseMessage>;
}

#endif // BASE_MESSAGE_H
