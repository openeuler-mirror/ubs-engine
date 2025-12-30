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

#ifndef UBSE_URMA_CONTROLLER_H
#define UBSE_URMA_CONTROLLER_H

#include <vector>
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_urma_def.h"

namespace ubse::urmaController {
using namespace ubse::common::def;
using namespace ubse::urma;

typedef struct {
    std::string vfe0Path;
    std::string vfe1Path;
    std::string bondingPath;
} UbseUrmaDevPath;

typedef struct {
    std::string bondingName;
    std::string fe1Name;
    std::string fe2Name;
    UrmaDevType bondingType;
    UrmaDevState state;
} UbseUrmaInfoForQuery;

class UrmaController {
public:
    static UrmaController &GetInstance()
    {
        static UrmaController instance;
        return instance;
    }

    UbseResult UbseUrmaBandWidthSet(const std::string urmaName, uint32_t minBandWidth, uint32_t maxBandWidth);
    UbseResult UbseUrmaBandWidthGet(const std::string urmaName, uint32_t &minBandWidth, uint32_t &maxBandWidth);
    UbseResult UbseUrmaBandWidthReset(const std::string urmaName);
    void UbseUrmaBandWidthUpdate(const std::string urmaName);
private:
};
} // namespace ubse::urmaController
#endif // UBSE_URMA_CONTROLLER_H