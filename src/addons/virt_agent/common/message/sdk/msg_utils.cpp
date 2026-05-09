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

#include "msg_utils.h"
#include <securec.h>

namespace vm {

VmResult StringToC(char *dest, const std::string &src, size_t maxSize)
{
    if (dest == nullptr || maxSize == 0) {
        return VM_INVALID_PARAM_ERROR;
    }
    size_t copyLen = std::min(src.length(), maxSize - 1);
    auto ret = memcpy_s(dest, maxSize, src.c_str(), copyLen);
    if (ret != EOK) {
        return VM_ERROR_NOMEM;
    }
    dest[copyLen] = '\0';
    return VM_OK;
}

} // namespace vm