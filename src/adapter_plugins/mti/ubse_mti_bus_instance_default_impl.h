/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_MTI_BUS_INSTANCE_DEFAULT_IMPL_H
#define UBSE_MTI_BUS_INSTANCE_DEFAULT_IMPL_H
#include "adapter_plugins/mti/ubse_mti_bus_instance.h"
namespace ubse::mti::bus_instance {
class UbseMtiBusInstanceDefaultImpl : public UbseMtiBusInstance {
    UbseResult GetBusInstanceList(std::vector<UbseMtiBusInst> &busInstanceList);

    UbseResult CreateVmBusInstance(uint16_t upi, UbseMtiBusInst &busInstance);

    UbseResult DestroyVmBusInstance(const UbseMtiBusInst &busInstance);
};
} // namespace ubse::mti::bus_instance
#endif // UBSE_MTI_BUS_INSTANCE_DEFAULT_IMPL_H
