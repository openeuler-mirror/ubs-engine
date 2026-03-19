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

#include "vm_vector_util.h"

#include <algorithm>

namespace vm {
void VectorUtil::RemoveCommonElements(std::vector<uint16_t> &sourceVector, std::vector<uint16_t> &elementsToRemove)
{
    sourceVector.erase(std::remove_if(sourceVector.begin(), sourceVector.end(),
        [&elementsToRemove](uint16_t elem) {
            return std::find(elementsToRemove.begin(), elementsToRemove.end(), elem) != elementsToRemove.end();
        }),
        sourceVector.end());
}
}
