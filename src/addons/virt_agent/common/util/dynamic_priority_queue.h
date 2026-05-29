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

#ifndef VM_DYNAMIC_PRIORITY_QUEUE_H
#define VM_DYNAMIC_PRIORITY_QUEUE_H

#include <algorithm>
#include <vector>

namespace vm {
template <typename T>
class DynamicPriorityQueue {
public:
    void Push(const T& t)
    {
        elements.push_back(t);
        std::push_heap(elements.begin(), elements.end());
    }

    T Top()
    {
        return elements.front();
    }

    void Pop()
    {
        std::pop_heap(elements.begin(), elements.end());
        elements.pop_back();
    }

    void Update(T& t)
    {
        auto it = std::find_if(elements.begin(), elements.end(), [t](const T& item) { return item == t; });
        if (it == elements.end()) {
            return;
        }
        T oldItem = *it;
        *it = t;
        if (oldItem < t) {
            std::push_heap(elements.begin(), it + 1);
        } else if (oldItem > t) {
            std::make_heap(elements.begin(), elements.end());
        }
    }

    bool Empty() const
    {
        return elements.empty();
    }

    size_t Size() const
    {
        return elements.size();
    }

private:
    std::vector<T> elements;
};
} // namespace vm
#endif // VM_DYNAMIC_PRIORITY_QUEUE_H
