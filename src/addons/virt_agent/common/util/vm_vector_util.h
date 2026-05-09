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

#ifndef VM_VECTOR_UTIL_H
#define VM_VECTOR_UTIL_H

#include <sstream>
#include <string>
#include <vector>

namespace vm {
class VectorUtil {
public:
    template <typename T>
    static std::string VectorToString(const std::vector<T> &vec, const std::string &delimiter = ", ")
    {
        if (vec.empty()) {
            return "";
        }

        std::ostringstream oss;
        for (size_t i = 0; i < vec.size(); ++i) {
            oss << vec[i];
            if (i != vec.size() - 1) {
                oss << delimiter;
            }
        }

        return oss.str();
    }

    static void RemoveCommonElements(std::vector<uint16_t> &sourceVector, std::vector<uint16_t> &elementsToRemove);
};
} // namespace vm
#endif // VM_VECTOR_UTIL_H
