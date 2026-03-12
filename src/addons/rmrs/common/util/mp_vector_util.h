/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MP_VECTOR_UTIL_H
#define MP_VECTOR_UTIL_H

#include <vector>
#include <string>
#include <sstream>

namespace mempooling {
class VectorUtil {
public:
    template <typename T>
    static std::string VectorToStr(const std::vector<T> &vec, const std::string delimiter = ",");
};
template <typename T>
std::string VectorUtil::VectorToStr(const std::vector<T> &vec, const std::string delimiter)
{
    std::ostringstream oss;
    oss << R"([)";
    const size_t count = vec.size();
    for (size_t i = 0; i < count; ++i) {
        try {
            oss << vec[i].ToString();
        } catch(const std::exception& e) {
            oss << delimiter;
        }
    }
    oss << R"(])";
    return oss.str();
}
} // namespace mempooling
#endif // MP_VECTOR_UTIL_H
