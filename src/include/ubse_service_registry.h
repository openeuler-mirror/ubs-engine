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
#ifndef UBSE_SERVICE_REGISTRY_H
#define UBSE_SERVICE_REGISTRY_H

#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <typeindex>
namespace ubse::service {

class UbseIService {
public:
    virtual ~UbseIService() = default;
    virtual std::string GetServiceName() const = 0;
};

class UbseServiceRegistry {
public:
    static UbseServiceRegistry &GetInstance()
    {
        static UbseServiceRegistry instance;
        return instance;
    }

    template <typename T>
    void RegisterService(std::shared_ptr<T> service)
    {
        static_assert(std::is_base_of_v<UbseIService, T>, "T must derive from IService");
        std::unique_lock lock(mutex_);
        std::string name = service->GetServiceName();
        services_[name] = service;
    }

    template <typename T>
    void UnRegisterService(const std::string &name)
    {
        static_assert(std::is_base_of_v<UbseIService, T>, "T must derive from IService");
        std::unique_lock lock(mutex_);
        if (auto it = services_.find(name); it != services_.end()) {
            services_.erase(it);
        }
    }
    template <typename T>
    std::weak_ptr<T> GetService(const std::string &name)
    {
        static_assert(std::is_base_of_v<UbseIService, T>, "T must derive from IService");
        std::shared_lock lock(mutex_);
        auto it = services_.find(name);
        if (it != services_.end()) {
            return std::dynamic_pointer_cast<T>(it->second);
        }
        return std::weak_ptr<T>();
    }
    bool HasService(const std::string &name) const
    {
        std::shared_lock lock(mutex_);
        return services_.find(name) != services_.end();
    }

private:
    std::map<std::string, std::shared_ptr<UbseIService>> services_;
    mutable std::shared_mutex mutex_;
};
} // namespace ubse::service
#endif