/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "ubse_mem_controller_ledger_filter.h"

#include <unordered_set>

#include "ubse_logger_module.h"

namespace ubse::mem::controller {
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_CONTROLLER_MID)

std::string TransState(UbseMemState state)
{
    switch (state) {
        case UbseMemState::UBSE_MEM_SCHEDULING:
            return "scheduling";
        case UbseMemState::UBSE_MEM_EXPORT_RUNNING:
            return "export_running";
        case UbseMemState::UBSE_MEM_EXPORT_SUCCESS:
            return "export_success";
        case UbseMemState::UBSE_MEM_EXPORT_DESTROYING:
            return "export_destroying";
        case UbseMemState::UBSE_MEM_EXPORT_DESTROYED:
            return "export_destroyed";
        case UbseMemState::UBSE_MEM_EXPORT_DESTROYING_WAIT:
            return "export_destroying_wait";
        case UbseMemState::UBSE_MEM_IMPORT_RUNNING:
            return "import_running";
        case UbseMemState::UBSE_MEM_IMPORT_SUCCESS:
            return "import_success";
        case UbseMemState::UBSE_MEM_IMPORT_DESTROYING:
            return "import_destroying";
        case UbseMemState::UBSE_MEM_IMPORT_DESTROYED:
            return "import_destroyed";
        case UbseMemState::UBSE_MEM_IMPORT_DESTROYING_WAIT:
            return "import_destroying_wait";
        default:
            return "unknown_state";
    }
}

void PrintFdExportObj(std::string module, std::vector<UbseMemFdBorrowExportObj> exportObjs)
{
    for (auto obj : exportObjs) {
        if (obj.algoResult.exportNumaInfos.empty()) {
            continue;
        }
        UBSE_LOG_INFO << module << ", node= " << obj.algoResult.exportNumaInfos[0].nodeId
                    << ", fd export name=" << obj.req.name << ", state=" << TransState(obj.status.state)
                    << ", origin state=" << static_cast<uint32_t>(obj.status.state);
    }
}

void PrintFdImportObj(std::string module, std::vector<UbseMemFdBorrowImportObj> importObjs)
{
    for (auto obj : importObjs) {
        if (obj.algoResult.importNumaInfos.empty()) {
            continue;
        }
        UBSE_LOG_INFO << module << ", node= " << obj.algoResult.importNumaInfos[0].nodeId
                    << ", fd import name=" << obj.req.name << ", state=" << TransState(obj.status.state)
                    << ", origin state=" << static_cast<uint32_t>(obj.status.state);
    }
}

void PrintNumaExportObj(std::string module, std::vector<UbseMemNumaBorrowExportObj> exportObjs)
{
    for (auto obj : exportObjs) {
        if (obj.algoResult.exportNumaInfos.empty()) {
            continue;
        }
        UBSE_LOG_INFO << module << ", node= " << obj.algoResult.exportNumaInfos[0].nodeId
                    << ", numa export name=" << obj.req.name << ", state=" << TransState(obj.status.state)
                    << ", origin state=" << static_cast<uint32_t>(obj.status.state);
    }
}

void PrintNumaImportObj(std::string module, std::vector<UbseMemNumaBorrowImportObj> importObjs)
{
    for (auto obj : importObjs) {
        if (obj.algoResult.importNumaInfos.empty()) {
            continue;
        }
        UBSE_LOG_INFO << module << ", node= " << obj.algoResult.importNumaInfos[0].nodeId
                    << ", numa import name=" << obj.req.name << ", state=" << TransState(obj.status.state)
                    << ", origin state=" << static_cast<uint32_t>(obj.status.state);
    }
}

void FilterRunningFdExport(std::vector<UbseMemFdBorrowExportObj> masterExportObjs,
                           std::vector<UbseMemFdBorrowExportObj> agentExportObjs,
                           std::vector<UbseMemFdBorrowExportObj> &masterRunningExportObjs,
                           std::vector<UbseMemFdBorrowExportObj> &masterFilterRunningExportObjs,
                           std::vector<UbseMemFdBorrowExportObj> &agentFilterRunningExportObjs)
{
    PrintFdExportObj("print master all fd export obj", masterExportObjs);
    PrintFdExportObj("print agent all fd export obj", agentExportObjs);

    std::unordered_set<std::string> masterRunningObj{};
    for (auto obj : masterExportObjs) {
        if (obj.status.state == UbseMemState::UBSE_MEM_SCHEDULING ||
            obj.status.state == UbseMemState::UBSE_MEM_EXPORT_RUNNING ||
            obj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYING ||
            obj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYING_WAIT) {
            masterRunningObj.insert(obj.req.name);
            masterRunningExportObjs.push_back(obj);
        }
    }

    PrintFdExportObj("print master running fd export obj", masterRunningExportObjs);

    auto findDiff = [](const std::vector<UbseMemFdBorrowExportObj> &source,
                       const std::unordered_set<std::string> &runningSet) {
        std::vector<UbseMemFdBorrowExportObj> result;
        for (const auto &elem : source) {
            if (runningSet.find(elem.req.name) == runningSet.end()) {
                result.push_back(elem);
            }
        }
        return result;
    };
    masterFilterRunningExportObjs = findDiff(masterExportObjs, masterRunningObj);
    agentFilterRunningExportObjs = findDiff(agentExportObjs, masterRunningObj);

    PrintFdExportObj("print master not running fd export obj", masterFilterRunningExportObjs);
    PrintFdExportObj("print agent not running fd export obj", agentFilterRunningExportObjs);
}

void FilterRunningFdImport(std::vector<UbseMemFdBorrowImportObj> masterImportObjs,
                           std::vector<UbseMemFdBorrowImportObj> agentImportObjs,
                           std::vector<UbseMemFdBorrowImportObj> &masterRunningImportObjs,
                           std::vector<UbseMemFdBorrowImportObj> &masterFilterRunningImportObjs,
                           std::vector<UbseMemFdBorrowImportObj> &agentFilterRunningImportObjs)
{
    PrintFdImportObj("print master all fd import obj", masterImportObjs);
    PrintFdImportObj("print agent all fd import obj", agentImportObjs);

    std::unordered_set<std::string> masterRunningObj{};
    for (auto obj : masterImportObjs) {
        if (obj.status.state == UbseMemState::UBSE_MEM_SCHEDULING ||
            obj.status.state == UbseMemState::UBSE_MEM_IMPORT_RUNNING ||
            obj.status.state == UbseMemState::UBSE_MEM_IMPORT_DESTROYING ||
            obj.status.state == UbseMemState::UBSE_MEM_IMPORT_DESTROYING_WAIT) {
            masterRunningObj.insert(obj.req.name);
            masterRunningImportObjs.push_back(obj);
        }
    }

    PrintFdImportObj("print master running fd import obj", masterRunningImportObjs);

    auto findDiff = [](const std::vector<UbseMemFdBorrowImportObj> &source,
                       const std::unordered_set<std::string> &runningSet) {
        std::vector<UbseMemFdBorrowImportObj> result;
        for (const auto &elem : source) {
            if (runningSet.find(elem.req.name) == runningSet.end()) {
                result.push_back(elem);
            }
        }
        return result;
    };
    masterFilterRunningImportObjs = findDiff(masterImportObjs, masterRunningObj);
    agentFilterRunningImportObjs = findDiff(agentImportObjs, masterRunningObj);

    PrintFdImportObj("print master not running fd import obj", masterFilterRunningImportObjs);
    PrintFdImportObj("print agent not running fd import obj", agentFilterRunningImportObjs);
}

void FilterRunningNumaExport(std::vector<UbseMemNumaBorrowExportObj> masterExportObjs,
                             std::vector<UbseMemNumaBorrowExportObj> agentExportObjs,
                             std::vector<UbseMemNumaBorrowExportObj> &masterRunningExportObjs,
                             std::vector<UbseMemNumaBorrowExportObj> &masterFilterRunningExportObjs,
                             std::vector<UbseMemNumaBorrowExportObj> &agentFilterRunningExportObjs)
{
    PrintNumaExportObj("print master all numa export obj", masterExportObjs);
    PrintNumaExportObj("print agent all numa export obj", agentExportObjs);

    std::unordered_set<std::string> masterRunningObj{};
    for (auto obj : masterExportObjs) {
        if (obj.status.state == UbseMemState::UBSE_MEM_SCHEDULING ||
            obj.status.state == UbseMemState::UBSE_MEM_EXPORT_RUNNING ||
            obj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYING ||
            obj.status.state == UbseMemState::UBSE_MEM_EXPORT_DESTROYING_WAIT) {
            masterRunningObj.insert(obj.req.name);
            masterRunningExportObjs.push_back(obj);
        }
    }

    PrintNumaExportObj("print master running numa export obj", masterRunningExportObjs);

    auto findDiff = [](const std::vector<UbseMemNumaBorrowExportObj> &source,
                       const std::unordered_set<std::string> &runningSet) {
        std::vector<UbseMemNumaBorrowExportObj> result;
        for (const auto &elem : source) {
            if (runningSet.find(elem.req.name) == runningSet.end()) {
                result.push_back(elem);
            }
        }
        return result;
    };
    masterFilterRunningExportObjs = findDiff(masterExportObjs, masterRunningObj);
    agentFilterRunningExportObjs = findDiff(agentExportObjs, masterRunningObj);

    PrintNumaExportObj("print master not running numa export obj", masterFilterRunningExportObjs);
    PrintNumaExportObj("print agent not running numa export obj", agentFilterRunningExportObjs);
}

void FilterRunningNumaImport(std::vector<UbseMemNumaBorrowImportObj> masterImportObjs,
                             std::vector<UbseMemNumaBorrowImportObj> agentImportObjs,
                             std::vector<UbseMemNumaBorrowImportObj> &masterRunningImportObjs,
                             std::vector<UbseMemNumaBorrowImportObj> &masterFilterRunningImportObjs,
                             std::vector<UbseMemNumaBorrowImportObj> &agentFilterRunningImportObjs)
{
    PrintNumaImportObj("print master all numa import obj", masterImportObjs);
    PrintNumaImportObj("print agent all numa import obj", agentImportObjs);

    std::unordered_set<std::string> masterRunningObj{};
    for (auto obj : masterImportObjs) {
        if (obj.status.state == UbseMemState::UBSE_MEM_SCHEDULING ||
            obj.status.state == UbseMemState::UBSE_MEM_IMPORT_RUNNING ||
            obj.status.state == UbseMemState::UBSE_MEM_IMPORT_DESTROYING ||
            obj.status.state == UbseMemState::UBSE_MEM_IMPORT_DESTROYING_WAIT) {
            masterRunningObj.insert(obj.req.name);
            masterRunningImportObjs.push_back(obj);
        }
    }

    PrintNumaImportObj("print master running numa import obj", masterRunningImportObjs);

    auto findDiff = [](const std::vector<UbseMemNumaBorrowImportObj> &source,
                       const std::unordered_set<std::string> &runningSet) {
        std::vector<UbseMemNumaBorrowImportObj> result;
        for (const auto &elem : source) {
            if (runningSet.find(elem.req.name) == runningSet.end()) {
                result.push_back(elem);
            }
        }
        return result;
    };
    masterFilterRunningImportObjs = findDiff(masterImportObjs, masterRunningObj);
    agentFilterRunningImportObjs = findDiff(agentImportObjs, masterRunningObj);

    PrintNumaImportObj("print master not running numa import obj", masterFilterRunningImportObjs);
    PrintNumaImportObj("print agent not running numa import obj", agentFilterRunningImportObjs);
}

std::vector<UbseMemFdBorrowExportObj> FilterBothExistsFdExport(std::vector<UbseMemFdBorrowExportObj> masterExportObjs,
                                                               std::vector<UbseMemFdBorrowExportObj> agentExportObjs)
{
    std::vector<UbseMemFdBorrowExportObj> fdExportObjs{};
    std::unordered_set<std::string> masterNames;
    for (const auto &obj : masterExportObjs) {
        masterNames.insert(obj.req.name);
    }
    for (const auto &obj : agentExportObjs) {
        if (masterNames.find(obj.req.name) != masterNames.end()) {
            fdExportObjs.push_back(obj);
        }
    }
    return fdExportObjs;
}

std::vector<UbseMemNumaBorrowExportObj> FilterBothExistsNumaExport(
    std::vector<UbseMemNumaBorrowExportObj> masterExportObjs, std::vector<UbseMemNumaBorrowExportObj> agentExportObjs)
{
    std::vector<UbseMemNumaBorrowExportObj> fdExportObjs{};
    std::unordered_set<std::string> masterNames;
    for (const auto &obj : masterExportObjs) {
        masterNames.insert(obj.req.name);
    }
    for (const auto &obj : agentExportObjs) {
        if (masterNames.find(obj.req.name) != masterNames.end()) {
            fdExportObjs.push_back(obj);
        }
    }
    return fdExportObjs;
}

/**
* master和agent账本取差集，获得master独有的账本以及agent独有的账本
* @param masterExportObjs
* @param agentExportObjs
* @param masterDiffExportObjs
* @param agentDiffExportObjs
*/
void FilterFdDifferentExportSet(std::vector<UbseMemFdBorrowExportObj> masterExportObjs,
                                std::vector<UbseMemFdBorrowExportObj> agentExportObjs,
                                std::vector<UbseMemFdBorrowExportObj> &masterDiffExportObjs,
                                std::vector<UbseMemFdBorrowExportObj> &agentDiffExportObjs)
{
    PrintFdExportObj("print master not running fd export obj when diff", masterExportObjs);
    PrintFdExportObj("print agent not running fd export obj when diff", agentExportObjs);

    std::unordered_set<std::string> masterExportObjsSet;
    for (const auto &elem : masterExportObjs) {
        masterExportObjsSet.insert(elem.req.name);
    }
    std::unordered_set<std::string> agentExportObjsSet;
    for (const auto &elem : agentExportObjs) {
        agentExportObjsSet.insert(elem.req.name);
    }
    auto find_diff = [](const std::vector<UbseMemFdBorrowExportObj> &source,
                        const std::unordered_set<std::string> &targetSet) {
        std::vector<UbseMemFdBorrowExportObj> result;
        for (const auto &elem : source) {
            if (targetSet.find(elem.req.name) == targetSet.end()) {
                result.push_back(elem);
            }
        }
        return result;
    };
    // master中有，但是agent没有
    masterDiffExportObjs = find_diff(masterExportObjs, agentExportObjsSet);
    // agent中有，但是master没有
    agentDiffExportObjs = find_diff(agentExportObjs, masterExportObjsSet);

    PrintFdExportObj("print master diff fd export obj", masterDiffExportObjs);
    PrintFdExportObj("print agent diff fd export obj", agentDiffExportObjs);
}

void FilterFdDifferentImportSet(std::vector<UbseMemFdBorrowImportObj> masterImportObjs,
                                std::vector<UbseMemFdBorrowImportObj> agentImportObjs,
                                std::vector<UbseMemFdBorrowImportObj> &masterDiffImportObjs,
                                std::vector<UbseMemFdBorrowImportObj> &agentDiffImportObjs)
{
    PrintFdImportObj("print master not running fd import obj when diff", masterImportObjs);
    PrintFdImportObj("print agent not running fd import obj when diff", agentImportObjs);

    std::unordered_set<std::string> masterImportObjsSet;
    for (const auto &elem : masterImportObjs) {
        masterImportObjsSet.insert(elem.req.name);
    }
    std::unordered_set<std::string> agentImportObjsSet;
    for (const auto &elem : agentImportObjs) {
        agentImportObjsSet.insert(elem.req.name);
    }
    auto find_diff = [](const std::vector<UbseMemFdBorrowImportObj> &source,
                        const std::unordered_set<std::string> &targetSet) {
        std::vector<UbseMemFdBorrowImportObj> result;
        for (const auto &elem : source) {
            if (targetSet.find(elem.req.name) == targetSet.end()) {
                result.push_back(elem);
            }
        }
        return result;
    };
    // master中有，但是agent没有
    masterDiffImportObjs = find_diff(masterImportObjs, agentImportObjsSet);
    // agent中有，但是master没有
    agentDiffImportObjs = find_diff(agentImportObjs, masterImportObjsSet);

    PrintFdImportObj("print master diff fd import obj", masterDiffImportObjs);
    PrintFdImportObj("print agent diff fd import obj", agentDiffImportObjs);
}

void FilterNumaDifferentExportSet(std::vector<UbseMemNumaBorrowExportObj> masterExportObjs,
                                  std::vector<UbseMemNumaBorrowExportObj> agentExportObjs,
                                  std::vector<UbseMemNumaBorrowExportObj> &masterDiffExportObjs,
                                  std::vector<UbseMemNumaBorrowExportObj> &agentDiffExportObjs)
{
    PrintNumaExportObj("print master not running numa export obj when diff", masterExportObjs);
    PrintNumaExportObj("print agent not running numa export obj when diff", agentExportObjs);

    std::unordered_set<std::string> masterExportObjsSet;
    for (const auto &elem : masterExportObjs) {
        masterExportObjsSet.insert(elem.req.name);
    }
    std::unordered_set<std::string> agentExportObjsSet;
    for (const auto &elem : agentExportObjs) {
        agentExportObjsSet.insert(elem.req.name);
    }
    auto find_diff = [](const std::vector<UbseMemNumaBorrowExportObj> &source,
                        const std::unordered_set<std::string> &targetSet) {
        std::vector<UbseMemNumaBorrowExportObj> result;
        for (const auto &elem : source) {
            if (targetSet.find(elem.req.name) == targetSet.end()) {
                result.push_back(elem);
            }
        }
        return result;
    };
    // master中有，但是agent没有
    masterDiffExportObjs = find_diff(masterExportObjs, agentExportObjsSet);
    // agent中有，但是master没有
    agentDiffExportObjs = find_diff(agentExportObjs, masterExportObjsSet);

    PrintNumaExportObj("print master diff numa export obj", masterDiffExportObjs);
    PrintNumaExportObj("print agent diff numa export obj", agentDiffExportObjs);
}

void FilterNumaDifferentImportSet(std::vector<UbseMemNumaBorrowImportObj> masterImportObjs,
                                  std::vector<UbseMemNumaBorrowImportObj> agentImportObjs,
                                  std::vector<UbseMemNumaBorrowImportObj> &masterDiffImportObjs,
                                  std::vector<UbseMemNumaBorrowImportObj> &agentDiffImportObjs)
{
    PrintNumaImportObj("print master not running numa import obj when diff", masterImportObjs);
    PrintNumaImportObj("print agent not running numa import obj when diff", agentImportObjs);

    std::unordered_set<std::string> masterImportObjsSet;
    for (const auto &elem : masterImportObjs) {
        masterImportObjsSet.insert(elem.req.name);
    }
    std::unordered_set<std::string> agentImportObjsSet;
    for (const auto &elem : agentImportObjs) {
        agentImportObjsSet.insert(elem.req.name);
    }
    auto find_diff = [](const std::vector<UbseMemNumaBorrowImportObj> &source,
                        const std::unordered_set<std::string> &targetSet) {
        std::vector<UbseMemNumaBorrowImportObj> result;
        for (const auto &elem : source) {
            if (targetSet.find(elem.req.name) == targetSet.end()) {
                result.push_back(elem);
            }
        }
        return result;
    };
    // master中有，但是agent没有
    masterDiffImportObjs = find_diff(masterImportObjs, agentImportObjsSet);
    // agent中有，但是master没有
    agentDiffImportObjs = find_diff(agentImportObjs, masterImportObjsSet);

    PrintNumaImportObj("print master diff numa import obj", masterDiffImportObjs);
    PrintNumaImportObj("print agent diff numa import obj", agentDiffImportObjs);
}

std::vector<UbseMemFdBorrowExportObj> TransFdExportList(UbseMemFdExportObjMap exportObjMap)
{
    std::vector<UbseMemFdBorrowExportObj> objs{};
    for (const auto &obj : exportObjMap) {
        objs.push_back(obj.second);
    }
    return objs;
}

std::vector<UbseMemFdBorrowImportObj> TransFdImportList(UbseMemFdImportObjMap importObjMap)
{
    std::vector<UbseMemFdBorrowImportObj> objs{};
    for (const auto &obj : importObjMap) {
        objs.push_back(obj.second);
    }
    return objs;
}

std::vector<UbseMemNumaBorrowExportObj> TransNumaExportList(UbseMemNumaExportObjMap exportObjMap)
{
    std::vector<UbseMemNumaBorrowExportObj> objs{};
    for (const auto &obj : exportObjMap) {
        objs.push_back(obj.second);
    }
    return objs;
}

std::vector<UbseMemNumaBorrowImportObj> TransNumaImportList(UbseMemNumaImportObjMap importObjMap)
{
    std::vector<UbseMemNumaBorrowImportObj> objs{};
    for (const auto &obj : importObjMap) {
        objs.push_back(obj.second);
    }
    return objs;
}
} // namespace ubse::mem::controller
