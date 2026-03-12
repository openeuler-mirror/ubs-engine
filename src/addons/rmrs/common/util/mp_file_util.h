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

#ifndef MP_FILE_UTIL_H
#define MP_FILE_UTIL_H

#include <string>
#include <vector>
#include <mp_error.h>

namespace mempooling {
using std::string;
using std::vector;

class MpFileUtil {
public:
    static MEM_POOLING_RES GetFileInfo(const string &path, vector<string> &info);
};
}

#endif // MP_FILE_UTIL_H
