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

#ifndef UBSE_REF_H
#define UBSE_REF_H

#include <cstdint>
#include <new>
#include <utility>

namespace ubse::utils {
class Referable {
public:
    Referable() = default;
    virtual ~Referable() = default;

    inline void IncreaseRef()
    {
        __sync_fetch_and_add(&mRefCount_, 1);
    }

    inline void DecreaseRef()
    {
        // delete itself if reference count equal to 0
        if (__sync_sub_and_fetch(&mRefCount_, 1) == 0) {
            delete this;
        }
    }

    inline int32_t GetRef()
    {
        return __sync_fetch_and_add(&mRefCount_, 0);
    }

protected:
    int32_t mRefCount_ = 0;
};

#if __GNUC__ == 4 && __GNUC_MINOR__ == 8 && __GNUC_PATCHLEVEL__ == 5
template <class T, class U = T>
T exchangeHdagger(T &obj, U &&new_value)
{
    T old_value = std::move(obj);
    obj = std::forward<U>(new_value);
    return old_value;
}
#endif

template <typename T>
class Ref {
public:
    // constructor
    Ref() noexcept = default;

    // fix: can't be explicit
    Ref(T *newObj) noexcept
    {
        // if new obj is not null, increase reference count and assign to mObj
        // else nothing need to do as mObj is nullptr by default
        if (newObj != nullptr) {
            newObj->IncreaseRef();
            mObj_ = newObj;
        }
    }

    Ref(const Ref<T> &other) noexcept
    {
        // if other's obj is not null, increase reference count and assign to mObj
        // else nothing need to do as mObj is nullptr by default
        if (other.mObj_ != nullptr) {
            other.mObj_->IncreaseRef();
            mObj_ = other.mObj_;
        }
    }

#if __GNUC__ == 4 && __GNUC_MINOR__ == 8 && __GNUC_PATCHLEVEL__ == 5
    Ref(Ref<T> &&other) noexcept : mObj(exchangeHdagger(other.mObj, nullptr))
#else
    Ref(Ref<T> &&other) noexcept : mObj_(std::__exchange(other.mObj_, nullptr))
#endif
    {
        // move constructor
        // since this mObj is null, just exchange
    }

    // de-constructor
    ~Ref()
    {
        if (mObj_ != nullptr) {
            mObj_->DecreaseRef();
        }
    }

    // operator =
    inline Ref<T> &operator=(T *newObj)
    {
        this->Set(newObj);
        return *this;
    }

    inline Ref<T> &operator=(const Ref<T> &other)
    {
        if (this != &other) {
            this->Set(other.mObj_);
        }
        return *this;
    }

    Ref<T> &operator=(Ref<T> &&other) noexcept
    {
        if (this != &other) {
            auto tmp = mObj_;
#if __GNUC__ == 4 && __GNUC_MINOR__ == 8 && __GNUC_PATCHLEVEL__ == 5
            mObj = exchangeHdagger(other.mObj, nullptr);
#else
            mObj_ = std::__exchange(other.mObj_, nullptr);
#endif
            if (tmp != nullptr) {
                tmp->DecreaseRef();
            }
        }
        return *this;
    }

    // equal operator
    inline bool operator==(const Ref<T> &other) const
    {
        return mObj_ == other.mObj_;
    }

    inline bool operator==(T *other) const
    {
        return mObj_ == other;
    }

    inline bool operator!=(const Ref<T> &other) const
    {
        return mObj_ != other.mObj_;
    }

    inline bool operator!=(T *other) const
    {
        return mObj_ != other;
    }

    // get operator and set
    inline T *operator->() const
    {
        return mObj_;
    }

    inline T *Get() const
    {
        return mObj_;
    }

    inline void Set(T *newObj)
    {
        if (newObj == mObj_) {
            return;
        }

        if (newObj != nullptr) {
            newObj->IncreaseRef();
        }

        if (mObj_ != nullptr) {
            mObj_->DecreaseRef();
        }

        mObj_ = newObj;
    }

private:
    T *mObj_ = nullptr;
};

/*
 * @brief New an object return with ref object
 *
 * @param args             [in] args of object
 *
 * @return Ref object, if new failed internal, an empty Ref object will be returned
 */
template <typename C, typename... ARGS>
static inline Ref<C> MakeRef(ARGS... args)
{
    return new (std::nothrow) C(args...);
}
} // namespace ubse::utils

#endif // UBSE_REF_H
