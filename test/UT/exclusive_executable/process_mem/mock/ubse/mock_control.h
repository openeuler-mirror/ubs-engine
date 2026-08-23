#pragma once

#include <string>
#include <vector>

#include "ubse_mem_controller.h"
#include "process_mem_pid_bridge.h"

namespace ubse::mem::controller {
void MockSetDebtInfos(const std::vector<UbseNumaMemoryDebtInfo>& infos);
void MockSetImportDebtInfos(const std::vector<UbseNumaMemoryImportDebtInfo>& infos);
void MockClearDebtInfos();
void MockSetNumaCreateError(uint32_t err);
void MockSetNumaCreateErrorOnce(uint32_t err);
void MockSetNumaDeleteError(uint32_t err);
void MockSetNumaDeleteErrorOnce(uint32_t err);
uint32_t MockGetNumaDeleteCallCount();
void MockSetDebtInfoWithNodeError(uint32_t err);
void MockSetNumaCreateDesc(int64_t numaId, uint32_t exportSlotId, uint64_t size);
std::string MockGetLastNumaCreateName();
uint64_t MockGetLastNumaCreateSize();
void MockBlockNumaCreate();
void MockReleaseNumaCreate();
bool MockWaitNumaCreateEntered(int timeoutMs);
void MockBlockNumaDelete();
void MockReleaseNumaDelete();
bool MockWaitNumaDeleteEntered(int timeoutMs);
void MockResetAllErrors();

void MockSetNodeNumaInfos(const std::vector<UbseNodeNumaInfo>& infos);
std::string MockGetLastNumaDeleteName();
void MockAdjustNodeFree(uint64_t deltaBytes);
} // namespace ubse::mem::controller

namespace ubse::com {
struct MockRpcSendRecord {
    std::string address;
    std::string payload;
};
void MockResetRpcState();
const std::vector<MockRpcSendRecord>& MockGetRpcSendRecords();
} // namespace ubse::com

namespace ubse::nodeController {
void MockSetCurrentNodeId(const std::string& nodeId);
void MockResetCurrentNodeId();
} // namespace ubse::nodeController

namespace ubse::task_executor {
void MockSetExecutorAsync(bool async);
void MockWaitExecutorIdle();
} // namespace ubse::task_executor

namespace ubse::storage {
void MockSetStoragePutError(uint32_t err);
void MockSetStorageQueryError(uint32_t err);
void MockSetStorageDeleteError(uint32_t err);
void MockSetStorageListError(uint32_t err);
void MockSetStorageKeys(const std::vector<std::string>& keys);
void MockSetStorageQueryPayload(const std::string& keyPrefix, const std::string& key,
                                const std::vector<uint8_t>& payload);
void MockResetStorageErrors();
} // namespace ubse::storage
