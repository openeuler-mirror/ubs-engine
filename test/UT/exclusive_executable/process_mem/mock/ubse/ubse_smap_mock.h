#pragma once

#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include "process_mem_pid_bridge.h"

namespace ubse::smap {
void MockResetMigrateState();
void MockSetMigrateFail(int successCount, int failRet);
int MockRmrsMigrateOut(const std::vector<mempooling::smap::MigrateOutPayload>& payloads, int);
uint64_t MockGetMigrateTargetKb(pid_t pid, int numaId);
std::map<int, uint64_t> MockGetMigrateTargets(pid_t pid);
uint32_t MockGetMigrateCallCount();
std::vector<std::pair<int, uint64_t>> MockGetMigrateCalls();
} // namespace ubse::smap
