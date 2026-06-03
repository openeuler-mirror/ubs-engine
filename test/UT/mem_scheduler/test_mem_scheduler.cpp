/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#include "test_mem_scheduler.h"

#include "ubse_context.h"
#include "ubse_election_module.h"
#include "ubse_mmi_def.h"
#include "ubse_node.h"
#include "ubse_node_controller.h"
#include "src/controllers/include/ubse_mem_configuration.h"
#include "src/controllers/mem/mem_scheduler/ubse_mem_algo_account.h"
#include "src/controllers/mem/mem_scheduler/ubse_mem_scheduler.h"
#include "src/controllers/mem/mem_scheduler/ubse_mem_strategy_helper.h"
#include "src/controllers/mem/mem_scheduler/ubse_mem_topology_info_manager.h"

namespace ubse::mem::mem_scheduler::ut {
using namespace ubse::nodeController;
using namespace ubse::mem::strategy;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::election;
using namespace ubse::context;

constexpr uint64_t FOUR_GB = 4194304 * 1024L;
constexpr uint64_t TWO_GB = 2097152 * 1024L;
constexpr uint32_t ONE_GB = 1048576;
constexpr uint32_t NUM_32 = 32;
constexpr uint32_t NUM_16 = 16;
constexpr uint32_t NUM_8 = 8;
constexpr uint64_t TIME_STAMP = 1755143100;
constexpr uint64_t MOCK_POOL_MEM_RATIO = 100;
constexpr uint64_t MOCK_MAX_BORROW_SIZE = 4194304;
constexpr uint32_t BLOCKSIZE_64M = 64;
constexpr uint32_t BLOCKSIZE_1024M = 1024;
constexpr uint64_t MOCK_MAX_SOCKET_IMPORT_SIZE = 274877906944;
constexpr uint64_t MB_128 = 128 * 1024 * 1024;
constexpr uint64_t MB_1024 = 1024 * 1024 * 1024;
constexpr uint64_t MOCK_BLOCK_SIZE = MB_128;
std::unordered_map<std::string, std::vector<ubse::nodeController::MemNodeData>> g_mockTopo;

namespace {
void MockTwoNodeTopo()
{
    g_mockTopo.clear();
    ubse::nodeController::MemNodeData nodeData1{};
    nodeData1.nodeId = "1";
    nodeData1.hostname = "host1";
    nodeData1.socket.numas = {{"1"}, {"2"}};
    nodeData1.socket.cpus = {{"1"}, {"2"}};
    nodeData1.isRegisterRm = true;
    nodeData1.socket.socketId = "36";
    g_mockTopo["2-36"] = {nodeData1};
    nodeData1.socket.socketId = "276";
    nodeData1.socket.numas = {{"3"}, {"4"}};
    g_mockTopo["2-276"] = {nodeData1};
    ubse::nodeController::MemNodeData nodeData2{};
    nodeData2.nodeId = "2";
    nodeData2.hostname = "host2";
    nodeData2.socket.numas = {{"1"}, {"2"}};
    nodeData2.socket.cpus = {{"1"}, {"2"}};
    nodeData2.isRegisterRm = true;
    nodeData2.socket.socketId = "36";
    g_mockTopo["1-36"] = {nodeData2};
    nodeData2.socket.socketId = "276";
    nodeData2.socket.numas = {{"3"}, {"4"}};
    g_mockTopo["1-276"] = {nodeData2};
}

// 默认1D full mesh
void MockNumNodeTopo(uint32_t num = 2)
{
    g_mockTopo.clear();
    std::vector<std::string> socketIds{"36", "276"};
    std::vector<int> numaIds{1, 2, 3, 4};
    for (auto i = 1; i <= num; ++i) {
        for (auto j = 1; j <= num; ++j) {
            if (j == i) {
                continue;
            }
            for (auto k = 0; k < 2; k++) {
                ubse::nodeController::MemNodeData nodeData{};
                nodeData.nodeId = std::to_string(j);
                nodeData.hostname = "host" + std::to_string(j);
                nodeData.socket.socketId = socketIds[k];
                nodeData.socket.cpus = {{"1"}, {"2"}};
                nodeData.socket.numas = {{std::to_string(numaIds[2 * k])}, {std::to_string(numaIds[2 * k + 1])}};
                nodeData.isRegisterRm = true;
                g_mockTopo[std::to_string(i) + "-" + socketIds[k]].push_back(nodeData);
            }
        }
    }
}

void MockThreeNodeTopo()
{
    g_mockTopo.clear();
    ubse::nodeController::MemNodeData nodeData1{};
    nodeData1.nodeId = "1";
    nodeData1.hostname = "host1";
    nodeData1.socket.numas = {{"1"}, {"2"}};
    nodeData1.socket.cpus = {{"1"}, {"2"}};
    nodeData1.isRegisterRm = true;

    ubse::nodeController::MemNodeData nodeData2{};
    nodeData2.nodeId = "2";
    nodeData2.hostname = "host2";
    nodeData2.socket.numas = {{"1"}, {"2"}};
    nodeData2.socket.cpus = {{"1"}, {"2"}};
    nodeData2.isRegisterRm = true;

    ubse::nodeController::MemNodeData nodeData3{};
    nodeData3.nodeId = "3";
    nodeData3.hostname = "host3";
    nodeData3.socket.numas = {{"1"}, {"2"}};
    nodeData2.socket.cpus = {{"1"}, {"2"}};

    nodeData2.socket.socketId = "36";
    nodeData3.socket.socketId = "36";
    g_mockTopo["1-36"] = {nodeData2, nodeData3};
    nodeData2.socket.socketId = "276";
    nodeData2.socket.numas = {{"3"}, {"4"}};

    nodeData3.socket.socketId = "276";
    nodeData3.socket.numas = {{"3"}, {"4"}};
    g_mockTopo["1-276"] = {nodeData2, nodeData3};

    nodeData1.socket.socketId = "36";
    nodeData1.socket.numas = {{"1"}, {"2"}};
    nodeData3.socket.socketId = "36";
    nodeData3.socket.numas = {{"1"}, {"2"}};
    g_mockTopo["2-36"] = {nodeData1, nodeData3};
    nodeData1.socket.socketId = "276";
    nodeData3.socket.socketId = "276";
    g_mockTopo["2-276"] = {nodeData1, nodeData3};

    nodeData1.socket.socketId = "36";
    nodeData2.socket.socketId = "276";
    g_mockTopo["3-36"] = {nodeData1, nodeData2};
    nodeData1.socket.socketId = "36";
    nodeData2.socket.socketId = "276";
    g_mockTopo["3-276"] = {nodeData1, nodeData2};
}

void MockThreeTiktokNodeTopo()
{
    g_mockTopo.clear();
    ubse::nodeController::MemNodeData nodeData1{};
    nodeData1.nodeId = "1";
    nodeData1.hostname = "host1";
    nodeData1.socket.numas = {{"1"}, {"2"}};
    nodeData1.socket.cpus = {{"1"}, {"2"}};
    nodeData1.isRegisterRm = true;

    ubse::nodeController::MemNodeData nodeData2{};
    nodeData2.nodeId = "2";
    nodeData2.hostname = "host2";
    nodeData2.socket.numas = {{"1"}, {"2"}};
    nodeData2.socket.cpus = {{"1"}, {"2"}};
    nodeData2.isRegisterRm = true;

    ubse::nodeController::MemNodeData nodeData3{};
    nodeData3.nodeId = "3";
    nodeData3.hostname = "host3";
    nodeData3.socket.numas = {{"1"}, {"2"}};
    nodeData2.socket.cpus = {{"1"}, {"2"}};

    nodeData2.socket.socketId = "36";
    nodeData3.socket.socketId = "36";
    g_mockTopo["1-36"] = {nodeData3};
    nodeData2.socket.socketId = "276";
    nodeData3.socket.socketId = "276";
    g_mockTopo["1-276"] = {nodeData3};

    nodeData1.socket.socketId = "36";
    nodeData3.socket.socketId = "36";
    g_mockTopo["2-36"] = {nodeData3};
    nodeData1.socket.socketId = "276";
    nodeData3.socket.socketId = "276";
    g_mockTopo["2-276"] = {nodeData3};

    nodeData1.socket.socketId = "36";
    nodeData2.socket.socketId = "276";
    g_mockTopo["3-36"] = {nodeData1, nodeData2};
    nodeData1.socket.socketId = "36";
    nodeData2.socket.socketId = "276";
    g_mockTopo["3-276"] = {nodeData1, nodeData2};
}

void MockGetNumValueTopo(int num)
{
    MockNumNodeTopo(num);
    MOCKER(&ubse::nodeController::UbseMemGetTopologyInfo).stubs().with(outBound(g_mockTopo)).will(returnValue(UBSE_OK));
}

void MockGetValueFromConf()
{
    MOCKER(&UbseMemConfiguration::GetMaxBorrowSize).stubs().will(returnValue(MOCK_MAX_BORROW_SIZE));
    MOCKER(&UbseMemConfiguration::GetMaxSocketImportSize).stubs().will(returnValue(MOCK_MAX_SOCKET_IMPORT_SIZE));
}

bool CompareNumaInfo(const UbseNumaInfo& numaInfo, const MemNumaInfo& memNumaInfo)
{
    return memNumaInfo.mMemTotal == numaInfo.size * 1024 && memNumaInfo.mActualMemTotal == numaInfo.size * 1024 &&
           memNumaInfo.mMemFree == numaInfo.freeSize * 1024 && memNumaInfo.mTimestamp == numaInfo.timestamp;
}

// 从数据里面获取mock的配置
void MockNumNodeInfo(std::vector<ubse::nodeController::UbseNodeInfo>& nodeInfos, uint32_t nodeNum = 2)
{
    nodeInfos.clear();
    ubse::nodeController::UbseNumaLocation loc1{"1", 1};
    ubse::nodeController::UbseNumaLocation loc2{"1", 2};
    ubse::nodeController::UbseNumaLocation loc3{"1", 3};
    ubse::nodeController::UbseNumaLocation loc4{"1", 4};
    UbseNumaInfo numa1{.location = loc1,
                       .socketId = 36,
                       .bindCore = {1, 2},
                       .size = FOUR_GB,
                       .freeSize = TWO_GB,
                       .nr_hugepages_2M = NUM_32,
                       .free_hugepages_2M = NUM_32,
                       .nr_hugepages_1G = NUM_32,
                       .free_hugepages_1G = NUM_16,
                       .timestamp = TIME_STAMP};
    UbseNumaInfo numa2{.location = loc2,
                       .socketId = 36,
                       .bindCore = {1, 2},
                       .size = TWO_GB,
                       .freeSize = ONE_GB,
                       .nr_hugepages_2M = NUM_32,
                       .free_hugepages_2M = NUM_32,
                       .nr_hugepages_1G = NUM_32,
                       .free_hugepages_1G = NUM_16,
                       .timestamp = TIME_STAMP};
    UbseNumaInfo numa3{.location = loc3,
                       .socketId = 276,
                       .bindCore = {1, 2},
                       .size = FOUR_GB,
                       .freeSize = TWO_GB,
                       .nr_hugepages_2M = NUM_32,
                       .free_hugepages_2M = NUM_32,
                       .nr_hugepages_1G = NUM_32,
                       .free_hugepages_1G = NUM_16,
                       .timestamp = TIME_STAMP};
    UbseNumaInfo numa4{.location = loc4,
                       .socketId = 276,
                       .bindCore = {1, 2},
                       .size = TWO_GB,
                       .freeSize = ONE_GB,
                       .nr_hugepages_2M = NUM_32,
                       .free_hugepages_2M = NUM_32,
                       .nr_hugepages_1G = NUM_32,
                       .free_hugepages_1G = NUM_16,
                       .timestamp = TIME_STAMP};
    for (int i = 1; i <= nodeNum; ++i) {
        ubse::nodeController::UbseNodeInfo node{};
        std::string nodeIndex = std::to_string(i);
        node.nodeId = nodeIndex;
        node.slotId = i;
        node.hostName = "host" + nodeIndex;
        node.bondingEid[0] = 1;
        UbseIpAddr ip{};
        node.ipList.push_back(ip);
        loc1.nodeId = nodeIndex;
        loc2.nodeId = nodeIndex;
        loc3.nodeId = nodeIndex;
        loc4.nodeId = nodeIndex;
        numa1.location.nodeId = nodeIndex;
        numa2.location.nodeId = nodeIndex;
        numa3.location.nodeId = nodeIndex;
        numa4.location.nodeId = nodeIndex;

        node.numaInfos[loc1] = numa1;
        node.numaInfos[loc2] = numa2;
        node.numaInfos[loc3] = numa3;
        node.numaInfos[loc4] = numa4;
        node.clusterState = ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING;
        node.localState = ubse::nodeController::UbseNodeLocalState::UBSE_NODE_READY;
        nodeInfos.push_back(node);
    }
}

void MockdevDirConnectInfo(std::vector<ubse::nodeController::UbseNodeInfo>& nodeInfos)
{
    std::map<std::string, PhysicalLink> physicalLinkMap{};
    const size_t nodeNum = nodeInfos.size();
    for (uint32_t i = 1; i <= nodeNum; ++i) {
        for (uint32_t j = 1; j <= nodeNum; ++j) {
            const PhysicalLink pLink{
                .slotId = i, .chipId = 0, .portId = 0, .peerSlotId = j, .peerChipId = 1, .peerPortId = 1};
            physicalLinkMap[std::to_string(i) + std::to_string(j)] = pLink;
        }
    }
    MOCKER_CPP(&context::UbseContext::GetModule<UbseElectionModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseElectionModule>()));
    MOCKER_CPP(&UbseElectionModule::IsLeader).stubs().will(returnValue(true));
    UbseNodeController::GetInstance().devDirConnectInfo = physicalLinkMap;
}

void MockNumNodeAndCheckSuccess(std::vector<ubse::nodeController::UbseNodeInfo>& nodeInfos,
                                std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo>& nodeInfoMap,
                                uint32_t n)
{
    nodeInfos.clear();
    nodeInfoMap.clear();
    MockNumNodeInfo(nodeInfos, n);
    for (const auto& nodeInfo : nodeInfos) {
        nodeInfoMap[nodeInfo.nodeId] = nodeInfo;
    }
    MOCKER(&UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeInfoMap));
    for (auto nodeInfo : nodeInfos) {
        nodeInfo.clusterState = UbseNodeClusterState::UBSE_NODE_INIT;
        auto ret = ubse::mem::scheduler::UbseMemNodeObjChangeHandler(nodeInfo);
        EXPECT_EQ(ret, UBSE_OK);
    }
    MockdevDirConnectInfo(nodeInfos);
}

void CreateNumNode(std::vector<ubse::nodeController::UbseNodeInfo>& nodeInfos,
                   std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo>& nodeInfoMap, uint32_t n)
{
    nodeInfos.clear();
    nodeInfoMap.clear();
    MockNumNodeInfo(nodeInfos, n);
    for (const auto nodeInfo : nodeInfos) {
        nodeInfoMap[nodeInfo.nodeId] = nodeInfo;
    }
    MockdevDirConnectInfo(nodeInfos);
}

void SetNodeInfo(std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo>& nodeInfoMap, uint32_t blockSize,
                 uint32_t pmdMapping, UbseAllocator allocator)
{
    for (auto& [_, node] : nodeInfoMap) {
        node.blockSize = blockSize;
        node.pmdMapping = pmdMapping;
        node.allocator = allocator;
    }
}

uint32_t MockGetCnaInfo(const ubse::nodeController::UbseNodeMemCnaInfoInput& ubseNodeMemCnaInfoInput,
                        ubse::nodeController::UbseNodeMemCnaInfoOutput& ubseNodeMemCnaInfoOutput)

{
    if (ubseNodeMemCnaInfoInput.exportSocketId == "36") {
        ubseNodeMemCnaInfoOutput.borrowSocketId = "36";
    } else if (ubseNodeMemCnaInfoInput.exportSocketId == "276") {
        ubseNodeMemCnaInfoOutput.borrowSocketId = "276";
    }
    return UBSE_OK;
}

void MockFdBorrow()
{
    UbseMemFdBorrowImportObj importObj{};
    importObj.req.requestNodeId = "1";
    importObj.req.importNodeId = "1";
    importObj.req.size = MB_128;
    importObj.req.udsInfo.gid = 1;
    importObj.req.udsInfo.pid = 2;
    importObj.req.udsInfo.uid = 3;
    importObj.status.state = UBSE_MEM_SCHEDULING;
    MOCKER(&ubse::nodeController::UbseNodeMemGetTopologyCnaInfo).stubs().will(invoke(MockGetCnaInfo));
    auto ret = scheduler::UbseMemFdImportObjStateChangeHandler(importObj); // 算法决策
    importObj.status.state = ubse::adapter_plugins::mmi::UBSE_MEM_EXPORT_SUCCESS;
    UbseMemFdBorrowExportObj exportObj{};
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    ret = scheduler::UbseMemFdExportObjStateChangeHandler(exportObj);
}

void ClearSchedulerCache()
{
    UbseMemTopologyInfoManager::GetInstance().Clear();
    UbseMemStrategyHelper::GetInstance().Clear();
    AlgoAccountManger::GetInstance().Clear();
}

bool CompareMemDebtNumaInfo(const ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo& infoA,
                            const ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo& infoB)
{
    return infoA.numaId == infoB.numaId && infoA.nodeId == infoB.nodeId && infoA.size == infoB.size &&
           infoA.socketId == infoB.socketId;
}

bool CheckAccontByImportObj(const UbseMemFdBorrowImportObj& importObj, std::shared_ptr<BaseAlgoAccount> algoAccountPtr)
{
    auto accountResult = algoAccountPtr->GetAlgoResult();
    if (algoAccountPtr->name != importObj.req.name ||
        accountResult.importNumaInfos.size() != importObj.algoResult.importNumaInfos.size() ||
        accountResult.exportNumaInfos.size() != importObj.algoResult.exportNumaInfos.size() ||
        importObj.algoResult.exportNumaInfos.empty() ||
        accountResult.attachSocketId != importObj.algoResult.attachSocketId) {
        return false;
    }

    for (int i = 0; i < importObj.algoResult.importNumaInfos.size(); ++i) {
        if (!CompareMemDebtNumaInfo(accountResult.importNumaInfos[i], importObj.algoResult.importNumaInfos[i])) {
            return false;
        }
    }

    for (int i = 0; i < importObj.algoResult.exportNumaInfos.size(); ++i) {
        if (!CompareMemDebtNumaInfo(accountResult.exportNumaInfos[i], importObj.algoResult.exportNumaInfos[i])) {
            return false;
        }
    }

    return true;
}

void CheckAndSubExportNumaSize(MemNumaInfo& numaInfo, const UbseMemDebtNumaInfo& debtNumaInfo,
                               const std::shared_ptr<BaseAlgoAccount> algoAccount)
{
    if (numaInfo.mUbseMemNumaLoc.nodeId == debtNumaInfo.nodeId &&
        numaInfo.mUbseMemNumaLoc.numaId == debtNumaInfo.numaId) {
        if (algoAccount->type == BorrowedType::SHM) {
            numaInfo.mMemShared -= debtNumaInfo.size;
        } else {
            numaInfo.mMemLent -= debtNumaInfo.size;
        }
    }
}

void CheckAndSubImportNumaSize(MemNumaInfo& numaInfo, const UbseMemDebtNumaInfo& debtNumaInfo,
                               const std::shared_ptr<BaseAlgoAccount> algoAccount)
{
    if (numaInfo.mUbseMemNumaLoc.nodeId == debtNumaInfo.nodeId &&
        numaInfo.mUbseMemNumaLoc.numaId == debtNumaInfo.numaId) {
        numaInfo.mMemBorrowed -= debtNumaInfo.size;
    }
}

bool CheckNumaStatus(const std::vector<std::shared_ptr<BaseAlgoAccount>> algoAccounts)
{
    auto allNumaInfo = UbseMemTopologyInfoManager::GetInstance().GetAllNumaInfo("");
    std::vector<MemNumaInfo> copyNumaInfo{};
    for (auto& numaInfo : allNumaInfo) {
        MemNumaInfo copyNuma = *numaInfo;
        copyNumaInfo.push_back(copyNuma);
    }

    for (const auto& algoAccount : algoAccounts) {
        auto algoResult = algoAccount->GetAlgoResult();
        for (const auto& debtNumaInfo : algoResult.exportNumaInfos) {
            for (auto& numaInfo : copyNumaInfo) {
                CheckAndSubExportNumaSize(numaInfo, debtNumaInfo, algoAccount);
            }
        }
    }

    for (const auto& algoAccount : algoAccounts) {
        auto algoResult = algoAccount->GetAlgoResult();
        for (const auto& debtNumaInfo : algoResult.importNumaInfos) {
            for (auto& numaInfo : copyNumaInfo) {
                CheckAndSubImportNumaSize(numaInfo, debtNumaInfo, algoAccount);
            }
        }
    }

    for (const auto& numaInfo : copyNumaInfo) {
        if (numaInfo.mMemLent != 0 || numaInfo.mMemBorrowed != 0 || numaInfo.mMemShared != 0) {
            return false;
        }
    }
    return true;
}

bool CheckBorrowDebtDetail(const std::vector<std::shared_ptr<BaseAlgoAccount>>& algoAccounts)
{
    auto ret = CheckNumaStatus(algoAccounts);
    return ret;
}

// 得到指定数量的fdBorrow mockObj
void MakeMockObj(const uint32_t& mockNums, std::unordered_map<std::string, UbseMemFdBorrowImportObj>& mockFdImportObjs,
                 std::unordered_map<std::string, UbseMemFdBorrowExportObj>& mockFdExportObjs)
{
    UbseMemFdBorrowExportObj exportObj{};
    UbseMemFdBorrowImportObj importObj{};
    importObj.req.name = "test";
    importObj.req.requestNodeId = "1";
    importObj.req.importNodeId = "1";
    importObj.req.size = MB_128;
    importObj.req.udsInfo.gid = 1;
    importObj.req.udsInfo.pid = 2;
    importObj.req.udsInfo.uid = 3;
    importObj.status.state = UBSE_MEM_SCHEDULING;

    exportObj.req.name = "test";
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;

    for (int i = 1; i <= mockNums; ++i) {
        std::string name = "test" + std::to_string(i);
        auto copyImportObj = importObj;
        auto copyExporyObj = exportObj;
        copyImportObj.req.name = name;
        copyExporyObj.req.name = name;
        mockFdImportObjs[name] = copyImportObj;
        mockFdExportObjs[name] = copyExporyObj;
    }
}

void MakeMockShareObj(const uint32_t& mockNums, const uint32_t nodeNums,
                      std::unordered_map<std::string, UbseMemShareBorrowImportObj>& mockShareImportObjs,
                      std::unordered_map<std::string, UbseMemShareBorrowExportObj>& mockShareExportObjs)
{
    UbseMemShareBorrowExportObj exportObj{};
    UbseMemShareBorrowImportObj importObj{};
    exportObj.req.requestNodeId = "1";
    exportObj.req.shmRegion.nodeNum = nodeNums;
    exportObj.req.size = MB_128;
    exportObj.req.udsInfo.gid = 1;
    exportObj.req.udsInfo.pid = 2;
    exportObj.req.udsInfo.uid = 3;

    exportObj.status.state = UBSE_MEM_SCHEDULING;
    std::vector<ubse::adapter_plugins::mmi::UbseNodeInfo> nodelist;
    for (uint32_t i = 1; i <= nodeNums; i++) {
        std::string name = "test" + std::to_string(i);
        ubse::adapter_plugins::mmi::UbseNodeInfo node;
        node.index = i - 1;
        node.nodeId = std::to_string(i);
        node.hostName = name;
        node.status = UBSE_NODE_STATUS_NORMAL;
        nodelist.push_back(node);
    }
    exportObj.req.shmRegion.nodelist = nodelist;
    for (uint32_t i = 1; i <= mockNums; ++i) {
        std::string name = "test" + std::to_string(i);
        auto copyImportObj = importObj;
        auto copyExporyObj = exportObj;
        copyImportObj.req.name = name;
        copyExporyObj.req.name = name;
        mockShareImportObjs[name] = copyImportObj;
        mockShareExportObjs[name] = copyExporyObj;
    }
}

struct NumaParam {
    int srcSocket;
    int srcNuma;
    size_t highWatermark; // 必填，算法百分比，vm自己决策场景，单位%
    std::vector<ubse::adapter_plugins::mmi::UbseNumaLocation>
        lenderLocs{}; // 借出方地址信息，与lenderSizes一一对应，为空则走基础算法进行决策
    std::vector<uint64_t> lenderSizes{}; // 借出大小，与lenderLocs一一对应
};

void MakeMockWaterNumaObj(const uint32_t& mockNums, NumaParam param,
                          std::unordered_map<std::string, UbseMemNumaBorrowImportObj>& mockNumaImportObjs,
                          std::unordered_map<std::string, UbseMemNumaBorrowExportObj>& mockNumaExportObjs)
{
    UbseMemNumaBorrowExportObj exportObj;
    UbseMemNumaBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.requestNodeId = "1";
    importObj.req.importNodeId = "1";
    importObj.req.size = MB_128;
    importObj.req.udsInfo.gid = 1;
    importObj.req.udsInfo.pid = 2;
    importObj.req.udsInfo.uid = 3;
    importObj.status.state = UBSE_MEM_SCHEDULING;
    importObj.req.srcSocket = param.srcSocket;
    importObj.req.srcNuma = param.srcNuma;
    importObj.req.highWatermark = param.highWatermark;
    importObj.req.lenderLocs = param.lenderLocs;
    importObj.req.lenderSizes = param.lenderSizes;

    exportObj.req.name = "test";
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;

    for (int i = 1; i <= mockNums; ++i) {
        std::string name = "test" + std::to_string(i);
        auto copyImportObj = importObj;
        auto copyExporyObj = exportObj;
        copyImportObj.req.name = name;
        copyExporyObj.req.name = name;
        mockNumaImportObjs[name] = copyImportObj;
        mockNumaExportObjs[name] = copyExporyObj;
    }
}

ubse::common::def::UbseResult FAKE_GetUbseConfForRadius(const std::string& section, const std::string& configKey,
                                                        std::string& configValue)
{
    if (section == "ubse.memory" && configKey == "radius.borrow") {
        configValue = "4";
        return UBSE_OK;
    }
    if (section == "ubse.memory" && configKey == "radius.lender") {
        configValue = "4";
        return UBSE_OK;
    }
    return UBSE_ERROR;
}
} // namespace

void TestMemScheduler::SetUp()
{
    Test::SetUp();
    MockGetValueFromConf();
    MOCKER(&ubse::nodeController::UbseNodeMemGetTopologyCnaInfo).stubs().will(invoke(MockGetCnaInfo));
}

void TestMemScheduler::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
    ClearSchedulerCache();
    UbseNodeController::GetInstance().devDirConnectInfo.clear();
}

/*
 * 用例描述：
 * 测试两节点算法初始化成功
 * 测试步骤：
 * 1. Mock nodeController启动对账回调
 * 2. Scheduler拿到mock的节点信息
 * 3. 初始化算法成功
 * 预期结果：
 * 1. 状态回调函数执行成功
 * 2. Scheduler内部缓存与mock的信息一致
 */
TEST_F(TestMemScheduler, SchedulerStartSuccess)
{
    MockGetNumValueTopo(2);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 2);
    auto numaInfos = UbseMemTopologyInfoManager::GetInstance().GetAllNumaInfo("");
    for (const auto numaInfo : numaInfos) {
        auto loc = numaInfo->mUbseMemNumaLoc;
        ubse::nodeController::UbseNumaLocation realLoc{loc.nodeId, static_cast<uint32_t>(loc.numaId)};
        auto mockNumaInfo = nodeInfoMap[loc.nodeId].numaInfos[realLoc];
        EXPECT_EQ(CompareNumaInfo(mockNumaInfo, *numaInfo), true);
    }
}

/*
 * 用例描述：
 * 测试两节点算法决策成功
 * 测试步骤：
 * 1. 使用两节点mock的数据
 * 2. 算法初始化成功
 * 3. 节点一fd借用申请内存
 * 预期结果：
 * 1. 节点二借出内存
 */
TEST_F(TestMemScheduler, Node1BorrowNode2Lend)
{
    MockGetNumValueTopo(2);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 2);
    UbseMemFdBorrowImportObj importObj{};
    importObj.req.name = "test";
    importObj.req.requestNodeId = "1";
    importObj.req.importNodeId = "1";
    importObj.req.size = MB_128;
    importObj.req.udsInfo.gid = 1;
    importObj.req.udsInfo.pid = 2;
    importObj.req.udsInfo.uid = 3;
    importObj.status.state = UBSE_MEM_SCHEDULING;
    auto ret = scheduler::UbseMemFdImportObjStateChangeHandler(importObj);
    ubse::nodeController::UbseNodeMemCnaInfoOutput outCna;
    outCna.borrowSocketId = 36;
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(importObj.algoResult.exportNumaInfos.size(), 1);
    EXPECT_EQ(importObj.algoResult.exportNumaInfos.back().nodeId, "2");
}

/*
 * 用例描述：
 * 测试四节点算法决策成功with
 * 测试步骤：
 * 1. 使用四节点mock的数据
 * 2. 算法初始化成功
 * 3. 节点一fd借用申请内存
 * 预期结果：
 * 1. 节点二借出内存
 */
TEST_F(TestMemScheduler, TestFourNodeFdBorrowWithcandidateNodeList)
{
    MockGetNumValueTopo(4);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 4);
    UbseMemFdBorrowImportObj importObj{};
    importObj.req.name = "test";
    importObj.req.requestNodeId = "1";
    importObj.req.importNodeId = "1";
    importObj.req.size = MB_128;
    importObj.req.udsInfo.gid = 1;
    importObj.req.udsInfo.pid = 2;
    importObj.req.udsInfo.uid = 3;
    importObj.req.candidateNodeList = {"2"};
    importObj.status.state = UBSE_MEM_SCHEDULING;
    auto ret = scheduler::UbseMemFdImportObjStateChangeHandler(importObj);
    ubse::nodeController::UbseNodeMemCnaInfoOutput outCna;
    outCna.borrowSocketId = 36;
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(importObj.algoResult.exportNumaInfos.size(), 1);
    EXPECT_EQ(importObj.algoResult.exportNumaInfos.back().nodeId, "2");
}

/*
 * 用例描述：
 * 测试两节点算法成环
 * 测试步骤：
 * 1. 使用两节点mock的数据
 * 2. 算法初始化成功
 * 3. 节点一fd借用申请内存
 * 4. 节点一fd借用成功,节点二借出内存
 * 5. 节点二申请借用内存
 * 预期结果：
 * 1. 节点二借用内存失败，算法借用成环
 */
TEST_F(TestMemScheduler, Node2LendOutNextBorrowFailed)
{
    MockGetNumValueTopo(2);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 2);
    UbseMemFdBorrowImportObj importObj{};
    importObj.req.name = "test1";
    importObj.req.requestNodeId = "1";
    importObj.req.importNodeId = "1";
    importObj.req.size = MB_128;
    importObj.req.udsInfo.gid = 1;
    importObj.req.udsInfo.pid = 2;
    importObj.req.udsInfo.uid = 3;
    importObj.status.state = UBSE_MEM_SCHEDULING;
    auto ret = scheduler::UbseMemFdImportObjStateChangeHandler(importObj);
    ubse::nodeController::UbseNodeMemCnaInfoOutput outCna;
    outCna.borrowSocketId = 36;
    EXPECT_EQ(ret, UBSE_OK);
    UbseMemFdBorrowImportObj node2ImportObj{};
    node2ImportObj.req.name = "test2";
    node2ImportObj.req.requestNodeId = "2";
    node2ImportObj.req.importNodeId = "2";
    node2ImportObj.req.size = MB_128;
    node2ImportObj.req.udsInfo.gid = 1;
    node2ImportObj.req.udsInfo.pid = 2;
    node2ImportObj.req.udsInfo.uid = 3;
    node2ImportObj.status.state = UBSE_MEM_SCHEDULING;
    ret = scheduler::UbseMemFdImportObjStateChangeHandler(node2ImportObj);
    EXPECT_EQ(ret, UBSE_SCHEDULER_ERROR_INVAL);
}

/*
 * 用例描述：
 * 测试两节点内存借用，借用缓存更新成功
 * 测试步骤：
 * 1. 使用两节点mock数据
 * 2. 算法初始化成功
 * 3. 节点一fd借用申请内存
 * 4. 执行export和import执行对应的回调
 * 5. 归还内存
 * 6. 执行unexport和unimport对应的回调
 * 预期结果：
 * 1. 借入借出关系更新
 * 2. 实际的占用内存成功更新
 * 3. 缓存信息被成功清理
 */
TEST_F(TestMemScheduler, FDBorrowSuccess)
{
    MockGetNumValueTopo(2);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 2);
    UbseMemFdBorrowImportObj importObj{};
    importObj.req.name = "test";
    importObj.req.requestNodeId = "1";
    importObj.req.importNodeId = "1";
    importObj.req.size = MB_128;
    importObj.req.udsInfo.gid = 1;
    importObj.req.udsInfo.pid = 2;
    importObj.req.udsInfo.uid = 3;
    importObj.status.state = UBSE_MEM_SCHEDULING;
    auto ret = scheduler::UbseMemFdImportObjStateChangeHandler(importObj); // 算法决策
    auto accountPtr = AlgoAccountManger::GetInstance().GetAlgoAccount(
        {importObj.req.name, importObj.req.requestNodeId, BorrowedType::FD});
    std::vector<std::shared_ptr<BaseAlgoAccount>> algoAccounts{accountPtr};
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(CheckBorrowDebtDetail(algoAccounts), true);
    std::string key =
        importObj.algoResult.exportNumaInfos[0].nodeId + std::to_string(importObj.algoResult.exportNumaInfos[0].numaId);
    EXPECT_TRUE(CheckAccontByImportObj(importObj, accountPtr));
    UbseMemFdBorrowExportObj exportObj{};
    exportObj.req.name = "test";
    exportObj.status.state = ubse::adapter_plugins::mmi::UBSE_MEM_EXPORT_SUCCESS;
    ret = scheduler::UbseMemFdExportObjStateChangeHandler(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
    importObj.status.state = ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS;
    ret = scheduler::UbseMemFdImportObjStateChangeHandler(importObj);
    EXPECT_EQ(CheckBorrowDebtDetail(algoAccounts), true);
    EXPECT_EQ(ret, UBSE_OK);
}

/*
 * 用例描述：
 * 测试两节点内存互相借用，在算法决策阶段发生借用成环，决策失败
 * 测试步骤：
 * 1. 使用两节点mock数据
 * 2. 算法初始化成功
 * 3. 节点一fd借用进入算法决策
 * 4. 节点二fd借用进入算法决策
 * 预期结果：
 * 1. 节点一决策成功
 * 2. 节点二决策失败
 */
TEST_F(TestMemScheduler, Node2AlgoFailedWhenNode2HasLendOut)
{
    MockGetNumValueTopo(2);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 2);
    UbseMemFdBorrowImportObj importObj1{};
    importObj1.req.name = "test";
    importObj1.req.requestNodeId = "1";
    importObj1.req.importNodeId = "1";
    importObj1.req.size = MB_128;
    importObj1.req.udsInfo.gid = 1;
    importObj1.req.udsInfo.pid = 2;
    importObj1.req.udsInfo.uid = 3;
    importObj1.status.state = UBSE_MEM_SCHEDULING;

    UbseMemFdBorrowImportObj importObj2{};
    importObj2.req.name = "test";
    importObj2.req.requestNodeId = "2";
    importObj2.req.importNodeId = "2";
    importObj2.req.size = MB_128;
    importObj2.req.udsInfo.gid = 1;
    importObj2.req.udsInfo.pid = 2;
    importObj2.req.udsInfo.uid = 3;
    importObj2.status.state = UBSE_MEM_SCHEDULING;

    auto ret = scheduler::UbseMemFdImportObjStateChangeHandler(importObj1); // 算法决策
    EXPECT_EQ(ret, UBSE_OK);
    ret = scheduler::UbseMemFdImportObjStateChangeHandler(importObj2); // 算法决策
    EXPECT_EQ(ret, UBSE_SCHEDULER_ERROR_INVAL);
}

/*
 * 用例描述：
 * 测试节点一借用三次内存，步骤为，借用，借用，归还，借用，归还，归还，每次借用的缓存都符合预期
 * 测试步骤：
 * 1. 使用两节点mock数据
 * 2. 算法初始化成功
 * 3. 模拟节点一fd借用成功
 * 4. 模拟节点一fd借用成功
 * 5. 模拟节点一fd归还成功
 * 6. 模拟节点一fd借用成功
 * 7. 模拟节点一fd归还成功
 * 8. 模拟节点一fd归还成功
 * 预期结果：
 * 1. 节点一决策成功
 * 2. 节点二决策失败
 */
TEST_F(TestMemScheduler, CheckMixBorrowReturnIsOk)
{
    MockGetNumValueTopo(2);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    std::unordered_map<std::string, UbseMemFdBorrowImportObj> mockImportObjs;
    std::unordered_map<std::string, UbseMemFdBorrowExportObj> mockExportObjs;
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 2);
    MakeMockObj(3, mockImportObjs, mockExportObjs);

    // test1 借用
    auto ret = scheduler::UbseMemFdImportObjStateChangeHandler(mockImportObjs["test1"]); // 算法决策
    mockExportObjs["test1"].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_EXPORT_SUCCESS;
    ret |= scheduler::UbseMemFdExportObjStateChangeHandler(mockExportObjs["test1"]);
    mockImportObjs["test1"].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS;
    ret |= scheduler::UbseMemFdImportObjStateChangeHandler(mockImportObjs["test1"]);
    EXPECT_TRUE(CheckBorrowDebtDetail(AlgoAccountManger::GetInstance().GetAllAlgoAccount()));

    // test2 借用
    scheduler::UbseMemFdImportObjStateChangeHandler(mockImportObjs["test2"]); // 算法决策
    mockExportObjs["test2"].algoResult = mockImportObjs["test2"].algoResult;
    mockExportObjs["test2"].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_EXPORT_SUCCESS;
    ret |= scheduler::UbseMemFdExportObjStateChangeHandler(mockExportObjs["test2"]);
    mockImportObjs["test2"].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS;
    ret |= scheduler::UbseMemFdImportObjStateChangeHandler(mockImportObjs["test2"]);
    EXPECT_TRUE(CheckBorrowDebtDetail(AlgoAccountManger::GetInstance().GetAllAlgoAccount()));

    // test2 归还
    mockExportObjs["test2"].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_EXPORT_DESTROYED;
    mockImportObjs["test2"].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED;
    ret |= scheduler::UbseMemFdImportObjStateChangeHandler(mockImportObjs["test2"]);
    ret |= scheduler::UbseMemFdExportObjStateChangeHandler(mockExportObjs["test2"]);
    EXPECT_TRUE(CheckBorrowDebtDetail(AlgoAccountManger::GetInstance().GetAllAlgoAccount()));

    // test3 借用
    scheduler::UbseMemFdImportObjStateChangeHandler(mockImportObjs["test3"]); // 算法决策
    mockExportObjs["test3"].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_EXPORT_SUCCESS;
    ret |= scheduler::UbseMemFdExportObjStateChangeHandler(mockExportObjs["test3"]);
    mockImportObjs["test3"].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS;
    ret |= scheduler::UbseMemFdImportObjStateChangeHandler(mockImportObjs["test3"]);
    EXPECT_TRUE(CheckBorrowDebtDetail(AlgoAccountManger::GetInstance().GetAllAlgoAccount()));

    // test1 归还
    mockExportObjs["test1"].algoResult = mockImportObjs["test1"].algoResult;
    mockExportObjs["test1"].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_EXPORT_DESTROYED;
    mockImportObjs["test1"].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED;
    ret |= scheduler::UbseMemFdImportObjStateChangeHandler(mockImportObjs["test1"]);
    ret |= scheduler::UbseMemFdExportObjStateChangeHandler(mockExportObjs["test1"]);
    EXPECT_TRUE(CheckBorrowDebtDetail(AlgoAccountManger::GetInstance().GetAllAlgoAccount()));

    // test3 归还
    mockExportObjs["test3"].algoResult = mockImportObjs["test3"].algoResult;
    mockExportObjs["test3"].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_EXPORT_DESTROYED;
    mockImportObjs["test3"].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED;
    ret |= scheduler::UbseMemFdImportObjStateChangeHandler(mockImportObjs["test3"]);
    ret |= scheduler::UbseMemFdExportObjStateChangeHandler(mockExportObjs["test3"]);
    EXPECT_TRUE(CheckBorrowDebtDetail(AlgoAccountManger::GetInstance().GetAllAlgoAccount()));
    EXPECT_EQ(ret, UBSE_OK);
}

/*
 * 用例描述：
 * 测试水线算法决策，绑定numa与不绑定numa
 * 测试步骤：
 * 1. 使用两节点mock数据
 * 2. 算法初始化成功
 * 3. 填入水线参数
 * 预期结果：
 * 1. 节点一水线决策成功
 */
TEST_F(TestMemScheduler, TestWaterMemoryBorrow)
{
    MockGetNumValueTopo(2);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    std::unordered_map<std::string, UbseMemNumaBorrowImportObj> mockImportObjs;
    std::unordered_map<std::string, UbseMemNumaBorrowExportObj> mockExportObjs;
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 2);
    NumaParam param{36, 1, 100};
    MakeMockWaterNumaObj(1, param, mockImportObjs, mockExportObjs);
    MOCKER(&UbseMemGetTopologyInfo).stubs().with(outBound(g_mockTopo)).will(returnValue(UBSE_OK));
    // test1 借用
    auto ret = scheduler::UbseMemNumaImportObjStateChangeHandler(mockImportObjs["test1"]); // 算法决策
    mockExportObjs["test1"].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_EXPORT_SUCCESS;
    ret |= scheduler::UbseMemNumaExportObjStateChangeHandler(mockExportObjs["test1"]);
    mockImportObjs["test1"].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS;
    ret |= scheduler::UbseMemNumaImportObjStateChangeHandler(mockImportObjs["test1"]);
    EXPECT_TRUE(CheckBorrowDebtDetail(AlgoAccountManger::GetInstance().GetAllAlgoAccount()));
    EXPECT_EQ(ret, UBSE_OK);
}

/*
 * 用例描述：
 * 测试水线算法决策，绑定numa,无直连的socket,决策失败
 * 测试步骤：
 * 1. 使用两节点mock数据
 * 2. 算法初始化成功
 * 3. 填入水线参数,绑定numa
 * 预期结果：
 * 1. 节点一水线决策失败，无直连的socket
 */
TEST_F(TestMemScheduler, TestWaterMemoryBorrowWillFailed)
{
    MockGetNumValueTopo(2);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    std::unordered_map<std::string, UbseMemNumaBorrowImportObj> mockImportObjs;
    std::unordered_map<std::string, UbseMemNumaBorrowExportObj> mockExportObjs;
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 2);
    NumaParam param{36, 1, 100};
    MakeMockWaterNumaObj(1, param, mockImportObjs, mockExportObjs);
    auto copyMap = g_mockTopo;
    copyMap["1-36"].clear();
    GlobalMockObject::verify();
    MOCKER(&ubse::nodeController::UbseMemGetTopologyInfo).stubs().with(outBound(copyMap)).will(returnValue(UBSE_OK));
    // test1 借用
    auto ret = scheduler::UbseMemNumaImportObjStateChangeHandler(mockImportObjs["test1"]); // 算法决策
    EXPECT_NE(ret, UBSE_OK);
}

/*
 * 用例描述：
 * 测试Numa借用，指定链路借用
 * 测试步骤：
 * 1. 使用两节点mock数据
 * 2. 算法初始化成功
 * 3. 填入指定链路信息
 * 预期结果：
 * 1. 节点一指定链路借用成功
 */
TEST_F(TestMemScheduler, TestNumaBorrowWithLendSocket)
{
    MockGetNumValueTopo(2);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    std::unordered_map<std::string, UbseMemNumaBorrowImportObj> mockImportObjs;
    std::unordered_map<std::string, UbseMemNumaBorrowExportObj> mockExportObjs;
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 2);
    NumaParam param{36, 1, 100};
    MakeMockWaterNumaObj(1, param, mockImportObjs, mockExportObjs);
    UbseMemLenderLinkInfo linkInfo{"2", 36, 0};
    mockImportObjs["test1"].req.linkInfo = linkInfo;
    // test1 借用
    auto ret = scheduler::UbseMemNumaImportObjStateChangeHandler(mockImportObjs["test1"]); // 算法决策
    EXPECT_EQ(ret, UBSE_OK);
}

/*
 * 用例描述：
 * 测试用户自定义算法决策，决策成功
 * 测试步骤：
 * 1. 使用两节点mock数据
 * 2. 算法初始化成功
 * 3. 填入用户自定义借出参数
 * 预期结果：
 * 1. 主节点决策成功
 */
TEST_F(TestMemScheduler, TestUserMemoryBorrowWillSuccess)
{
    MockGetNumValueTopo(2);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    std::unordered_map<std::string, UbseMemFdBorrowImportObj> mockImportObjs;
    std::unordered_map<std::string, UbseMemFdBorrowExportObj> mockExportObjs;
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 2);
    MakeMockObj(1, mockImportObjs, mockExportObjs);
    auto& importObj = mockImportObjs["test1"];
    auto& exportObj = mockExportObjs["test1"];
    ubse::adapter_plugins::mmi::UbseNumaLocation loc{"2", 1};
    importObj.req.lenderLocs.push_back(loc);
    importObj.req.lenderSizes.push_back(MB_128);
    auto ret = scheduler::UbseMemFdImportObjStateChangeHandler(mockImportObjs["test1"]); // 算法决策成功
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_TRUE(CheckBorrowDebtDetail(AlgoAccountManger::GetInstance().GetAllAlgoAccount()));
}

/*
 * 用例描述：
 * 测试共享算法决策，决策成功
 * 测试步骤：
 * 1. 使用两节点mock数据
 * 2. 算法初始化成功
 * 3. 填入算法借出参数
 * 预期结果：
 * 1. 主节点决策成功
 */
TEST_F(TestMemScheduler, TestShareMemoryBorrowWillSuccess)
{
    MockGetNumValueTopo(2);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    std::unordered_map<std::string, UbseMemShareBorrowImportObj> mockImportObjs;
    std::unordered_map<std::string, UbseMemShareBorrowExportObj> mockExportObjs;
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 2);
    MakeMockShareObj(2, 2, mockImportObjs, mockExportObjs);
    auto& exportObj = mockExportObjs["test"];
    ubse::adapter_plugins::mmi::UbseNumaLocation loc{"2", 1};
    auto ret = scheduler::UbseMemShmExportObjStateChangeHandler(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_TRUE(CheckBorrowDebtDetail(AlgoAccountManger::GetInstance().GetAllAlgoAccount()));
}

/*
 * 用例描述：
 * 测试算法决策，决策成功
 * 测试步骤：
 * 1. 使用八节点mock数据
 * 2. 算法初始化成功
 * 3. 填入算法借出参数
 * 预期结果：
 * 1. 主节点决策成功
 */
TEST_F(TestMemScheduler, TestEightNodeInit)
{
    MockGetNumValueTopo(8);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 8);
    UbseMemFdBorrowImportObj importObj{};
    importObj.req.name = "test";
    importObj.req.requestNodeId = "1";
    importObj.req.importNodeId = "1";
    importObj.req.size = MB_128;
    importObj.req.udsInfo.gid = 1;
    importObj.req.udsInfo.pid = 2;
    importObj.req.udsInfo.uid = 3;
    importObj.status.state = UBSE_MEM_SCHEDULING;
    auto ret = scheduler::UbseMemFdImportObjStateChangeHandler(importObj);
    ubse::nodeController::UbseNodeMemCnaInfoOutput outCna;
    outCna.borrowSocketId = 36;
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(importObj.algoResult.exportNumaInfos.size(), 1);
}

/*
 * 用例描述：
 * 测试共享算法决策，决策成功
 * 测试步骤：
 * 1. 使用两节点节点mock数据
 * 2. 算法初始化成功
 * 3. 填入算法借出参数
 * 预期结果：
 * 1. 主节点决策成功
 */
TEST_F(TestMemScheduler, TestTwoNodeCreateShare)
{
    MockGetNumValueTopo(2);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 2);
    UbseMemShareBorrowExportObj exportObj{};
    exportObj.req.name = "test";
    exportObj.req.requestNodeId = "1";
    exportObj.req.size = MB_128;
    exportObj.req.udsInfo.gid = 1;
    exportObj.req.udsInfo.pid = 2;
    exportObj.req.udsInfo.uid = 3;
    ubse::adapter_plugins::mmi::UbseNodeInfo node1{0, "1", "host1", UBSE_NODE_STATUS_NORMAL};
    ubse::adapter_plugins::mmi::UbseNodeInfo node2{1, "2", "host2", UBSE_NODE_STATUS_NORMAL};

    exportObj.req.shmRegion.nodelist = {node1, node2};
    exportObj.req.shmRegion.nodeNum = 2;
    exportObj.status.state = UBSE_MEM_SCHEDULING;
    auto ret = scheduler::UbseMemShmExportObjStateChangeHandler(exportObj);
    ubse::nodeController::UbseNodeMemCnaInfoOutput outCna;
    outCna.borrowSocketId = 36;
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(exportObj.algoResult.exportNumaInfos.size(), 1);
}

/*
 * 用例描述：
 * 测试共享算法决策，决策成功
 * 测试步骤：
 * 1. 使用两节点节点mock数据
 * 2. 算法初始化成功
 * 3. 填入算法借出参数
 * 预期结果：
 * 1. 主节点决策成功
 */
TEST_F(TestMemScheduler, TestTwoNodeCreateShareWithAffinity)
{
    MockGetNumValueTopo(2);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 2);
    UbseMemShareBorrowExportObj exportObj{};
    exportObj.req.name = "test";
    exportObj.req.requestNodeId = "1";
    UbseMemShmAffinitySocketInfo withAffinity{36, "1", true};
    exportObj.req.withAffinity = withAffinity;
    exportObj.req.size = MB_128;
    exportObj.req.udsInfo.gid = 1;
    exportObj.req.udsInfo.pid = 2;
    exportObj.req.udsInfo.uid = 3;
    ubse::adapter_plugins::mmi::UbseNodeInfo node1{0, "1", "host1", UBSE_NODE_STATUS_NORMAL};
    ubse::adapter_plugins::mmi::UbseNodeInfo node2{1, "2", "host2", UBSE_NODE_STATUS_NORMAL};

    exportObj.req.shmRegion.nodelist = {node1, node2};
    exportObj.req.shmRegion.nodeNum = 2;
    exportObj.status.state = UBSE_MEM_SCHEDULING;
    auto ret = scheduler::UbseMemShmExportObjStateChangeHandler(exportObj);
    ubse::nodeController::UbseNodeMemCnaInfoOutput outCna;
    outCna.borrowSocketId = 36;
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(exportObj.algoResult.exportNumaInfos.size(), 1);
}

/*
 * 用例描述：
 * 测试共享算法决策, 只将三节点可选择节点 决策成功
 * 测试步骤：
 * 1. 使用四节点节点mock数据
 * 2. 算法初始化成功
 * 3. 填入算法借出参数
 * 预期结果：
 * 1. 主节点决策成功
 */
TEST_F(TestMemScheduler, TestForuNodeCreateShareWithProviderList)
{
    MockGetNumValueTopo(4);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 4);
    UbseMemShareBorrowExportObj exportObj{};
    exportObj.req.name = "test";
    exportObj.req.requestNodeId = "1";
    exportObj.req.size = MB_128;
    exportObj.req.udsInfo.gid = 1;
    exportObj.req.udsInfo.pid = 2;
    exportObj.req.udsInfo.uid = 3;
    ubse::adapter_plugins::mmi::UbseNodeInfo node1{0, "1", "host1", UBSE_NODE_STATUS_NORMAL};
    ubse::adapter_plugins::mmi::UbseNodeInfo node2{1, "2", "host2", UBSE_NODE_STATUS_NORMAL};
    ubse::adapter_plugins::mmi::UbseNodeInfo node3{2, "3", "host3", UBSE_NODE_STATUS_NORMAL};
    ubse::adapter_plugins::mmi::UbseNodeInfo node4{3, "4", "host4", UBSE_NODE_STATUS_NORMAL};
    exportObj.req.shmRegion.nodelist = {node1, node2, node3, node4};
    exportObj.req.shmRegion.nodeNum = 4;
    exportObj.req.providerList = {"3"};
    exportObj.status.state = UBSE_MEM_SCHEDULING;
    auto ret = scheduler::UbseMemShmExportObjStateChangeHandler(exportObj);
    ubse::nodeController::UbseNodeMemCnaInfoOutput outCna;
    outCna.borrowSocketId = 36;
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(exportObj.algoResult.exportNumaInfos[0].nodeId, "3");
}

/*
 * 用例描述：
 * 测试addr算法决策
 * 测试步骤：
 * 1. 使用四节点节点mock数据
 * 2. 算法初始化成功
 * 3. 填入算法借出参数
 * 预期结果：
 * 1. 主节点决策成功
 */
TEST_F(TestMemScheduler, TestForuNodeCreateAddrBorrow)
{
    MockGetNumValueTopo(4);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 4);
    UbseMemAddrBorrowImportObj importObj{};
    importObj.req.name = "test";
    importObj.req.srcSocket = 36;
    importObj.req.srcNuma = 1;
    importObj.req.importPid = 999;
    importObj.req.dstSocket = 276;
    importObj.req.dstNuma = 1;
    importObj.req.exportPid = 777;
    UbseMemAddrInfo info{1, MB_128};
    importObj.req.exportAddrList.push_back(info);

    auto ret = scheduler::UbseMemAddrImportObjStateChangeHandler(importObj);
    ubse::nodeController::UbseNodeMemCnaInfoOutput outCna;
    outCna.borrowSocketId = 36;
    EXPECT_EQ(ret, UBSE_OK);
}

/*
 * 用例描述：
 * hugetlb_pmd场景测试Fd借用算法决策
 * 测试步骤：
 * 1. 使用2节点mock数据
 * 2. 算法初始化成功
 * 3. 填入算法借出参数
 * 预期结果：
 * 1. 主节点决策成功
 */
TEST_F(TestMemScheduler, TestFdBorrowInHugetlbPmd)
{
    MockGetNumValueTopo(2);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    CreateNumNode(nodeInfos, nodeInfoMap, 2);
    SetNodeInfo(nodeInfoMap, BLOCKSIZE_64M, MAX_PERCENT, UbseAllocator::HUGETLB_PMD);
    MOCKER(&UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeInfoMap));
    for (auto nodeInfo : nodeInfos) {
        nodeInfo.clusterState = UbseNodeClusterState::UBSE_NODE_INIT;
        auto ret = ubse::mem::scheduler::UbseMemNodeObjChangeHandler(nodeInfo);
        EXPECT_EQ(ret, UBSE_OK);
    }
    UbseMemFdBorrowImportObj importObj{};
    importObj.req.name = "test";
    importObj.req.requestNodeId = "1";
    importObj.req.importNodeId = "1";
    importObj.req.size = MB_128;
    importObj.req.udsInfo.gid = 1;
    importObj.req.udsInfo.pid = 2;
    importObj.req.udsInfo.uid = 3;
    importObj.status.state = UBSE_MEM_SCHEDULING;
    auto ret = scheduler::UbseMemFdImportObjStateChangeHandler(importObj);
    ubse::nodeController::UbseNodeMemCnaInfoOutput outCna;
    outCna.borrowSocketId = 36;
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(importObj.algoResult.exportNumaInfos.size(), 2);
    EXPECT_EQ(importObj.algoResult.exportNumaInfos.back().nodeId, "2");
}

/*
 * 用例描述：
 * hugetlb_pmd场景测试Share借用算法决策
 * 测试步骤：
 * 1. 使用2节点mock数据
 * 2. 算法初始化成功
 * 3. 填入算法借出参数
 * 预期结果：
 * 1. 主节点决策成功
 */
TEST_F(TestMemScheduler, TestShareBorrowInHugetlbPmd)
{
    MockGetNumValueTopo(2);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    CreateNumNode(nodeInfos, nodeInfoMap, 2);
    SetNodeInfo(nodeInfoMap, BLOCKSIZE_64M, MAX_PERCENT, UbseAllocator::HUGETLB_PMD);
    MOCKER(&UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeInfoMap));
    for (auto nodeInfo : nodeInfos) {
        nodeInfo.clusterState = UbseNodeClusterState::UBSE_NODE_INIT;
        auto ret = ubse::mem::scheduler::UbseMemNodeObjChangeHandler(nodeInfo);
        EXPECT_EQ(ret, UBSE_OK);
    }
    UbseMemShareBorrowExportObj exportObj{};
    exportObj.req.name = "test";
    exportObj.req.requestNodeId = "1";
    exportObj.req.size = MB_128;
    exportObj.req.udsInfo.gid = 1;
    exportObj.req.udsInfo.pid = 2;
    exportObj.req.udsInfo.uid = 3;
    ubse::adapter_plugins::mmi::UbseNodeInfo node1{0, "1", "host1", UBSE_NODE_STATUS_NORMAL};
    ubse::adapter_plugins::mmi::UbseNodeInfo node2{1, "2", "host2", UBSE_NODE_STATUS_NORMAL};
    exportObj.req.shmRegion.nodelist = {node1, node2};
    exportObj.req.shmRegion.nodeNum = 2;
    exportObj.status.state = UBSE_MEM_SCHEDULING;
    auto ret = scheduler::UbseMemShmExportObjStateChangeHandler(exportObj);
    ubse::nodeController::UbseNodeMemCnaInfoOutput outCna;
    outCna.borrowSocketId = 36;
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(exportObj.algoResult.exportNumaInfos.size(), 2);
}

/*
 * 用例描述：
 * hugetlb_pud场景测试Share借用算法决策
 * 测试步骤：
 * 1. 使用2节点mock数据
 * 2. 算法初始化成功
 * 3. 填入算法借出参数
 * 预期结果：
 * 1. 主节点决策成功
 */
TEST_F(TestMemScheduler, TestShareBorrowInHugetlbPud)
{
    MockGetNumValueTopo(2);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    CreateNumNode(nodeInfos, nodeInfoMap, 2);
    SetNodeInfo(nodeInfoMap, BLOCKSIZE_1024M, MAX_PERCENT, UbseAllocator::HUGETLB_PUD);
    MOCKER(&UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeInfoMap));
    for (auto nodeInfo : nodeInfos) {
        nodeInfo.clusterState = UbseNodeClusterState::UBSE_NODE_INIT;
        auto ret = ubse::mem::scheduler::UbseMemNodeObjChangeHandler(nodeInfo);
        EXPECT_EQ(ret, UBSE_OK);
    }
    UbseMemShareBorrowExportObj exportObj{};
    exportObj.req.name = "test";
    exportObj.req.requestNodeId = "1";
    exportObj.req.size = MB_128;
    exportObj.req.udsInfo.gid = 1;
    exportObj.req.udsInfo.pid = 2;
    exportObj.req.udsInfo.uid = 3;
    ubse::adapter_plugins::mmi::UbseNodeInfo node1{0, "1", "host1", UBSE_NODE_STATUS_NORMAL};
    ubse::adapter_plugins::mmi::UbseNodeInfo node2{1, "2", "host2", UBSE_NODE_STATUS_NORMAL};
    exportObj.req.shmRegion.nodelist = {node1, node2};
    exportObj.req.shmRegion.nodeNum = 2;
    exportObj.status.state = UBSE_MEM_SCHEDULING;
    auto ret = scheduler::UbseMemShmExportObjStateChangeHandler(exportObj);
    ubse::nodeController::UbseNodeMemCnaInfoOutput outCna;
    outCna.borrowSocketId = 36;
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(exportObj.algoResult.exportNumaInfos.size(), 1);
}

/*
 * 用例描述：
 * 测试一个socket上借用次数超最大限制
 * 测试步骤：
 * 1. 使用2节点mock数据
 * 2. 算法初始化成功
 * 3. 填入算法借出参数
 * 4. 进行2048次 blocksize为1的内存借用
 * 5. 再进行一次内存借用
 * 预期结果：
 * 1. 2048次内存借用成功
 * 2. socket1和socket2分别被借用1024次
 * 3. 再进行内存借用失败
 */
TEST_F(TestMemScheduler, TestFdBorrowMaxTimes)
{
    GTEST_SKIP();
    AlgoAccountManger::GetInstance().Clear();
    MockGetNumValueTopo(2);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 2);

    std::vector<int> exportResult{0, 0};

    for (size_t i = 1; i <= 2048; i++) {
        std::unordered_map<std::string, UbseMemFdBorrowImportObj> mockImportObjs;
        std::unordered_map<std::string, UbseMemFdBorrowExportObj> mockExportObjs;
        MakeMockObj(2048, mockImportObjs, mockExportObjs);
        std::string key = "test" + std::to_string(i);

        // test 借用
        auto ret = scheduler::UbseMemFdImportObjStateChangeHandler(mockImportObjs[key]); // 算法决策
        ASSERT_EQ(ret, UBSE_OK);
        if (mockImportObjs[key].algoResult.exportNumaInfos[0].socketId == 36) {
            exportResult[0]++;
        } else if (mockImportObjs[key].algoResult.exportNumaInfos[0].socketId == 276) {
            exportResult[1]++;
        }
        mockExportObjs[key].algoResult = mockImportObjs[key].algoResult;
        mockExportObjs[key].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_EXPORT_SUCCESS;
        ret |= scheduler::UbseMemFdExportObjStateChangeHandler(mockExportObjs[key]);
        mockImportObjs[key].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS;
        ret |= scheduler::UbseMemFdImportObjStateChangeHandler(mockImportObjs[key]);
        EXPECT_TRUE(CheckBorrowDebtDetail(AlgoAccountManger::GetInstance().GetAllAlgoAccount()));
    }
    EXPECT_EQ(exportResult[0], 1024);
    EXPECT_EQ(exportResult[1], 1024);

    std::unordered_map<std::string, UbseMemFdBorrowImportObj> mockImportObjs;
    std::unordered_map<std::string, UbseMemFdBorrowExportObj> mockExportObjs;
    MakeMockObj(1, mockImportObjs, mockExportObjs);
    auto ret = scheduler::UbseMemFdImportObjStateChangeHandler(mockImportObjs["test1"]); // 算法决策
    ASSERT_EQ(ret, UBSE_SCHEDULER_ERROR_NO_NODE_CAN_LEND);
}

/*
 * 用例描述：
 * Numa借用测试一个socket上借用次数超最大限制
 * 测试步骤：
 * 1. 使用2节点mock数据
 * 2. 算法初始化成功
 * 3. 填入算法借出参数
 * 4. 进行256次size为1024M的内存借用
 * 5. 再进行一次内存借用
 * 预期结果：
 * 1. 256次内存借用成功
 * 2. socket1和socket2分别被借用128次
 * 3. 再进行内存借用失败
 */
TEST_F(TestMemScheduler, TestNumaBorrowMaxTimes)
{
    AlgoAccountManger::GetInstance().Clear();
    MockGetNumValueTopo(2);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 2);

    std::vector<int> exportResult{0, 0};

    for (size_t i = 1; i <= 256; i++) {
        std::unordered_map<std::string, UbseMemNumaBorrowImportObj> mockImportObjs;
        std::unordered_map<std::string, UbseMemNumaBorrowExportObj> mockExportObjs;
        NumaParam param{-1, 1, 100};
        MakeMockWaterNumaObj(256, param, mockImportObjs, mockExportObjs);
        std::string key = "test" + std::to_string(i);
        mockImportObjs[key].req.size = MB_1024;

        // test 借用
        auto ret = scheduler::UbseMemNumaImportObjStateChangeHandler(mockImportObjs[key]); // 算法决策
        ASSERT_EQ(ret, UBSE_OK);
        if (mockImportObjs[key].algoResult.exportNumaInfos[0].socketId == 36) {
            exportResult[0]++;
        } else if (mockImportObjs[key].algoResult.exportNumaInfos[0].socketId == 276) {
            exportResult[1]++;
        }
        mockExportObjs[key].algoResult = mockImportObjs[key].algoResult;
        mockExportObjs[key].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_EXPORT_SUCCESS;
        ret |= scheduler::UbseMemNumaExportObjStateChangeHandler(mockExportObjs[key]);
        mockImportObjs[key].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS;
        ret |= scheduler::UbseMemNumaImportObjStateChangeHandler(mockImportObjs[key]);
        EXPECT_TRUE(CheckBorrowDebtDetail(AlgoAccountManger::GetInstance().GetAllAlgoAccount()));
    }
    EXPECT_EQ(exportResult[0], 128);
    EXPECT_EQ(exportResult[1], 128);

    std::unordered_map<std::string, UbseMemFdBorrowImportObj> mockImportObjs;
    std::unordered_map<std::string, UbseMemFdBorrowExportObj> mockExportObjs;
    MakeMockObj(1, mockImportObjs, mockExportObjs);
    auto ret = scheduler::UbseMemFdImportObjStateChangeHandler(mockImportObjs["test1"]); // 算法决策
    ASSERT_EQ(ret, UBSE_SCHEDULER_ERROR_NO_NODE_CAN_LEND);
}

/*
 * 用例描述：
 * Share借用测试一个socket上借用次数超最大限制
 * 测试步骤：
 * 1. 使用2节点mock数据
 * 2. 算法初始化成功
 * 3. 填入算法借出参数
 * 4. 创建256次size为1024M的内存借用
 * 5. 再进行一次内存借用
 * 预期结果：
 * 1. 256次内存借用成功
 * 2. socket1和socket2分别被借用128次
 * 3. 再进行内存借用失败
 */
TEST_F(TestMemScheduler, TestShareBorrowMaxTimes)
{
    AlgoAccountManger::GetInstance().Clear();
    MockGetNumValueTopo(2);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 2);

    std::vector<int> exportResult{0, 0};

    for (size_t i = 1; i <= 256; i++) {
        std::unordered_map<std::string, UbseMemShareBorrowImportObj> mockImportObjs;
        std::unordered_map<std::string, UbseMemShareBorrowExportObj> mockExportObjs;
        MakeMockShareObj(256, 2, mockImportObjs, mockExportObjs);
        std::string key = "test" + std::to_string(i);
        mockExportObjs[key].req.size = MB_1024;
        mockExportObjs[key].req.providerList = {"2"};

        // test 借用
        auto ret = scheduler::UbseMemShmExportObjStateChangeHandler(mockExportObjs[key]); // 算法决策
        ASSERT_EQ(ret, UBSE_OK);
        if (mockExportObjs[key].algoResult.exportNumaInfos[0].socketId == 36) {
            exportResult[0]++;
        } else if (mockExportObjs[key].algoResult.exportNumaInfos[0].socketId == 276) {
            exportResult[1]++;
        }
        mockExportObjs[key].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_EXPORT_SUCCESS;
        ret |= scheduler::UbseMemShmImportObjStateChangeHandler(mockImportObjs[key]);
        mockImportObjs[key].algoResult = mockExportObjs[key].algoResult;
        mockImportObjs[key].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS;
        ret |= scheduler::UbseMemShmImportObjStateChangeHandler(mockImportObjs[key]);
        ASSERT_EQ(ret, UBSE_OK);
        EXPECT_TRUE(CheckBorrowDebtDetail(AlgoAccountManger::GetInstance().GetAllAlgoAccount()));
    }
    EXPECT_EQ(exportResult[0], 128);
    EXPECT_EQ(exportResult[1], 128);

    std::unordered_map<std::string, UbseMemFdBorrowImportObj> mockImportObjs;
    std::unordered_map<std::string, UbseMemFdBorrowExportObj> mockExportObjs;
    MakeMockObj(1, mockImportObjs, mockExportObjs);
    auto ret = scheduler::UbseMemFdImportObjStateChangeHandler(mockImportObjs["test1"]); // 算法决策
    ASSERT_EQ(ret, UBSE_SCHEDULER_ERROR_NO_NODE_CAN_LEND);
}

/*
 * 用例描述：
 * Numa借用测试一个socket上借用次数超最大限制，借用之后归还，再进行借用成功
 * 测试步骤：
 * 1. 使用2节点mock数据
 * 2. 算法初始化成功
 * 3. 填入算法借出参数
 * 4. 进行256次size为1024M的内存借用
 * 5. 归还其中32次借用
 * 6. 进行新的32次借用
 * 预期结果：
 * 1. 256次内存借用成功
 * 2. socket1和socket2分别被借用128次
 * 3. 32次内存归还成功
 * 4. 进行新的内存借用成功
 */
TEST_F(TestMemScheduler, TestNumaBorrowMaxTimes2)
{
    AlgoAccountManger::GetInstance().Clear();
    MockGetNumValueTopo(2);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 2);

    std::vector<int> exportResult{0, 0};
    std::vector<UbseMemNumaBorrowImportObj> importObjs;
    importObjs.reserve(32);
    std::vector<UbseMemNumaBorrowExportObj> exportObjs;
    exportObjs.reserve(32);

    for (size_t i = 1; i <= 256; i++) {
        std::unordered_map<std::string, UbseMemNumaBorrowImportObj> mockImportObjs;
        std::unordered_map<std::string, UbseMemNumaBorrowExportObj> mockExportObjs;
        NumaParam param{-1, 1, 100};
        MakeMockWaterNumaObj(256, param, mockImportObjs, mockExportObjs);
        std::string key = "test" + std::to_string(i);
        mockImportObjs[key].req.size = MB_1024;

        // test 借用
        auto ret = scheduler::UbseMemNumaImportObjStateChangeHandler(mockImportObjs[key]); // 算法决策
        ASSERT_EQ(ret, UBSE_OK);
        if (mockImportObjs[key].algoResult.exportNumaInfos[0].socketId == 36) {
            exportResult[0]++;
        } else if (mockImportObjs[key].algoResult.exportNumaInfos[0].socketId == 276) {
            exportResult[1]++;
        }
        mockExportObjs[key].algoResult = mockImportObjs[key].algoResult;
        mockExportObjs[key].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_EXPORT_SUCCESS;
        ret |= scheduler::UbseMemNumaExportObjStateChangeHandler(mockExportObjs[key]);
        mockImportObjs[key].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS;
        ret |= scheduler::UbseMemNumaImportObjStateChangeHandler(mockImportObjs[key]);
        EXPECT_TRUE(CheckBorrowDebtDetail(AlgoAccountManger::GetInstance().GetAllAlgoAccount()));
        if (i % 8 == 0) {
            importObjs.push_back(mockImportObjs[key]);
            exportObjs.push_back(mockExportObjs[key]);
        }
    }
    EXPECT_EQ(exportResult[0], 128);
    EXPECT_EQ(exportResult[1], 128);

    // test2 归还
    for (uint32_t i = 0; i < 32; i++) {
        exportObjs[i].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_EXPORT_DESTROYED;
        auto ret = scheduler::UbseMemNumaExportObjStateChangeHandler(exportObjs[i]);
        importObjs[i].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED;
        ret |= scheduler::UbseMemNumaImportObjStateChangeHandler(importObjs[i]);
        ASSERT_EQ(ret, UBSE_OK);
    }

    // 进行新的32次内存借用成功
    exportResult[0] = 0;
    exportResult[1] = 0;
    for (size_t i = 1; i <= 32; i++) {
        std::unordered_map<std::string, UbseMemNumaBorrowImportObj> mockImportObjs;
        std::unordered_map<std::string, UbseMemNumaBorrowExportObj> mockExportObjs;
        NumaParam param{-1, 1, 100};
        MakeMockWaterNumaObj(256, param, mockImportObjs, mockExportObjs);
        std::string key = "test" + std::to_string(i);
        mockImportObjs[key].req.name = "test_" + std::to_string(i);
        mockImportObjs[key].req.size = MB_1024;

        // test 借用
        auto ret = scheduler::UbseMemNumaImportObjStateChangeHandler(mockImportObjs[key]); // 算法决策
        ASSERT_EQ(ret, UBSE_OK);
        if (mockImportObjs[key].algoResult.exportNumaInfos[0].socketId == 36) {
            exportResult[0]++;
        } else if (mockImportObjs[key].algoResult.exportNumaInfos[0].socketId == 276) {
            exportResult[1]++;
        }
        mockExportObjs[key].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_EXPORT_SUCCESS;
        ret |= scheduler::UbseMemNumaExportObjStateChangeHandler(mockExportObjs[key]);
        mockImportObjs[key].status.state = ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS;
        ret |= scheduler::UbseMemNumaImportObjStateChangeHandler(mockImportObjs[key]);
        EXPECT_TRUE(CheckBorrowDebtDetail(AlgoAccountManger::GetInstance().GetAllAlgoAccount()));
    }
}

struct NumaInfoHash {
    size_t operator()(const UbseMemDebtNumaInfo& info) const
    {
        return std::hash<std::string>{}(info.nodeId) + std::hash<int>{}(info.numaId) + std::hash<int>{}(info.socketId);
    }

    bool operator()(const UbseMemDebtNumaInfo& info1, const UbseMemDebtNumaInfo& info2) const
    {
        return info1.socketId == info2.socketId && info1.numaId == info2.numaId && info1.nodeId == info2.nodeId;
    }
};

/*
 * 用例描述：
 * 在借出numa完全够用的情况下，测试内存非交织模式，4个借入节点 2个借出节点，一个socket下面有两个numa
 * 测试步骤：
 * 1. 使用8节点mock数据，并且一个socket下面有2个numa，
 * 2. 算法初始化成功
 * 3. 四个节点分别借入同一个借出节点，查看numa分布的情况
 * 预期结果：
 * 1. 四个借入节点分布在借出节点的四个numa上
 */
TEST_F(TestMemScheduler, TestLentNodeNumaMemory)
{
    AlgoAccountManger::GetInstance().Clear();
    strategy::UbseMemConfiguration::GetInstance().Init();
    MOCKER(&strategy::UbseMemConfiguration::IsLenderBalance).stubs().will(returnValue(true));
    MockGetNumValueTopo(8);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 8);
    std::unordered_set<UbseMemDebtNumaInfo, NumaInfoHash, NumaInfoHash> numaIdSet;
    UbseMemFdBorrowImportObj importObj{};
    importObj.req.name = "test1";
    importObj.req.requestNodeId = "1";
    importObj.req.importNodeId = "1";
    importObj.req.size = MB_128;
    importObj.req.udsInfo.gid = 1;
    importObj.req.udsInfo.pid = 2;
    importObj.req.udsInfo.uid = 3;
    importObj.req.candidateNodeList.emplace_back("5");
    importObj.status.state = UBSE_MEM_SCHEDULING;
    auto ret = scheduler::UbseMemFdImportObjStateChangeHandler(importObj);
    ASSERT_EQ(ret, UBSE_OK);
    numaIdSet.insert(importObj.algoResult.exportNumaInfos[0]);
    importObj.req.name = "test4";
    importObj.algoResult.exportNumaInfos.clear();
    importObj.algoResult.importNumaInfos.clear();
    ret |= scheduler::UbseMemFdImportObjStateChangeHandler(importObj);
    ASSERT_EQ(ret, UBSE_OK);
    EXPECT_EQ(numaIdSet.count(importObj.algoResult.exportNumaInfos[0]), 1);

    importObj.req.name = "test2";
    importObj.req.requestNodeId = "2";
    importObj.req.importNodeId = "2";
    importObj.algoResult.exportNumaInfos.clear();
    importObj.algoResult.importNumaInfos.clear();
    ret |= scheduler::UbseMemFdImportObjStateChangeHandler(importObj);
    ASSERT_EQ(ret, UBSE_OK);
    EXPECT_NE(numaIdSet.count(importObj.algoResult.exportNumaInfos[0]), 1);
    numaIdSet.insert(importObj.algoResult.exportNumaInfos[0]);

    importObj.req.name = "test3";
    importObj.req.requestNodeId = "3";
    importObj.req.importNodeId = "3";
    importObj.algoResult.exportNumaInfos.clear();
    importObj.algoResult.importNumaInfos.clear();
    ret |= scheduler::UbseMemFdImportObjStateChangeHandler(importObj);
    ASSERT_EQ(ret, UBSE_OK);
    EXPECT_NE(numaIdSet.count(importObj.algoResult.exportNumaInfos[0]), 1);
    numaIdSet.insert(importObj.algoResult.exportNumaInfos[0]);

    importObj.req.name = "test4";
    importObj.req.requestNodeId = "4";
    importObj.req.importNodeId = "4";
    importObj.algoResult.exportNumaInfos.clear();
    importObj.algoResult.importNumaInfos.clear();
    ret |= scheduler::UbseMemFdImportObjStateChangeHandler(importObj);
    ASSERT_EQ(ret, UBSE_OK);
    EXPECT_NE(numaIdSet.count(importObj.algoResult.exportNumaInfos[0]), 1);
    numaIdSet.insert(importObj.algoResult.exportNumaInfos[0]);
}

/*
 * 用例描述：
 * 测试内存借用指定借入半径，MOCK GetUbseConf(UBSE_MEMORY, "radius.borrow", borrowRadiusStr)返回值为4
 * 测试步骤：
 * 1. 使用8节点mock数据（一个socket下面有2个numa）
 * 2. 算法初始化成功
 * 3. 分别指定节点1向节点2，3，4，5，6，7，8借用内存
 * 预期结果：
 * 1. 前四个内存借用成功，后面的借用失败
 */
TEST_F(TestMemScheduler, TestBorrowWithRadius)
{
    AlgoAccountManger::GetInstance().Clear();
    MOCKER_CPP(GetUbseConf<std::string>).stubs().will(invoke(FAKE_GetUbseConfForRadius));
    UbseMemConfiguration::GetInstance().Init();

    MockGetNumValueTopo(8);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 8);

    UbseMemFdBorrowImportObj importObj{};
    importObj.req.requestNodeId = "1";
    importObj.req.importNodeId = "1";
    importObj.req.size = MB_128;
    importObj.req.udsInfo.gid = 1;
    importObj.req.udsInfo.pid = 2;
    importObj.req.udsInfo.uid = 3;
    importObj.status.state = UBSE_MEM_SCHEDULING;

    for (int i = 2; i <= 5; i++) {
        std::string name = "test" + std::to_string(i);
        importObj.req.name = name;
        importObj.req.candidateNodeList = {std::to_string(i)};
        importObj.algoResult.exportNumaInfos.clear();
        importObj.algoResult.importNumaInfos.clear();
        auto ret = scheduler::UbseMemFdImportObjStateChangeHandler(importObj);
        EXPECT_EQ(ret, UBSE_OK);
    }

    for (int i = 6; i <= 8; i++) {
        std::string name = "test" + std::to_string(i);
        importObj.req.name = name;
        importObj.req.candidateNodeList = {std::to_string(i)};
        importObj.algoResult.exportNumaInfos.clear();
        importObj.algoResult.importNumaInfos.clear();
        auto ret = scheduler::UbseMemFdImportObjStateChangeHandler(importObj);
        EXPECT_NE(ret, UBSE_OK);
    }
}

/*
 * 用例描述：
 * 测试内存借用指定借出半径，MOCK GetUbseConf(UBSE_MEMORY, "radius.lender", lenderRadiusStr)返回值为4
 * 测试步骤：
 * 1. 使用8节点mock数据（一个socket下面有2个numa）
 * 2. 算法初始化成功
 * 3. 分别指定节点1，2，3，4，5，6，7向节点8导出内存
 * 预期结果：
 * 1. 前四个内存导出成功，后面的导出失败
 */
TEST_F(TestMemScheduler, TestLendWithRadius)
{
    AlgoAccountManger::GetInstance().Clear();
    MOCKER_CPP(GetUbseConf<std::string>).stubs().will(invoke(FAKE_GetUbseConfForRadius));
    UbseMemConfiguration::GetInstance().Init();

    MockGetNumValueTopo(8);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 8);

    UbseMemFdBorrowImportObj importObj{};
    importObj.req.size = MB_128;
    importObj.req.udsInfo.gid = 1;
    importObj.req.udsInfo.pid = 2;
    importObj.req.udsInfo.uid = 3;
    importObj.status.state = UBSE_MEM_SCHEDULING;

    for (int i = 1; i <= 4; i++) {
        std::string name = "test" + std::to_string(i);
        importObj.req.name = name;
        importObj.req.requestNodeId = std::to_string(i);
        importObj.req.importNodeId = std::to_string(i);
        importObj.req.candidateNodeList = {"8"};
        importObj.algoResult.exportNumaInfos.clear();
        importObj.algoResult.importNumaInfos.clear();
        auto ret = scheduler::UbseMemFdImportObjStateChangeHandler(importObj);
        EXPECT_EQ(ret, UBSE_OK);
    }

    for (int i = 5; i <= 7; i++) {
        std::string name = "test" + std::to_string(i);
        importObj.req.name = name;
        importObj.req.requestNodeId = std::to_string(i);
        importObj.req.importNodeId = std::to_string(i);
        importObj.req.candidateNodeList = {"8"};
        importObj.algoResult.exportNumaInfos.clear();
        importObj.algoResult.importNumaInfos.clear();
        auto ret = scheduler::UbseMemFdImportObjStateChangeHandler(importObj);
        EXPECT_NE(ret, UBSE_OK);
    }
}

namespace {
ubse::common::def::UbseResult FAKE_GetUbseConfForRadiusZero(const std::string& section, const std::string& configKey,
                                                            std::string& configValue)
{
    if (section == "ubse.memory" && configKey == "radius.borrow") {
        configValue = "0";
        return UBSE_OK;
    }
    if (section == "ubse.memory" && configKey == "radius.lender") {
        configValue = "0";
        return UBSE_OK;
    }
    return UBSE_ERROR;
}

ubse::common::def::UbseResult FAKE_GetUbseConfForRadiusMax(const std::string& section, const std::string& configKey,
                                                           std::string& configValue)
{
    if (section == "ubse.memory" && configKey == "radius.borrow") {
        configValue = "7";
        return UBSE_OK;
    }
    if (section == "ubse.memory" && configKey == "radius.lender") {
        configValue = "7";
        return UBSE_OK;
    }
    return UBSE_ERROR;
}

ubse::common::def::UbseResult FAKE_GetUbseConfForRadius2(const std::string& section, const std::string& configKey,
                                                         std::string& configValue)
{
    if (section == "ubse.memory" && configKey == "radius.borrow") {
        configValue = "2";
        return UBSE_OK;
    }
    if (section == "ubse.memory" && configKey == "radius.lender") {
        configValue = "2";
        return UBSE_OK;
    }
    return UBSE_ERROR;
}
} // namespace

/*
 * 用例描述：
 * 测试内存借用指定借入半径=0，任何借用都应该失败
 * 测试步骤：
 * 1. 使用8节点mock数据
 * 2. 算法初始化成功
 * 3. 节点1向节点2借用内存
 * 预期结果：
 * 1. 借用失败
 */
TEST_F(TestMemScheduler, TestBorrowWithRadiusZero)
{
    AlgoAccountManger::GetInstance().Clear();
    MOCKER_CPP(GetUbseConf<std::string>).stubs().will(invoke(FAKE_GetUbseConfForRadiusZero));
    UbseMemConfiguration::GetInstance().Init();

    MockGetNumValueTopo(8);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 8);

    UbseMemFdBorrowImportObj importObj{};
    importObj.req.requestNodeId = "1";
    importObj.req.importNodeId = "1";
    importObj.req.size = MB_128;
    importObj.req.name = "test1";
    importObj.req.udsInfo.gid = 1;
    importObj.req.udsInfo.pid = 2;
    importObj.req.udsInfo.uid = 3;
    importObj.req.candidateNodeList = {"2"};
    importObj.status.state = UBSE_MEM_SCHEDULING;
    auto ret = scheduler::UbseMemFdImportObjStateChangeHandler(importObj);
    EXPECT_NE(ret, UBSE_OK);
}

/*
 * 用例描述：
 * 测试内存借用指定借入半径=7，应允许向最多7个不同节点借用
 * 测试步骤：
 * 1. 使用8节点mock数据
 * 2. 算法初始化成功
 * 3. 节点1分别向节点2,3,4,5,6,7,8借用内存
 * 预期结果：
 * 1. 前7个借用成功，第8个借用失败
 */
TEST_F(TestMemScheduler, TestBorrowWithRadiusMax)
{
    AlgoAccountManger::GetInstance().Clear();
    MOCKER_CPP(GetUbseConf<std::string>).stubs().will(invoke(FAKE_GetUbseConfForRadiusMax));
    UbseMemConfiguration::GetInstance().Init();

    MockGetNumValueTopo(8);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 8);

    UbseMemFdBorrowImportObj importObj{};
    importObj.req.requestNodeId = "1";
    importObj.req.importNodeId = "1";
    importObj.req.size = MB_128;
    importObj.req.udsInfo.gid = 1;
    importObj.req.udsInfo.pid = 2;
    importObj.req.udsInfo.uid = 3;
    importObj.status.state = UBSE_MEM_SCHEDULING;

    for (int i = 2; i <= 8; i++) {
        std::string name = "test" + std::to_string(i);
        importObj.req.name = name;
        importObj.req.candidateNodeList = {std::to_string(i)};
        importObj.algoResult.exportNumaInfos.clear();
        importObj.algoResult.importNumaInfos.clear();
        auto ret = scheduler::UbseMemFdImportObjStateChangeHandler(importObj);
        if (i <= 8) {
            EXPECT_EQ(ret, UBSE_OK) << "node 1 borrow from " << i << " should succeed";
        }
    }
}

/*
 * 用例描述：
 * 测试CheckByMemoryRadius在用户请求路径下的借入半径校验（对称校验的借入侧）：
 * 1. 走FD路径（candidateNodeList）建立债务：节点1→节点2、节点1→节点3（借入半径=2耗尽）
 * 2. 借入半径耗尽后向新节点借用 → 失败（CheckByMemoryRadius立即拒绝）
 * 3. 借入半径耗尽后向已有借入节点借用 → 成功（已有关系不受半径限制）
 */
TEST_F(TestMemScheduler, TestCheckByMemoryRadiusBorrow)
{
    AlgoAccountManger::GetInstance().Clear();
    MOCKER_CPP(GetUbseConf<std::string>).stubs().will(invoke(FAKE_GetUbseConfForRadius2));
    UbseMemConfiguration::GetInstance().Init();

    MockGetNumValueTopo(6);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 6);

    UbseMemFdBorrowImportObj setupObj{};
    setupObj.req.requestNodeId = "1";
    setupObj.req.importNodeId = "1";
    setupObj.req.size = MB_128;
    setupObj.req.udsInfo.gid = 1;
    setupObj.req.udsInfo.pid = 2;
    setupObj.req.udsInfo.uid = 3;
    setupObj.status.state = UBSE_MEM_SCHEDULING;

    setupObj.req.name = "setup2";
    setupObj.req.candidateNodeList = {"2"};
    setupObj.algoResult.exportNumaInfos.clear();
    setupObj.algoResult.importNumaInfos.clear();
    EXPECT_EQ(scheduler::UbseMemFdImportObjStateChangeHandler(setupObj), UBSE_OK);

    setupObj.req.name = "setup3";
    setupObj.req.candidateNodeList = {"3"};
    setupObj.algoResult.exportNumaInfos.clear();
    setupObj.algoResult.importNumaInfos.clear();
    EXPECT_EQ(scheduler::UbseMemFdImportObjStateChangeHandler(setupObj), UBSE_OK);

    // borrow radius=2 exceeded, new lender node 4 → should be rejected
    {
        UbseMemFdBorrowImportObj importObj{};
        importObj.req.requestNodeId = "1";
        importObj.req.importNodeId = "1";
        importObj.req.size = MB_128;
        importObj.req.name = "test_new_lender";
        importObj.req.udsInfo.gid = 1;
        importObj.req.udsInfo.pid = 2;
        importObj.req.udsInfo.uid = 3;
        importObj.status.state = UBSE_MEM_SCHEDULING;
        importObj.req.lenderLocs.push_back({"4", 1});
        importObj.req.lenderSizes.push_back(MB_128);
        auto ret = scheduler::UbseMemFdImportObjStateChangeHandler(importObj);
        EXPECT_NE(ret, UBSE_OK);
    }

    // borrow radius=2 exceeded, existing lender node 2 → should succeed
    {
        UbseMemFdBorrowImportObj importObj{};
        importObj.req.requestNodeId = "1";
        importObj.req.importNodeId = "1";
        importObj.req.size = MB_128;
        importObj.req.name = "test_existing_lender";
        importObj.req.udsInfo.gid = 1;
        importObj.req.udsInfo.pid = 2;
        importObj.req.udsInfo.uid = 3;
        importObj.status.state = UBSE_MEM_SCHEDULING;
        importObj.req.lenderLocs.push_back({"2", 1});
        importObj.req.lenderSizes.push_back(MB_128);
        auto ret = scheduler::UbseMemFdImportObjStateChangeHandler(importObj);
        EXPECT_EQ(ret, UBSE_OK);
    }
}

/*
 * 用例描述：
 * 测试CheckByMemoryRadius在用户请求路径下的借出半径校验（对称校验的借出侧）：
 * 1. 走FD路径（candidateNodeList）建立债务：节点1→节点5、节点2→节点5（借出半径=2耗尽）
 * 2. 借出半径耗尽后新借入节点借用 → 失败（CheckByMemoryRadius立即拒绝）
 * 3. 借出半径耗尽后已有借入节点借用 → 成功（已有关系不受半径限制）
 */
TEST_F(TestMemScheduler, TestCheckByMemoryRadiusLender)
{
    AlgoAccountManger::GetInstance().Clear();
    MOCKER_CPP(GetUbseConf<std::string>).stubs().will(invoke(FAKE_GetUbseConfForRadius2));
    UbseMemConfiguration::GetInstance().Init();

    MockGetNumValueTopo(6);
    std::vector<ubse::nodeController::UbseNodeInfo> nodeInfos{};
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfoMap{};
    MockNumNodeAndCheckSuccess(nodeInfos, nodeInfoMap, 6);

    UbseMemFdBorrowImportObj setupObj{};
    setupObj.req.size = MB_128;
    setupObj.req.udsInfo.gid = 1;
    setupObj.req.udsInfo.pid = 2;
    setupObj.req.udsInfo.uid = 3;
    setupObj.status.state = UBSE_MEM_SCHEDULING;

    setupObj.req.name = "setup1";
    setupObj.req.requestNodeId = "1";
    setupObj.req.importNodeId = "1";
    setupObj.req.candidateNodeList = {"5"};
    setupObj.algoResult.exportNumaInfos.clear();
    setupObj.algoResult.importNumaInfos.clear();
    EXPECT_EQ(scheduler::UbseMemFdImportObjStateChangeHandler(setupObj), UBSE_OK);

    setupObj.req.name = "setup2";
    setupObj.req.requestNodeId = "2";
    setupObj.req.importNodeId = "2";
    setupObj.req.candidateNodeList = {"5"};
    setupObj.algoResult.exportNumaInfos.clear();
    setupObj.algoResult.importNumaInfos.clear();
    EXPECT_EQ(scheduler::UbseMemFdImportObjStateChangeHandler(setupObj), UBSE_OK);

    // lender radius=2 exceeded, new borrower node 3 from node 5 → should be rejected
    {
        UbseMemFdBorrowImportObj importObj{};
        importObj.req.requestNodeId = "3";
        importObj.req.importNodeId = "3";
        importObj.req.size = MB_128;
        importObj.req.name = "test_new_borrower";
        importObj.req.udsInfo.gid = 1;
        importObj.req.udsInfo.pid = 2;
        importObj.req.udsInfo.uid = 3;
        importObj.status.state = UBSE_MEM_SCHEDULING;
        importObj.req.lenderLocs.push_back({"5", 1});
        importObj.req.lenderSizes.push_back(MB_128);
        auto ret = scheduler::UbseMemFdImportObjStateChangeHandler(importObj);
        EXPECT_NE(ret, UBSE_OK);
    }

    // lender radius=2 exceeded, existing borrower node 1 from node 5 → should succeed
    {
        UbseMemFdBorrowImportObj importObj{};
        importObj.req.requestNodeId = "1";
        importObj.req.importNodeId = "1";
        importObj.req.size = MB_128;
        importObj.req.name = "test_existing_borrower";
        importObj.req.udsInfo.gid = 1;
        importObj.req.udsInfo.pid = 2;
        importObj.req.udsInfo.uid = 3;
        importObj.status.state = UBSE_MEM_SCHEDULING;
        importObj.req.lenderLocs.push_back({"5", 1});
        importObj.req.lenderSizes.push_back(MB_128);
        auto ret = scheduler::UbseMemFdImportObjStateChangeHandler(importObj);
        EXPECT_EQ(ret, UBSE_OK);
    }
}

} // namespace ubse::mem::mem_scheduler::ut
