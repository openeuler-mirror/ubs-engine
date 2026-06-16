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

#ifndef UBSE_PLUGIN_LOADER_H
#define UBSE_PLUGIN_LOADER_H

#include <string>
#include <vector>

namespace ubse::plugin {

class UbsePluginLoader {
public:
    static UbsePluginLoader &GetInstance()
    {
        static UbsePluginLoader instance;
        return instance;
    }

    void DiscoverAndLoad();

    void UnloadAll();

private:
    UbsePluginLoader() = default;

    std::vector<void *> handles_;
};

} // namespace ubse::plugin

#endif // UBSE_PLUGIN_LOADER_H
