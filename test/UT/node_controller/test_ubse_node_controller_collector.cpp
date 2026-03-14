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

#include "test_ubse_node_controller_collector.h"

#include <src/framework/config/ubse_conf_module.h>
#include <src/framework/misc/ubse_net_util.h>

#include <filesystem>

#include "ubse_lcne_module.h"
#include "ubse_node_controller_collector.cpp"
#include "adapter_plugins/mti/ubse_mti_def.h"
#include "adapter_plugins/mti/ubse_topology_interface.h"
namespace ubse::node_controller::ut {
using namespace ubse::adapter_plugins::mti;
void TestNodeControllerCollector::SetUp()
{
    Test::SetUp();
    std::string certDir = std::filesystem::current_path().string();
    try {
        if (!std::filesystem::exists(certDir)) {
            std::filesystem::create_directories(certDir);
            chmod(certDir.c_str(), 0700);
        }
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Failed to create directory: " << e.what() << std::endl;
    }
}

void TestNodeControllerCollector::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestNodeControllerCollector, CollectNodeBaseInfo)
{
    MOCKER(GetCurNodeInfo).stubs().will(ignoreReturnValue());
    MOCKER(gethostname).stubs().will(returnValue(1)).then(returnValue(0));
    UbseNodeInfo ubseNodeInfo{};
    EXPECT_EQ(CollectNodeBaseInfo(ubseNodeInfo), 1);
}

TEST_F(TestNodeControllerCollector, CollectNodeBaseInfoWhenConfModuleIsNull)
{
    UbseNodeInfo info{.nodeId = "0", .slotId = 0};
    MOCKER_CPP(GetCurNodeInfo).stubs().with(outBound(info));
    MOCKER(gethostname).stubs().will(returnValue(EOK));
    std::shared_ptr<UbseConfModule> nullModule;
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(nullModule));
    EXPECT_EQ(CollectNodeBaseInfo(info), UBSE_ERROR_MODULE_LOAD_FAILED);
}
TEST_F(TestNodeControllerCollector, CollectCpuInfo)
{
    std::map<std::string, std::string> portEidList;
    ubse::mti::IODieInfo ioDieInfo;

    UbseDevName devName("1", "1");
    std::string portInfo;
    portEidList.emplace("236", portInfo);

    UbseDeviceInfo deviceInfo;
    deviceInfo.slotId = "1";
    UbseMtiCpuTopoPortInfo portInfo1;
    portInfo1.portId = "5100";
    portInfo1.remoteSlotId = "1";
    portInfo1.remoteChipId = "2";
    portInfo1.remotePortId = "236";
    std::unordered_map<UbseDevPortName, UbseMtiCpuTopoPortInfo, UbseDevPortNameHash> portMap;
    portMap[UbseDevPortName("236")] = portInfo1;
    UbseDevTopology::mapped_type entry;
    entry.first = deviceInfo;
    entry.second = portMap;
    UbseDevTopology topology{};
    topology[devName] = entry;
    auto lcneModule = std::make_shared<ubse::mti::UbseLcneModule>();
    UbseUrmaEidInfo socketInfo;
    std::string remotePortInfo{};
    socketInfo.primaryEid = "1";
    socketInfo.portEidList.emplace("236", remotePortInfo);

    lcneModule->allSocketComEid.emplace(devName, socketInfo);
    mti::UbseLcneIODieInfo info;
    info.guid = "01-0101-0-1-0101-0101-010101-0101010101";
    info.primaryCna = "0x0085a7";
    info.chipType = mti::DevType::CPU;
    lcneModule->localBoardIOInfo.emplace(devName, info);
    MOCKER_CPP(&ubse::context::UbseContext::GetModule<ubse::mti::UbseLcneModule>).stubs().will(returnValue(lcneModule));
    MOCKER_CPP(&mti::UbseLcneModule::UbseGetDevTopology).stubs().with(outBound(topology)).will(returnValue(UBSE_OK));
    ubse::nodeController::UbseNodeInfo ubseNodeInfo{};
    auto ret = CollectCpuInfo(ubseNodeInfo, "1");
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(ubseNodeInfo.cpuInfos.size(), 1);
}
TEST_F(TestNodeControllerCollector, CollectNodeTopology)
{
    UbseNodeInfo ubseNodeInfo{};
    MOCKER(CollectIpList).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    MOCKER(CollectNumaInfo).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    MOCKER(CollectCpuInfo).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));

    EXPECT_EQ(CollectNodeTopology(ubseNodeInfo), UBSE_ERROR);
    EXPECT_EQ(CollectNodeTopology(ubseNodeInfo), UBSE_ERROR);
    EXPECT_EQ(CollectNodeTopology(ubseNodeInfo), UBSE_ERROR);
    EXPECT_EQ(CollectNodeTopology(ubseNodeInfo), UBSE_OK);
}

TEST_F(TestNodeControllerCollector, ParseIP)
{
    UbseIpV4Addr ipv4{};
    EXPECT_EQ(ParseIP("a.1.1.1", ipv4), UBSE_ERROR_INVAL);
    EXPECT_EQ(ParseIP("2147483648.1.1.1", ipv4), UBSE_ERROR_INVAL);
    EXPECT_EQ(ParseIP("256.1.1.1", ipv4), UBSE_ERROR_INVAL);
    EXPECT_EQ(ParseIP("127.0.0.0.1", ipv4), UBSE_ERROR_INVAL);
    EXPECT_EQ(ParseIP("127.0.0.1", ipv4), UBSE_OK);

    EXPECT_EQ(ipv4.addr[0], 127);
    EXPECT_EQ(ipv4.addr[1], 0);
    EXPECT_EQ(ipv4.addr[2], 0);
    EXPECT_EQ(ipv4.addr[3], 1);
}

TEST_F(TestNodeControllerCollector, IsSpecialIP)
{
    EXPECT_EQ(UbseNetUtil::IsSpecialIP("127.0.0.1"), true);
    EXPECT_EQ(UbseNetUtil::IsSpecialIP("6.123.2.27"), false);
}

std::string FibTrieStr()
{
    return "Main:\n"
           "  +-- 0.0.0.0/0 3 0 5\n"
           "     +-- 0.0.0.0/5 2 0 2\n"
           "        |-- 0.0.0.0\n"
           "           /0 universe UNICAST\n"
           "        +-- 7.218.100.0/23 2 0 1\n"
           "           |-- 7.218.100.0\n"
           "              /23 link UNICAST\n"
           "           |-- 7.218.101.27\n"
           "              /32 host LOCAL\n"
           "           |-- 7.218.101.255\n"
           "              /32 link BROADCAST\n"
           "     +-- 127.0.0.0/8 2 0 2\n"
           "        +-- 127.0.0.0/31 1 0 0\n"
           "           |-- 127.0.0.0\n"
           "              /8 host LOCAL\n"
           "           |-- 127.0.0.1\n"
           "              /32 host LOCAL\n"
           "        |-- 127.255.255.255\n"
           "           /32 link BROADCAST\n"
           "     |-- 169.254.169.254\n"
           "        /32 universe UNICAST\n"
           "Local:\n"
           "  +-- 0.0.0.0/0 3 0 5\n"
           "     +-- 0.0.0.0/5 2 0 2\n"
           "        |-- 0.0.0.0\n"
           "           /0 universe UNICAST\n"
           "        +-- 7.218.100.0/23 2 0 1\n"
           "           |-- 7.218.100.0\n"
           "              /23 link UNICAST\n"
           "           |-- 7.218.101.27\n"
           "              /32 host LOCAL\n"
           "           |-- 7.218.101.255\n"
           "              /32 link BROADCAST\n"
           "     +-- 127.0.0.0/8 2 0 2\n"
           "        +-- 127.0.0.0/31 1 0 0\n"
           "           |-- 127.0.0.0\n"
           "              /8 host LOCAL\n"
           "           |-- 127.0.0.1\n"
           "              /32 host LOCAL\n"
           "        |-- 127.255.255.255\n"
           "           /32 link BROADCAST\n"
           "     |-- 169.254.169.254\n"
           "        /32 universe UNICAST";
}

int CreateFibTrieFile()
{
    std::string certDir = std::filesystem::current_path().string();
    FIB_TRIE_PATH = certDir + "fib_trie";

    std::ofstream fibTrie(FIB_TRIE_PATH, std::ios::binary);
    if (!fibTrie) {
        std::cerr << "Failed to create node files" << std::endl;
        return 1;
    }
    fibTrie << FibTrieStr();
    fibTrie.close();
}

uint32_t MockUbseNodeTelemetryGetIpInfo(std::vector<std::string> &ipInfos)
{
    ipInfos = {"7.218.101.255"};
    return UBSE_OK;
}

TEST_F(TestNodeControllerCollector, ProcessListLine)
{
    std::vector<uint16_t> cpuList{};
    EXPECT_EQ(ProcessListLine("a", cpuList), UBSE_ERROR);
    cpuList.clear();
    EXPECT_EQ(ProcessListLine("1", cpuList), UBSE_OK);
    EXPECT_EQ(cpuList.size(), 1);
    EXPECT_EQ(cpuList[0], 1);

    cpuList.clear();
    EXPECT_EQ(ProcessListLine("0-1", cpuList), UBSE_OK);
    EXPECT_EQ(cpuList.size(), 2);
    EXPECT_EQ(cpuList[0], 0);
    EXPECT_EQ(cpuList[1], 1);
}

int CreateCpuListFile(const std::string &content)
{
    std::string certDir = std::filesystem::current_path().string();
    CPU_LIST_PREFIX_PATH = certDir + "node";
    try {
        if (!std::filesystem::exists(CPU_LIST_PREFIX_PATH + "0")) {
            std::filesystem::create_directories(CPU_LIST_PREFIX_PATH + "0");
            chmod(certDir.c_str(), 0700);
        }
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Failed to create directory: " << e.what() << std::endl;
    }

    std::string path = CPU_LIST_PREFIX_PATH + "0" + CPU_LIST_SUFFIX_PATH;
    std::ofstream cpuList(path, std::ios::binary);
    if (!cpuList) {
        std::cerr << "Failed to create cpu list files" << std::endl;
        return 1;
    }
    cpuList << content;
    cpuList.close();
}

void RemoveCpuDir()
{
    CPU_LIST_PREFIX_PATH = std::filesystem::current_path().string() + "node";
    std::string cpuPath = CPU_LIST_PREFIX_PATH + "0";
    try {
        if (std::filesystem::exists(cpuPath)) {
            std::filesystem::remove_all(cpuPath);
        }
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "remove cpu dir failed." << e.what() << std::endl;
    }
}

TEST_F(TestNodeControllerCollector, CollectCpuList_FAIL)
{
    std::vector<uint16_t> cpuList{};
    CPU_LIST_PREFIX_PATH = "";
    EXPECT_EQ(CollectCpuList(0, cpuList), UBSE_ERROR);

    CPU_LIST_PREFIX_PATH = std::filesystem::current_path().string() + "node";
    CreateCpuListFile("a");
    EXPECT_EQ(CollectCpuList(0, cpuList), UBSE_ERROR);
    RemoveCpuDir();
}

TEST_F(TestNodeControllerCollector, CollectCpuList)
{
    std::vector<uint16_t> cpuList{};

    CPU_LIST_PREFIX_PATH = std::filesystem::current_path().string() + "node";
    CreateCpuListFile("0-1");
    EXPECT_EQ(CollectCpuList(0, cpuList), UBSE_OK);
    EXPECT_EQ(cpuList.size(), 2);
    EXPECT_EQ(cpuList[0], 0);
    EXPECT_EQ(cpuList[1], 1);
    RemoveCpuDir();
}

int CreateSocketIdFile(const std::string &content)
{
    std::string certDir = std::filesystem::current_path().string();
    SOCKET_ID_PREFIX_PATH = certDir + "socket";
    try {
        if (!std::filesystem::exists(SOCKET_ID_PREFIX_PATH + "0" + "/topology")) {
            std::filesystem::create_directories(SOCKET_ID_PREFIX_PATH + "0" + "/topology");
            chmod(certDir.c_str(), 0700);
        }
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Failed to create directory: " << e.what() << std::endl;
    }

    std::string path = SOCKET_ID_PREFIX_PATH + "0" + SOCKET_ID_SUFFIX_PATH;
    std::ofstream cpuList(path, std::ios::binary);
    if (!cpuList) {
        std::cerr << "Failed to create socket id files" << std::endl;
        return 1;
    }
    cpuList << content;
    cpuList.close();
}

void RemoveSocketDir()
{
    SOCKET_ID_PREFIX_PATH = std::filesystem::current_path().string() + "socket";
    std::string socketPath = SOCKET_ID_PREFIX_PATH + "0";
    try {
        if (std::filesystem::exists(socketPath)) {
            std::filesystem::remove_all(socketPath);
        }
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "remove socket dir failed." << e.what() << std::endl;
    }
}

TEST_F(TestNodeControllerCollector, GetSocketId)
{
    uint32_t socketId;
    SOCKET_ID_PREFIX_PATH = "";
    EXPECT_EQ(GetSocketId(socketId, 0), UBSE_ERROR);

    CreateSocketIdFile("36");
    EXPECT_EQ(GetSocketId(socketId, 0), UBSE_OK);
    EXPECT_EQ(socketId, 36);
    RemoveSocketDir();
}

int CreateMemFile(const std::string &content)
{
    std::string certDir = std::filesystem::current_path().string();
    MEM_PREFIX_PATH = certDir + "mem";
    try {
        if (!std::filesystem::exists(MEM_PREFIX_PATH + "0")) {
            std::filesystem::create_directories(MEM_PREFIX_PATH + "0");
            chmod(certDir.c_str(), 0700);
        }
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Failed to create directory: " << e.what() << std::endl;
    }

    std::string path = MEM_PREFIX_PATH + "0" + MEM_SUFFIX_PATH;
    std::ofstream cpuList(path, std::ios::binary);
    if (!cpuList) {
        std::cerr << "Failed to create mem files" << std::endl;
        return 1;
    }
    cpuList << content;
    cpuList.close();
}

void RemoveMemDir()
{
    MEM_PREFIX_PATH = std::filesystem::current_path().string() + "mem";
    std::string memPath = MEM_PREFIX_PATH + "0";
    try {
        if (std::filesystem::exists(memPath)) {
            std::filesystem::remove_all(memPath);
        }
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "remove mem dir failed." << e.what() << std::endl;
    }
}

TEST_F(TestNodeControllerCollector, CollectMemSize)
{
    uint64_t memSize;
    uint64_t memFree;

    MEM_PREFIX_PATH = "";
    EXPECT_EQ(CollectMemSize(0, memSize, memFree), UBSE_ERROR);

    std::string content = "Node 0 MemTotal:       65812844 kB\n"
                          "Node 0 MemFree:        60791520 kB\n"
                          "Node 0 ";
    CreateMemFile(content);
    EXPECT_EQ(CollectMemSize(0, memSize, memFree), UBSE_OK);
    EXPECT_EQ(memSize, 65812844);
    EXPECT_EQ(memFree, 60791520);
    RemoveMemDir();
}

int CreateNrPageFile(const std::string &content)
{
    std::string certDir = std::filesystem::current_path().string();
    NR_PAGE_PREFIX_PATH = certDir + "nr";
    try {
        if (!std::filesystem::exists(NR_PAGE_PREFIX_PATH + "0" + "/hugepages/hugepages-2048kB")) {
            std::filesystem::create_directories(NR_PAGE_PREFIX_PATH + "0" + "/hugepages/hugepages-2048kB");
            chmod(certDir.c_str(), 0700);
        }
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Failed to create directory: " << e.what() << std::endl;
    }

    std::string path = NR_PAGE_PREFIX_PATH + "0" + NR_PAGE_SUFFIX_PATH_1 + "2048" + NR_PAGE_SUFFIX_PATH_2;
    std::ofstream cpuList(path, std::ios::binary);
    if (!cpuList) {
        std::cerr << "Failed to create nr page files" << std::endl;
        return 1;
    }
    cpuList << content;
    cpuList.close();
}

void RemoveNrPageDir()
{
    NR_PAGE_PREFIX_PATH = std::filesystem::current_path().string() + "nr";
    std::string nrPagePath = NR_PAGE_PREFIX_PATH + "0";
    try {
        if (std::filesystem::exists(nrPagePath)) {
            std::filesystem::remove_all(nrPagePath);
        }
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "remove nr page dir failed." << e.what() << std::endl;
    }
}

TEST_F(TestNodeControllerCollector, CollectNrHugePages2048)
{
    uint32_t nrHugePages2048;

    NR_PAGE_PREFIX_PATH = "";
    EXPECT_EQ(CollectNrHugePages(0, nrHugePages2048, 2048), UBSE_ERROR);

    CreateNrPageFile("1024");
    EXPECT_EQ(CollectNrHugePages(0, nrHugePages2048, 2048), UBSE_OK);
    EXPECT_EQ(nrHugePages2048, 1024);
    RemoveNrPageDir();
}

int CreateFreePageFile(const std::string &content)
{
    std::string certDir = std::filesystem::current_path().string();
    FREE_PAGE_PREFIX_PATH = certDir + "free";
    try {
        if (!std::filesystem::exists(FREE_PAGE_PREFIX_PATH + "0" + "/hugepages/hugepages-2048kB")) {
            std::filesystem::create_directories(FREE_PAGE_PREFIX_PATH + "0" + "/hugepages/hugepages-2048kB");
            chmod(certDir.c_str(), 0700);
        }
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Failed to create directory: " << e.what() << std::endl;
    }

    std::string path = FREE_PAGE_PREFIX_PATH + "0" + FREE_PAGE_SUFFIX_PATH_1 + "2048" + FREE_PAGE_SUFFIX_PATH_2;
    std::ofstream cpuList(path, std::ios::binary);
    if (!cpuList) {
        std::cerr << "Failed to create free page files" << std::endl;
        return 1;
    }
    cpuList << content;
    cpuList.close();
}

void RemoveFreePageDir()
{
    FREE_PAGE_PREFIX_PATH = std::filesystem::current_path().string() + "free";
    std::string nrPagePath = FREE_PAGE_PREFIX_PATH + "0";
    try {
        if (std::filesystem::exists(nrPagePath)) {
            std::filesystem::remove_all(nrPagePath);
        }
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "remove nr page dir failed." << e.what() << std::endl;
    }
}

TEST_F(TestNodeControllerCollector, CollectFreeHugePages2048)
{
    uint32_t freeHugePages2048;

    FREE_PAGE_PREFIX_PATH = "";
    EXPECT_EQ(CollectFreeHugePages(0, freeHugePages2048, 2048), UBSE_ERROR);

    CreateFreePageFile("1024");
    EXPECT_EQ(CollectFreeHugePages(0, freeHugePages2048, 2048), UBSE_OK);
    EXPECT_EQ(freeHugePages2048, 1024);
    RemoveFreePageDir();
}

int CreateNumaFile(const std::string &content)
{
    std::string certDir = std::filesystem::current_path().string();
    NUMA_PATH = certDir + "numa";

    std::ofstream numa(NUMA_PATH, std::ios::binary);
    if (!numa) {
        std::cerr << "Failed to create numa files" << std::endl;
        return 1;
    }
    numa << content;
    numa.close();
}

TEST_F(TestNodeControllerCollector, GetLocalNumas_FAIL)
{
    std::vector<uint32_t> nodeIds{};

    NUMA_PATH = "";
    EXPECT_EQ(GetLocalNumas(nodeIds), UBSE_ERROR);

    CreateNumaFile("a");
    EXPECT_EQ(GetLocalNumas(nodeIds), UBSE_ERROR);

    const char *path = NUMA_PATH.c_str();
    std::remove(path);
}

TEST_F(TestNodeControllerCollector, GetLocalNumas)
{
    std::vector<uint32_t> nodeIds{};

    CreateNumaFile("0-1");
    EXPECT_EQ(GetLocalNumas(nodeIds), UBSE_OK);
    EXPECT_EQ(nodeIds.size(), 2);
    EXPECT_EQ(nodeIds[0], 0);
    EXPECT_EQ(nodeIds[1], 1);

    const char *path = NUMA_PATH.c_str();
    std::remove(path);
}

uint32_t MockGetLocalNumas(std::vector<uint32_t> &nodeIds)
{
    nodeIds = {1};
    return UBSE_OK;
}

UbseResult MockCollectCpuList(uint32_t numa, std::vector<uint16_t> &cpuList)
{
    cpuList = {1};
    return UBSE_OK;
}

TEST_F(TestNodeControllerCollector, CollectNumaInfo)
{
    UbseNodeInfo ubseNodeInfo{};
    MOCKER(GetLocalNumas).stubs().will(returnValue(UBSE_ERROR)).then(invoke(MockGetLocalNumas));
    EXPECT_EQ(CollectNumaInfo(ubseNodeInfo, "1"), UBSE_ERROR);

    MOCKER(CollectCpuList).stubs().will(returnValue(UBSE_ERROR)).then(invoke(MockCollectCpuList));
    EXPECT_EQ(CollectNumaInfo(ubseNodeInfo, "1"), UBSE_ERROR);

    MOCKER(GetSocketId).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(CollectNumaInfo(ubseNodeInfo, "1"), UBSE_ERROR);

    MOCKER(CollectMemSize).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(CollectNumaInfo(ubseNodeInfo, "1"), UBSE_ERROR);

    MOCKER(CollectNrHugePages).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(CollectNumaInfo(ubseNodeInfo, "1"), UBSE_ERROR);

    MOCKER(CollectFreeHugePages).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    MOCKER(CollectObmmMemPoolInfo).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(CollectNumaInfo(ubseNodeInfo, "1"), UBSE_ERROR);
    EXPECT_EQ(CollectNumaInfo(ubseNodeInfo, "1"), UBSE_OK);
}

TEST_F(TestNodeControllerCollector, AddEdgeInfo)
{
    std::pair<const UbseDevPortName, adapter_plugins::mti::UbseMtiCpuTopoPortInfo> port{};
    nodeController::UbsePortInfo portInfo{};

    adapter_plugins::mti::UbseMtiCpuTopoPortInfo info{};
    info.portId = "port";
    info.ifName = "ifName";
    info.portRole = "master";
    info.portStatus = UbseMtiCpuTopoPortStatus::UP;
    info.portCna = 1;
    info.remoteSlotId = "remoteSlot";
    info.remoteChipId = "remoteChip";
    info.remoteCardId = "remoteCard";
    info.remoteIfName = "remoteIfName";
    info.remotePortId = "remotePort";

    port.second = info;
    EXPECT_NO_THROW(AddEdgeInfo(port, portInfo));

    EXPECT_EQ(portInfo.portId, "port");
    EXPECT_EQ(portInfo.ifName, "ifName");
    EXPECT_EQ(portInfo.portRole, "master");
    EXPECT_EQ(portInfo.portStatus, PortStatus::UP);
    EXPECT_EQ(portInfo.portCna, 1);
    EXPECT_EQ(portInfo.remoteSlotId, "remoteSlot");
    EXPECT_EQ(portInfo.remoteChipId, "remoteChip");
    EXPECT_EQ(portInfo.remoteCardId, "remoteCard");
    EXPECT_EQ(portInfo.remoteIfName, "remoteIfName");
    EXPECT_EQ(portInfo.remotePortId, "remotePort");
}

TEST_F(TestNodeControllerCollector, CollectIpListWhenGetIpInfoFail)
{
    std::vector<std::string> ipInfos{"127.0.0.1"};
    MOCKER_CPP(UbseNetUtil::GetIpInfo).stubs().will(returnValue(UBSE_ERROR));
    UbseNodeInfo nodeInfo;
    ASSERT_EQ(CollectIpList(nodeInfo), UBSE_ERROR);
}

TEST_F(TestNodeControllerCollector, CollectIpListWhenSuccess)
{
    std::vector<std::string> ipInfos{"127.0.0.1"};
    MOCKER_CPP(UbseNetUtil::GetIpInfo).stubs().with(outBound(ipInfos)).will(returnValue(UBSE_OK));
    UbseNodeInfo nodeInfo;
    ASSERT_EQ(CollectIpList(nodeInfo), UBSE_OK);
    ASSERT_EQ(nodeInfo.ipList[0].ipv4.addr[0], 127);
}
} // namespace ubse::node_controller::ut