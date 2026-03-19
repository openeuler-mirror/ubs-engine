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

#ifndef FRAGMENTATION_VM_COLLECTION_H
#define FRAGMENTATION_VM_COLLECTION_H

#include <mutex>

#include "vm_error.h"
namespace vm {

class FragmentationVmCollection {
public:
    static FragmentationVmCollection &GetInstance();
    VmResult FragInit();
    void FragTerminate();
    /**
     * Get information about all VMs
     * @return uint32_t, 0 indicates success, and any non-zero value indicates fail or other exception
     */
    static uint32_t FragGetLocalHostVmCollectData();
private:
    static std::mutex timerTaskMutex;
    static std::string timerName;
};
}

#endif // FRAGMENTATION_VM_COLLECTION_H
