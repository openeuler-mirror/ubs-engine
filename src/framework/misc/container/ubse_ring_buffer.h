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

#ifndef UBSE_DG_RING_BUFFER_QUEUE_H
#define UBSE_DG_RING_BUFFER_QUEUE_H

#include <semaphore.h>
#include <sstream>

#include "../lock/ubse_lock.h"

namespace ubse::utils {
/*
 * @brief A ring buffer, guarded by spin lock, allow flex capacity
 */
template <typename T>
class RingBuffer {
public:
    explicit RingBuffer(uint32_t capacity) : mCapacity_(capacity) {}

    RingBuffer() = delete;

    ~RingBuffer()
    {
        UnInitialize();
    }

    /*
     * @brief Set capacity, which should be called before initialize
     *
     * @param capacity         [in] capacity number
     */
    inline void Capacity(uint32_t capacity)
    {
        if (mRingBuf_ == nullptr) {
            mCapacity_ = capacity;
        }
    }

    /*
     * @brief Get capacity
     *
     * @return capacity
     */
    inline uint32_t Capacity() const
    {
        return mCapacity_;
    }

    /*
     * @brief Initialize ring buffer
     *
     * @return 0 if successful, -1 in two cases:
     * a) capacity is not valid, i.e. 0
     * b) failed to malloc bucket for the ring
     */
    int32_t Initialize()
    {
        if (mCapacity_ == 0) {
            return -1;
        }

        mCount_ = 0;
        mHead_ = 0;
        mTail_ = 0;

        if (mRingBuf_ != nullptr) {
            return 0;
        }

        mRingBuf_ = new (std::nothrow) T[mCapacity_];
        if (mRingBuf_ == nullptr) {
            return -1;
        }

        return 0;
    }

    /*
     * @brief UnInitialize the ring buffer
     */
    inline void UnInitialize()
    {
        if (mRingBuf_ == nullptr) {
            return;
        }

        delete[] mRingBuf_;
        mRingBuf_ = nullptr;
    }

    /*
     * @brief Push back an item into ring buffer, must be initialized firstly
     *
     * @param item             [in] item to push back
     *
     * @return true if successful, false if the ring buffer is full
     */
    bool PushBack(const T &item)
    {
        mLock_.Lock();
        if (mCapacity_ <= mCount_) {
            mLock_.UnLock();
            return false;
        }

        mRingBuf_[mTail_] = item;
        if (mTail_ != mCapacity_ - 1) {
            ++mTail_;
        } else {
            mTail_ = 0;
        }
        ++mCount_;
        mLock_.UnLock();
        return true;
    }

    /*
     * @brief Push front an item into ring buffer, must be initialized firstly
     *
     * @param item             [in] item to push front
     *
     * @return true if successful, false if the ring buffer is full
     */
    bool PushFront(const T &item)
    {
        mLock_.Lock();
        if (mCapacity_ <= mCount_) {
            mLock_.UnLock();
            return false;
        }

        /* move to tail */
        if (mHead_ == 0) {
            mHead_ = mCapacity_ - 1;
        } else {
            mHead_--;
        }

        mRingBuf_[mHead_] = item;
        ++mCount_;

        mLock_.UnLock();
        return true;
    }

    /*
     * @brief Pop an item from front of ring buffer, must be initialized firstly
     *
     * @param item             [out] item popped in front
     *
     * @return true if successful, false if the ring buffer is empty
     */
    bool PopFront(T &item)
    {
        mLock_.Lock();
        if (mCount_ == 0) {
            mLock_.UnLock();
            return false;
        }

        item = mRingBuf_[mHead_];
        if (mHead_ != mCapacity_ - 1) {
            ++mHead_;
        } else {
            mHead_ = 0;
        }
        --mCount_;
        mLock_.UnLock();
        return true;
    }

    /*
     * @brief Get size of ring buffer
     *
     * @return size of item in ring buffer
     */
    inline uint32_t Size()
    {
        mLock_.Lock();
        auto temp = mCount_;
        mLock_.UnLock();
        return temp;
    }

    /*
     * @brief Brief ring buffer info to string
     */
    inline std::string ToString()
    {
        std::ostringstream oss;
        oss << "head " << mHead_ << ", tail " << mTail_ << ", capacity " << mCapacity_ << ", count " << mCount_;
        return oss.str();
    }

    RingBuffer(const RingBuffer &) = delete;
    RingBuffer(RingBuffer &&) = delete;
    RingBuffer &operator=(const RingBuffer &) = delete;
    RingBuffer &operator=(RingBuffer &&) = delete;

private:
    T *mRingBuf_ = nullptr;
    SpinLock mLock_;
    uint32_t mCapacity_ = 0;
    uint32_t mCount_ = 0;
    uint32_t mHead_ = 0;
    uint32_t mTail_ = 0;
};

/*
 * @brief A blocking queue on top of ring buffer
 */
template <typename T>
class RingBufferBlockingQueue {
public:
    explicit RingBufferBlockingQueue(uint32_t capacity) : mRingBuffer_(capacity) {}

    ~RingBufferBlockingQueue()
    {
        UnInitialize();
    }

    /*
     * @brief Initialize the queue
     *
     * @return 0 if successful, -1 if sem_init failed or inner ring buffer initialization failed
     */
    inline int Initialize()
    {
        if (sem_init(&mSem_, 0, 0) != 0) {
            return -1;
        }

        return mRingBuffer_.Initialize();
    }

    /*
     * @brief UnInitialized
     */
    inline void UnInitialize()
    {
        mRingBuffer_.UnInitialize();
        sem_destroy(&mSem_);
    }

    /*
     * @brief Enqueue an item into queue and notify to waiters
     *
     * @param item             [in] item to be enqueued
     *
     * @return true if successful, false if queue is full
     */
    inline bool Enqueue(const T &item)
    {
        auto result = mRingBuffer_.PushBack(item);
        if (result) {
            sem_post(&mSem_);
        }
        return result;
    }

    /*
     * @brief Enqueue an item into queue and notify to waiters
     *
     * @param item             [in] item to be enqueued
     *
     * @return true if successful, false if queue is full
     */
    inline bool Enqueue(T &item)
    {
        auto result = mRingBuffer_.PushBack(item);
        if (result) {
            sem_post(&mSem_);
        }
        return result;
    }

    /*
     * @brief Enqueue an item into queue in the front and notify to waiters
     *
     * @param item             [in] item to be enqueued
     *
     * @return true if successful, false if queue is full
     */
    inline bool EnqueueFirst(T &item)
    {
        auto result = mRingBuffer_.PushFront(item);
        if (result) {
            sem_post(&mSem_);
        }
        return result;
    }

    /*
     * @brief Enqueue an item into queue in the front and notify to waiters
     *
     * @param item             [in] item to be enqueued
     *
     * @return true if successful, false if queue is full
     */
    inline bool EnqueueFirst(const T &item)
    {
        auto result = mRingBuffer_.PushFront(item);
        if (result) {
            sem_post(&mSem_);
        }
        return result;
    }

    /*
     * @brief Dequeue an item from queue in the front, wait if no item
     *
     * @param item             [in] item to be enqueued
     *
     * @return true if successful, false if queue is empty
     */
    inline bool Dequeue(T &item)
    {
        while (true) {
            auto result = mRingBuffer_.PopFront(item);
            if (!result) {
                sem_wait(&mSem_);
            } else {
                return result;
            }
        }
    }

private:
    RingBuffer<T> mRingBuffer_; /* ring buffer to data store */
    sem_t mSem_{};              /* semaphore to wait and notify */
};
} // namespace ubse::utils

#endif // UBSE_DG_RING_BUFFER_QUEUE_H
