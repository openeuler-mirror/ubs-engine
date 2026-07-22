#pragma once

#include "ubse_mem_controller.h"

namespace ubse::mem::controller {
void MockSetDebtInfos(const std::vector<UbseNumaMemoryDebtInfo>& infos);
void MockSetImportDebtInfos(const std::vector<UbseNumaMemoryImportDebtInfo>& infos);
void MockClearDebtInfos();
void MockSetNumaCreateError(uint32_t err);
void MockSetNumaDeleteError(uint32_t err);
void MockSetDebtInfoWithNodeError(uint32_t err);
void MockResetAllErrors();
} // namespace ubse::mem::controller

namespace ubse::nodeController {
void MockSetCurrentNodeId(const std::string& nodeId);
void MockResetCurrentNodeId();
} // namespace ubse::nodeController

namespace ubse::storage {
void MockSetStoragePutError(uint32_t err);
void MockSetStorageQueryError(uint32_t err);
void MockSetStorageDeleteError(uint32_t err);
void MockResetStorageErrors();
} // namespace ubse::storage
