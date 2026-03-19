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

#include "test_ubse_obmm_meta_restore.h"
#include <dirent.h>
#include <src/controllers/include/ubse_mem_def.h>
#include <securec.h>

#include "ubse_election.h"
#include "ubse_mem_common_utils.h"
#include "ubse_election.h"
#include "ubse_obmm_meta_restore.h"
#include "ubse_obmm_utils.h"
#include "ubse_mem_def.h"
#include "ubse_election.h"
namespace ubse::ut::mmi {
using namespace ubse::mmi;
using namespace ubse::election;

std::atomic<bool> g_getExportType{false};
std::atomic<bool> g_getImportType{false};
std::atomic<int64_t> g_getRemoteNuma{-1};
std::atomic<bool> g_getInvalidStr{false};
std::atomic<int32_t> g_getUbMemInfoFlag{0};
class TestUbseObmmMetaRestore : public testing::Test {
public:
    static uint32_t GetFileFirstLineMockerMemIdType(const std::string& path, std::string& line)
    {
        if (g_getExportType.load()) {
            line = "export";
        } else if (g_getImportType.load()) {
            line = "import";
        } else {
            line = "";
        }
        return UBSE_OK;
    }

    static uint32_t GetFileFirstLineMockerRemoteNuma(const std::string& path, std::string& line)
    {
        if (g_getInvalidStr.load()) {
            line = "invalid";
        } else {
            line = std::to_string(g_getRemoteNuma);
        }
        return UBSE_OK;
    }

    static uint32_t GetFileFirstLineMockerTotalSize(const std::string& path, std::string& line)
    {
        if (g_getInvalidStr.load()) {
            line = "invalid";
        } else {
            line = MOCK_TOTAL_SIZE;
        }
        return UBSE_OK;
    }

    static uint32_t GetFileFirstLineMockerUbMemInfo(const std::string& path, std::string& line)
    {
        if (g_getInvalidStr.load()) {
            line = "invalid";
        } else {
            if (g_getUbMemInfoFlag == 0) {  // 0  means uba
                line = MOCK_UBA;
                g_getUbMemInfoFlag.fetch_add(1);
                return UBSE_OK;
            }
            if (g_getUbMemInfoFlag == 1) {  // 1 means length
                line = MOCK_LENGTH;
                g_getUbMemInfoFlag.fetch_add(1);
                return UBSE_OK;
            }
            if (g_getUbMemInfoFlag == 2) {  // 2 means tokenId
                line = MOCK_TOKEN_ID;
                g_getUbMemInfoFlag.fetch_add(1);
                return UBSE_OK;
            }
            if (g_getUbMemInfoFlag == 3) {  // 3 means cacheable
                line = std::to_string(MOCK_CACHEABLE);
                g_getUbMemInfoFlag.fetch_add(1);
                return UBSE_OK;
            }
        }
        return UBSE_OK;
    }
    static uint32_t GetFileFirstLineMockerCustomMeta(const std::string& path, std::string& line)
    {
        line = MOCK_LENGTH;
        return UBSE_OK;
    }
    static constexpr auto MOCK_REMOTE_NUMA_ID = 4;
    static constexpr auto MOCK_TOTAL_SIZE = "0x40000000";  // 1G
    static constexpr auto MOCK_UBA = "0xffff80000000";
    static constexpr auto MOCK_LENGTH = "0x40000000";
    static constexpr auto MOCK_TOKEN_ID = "0x0";
    static constexpr uint64_t MOCK_CACHEABLE = 1;

    static constexpr uint64_t ONE_G = 1ULL * 134217728;
    static constexpr uint64_t TWO_G = 2ULL * 134217728;
    static constexpr uint64_t THREE_G = 3ULL * 134217728;

protected:
    void SetUp() override
    {
        Test::SetUp();
    }

    void TearDown() override
    {
        Test::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(TestUbseObmmMetaRestore, testcase_GetMemIdType_ShouldReturnSuccessWhenGetExportType)
{
    std::string path = "/sys/devices/obmm/obmm_shmdev12/";
    uint8_t memIdType{};
    g_getExportType.store(true);
    MOCKER(&RmCommonUtils::GetFileFirstLine).stubs().will(invoke(GetFileFirstLineMockerMemIdType));
    auto ret = RmObmmDevRead::GetMemIdType(path, memIdType);
    g_getExportType.store(false);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(memIdType, 1);
}

TEST_F(TestUbseObmmMetaRestore, testcase_GetMemIdType_ShouldReturnSuccessWhenGetImportType)
{
    std::string path = "/sys/devices/obmm/obmm_shmdev12/";
    uint8_t memIdType{};
    g_getImportType.store(true);
    MOCKER(&RmCommonUtils::GetFileFirstLine).stubs().will(invoke(GetFileFirstLineMockerMemIdType));
    auto ret = RmObmmDevRead::GetMemIdType(path, memIdType);
    g_getImportType.store(false);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(memIdType, 0);
}

TEST_F(TestUbseObmmMetaRestore, testcase_GetMemIdType_ShouldReturnFailWhenGetNone)
{
    std::string path = "/sys/devices/obmm/obmm_shmdev12/";
    uint8_t memIdType{};
    MOCKER(&RmCommonUtils::GetFileFirstLine).stubs().will(invoke(GetFileFirstLineMockerMemIdType));
    auto ret = RmObmmDevRead::GetMemIdType(path, memIdType);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseObmmMetaRestore, testcase_GetMemIdType_ShouldReturnFailWhenOpenFileFail)
{
    std::string path = "/sys/devices/obmm/obmm_shmdev12/";
    uint8_t memIdType{};
    MOCKER(&RmCommonUtils::GetFileFirstLine).stubs().will(returnValue(1U));
    auto ret = RmObmmDevRead::GetMemIdType(path, memIdType);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseObmmMetaRestore, testcase_GetRemoteNuma_ShouldReturnSuccessWhenGetRemoteNuma)
{
    std::string path = "/sys/devices/obmm/obmm_shmdev12/";
    int16_t remoteNuma{};
    g_getRemoteNuma.store(MOCK_REMOTE_NUMA_ID);
    MOCKER(&RmCommonUtils::GetFileFirstLine).stubs().will(invoke(GetFileFirstLineMockerRemoteNuma));
    auto ret = RmObmmDevRead::GetRemoteNuma(path, 0, remoteNuma);  // 0 means import
    g_getRemoteNuma.store(-1);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(remoteNuma, MOCK_REMOTE_NUMA_ID);
}

TEST_F(TestUbseObmmMetaRestore, testcase_GetRemoteNuma_ReturnInvalidRemoteNumaIdWhenMemTypeIsExport)
{
    std::string path = "/sys/devices/obmm/obmm_shmdev12/";
    int16_t remoteNuma{};
    auto ret = RmObmmDevRead::GetRemoteNuma(path, 1, remoteNuma);  // 1 means export
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(remoteNuma, -1);
}

TEST_F(TestUbseObmmMetaRestore, testcase_GetRemoteNuma_ShouldReturnFailWhenOpenFileFail)
{
    std::string path = "/sys/devices/obmm/obmm_shmdev12/";
    int16_t remoteNuma{};
    MOCKER(&RmCommonUtils::GetFileFirstLine).stubs().will(returnValue(1U));
    auto ret = RmObmmDevRead::GetRemoteNuma(path, 0, remoteNuma);  // 0 means import
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseObmmMetaRestore, testcase_GetRemoteNuma_ShouldReturnFailWhenStr2Long)
{
    std::string path = "/sys/devices/obmm/obmm_shmdev12/";
    int16_t remoteNuma{};
    g_getInvalidStr.store(true);
    MOCKER(&RmCommonUtils::GetFileFirstLine).stubs().will(returnValue(1U));
    auto ret = RmObmmDevRead::GetRemoteNuma(path, 0, remoteNuma);  // 0 means import
    g_getInvalidStr.store(false);

    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseObmmMetaRestore, testcase_GetTotalSize_ShouldReturnSuccessWhenTotalSize)
{
    std::string path = "/sys/devices/obmm/obmm_shmdev12/";
    uint64_t totalSize{};
    MOCKER(&RmCommonUtils::GetFileFirstLine).stubs().will(invoke(GetFileFirstLineMockerTotalSize));
    auto ret = RmObmmDevRead::GetTotalSize(path, totalSize);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(totalSize, 0x40000000);
}

TEST_F(TestUbseObmmMetaRestore, testcase_GetTotalSize_ShouldReturnFailWhenOpenFileFail)
{
    std::string path = "/sys/devices/obmm/obmm_shmdev12/";
    uint64_t totalSize{};
    MOCKER(&RmCommonUtils::GetFileFirstLine).stubs().will(returnValue(1U));
    auto ret = RmObmmDevRead::GetTotalSize(path, totalSize);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseObmmMetaRestore, testcase_GetTotalSize_ShouldReturnFailWhenGetInvalidSize)
{
    std::string path = "/sys/devices/obmm/obmm_shmdev12/";
    uint64_t totalSize{};
    g_getInvalidStr.store(true);
    MOCKER(&RmCommonUtils::GetFileFirstLine).stubs().will(invoke(GetFileFirstLineMockerTotalSize));
    auto ret = RmObmmDevRead::GetTotalSize(path, totalSize);
    g_getInvalidStr.store(false);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseObmmMetaRestore, testcase_GetLocalMemId_ShouldReturnSuccessWhenRegexSearchSuccess)
{
    std::string path = "/sys/devices/obmm/obmm_shmdev12/";
    uint64_t memId{};
    auto ret = RmObmmDevRead::GetLocalMemId(path, memId);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(memId, 12U);
}

TEST_F(TestUbseObmmMetaRestore, testcase_GetLocalMemId_ShouldReturnFailWhenRegexSearchFail)
{
    std::string path = "/sys/devices/obmm/obmm_shmdddddd/";
    uint64_t memId{};
    auto ret = RmObmmDevRead::GetLocalMemId(path, memId);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseObmmMetaRestore, testcase_GetMemUbMemInfo_ShouldReturnSuccessWhenMemIdTypeIsExport)
{
    std::string path = "/sys/devices/obmm/obmm_shm111/";
    ubse_mem_obmm_mem_desc ubMemInfo{};
    MOCKER(&RmCommonUtils::GetFileFirstLine).stubs().will(invoke(GetFileFirstLineMockerUbMemInfo));
    MOCKER(ParsePreOnlineEidStr).stubs().will(returnValue(true));
    auto ret = RmObmmDevRead::GetMemUbMemInfo(path, 1, ubMemInfo);  // 1 means export
    g_getUbMemInfoFlag.store(0);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(ubMemInfo.addr, 0xffff80000000);
    EXPECT_EQ(ubMemInfo.length, 0x40000000);
    EXPECT_EQ(ubMemInfo.tokenid, 0x0);
}

TEST_F(TestUbseObmmMetaRestore, testcase_GetMemUbMemInfo_ShouldReturnSuccessWhenMemIdTypeIsImport)
{
    std::string path = "/sys/devices/obmm/obmm_shm111/";
    ubse_mem_obmm_mem_desc ubMemInfo{};

    auto ret = RmObmmDevRead::GetMemUbMemInfo(path, 0, ubMemInfo);  // 0 means import
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseObmmMetaRestore, testcase_GetMemUbMemInfo_ShouldReturnFailWhenGetUbaFail)
{
    std::string path = "/sys/devices/obmm/obmm_shm111/";
    ubse_mem_obmm_mem_desc ubMemInfo{};
    MOCKER(&RmCommonUtils::GetFileFirstLine).stubs().will(returnValue(1));  // get uba fail
    auto ret = RmObmmDevRead::GetMemUbMemInfo(path, 1, ubMemInfo);  // 1 means export
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseObmmMetaRestore, testcase_GetMemUbMemInfo_ShouldReturnFailWhenGetLegnthFail)
{
    std::string path = "/sys/devices/obmm/obmm_shm111/";
    ubse_mem_obmm_mem_desc ubMemInfo{};
    MOCKER(&RmCommonUtils::GetFileFirstLine)
        .stubs()
        .will(repeat(0, 1))  // get uba success
        .then(returnValue(1));  // get length fail
    auto ret = RmObmmDevRead::GetMemUbMemInfo(path, 1, ubMemInfo);  // 1 means export
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseObmmMetaRestore, testcase_GetMemUbMemInfo_ShouldReturnFailWhenGetTokenIdFail)
{
    std::string path = "/sys/devices/obmm/obmm_shm111/";
    ubse_mem_obmm_mem_desc ubMemInfo{};
    MOCKER(&RmCommonUtils::GetFileFirstLine)
        .stubs()
        .will(repeat(0, 2))  // get uba success
        .then(returnValue(1));  // get tokenId fail

    auto ret = RmObmmDevRead::GetMemUbMemInfo(path, 1, ubMemInfo);  // 1 means export
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseObmmMetaRestore, testcase_GetMemUbMemInfo_ShouldReturnFailWhenGetCacheableFail)
{
    std::string path = "/sys/devices/obmm/obmm_shm111/";
    ubse_mem_obmm_mem_desc ubMemInfo{};
    MOCKER(&RmCommonUtils::GetFileFirstLine)
        .stubs()
        .will(repeat(0, 3))  // get uba success
        .then(returnValue(1));  // get cacheable fail
    auto ret = RmObmmDevRead::GetMemUbMemInfo(path, 1, ubMemInfo);  // 1 means export
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseObmmMetaRestore, testcase_GetMemUbMemInfo_ShouldReturnFailWhenGetInvalidMeta)
{
    std::string path = "/sys/devices/obmm/obmm_shm111/";
    ubse_mem_obmm_mem_desc ubMemInfo{};
    g_getInvalidStr.store(true);
    MOCKER(&RmCommonUtils::GetFileFirstLine).stubs().will(invoke(GetFileFirstLineMockerUbMemInfo));
    auto ret = RmObmmDevRead::GetMemUbMemInfo(path, 1, ubMemInfo);  // 1 means export
    g_getInvalidStr.store(false);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseObmmMetaRestore, testcase_GetCustomMeta_ShouldReturnFailWhenOpenfail)
{
    std::string path = "/sys/devices/obmm/obmm_shm111/";

    g_getInvalidStr.store(true);
    MOCKER(&RmCommonUtils::GetFileFirstLine).stubs().will(invoke(GetFileFirstLineMockerCustomMeta));
    UbseMemLocalObmmCustomMeta customMeta{};
    UbMemPrivData privData{};
    auto ret = RmObmmDevRead::GetCustomMeta(path, customMeta, privData);
    g_getInvalidStr.store(false);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseObmmMetaRestore, testcase_RestoreAgentLocalObmmMetaDataGetMemIdFail)
{
    std::string path = "/sys/devices/obmm/obmm_shm111/";
    UbseMemLocalObmmMetaData localObmmMetaData{};
    MOCKER(&RmCommonUtils::GetFileFirstLine).stubs().will(invoke(GetFileFirstLineMockerUbMemInfo));
    auto ret = RmObmmMetaRestore::RestoreAgentLocalObmmMetaData(path, localObmmMetaData);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseObmmMetaRestore, testcase_RestoreAgentLocalObmmMetaDataGetNumaFail)
{
    std::string path = "/sys/devices/obmm/obmm_shm111/";
    UbseMemLocalObmmMetaData localObmmMetaData{};
    MOCKER(&RmObmmDevRead::GetMemIdType).stubs().will(returnValue(0u));
    auto ret = RmObmmMetaRestore::RestoreAgentLocalObmmMetaData(path, localObmmMetaData);
    g_getRemoteNuma.store(-1);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseObmmMetaRestore, testcase_RestoreAgentLocalObmmMetaDataGetUsedPidCountFail)
{
    std::string path = "/sys/devices/obmm/obmm_shm111/";
    UbseMemLocalObmmMetaData localObmmMetaData{};
    MOCKER(&RmObmmDevRead::GetMemIdType).stubs().will(returnValue(0u));
    g_getRemoteNuma.store(MOCK_REMOTE_NUMA_ID);
    MOCKER(&RmCommonUtils::GetFileFirstLine).stubs().will(invoke(GetFileFirstLineMockerRemoteNuma));
    auto ret = RmObmmMetaRestore::RestoreAgentLocalObmmMetaData(path, localObmmMetaData);
    g_getRemoteNuma.store(-1);
    EXPECT_NE(ret, UBSE_OK);
}

uint32_t UbseGetCurrentNodeInfoMock(UbseRoleInfo& currentNode)
{
    currentNode.nodeId = "1";  // mock 1
    return 0;
}
TEST_F(TestUbseObmmMetaRestore, testcase_RestoreAgentLocalObmmMetaDataGetTotalSizeFail)
{
    std::string path = "/sys/devices/obmm/obmm_shm111/";
    UbseMemLocalObmmMetaData localObmmMetaData{};
    MOCKER(&RmObmmDevRead::GetMemIdType).stubs().will(returnValue(0u));
    MOCKER(&RmCommonUtils::GetFileFirstLine).stubs().will(invoke(GetFileFirstLineMockerRemoteNuma));
    MOCKER(&RmObmmDevRead::GetRemoteNuma).stubs().will(returnValue(0u));
    auto ret = RmObmmMetaRestore::RestoreAgentLocalObmmMetaData(path, localObmmMetaData);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseObmmMetaRestore, testcase_RestoreAgentLocalObmmMetaDataGetlLocalMemidFail)
{
    std::string path = "/sys/devices/obmm/obmm_shm111/";
    UbseMemLocalObmmMetaData localObmmMetaData{};
    MOCKER(&RmObmmDevRead::GetMemIdType).stubs().will(returnValue(0u));
    MOCKER(&RmCommonUtils::GetFileFirstLine).stubs().will(invoke(GetFileFirstLineMockerRemoteNuma));
    MOCKER(&RmObmmDevRead::GetRemoteNuma).stubs().will(returnValue(0u));
    MOCKER(&RmObmmDevRead::GetTotalSize).stubs().will(returnValue(0u));
    auto ret = RmObmmMetaRestore::RestoreAgentLocalObmmMetaData(path, localObmmMetaData);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseObmmMetaRestore, testcase_RestoreAgentLocalObmmMetaDataGetCurrentNodeInfoFail)
{
    std::string path = "/sys/devices/obmm/obmm_shm111/";
    UbseMemLocalObmmMetaData localObmmMetaData{};
    MOCKER(&RmObmmDevRead::GetMemIdType).stubs().will(returnValue(0u));
    MOCKER(&RmCommonUtils::GetFileFirstLine).stubs().will(invoke(GetFileFirstLineMockerRemoteNuma));
    MOCKER(&RmObmmDevRead::GetRemoteNuma).stubs().will(returnValue(0u));
    MOCKER(&RmObmmDevRead::GetTotalSize).stubs().will(returnValue(0u));
    MOCKER(&RmObmmDevRead::GetLocalMemId).stubs().will(returnValue(0u));
    auto ret = RmObmmMetaRestore::RestoreAgentLocalObmmMetaData(path, localObmmMetaData);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseObmmMetaRestore, testcase_RestoreAgentLocalObmmMetaDataFail)
{
    std::string path = "/sys/devices/obmm/obmm_shm111/";
    UbseMemLocalObmmMetaData localObmmMetaData{};
    MOCKER(&RmObmmDevRead::GetMemIdType).stubs().will(returnValue(0u));
    MOCKER(&RmCommonUtils::GetFileFirstLine).stubs().will(invoke(GetFileFirstLineMockerRemoteNuma));
    MOCKER(&RmObmmDevRead::GetRemoteNuma).stubs().will(returnValue(0u));
    MOCKER(&RmObmmDevRead::GetTotalSize).stubs().will(returnValue(0u));
    MOCKER(&RmObmmDevRead::GetLocalMemId).stubs().will(returnValue(0u));
    MOCKER(UbseGetCurrentNodeInfo).stubs().will(invoke(UbseGetCurrentNodeInfoMock));
    auto ret = RmObmmMetaRestore::RestoreAgentLocalObmmMetaData(path, localObmmMetaData);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseObmmMetaRestore, testcase_RestoreAgentLocalObmmMetaDataGetCustomMetaFail)
{
    std::string path = "/sys/devices/obmm/obmm_shm111/";
    UbseMemLocalObmmMetaData localObmmMetaData{};
    MOCKER(&RmObmmDevRead::GetMemIdType).stubs().will(returnValue(0u));
    MOCKER(&RmCommonUtils::GetFileFirstLine).stubs().will(invoke(GetFileFirstLineMockerRemoteNuma));
    MOCKER(&RmObmmDevRead::GetRemoteNuma).stubs().will(returnValue(0u));
    MOCKER(&RmObmmDevRead::GetTotalSize).stubs().will(returnValue(0u));
    MOCKER(&RmObmmDevRead::GetLocalMemId).stubs().will(returnValue(0u));
    MOCKER(UbseGetCurrentNodeInfo).stubs().will(invoke(UbseGetCurrentNodeInfoMock));
    MOCKER(&RmObmmDevRead::GetMemUbMemInfo).stubs().will(returnValue(0u));
    auto ret = RmObmmMetaRestore::RestoreAgentLocalObmmMetaData(path, localObmmMetaData);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseObmmMetaRestore, testcase_RestoreAgentLocalObmmMetaDataOK)
{
    std::string path = "/sys/devices/obmm/obmm_shm111/";
    UbseMemLocalObmmMetaData localObmmMetaData{};
    MOCKER(&RmObmmDevRead::GetMemIdType).stubs().will(returnValue(0u));
    MOCKER(&RmCommonUtils::GetFileFirstLine).stubs().will(invoke(GetFileFirstLineMockerRemoteNuma));
    MOCKER(&RmObmmDevRead::GetRemoteNuma).stubs().will(returnValue(0u));
    MOCKER(&RmObmmDevRead::GetTotalSize).stubs().will(returnValue(0u));
    MOCKER(&RmObmmDevRead::GetLocalMemId).stubs().will(returnValue(0u));
    MOCKER(UbseGetCurrentNodeInfo).stubs().will(invoke(UbseGetCurrentNodeInfoMock));
    MOCKER(&RmObmmDevRead::GetCustomMeta).stubs().will(returnValue(0u));
    auto ret = RmObmmMetaRestore::RestoreAgentLocalObmmMetaData(path, localObmmMetaData);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseObmmMetaRestore, testcase_ReadAllObmmShmDevPathFail)
{
    std::string path = "/sys/devices/obmm/obmm_shm111/";
    std::vector<std::string> pathList{};
    pathList.emplace_back(path);

    DIR* dir = reinterpret_cast<DIR*>(0x1234);

    dirent entry1;
    const char *source = "obmm_shmdev1";
    size_t source_length = strlen(source) + 1;
    errno_t result = memcpy_s(entry1.d_name, sizeof(entry1.d_name), source, source_length);
    EXPECT_EQ(result, 0);
    entry1.d_type = DT_DIR;
    MOCKER(opendir).stubs().will(returnValue(dir));

    MOCKER(readdir).stubs().will(returnValue(&entry1)).then(returnValue(static_cast<dirent*>(nullptr)));

    MOCKER(closedir).stubs().will(returnValue(0));
    auto obmmShmDevPath = RmObmmMetaRestore::ReadAllObmmShmDevPath();
    EXPECT_FALSE(obmmShmDevPath.empty());
    UbseMemLocalObmmCustomMeta customMeta{};
    UbMemPrivData privData{};

    auto ret = RmObmmMetaRestore::RestoreAgentLocalObmmMetaDataFromBuffer(nullptr, 0, customMeta, privData);
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);

    int size = 1024;  // 1024
    uint8_t* buffer = new uint8_t[size];
    ret = RmObmmMetaRestore::RestoreAgentLocalObmmMetaDataFromBuffer(buffer, size, customMeta, privData);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
    delete[] buffer;

    size = sizeof(customMeta) + sizeof(UbMemPrivData);
    uint8_t* buffer1 = new uint8_t[size];
    ret = RmObmmMetaRestore::RestoreAgentLocalObmmMetaDataFromBuffer(buffer1, size, customMeta, privData);
    EXPECT_EQ(ret, UBSE_OK);
    delete[] buffer1;
}

TEST_F(TestUbseObmmMetaRestore, testcase_ReadAgentLocalObmmMetaDataOk)
{
    uint64_t task_id = 1;
    std::vector<UbseMemLocalObmmMetaData> metaDatas{};
    std::vector<std::string> paths{};
    paths.emplace_back("/sys/devices/obmm/obmm_shm111/");
    MOCKER(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));
    bool lastPage = true;
    auto ret = RmObmmMetaRestore::ReadAgentLocalObmmMetaData(task_id, metaDatas, lastPage);
    EXPECT_EQ(ret, UBSE_OK);

    MOCKER(&RmObmmMetaRestore::RestoreAgentLocalObmmMetaData).stubs().will(returnValue(UBSE_OK));
    ret = RmObmmMetaRestore::ReadAgentLocalObmmMetaData(task_id, metaDatas, lastPage);
    EXPECT_EQ(ret, UBSE_OK);
}
}  // namespace ubse::ut::mmi