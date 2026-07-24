/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND.
 */

#ifndef UBSE_URMA_RESOURCE_VIEW_H
#define UBSE_URMA_RESOURCE_VIEW_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "ubse_common_def.h"
#include "ubse_urma_def.h"

namespace ubse::urmaController {
using ubse::common::def::UbseResult;
using ubse::urma::UbseUrmaDevBrief;
using ubse::urma::UbseUrmaInfo;

struct UrmaLogicalGroup {
    std::string backingName;
    UbseUrmaInfo backingInfo{};
    bool isHostBonding{false};
    std::vector<std::string> logicalNames;
};

struct UrmaLogicalProjection {
    size_t logicalDeviceCount{0};
    std::vector<UrmaLogicalGroup> groups;
};

class UrmaAllocTarget {
public:
    UrmaAllocTarget() = default;

    const std::string& GetBackingName() const
    {
        return backingName;
    }

    bool IsHostBonding() const
    {
        return isHostBonding;
    }

private:
    friend class UbseUrmaResourceView;
    std::string backingName;
    bool isHostBonding{false};
};

class UbseUrmaResourceView {
public:
    static UbseUrmaResourceView& GetInstance()
    {
        static UbseUrmaResourceView instance;
        return instance;
    }

    UbseResult BuildLogicalProjection(const std::vector<std::string>& filter, UrmaLogicalProjection& projection) const;
    UbseResult GetDeviceSummaries(std::vector<std::string>& names, std::vector<uint32_t>& states,
                                  std::vector<uint64_t>& hwResIds) const;
    UbseResult GetDeviceDetails(const std::vector<std::string>& filter, std::vector<UbseUrmaDevBrief>& devInfos) const;
    UbseResult ResolveAllocTarget(const std::string& logicalName, UrmaAllocTarget& target) const;
    bool IsBackingCreated(const UbseUrmaInfo& info) const;

private:
    UbseUrmaResourceView() = default;

    struct CurrentBacking {
        UbseUrmaInfo info{};
        bool isHostBonding{false};
    };

    struct CurrentBackingCompare {
        bool operator()(const std::string& lhs, const std::string& rhs) const
        {
            const bool lhsIsHost = lhs == ubse::common::def::UBSE_HOST_URMA_DEV_NAME;
            const bool rhsIsHost = rhs == ubse::common::def::UBSE_HOST_URMA_DEV_NAME;
            if (lhsIsHost != rhsIsHost) {
                return lhsIsHost;
            }
            return ubse::urma::UrmaNameCompare{}(lhs, rhs);
        }
    };

    using CurrentBackings = std::map<std::string, CurrentBacking, CurrentBackingCompare>;
    using LogicalBackingMap = std::map<std::string, std::string, ubse::urma::UrmaNameCompare>;

    UbseResult CollectCurrentBackings(CurrentBackings& backings) const;
    UbseResult CollectAllocBackings(CurrentBackings& backings) const;
    UbseResult ValidateProjectionBackings(const CurrentBackings& backings) const;
    UbseResult BuildProjectionGroups(const CurrentBackings& backings, const std::vector<std::string>& filter,
                                     UrmaLogicalProjection& projection) const;
    UbseResult BuildGroupedSummaries(std::vector<std::string>& names, std::vector<uint32_t>& states,
                                     std::vector<uint64_t>& hwResIds) const;
    UbseResult BuildDirectSummaries(std::vector<std::string>& names, std::vector<uint32_t>& states,
                                    std::vector<uint64_t>& hwResIds) const;
    UbseResult BuildGroupedDetails(const std::vector<std::string>& filter,
                                   std::vector<UbseUrmaDevBrief>& devInfos) const;
    UbseResult BuildDirectDetails(const std::vector<std::string>& filter,
                                  std::vector<UbseUrmaDevBrief>& devInfos) const;
    UbseResult ResolveGroupedAllocTarget(const std::string& logicalName, UrmaAllocTarget& target) const;
    UbseResult ValidateAllocTarget(const std::string& targetName, const CurrentBacking& target) const;
    UbseResult RefreshLogicalBackingMapLocked(const CurrentBackings& backings) const;

    mutable std::mutex mappingMutex;
    mutable std::vector<std::string> mappedBackingNames;
    mutable LogicalBackingMap logicalBackingMap;
};
} // namespace ubse::urmaController

#endif // UBSE_URMA_RESOURCE_VIEW_H
