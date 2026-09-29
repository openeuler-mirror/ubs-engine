/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include "ubse_error.h"
#include "ubse_ssu_vm_cache.h"
#include "plugin_services/ssu/ubse_ssu_service.h"
#include "rapidjson/document.h"

namespace ubse::ssu::vm_cache::ut {

using namespace ubse::plugin::service::ssu;
using ubse::ssu::controller::UbseSsuVmCache;

// ============================================================================
// 测试夹具
// ============================================================================

class TestUbseSsuVmCache : public testing::Test {
public:
    void SetUp() override
    {
        // 每个用例使用独立的临时文件,避免用例间残留
        char tmpl[] = "/tmp/ssu_vm_cache_ut_XXXXXX";
        int fd = mkstemp(tmpl);
        ASSERT_GE(fd, 0) << "mkstemp failed";
        close(fd);
        // mkstemp 创建空文件,删除后让 LoadFromDisk 走"文件不存在"路径
        unlink(tmpl);
        tempFilePath_ = tmpl;
        cache_.SetCacheFilePath(tempFilePath_);
    }

    void TearDown() override
    {
        cache_.Clear();
        unlink(tempFilePath_.c_str());
        // 清理可能残留的 .tmp
        unlink((tempFilePath_ + ".tmp").c_str());
    }

protected:
    static UbseSsuAllocResult MakeAllocResult(const std::string& name, uint64_t size, uint32_t nsCount = 1)
    {
        UbseSsuAllocResult r{};
        r.name = name;
        r.strategy = UbseSsuAllocStrategy::LINEAR;
        for (uint32_t i = 0; i < nsCount; ++i) {
            UbseSsuNameSpaceInfo ns{};
            ns.tgtEid = "eid_" + name + "_" + std::to_string(i);
            ns.tgtNqn = "nqn." + name + "." + std::to_string(i);
            ns.nsUuid = "uuid-" + name + "-" + std::to_string(i);
            ns.namespaceId = i + 1;
            ns.nsDevPath = "/dev/nvme0n" + std::to_string(i + 1);
            ns.nsSize = size;
            ns.lbaFormat = UbseSsuLBAFormat::LBA_FORMAT_512;
            ns.allowHostNqnList.push_back("nqn.host." + std::to_string(i));
            r.nameSpaceList.push_back(std::move(ns));
        }
        return r;
    }

    static UbseSsuConnectInfo MakeConnectInfo(const std::string& name, const std::string& tgtEid)
    {
        UbseSsuConnectInfo ci{};
        ci.srcEid = "src_" + name;
        ci.tgtEid = tgtEid;
        ci.tgtNqn = "nqn." + tgtEid;
        ci.hostNqn = "nqn.host." + name;
        ci.nsUuid = "uuid-" + name;
        ci.nsId = 1;
        return ci;
    }

    static std::vector<UbseSsuConnectInfo> MakeConnectList(const std::string& name, size_t n)
    {
        std::vector<UbseSsuConnectInfo> list;
        list.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            list.push_back(MakeConnectInfo(name + "_" + std::to_string(i), "eid_" + std::to_string(i)));
        }
        return list;
    }

    void WriteCacheFile(const std::string& content)
    {
        std::ofstream file(tempFilePath_);
        ASSERT_TRUE(file.is_open());
        file << content;
        ASSERT_TRUE(file.good());
    }

    std::string ReadCacheFile() const
    {
        std::ifstream file(tempFilePath_);
        EXPECT_TRUE(file.is_open());
        std::stringstream content;
        content << file.rdbuf();
        return content.str();
    }

    UbseSsuVmCache cache_;
    std::string tempFilePath_;
};

// ============================================================================
// A. 分配信息缓存
// ============================================================================

/*
 * 用例描述:推送单条分配信息后,缓存可按名查到
 * 测试步骤:
 * 1、UpdateAllocInfo 写入一条分配信息
 * 2、GetAllocInfo 按名查询
 * 预期结果:
 * 1、GetAllocInfo 返回 UBSE_OK
 * 2、返回的 name/ns 数量与写入一致
 */
TEST_F(TestUbseSsuVmCache, UpdateAllocInfo_Single_Success)
{
    auto ar = MakeAllocResult("ns1", 4096, 2);
    cache_.UpdateAllocInfo(ar);

    UbseSsuAllocResult got;
    EXPECT_EQ(cache_.GetAllocInfo("ns1", got), UBSE_OK);
    EXPECT_EQ(got.name, "ns1");
    EXPECT_EQ(got.nameSpaceList.size(), 2u);
}

/*
 * 用例描述:重复推送同名分配信息,后写覆盖前写
 * 测试步骤:
 * 1、UpdateAllocInfo 写入 ns1(nsCount=1)
 * 2、UpdateAllocInfo 再次写入 ns1(nsCount=3)
 * 3、GetAllocInfo 查询
 * 预期结果:
 * 1、返回 nsCount=3(以最新写入为准)
 */
TEST_F(TestUbseSsuVmCache, UpdateAllocInfo_Duplicate_Overwrite)
{
    cache_.UpdateAllocInfo(MakeAllocResult("ns1", 4096, 1));
    cache_.UpdateAllocInfo(MakeAllocResult("ns1", 8192, 3));

    UbseSsuAllocResult got;
    EXPECT_EQ(cache_.GetAllocInfo("ns1", got), UBSE_OK);
    EXPECT_EQ(got.nameSpaceList.size(), 3u);
    EXPECT_EQ(got.nameSpaceList[0].nsSize, 8192u);
}

/*
 * 用例描述:查询已存在的 namespace 返回正确分配信息
 * 测试步骤:
 * 1、写入分配信息
 * 2、按名查询
 * 预期结果:
 * 1、字段全部正确
 */
TEST_F(TestUbseSsuVmCache, GetAllocInfo_Exists_ReturnsResult)
{
    auto ar = MakeAllocResult("alloc1", 4096, 1);
    cache_.UpdateAllocInfo(ar);

    UbseSsuAllocResult got;
    EXPECT_EQ(cache_.GetAllocInfo("alloc1", got), UBSE_OK);
    EXPECT_EQ(got.name, "alloc1");
    EXPECT_EQ(got.nameSpaceList[0].tgtEid, "eid_alloc1_0");
    EXPECT_EQ(got.nameSpaceList[0].allowHostNqnList.size(), 1u);
}

/*
 * 用例描述:查询不存在的 namespace 返回 SPACE_NOT_FOUND
 * 测试步骤:
 * 1、空缓存查询 "not_exists"
 * 预期结果:
 * 1、返回 UBSE_SSU_ERROR_SPACE_NOT_FOUND
 */
TEST_F(TestUbseSsuVmCache, GetAllocInfo_NotExists_ReturnsError)
{
    UbseSsuAllocResult got;
    EXPECT_EQ(cache_.GetAllocInfo("not_exists", got), UBSE_SSU_ERROR_SPACE_NOT_FOUND);
}

/*
 * 用例描述:空缓存 ListAllocInfo 返回空列表
 * 测试步骤:
 * 1、ListAllocInfo 查询空缓存
 * 预期结果:
 * 1、返回 UBSE_OK 且 result 为空
 */
TEST_F(TestUbseSsuVmCache, ListAllocInfo_Empty_ReturnsEmpty)
{
    std::vector<UbseSsuAllocResult> result;
    EXPECT_EQ(cache_.ListAllocInfo(result), UBSE_OK);
    EXPECT_TRUE(result.empty());
}

/*
 * 用例描述:多条分配信息查询返回全部
 * 测试步骤:
 * 1、写入 3 条不同 name 的分配信息
 * 2、ListAllocInfo
 * 预期结果:
 * 1、返回 3 条
 */
TEST_F(TestUbseSsuVmCache, ListAllocInfo_Multiple_ReturnsAll)
{
    cache_.UpdateAllocInfo(MakeAllocResult("a1", 4096));
    cache_.UpdateAllocInfo(MakeAllocResult("a2", 4096));
    cache_.UpdateAllocInfo(MakeAllocResult("a3", 4096));

    std::vector<UbseSsuAllocResult> result;
    EXPECT_EQ(cache_.ListAllocInfo(result), UBSE_OK);
    EXPECT_EQ(result.size(), 3u);
}

// ============================================================================
// B. 连接信息缓存
// ============================================================================

/*
 * 用例描述:推送单条连接信息后,缓存可按名查到
 * 测试步骤:
 * 1、UpdateConnectInfo 写入连接信息列表
 * 2、GetConnectInfo 按名查询
 * 预期结果:
 * 1、返回列表与写入一致
 */
TEST_F(TestUbseSsuVmCache, UpdateConnectInfo_Single_Success)
{
    auto list = MakeConnectList("conn1", 2);
    cache_.UpdateConnectInfo("conn1", list);

    std::vector<UbseSsuConnectInfo> got;
    EXPECT_EQ(cache_.GetConnectInfo("conn1", got), UBSE_OK);
    EXPECT_EQ(got.size(), 2u);
    EXPECT_EQ(got[0].tgtEid, "eid_0");
}

/*
 * 用例描述:查询已存在的连接信息返回正确列表
 * 测试步骤:
 * 1、写入 3 条连接信息
 * 2、按名查询
 * 预期结果:
 * 1、列表大小=3,字段正确
 */
TEST_F(TestUbseSsuVmCache, GetConnectInfo_Exists_ReturnsList)
{
    cache_.UpdateConnectInfo("c1", MakeConnectList("c1", 3));

    std::vector<UbseSsuConnectInfo> got;
    EXPECT_EQ(cache_.GetConnectInfo("c1", got), UBSE_OK);
    EXPECT_EQ(got.size(), 3u);
    EXPECT_EQ(got[1].hostNqn, "nqn.host.c1_1");
}

/*
 * 用例描述:重复推送同名连接信息,后写覆盖前写
 * 测试步骤:
 * 1、UpdateConnectInfo 写入 c1(1 条,hostNqn=old)
 * 2、UpdateConnectInfo 再次写入 c1(2 条,hostNqn=new)
 * 3、GetConnectInfo 查询
 * 预期结果:
 * 1、返回 2 条新列表(以最新写入为准)
 */
TEST_F(TestUbseSsuVmCache, UpdateConnectInfo_Duplicate_Overwrite)
{
    cache_.UpdateConnectInfo("c1", MakeConnectList("old", 1));
    cache_.UpdateConnectInfo("c1", MakeConnectList("new", 2));

    std::vector<UbseSsuConnectInfo> got;
    ASSERT_EQ(cache_.GetConnectInfo("c1", got), UBSE_OK);
    ASSERT_EQ(got.size(), 2u);
    EXPECT_EQ(got[0].hostNqn, "nqn.host.new_0");
}

/*
 * 用例描述:查询不存在的 namespace 连接信息返回空
 * 测试步骤:
 * 1、查询未写入的 name
 * 预期结果:
 * 1、返回 UBSE_SSU_ERROR_SPACE_NOT_FOUND, 列表为空
 */
TEST_F(TestUbseSsuVmCache, GetConnectInfo_NotExists_ReturnsError)
{
    std::vector<UbseSsuConnectInfo> got;
    EXPECT_EQ(cache_.GetConnectInfo("not_exists", got), UBSE_SSU_ERROR_SPACE_NOT_FOUND);
    EXPECT_TRUE(got.empty());
}

// ============================================================================
// C. attach/detach 状态管理
// ============================================================================

/*
 * 用例描述:标记未 attach 的 namespace 为已 attach,IsAttached 返回 true
 * 测试步骤:
 * 1、写入分配信息
 * 2、MarkAttached
 * 3、IsAttached 查询
 * 预期结果:
 * 1、IsAttached 返回 true
 */
TEST_F(TestUbseSsuVmCache, MarkAttached_NotAttached_Success)
{
    cache_.UpdateAllocInfo(MakeAllocResult("ns1", 4096));
    EXPECT_FALSE(cache_.IsAttached("ns1"));
    cache_.MarkAttached("ns1");
    EXPECT_TRUE(cache_.IsAttached("ns1"));
}

/*
 * 用例描述:重复 MarkAttached 幂等,IsAttached 仍为 true
 * 测试步骤:
 * 1、MarkAttached 两次
 * 预期结果:
 * 1、IsAttached 返回 true
 */
TEST_F(TestUbseSsuVmCache, MarkAttached_AlreadyAttached_Idempotent)
{
    cache_.UpdateAllocInfo(MakeAllocResult("ns1", 4096));
    cache_.MarkAttached("ns1");
    cache_.MarkAttached("ns1");
    EXPECT_TRUE(cache_.IsAttached("ns1"));
}

/*
 * 用例描述:已 attach 的 namespace MarkDetached 后 IsAttached 返回 false
 * 测试步骤:
 * 1、MarkAttached → MarkDetached → IsAttached
 * 预期结果:
 * 1、IsAttached 返回 false
 */
TEST_F(TestUbseSsuVmCache, MarkDetached_Attached_Success)
{
    cache_.UpdateAllocInfo(MakeAllocResult("ns1", 4096));
    cache_.MarkAttached("ns1");
    cache_.MarkDetached("ns1");
    EXPECT_FALSE(cache_.IsAttached("ns1"));
}

/*
 * 用例描述:detach 未 attach 的 namespace 幂等
 * 测试步骤:
 * 1、MarkDetached(未先 attach)
 * 预期结果:
 * 1、IsAttached 返回 false(无异常)
 */
TEST_F(TestUbseSsuVmCache, MarkDetached_NotAttached_Idempotent)
{
    cache_.UpdateAllocInfo(MakeAllocResult("ns1", 4096));
    cache_.MarkDetached("ns1");
    EXPECT_FALSE(cache_.IsAttached("ns1"));
}

/*
 * 用例描述:查询不在缓存中的 namespace 的 attach 状态返回 false
 * 测试步骤:
 * 1、IsAttached("not_in_cache")
 * 预期结果:
 * 1、返回 false
 */
TEST_F(TestUbseSsuVmCache, IsAttached_NotInCache_ReturnsFalse)
{
    EXPECT_FALSE(cache_.IsAttached("not_in_cache"));
}

// ============================================================================
// D. 持久化加载 LoadFromDisk
// ============================================================================

/*
 * 用例描述:缓存文件不存在时 LoadFromDisk 返回 false
 * 测试步骤:
 * 1、SetCacheFilePath 指向不存在的路径
 * 2、LoadFromDisk
 * 预期结果:
 * 1、返回 false, 缓存为空
 */
TEST_F(TestUbseSsuVmCache, LoadFromDisk_FileNotExists_ReturnsFalse)
{
    cache_.SetCacheFilePath("/tmp/ssu_vm_cache_ut_not_exists_12345.json");
    EXPECT_FALSE(cache_.LoadFromDisk());

    std::vector<UbseSsuAllocResult> result;
    cache_.ListAllocInfo(result);
    EXPECT_TRUE(result.empty());
}

/*
 * 用例描述:合法缓存文件 LoadFromDisk 返回 true 且内存填充正确
 * 测试步骤:
 * 1、写入数据并 FlushToDisk
 * 2、新建 cache 实例 LoadFromDisk
 * 预期结果:
 * 1、返回 true, allocInfoMap_/connectInfoMap_ 正确填充
 */
TEST_F(TestUbseSsuVmCache, LoadFromDisk_ValidFile_ReturnsTrue)
{
    cache_.UpdateAllocInfo(MakeAllocResult("ns1", 4096, 2));
    cache_.UpdateConnectInfo("ns1", MakeConnectList("ns1", 1));
    ASSERT_TRUE(cache_.FlushToDisk());

    UbseSsuVmCache cache2;
    cache2.SetCacheFilePath(tempFilePath_);
    EXPECT_TRUE(cache2.LoadFromDisk());

    UbseSsuAllocResult ar;
    EXPECT_EQ(cache2.GetAllocInfo("ns1", ar), UBSE_OK);
    EXPECT_EQ(ar.nameSpaceList.size(), 2u);

    std::vector<UbseSsuConnectInfo> ci;
    EXPECT_EQ(cache2.GetConnectInfo("ns1", ci), UBSE_OK);
    EXPECT_EQ(ci.size(), 1u);
}

/*
 * 用例描述:JSON 解析失败(文件损坏)时删除文件并返回 false
 * 测试步骤:
 * 1、写入损坏内容到缓存文件
 * 2、LoadFromDisk
 * 预期结果:
 * 1、返回 false
 * 2、文件被删除
 */
TEST_F(TestUbseSsuVmCache, LoadFromDisk_JsonParseError_DeletesFile)
{
    // 写入无效 JSON
    std::ofstream badFile(tempFilePath_);
    badFile << "{ this is not valid json }}}";
    badFile.close();

    EXPECT_FALSE(cache_.LoadFromDisk());
    // 损坏文件应被删除
    std::ifstream check(tempFilePath_);
    EXPECT_FALSE(check.good()) << "损坏的缓存文件应被删除";
}

/*
 * 用例描述:文件 version 不匹配时删除文件并返回 false
 * 测试步骤:
 * 1、写入 version=999 的缓存文件
 * 2、LoadFromDisk
 * 预期结果:
 * 1、返回 false, 文件被删除
 */
TEST_F(TestUbseSsuVmCache, LoadFromDisk_VersionMismatch_DeletesFile)
{
    WriteCacheFile(R"({"version":999,"lastSyncTs":1,"allocList":[],"connectList":[]})");

    EXPECT_FALSE(cache_.LoadFromDisk());
    std::ifstream check(tempFilePath_);
    EXPECT_FALSE(check.good()) << "版本不匹配的缓存文件应被删除";
}

/*
 * 用例描述:缺失顶层必填字段时拒绝加载,且不修改现有内存缓存
 * 测试步骤:
 * 1、写入一条分配信息到内存缓存
 * 2、写入缺失 connectList 的缓存文件
 * 3、LoadFromDisk
 * 预期结果:
 * 1、返回 false
 * 2、内存缓存中已有数据不受影响
 * 3、文件被删除
 */
TEST_F(TestUbseSsuVmCache, LoadFromDisk_MissingRequiredField_PreservesMemoryCache)
{
    cache_.UpdateAllocInfo(MakeAllocResult("existing", 4096));
    WriteCacheFile(R"({"version":1,"lastSyncTs":1,"allocList":[]})");

    EXPECT_FALSE(cache_.LoadFromDisk());
    UbseSsuAllocResult result{};
    EXPECT_EQ(cache_.GetAllocInfo("existing", result), UBSE_OK);
    EXPECT_EQ(result.name, "existing");
    EXPECT_EQ(access(tempFilePath_.c_str(), F_OK), -1);
}

/*
 * 用例描述:分配信息的必填子字段缺失时判定缓存文件损坏
 * 测试步骤:
 * 1、写入 nameSpaceList 条目为空对象的缓存文件
 * 2、LoadFromDisk
 * 预期结果:
 * 1、返回 false
 * 2、文件被删除
 */
TEST_F(TestUbseSsuVmCache, LoadFromDisk_MissingNestedField_DeletesFile)
{
    WriteCacheFile(R"({
        "version":1,
        "lastSyncTs":1,
        "allocList":[{"name":"bad","strategy":1,"nameSpaceList":[{}]}],
        "connectList":[]
    })");

    EXPECT_FALSE(cache_.LoadFromDisk());
    EXPECT_EQ(access(tempFilePath_.c_str(), F_OK), -1);
}

/*
 * 用例描述:非法分配策略枚举值不得进入缓存
 * 测试步骤:
 * 1、写入 strategy=99 的缓存文件
 * 2、LoadFromDisk
 * 预期结果:
 * 1、返回 false
 * 2、文件被删除
 */
TEST_F(TestUbseSsuVmCache, LoadFromDisk_InvalidStrategy_DeletesFile)
{
    WriteCacheFile(R"({
        "version":1,
        "lastSyncTs":1,
        "allocList":[{"name":"bad","strategy":99,"nameSpaceList":[]}],
        "connectList":[]
    })");

    EXPECT_FALSE(cache_.LoadFromDisk());
    EXPECT_EQ(access(tempFilePath_.c_str(), F_OK), -1);
}

/*
 * 用例描述:从磁盘重新加载时不恢复运行期 attach 状态
 * 测试步骤:
 * 1、写入分配信息并 MarkAttached
 * 2、FlushToDisk 后确认 IsAttached 为 true
 * 3、LoadFromDisk 重新加载
 * 预期结果:
 * 1、返回 true
 * 2、IsAttached 返回 false(attachedNames_ 重置为空)
 */
TEST_F(TestUbseSsuVmCache, LoadFromDisk_ValidFile_ClearsAttachedState)
{
    cache_.UpdateAllocInfo(MakeAllocResult("ns1", 4096));
    cache_.MarkAttached("ns1");
    ASSERT_TRUE(cache_.FlushToDisk());
    ASSERT_TRUE(cache_.IsAttached("ns1"));

    ASSERT_TRUE(cache_.LoadFromDisk());
    EXPECT_FALSE(cache_.IsAttached("ns1"));
}

/*
 * 用例描述:空文件 LoadFromDisk 返回 false 并删除文件
 * 测试步骤:
 * 1、创建空文件
 * 2、LoadFromDisk
 * 预期结果:
 * 1、返回 false, 文件被删除
 */
TEST_F(TestUbseSsuVmCache, LoadFromDisk_EmptyFile_ReturnsFalse)
{
    std::ofstream emptyFile(tempFilePath_);
    emptyFile.close();

    EXPECT_FALSE(cache_.LoadFromDisk());
    std::ifstream check(tempFilePath_);
    EXPECT_FALSE(check.good()) << "空缓存文件应被删除";
}

// ============================================================================
// E. 持久化落盘 FlushToDisk
// ============================================================================

/*
 * 用例描述:正常落盘后文件内容与内存一致
 * 测试步骤:
 * 1、写入数据
 * 2、FlushToDisk
 * 3、重新 LoadFromDisk 校验
 * 预期结果:
 * 1、落盘成功
 * 2、加载后数据一致
 */
TEST_F(TestUbseSsuVmCache, FlushToDisk_Normal_Success)
{
    cache_.UpdateAllocInfo(MakeAllocResult("ns1", 4096, 1));
    cache_.UpdateAllocInfo(MakeAllocResult("ns2", 8192, 2));
    cache_.UpdateConnectInfo("ns1", MakeConnectList("ns1", 2));

    EXPECT_TRUE(cache_.FlushToDisk());

    UbseSsuVmCache cache2;
    cache2.SetCacheFilePath(tempFilePath_);
    ASSERT_TRUE(cache2.LoadFromDisk());

    std::vector<UbseSsuAllocResult> list;
    cache2.ListAllocInfo(list);
    EXPECT_EQ(list.size(), 2u);

    std::vector<UbseSsuConnectInfo> ci;
    cache2.GetConnectInfo("ns1", ci);
    EXPECT_EQ(ci.size(), 2u);
}

/*
 * 用例描述:空缓存落盘,文件写入空列表(可正常加载)
 * 测试步骤:
 * 1、空缓存 FlushToDisk
 * 2、新实例 LoadFromDisk
 * 预期结果:
 * 1、落盘成功
 * 2、加载后仍为空
 */
TEST_F(TestUbseSsuVmCache, FlushToDisk_EmptyCache_WriteEmptyFile)
{
    EXPECT_TRUE(cache_.FlushToDisk());

    UbseSsuVmCache cache2;
    cache2.SetCacheFilePath(tempFilePath_);
    EXPECT_TRUE(cache2.LoadFromDisk());

    std::vector<UbseSsuAllocResult> list;
    cache2.ListAllocInfo(list);
    EXPECT_TRUE(list.empty());
}

/*
 * 用例描述:落盘 JSON 顶层结构为 version/lastSyncTs/allocList/connectList,不含旧字段
 * 测试步骤:
 * 1、写入分配信息与连接信息后 FlushToDisk
 * 2、解析落盘 JSON,校验顶层字段
 * 预期结果:
 * 1、JSON 解析成功,version=1
 * 2、含 lastSyncTs/allocList/connectList,且不含旧字段 allocInfos/connectInfos
 */
TEST_F(TestUbseSsuVmCache, FlushToDisk_WritesDocumentedSchema)
{
    cache_.UpdateAllocInfo(MakeAllocResult("ns1", 4096));
    cache_.UpdateConnectInfo("ns1", MakeConnectList("ns1", 1));
    ASSERT_TRUE(cache_.FlushToDisk());

    rapidjson::Document doc;
    const auto content = ReadCacheFile();
    doc.Parse(content.c_str());
    ASSERT_FALSE(doc.HasParseError());
    ASSERT_TRUE(doc.IsObject());
    ASSERT_TRUE(doc.HasMember("version"));
    EXPECT_EQ(doc["version"].GetUint(), 1u);
    EXPECT_TRUE(doc.HasMember("lastSyncTs"));
    ASSERT_TRUE(doc.HasMember("allocList"));
    EXPECT_TRUE(doc["allocList"].IsArray());
    ASSERT_TRUE(doc.HasMember("connectList"));
    EXPECT_TRUE(doc["connectList"].IsArray());
    EXPECT_FALSE(doc.HasMember("allocInfos"));
    EXPECT_FALSE(doc.HasMember("connectInfos"));
}

/*
 * 用例描述:落盘文件不含 attachedNames 字段,重新加载后 attach 状态为空
 * 测试步骤:
 * 1、写入分配信息并 MarkAttached
 * 2、FlushToDisk,解析落盘 JSON
 * 3、新实例 LoadFromDisk 后查询 IsAttached
 * 预期结果:
 * 1、文件无 attachedNames 成员
 * 2、新实例 IsAttached 返回 false
 */
TEST_F(TestUbseSsuVmCache, FlushToDisk_AttachedNamesNotInFile)
{
    cache_.UpdateAllocInfo(MakeAllocResult("ns1", 4096));
    cache_.MarkAttached("ns1");
    ASSERT_TRUE(cache_.FlushToDisk());

    rapidjson::Document doc;
    const auto content = ReadCacheFile();
    doc.Parse(content.c_str());
    ASSERT_FALSE(doc.HasParseError());
    ASSERT_TRUE(doc.IsObject());
    EXPECT_FALSE(doc.HasMember("attachedNames"));

    UbseSsuVmCache cache2;
    cache2.SetCacheFilePath(tempFilePath_);
    ASSERT_TRUE(cache2.LoadFromDisk());
    EXPECT_FALSE(cache2.IsAttached("ns1"));
}

/*
 * 用例描述:落盘文件权限为 0600(仅属主读写)
 * 测试步骤:
 * 1、FlushToDisk
 * 2、stat 检查文件权限位
 * 预期结果:
 * 1、st_mode & 0777 == S_IRUSR | S_IWUSR
 */
TEST_F(TestUbseSsuVmCache, FlushToDisk_SetsOwnerReadWriteOnlyPermission)
{
    ASSERT_TRUE(cache_.FlushToDisk());

    struct stat fileStat {};
    ASSERT_EQ(stat(tempFilePath_.c_str(), &fileStat), 0);
    EXPECT_EQ(fileStat.st_mode & 0777, S_IRUSR | S_IWUSR);
}

/*
 * 用例描述:遗留的 .tmp 文件权限过宽时,写入前强制收紧为 0600
 * 测试步骤:
 * 1、预置权限为 0644 的 .tmp 文件
 * 2、FlushToDisk
 * 3、stat 检查正式文件权限位
 * 预期结果:
 * 1、落盘成功
 * 2、正式文件权限为 S_IRUSR | S_IWUSR
 */
TEST_F(TestUbseSsuVmCache, FlushToDisk_ExistingTmpFileResetsPermission)
{
    const std::string tmpPath = tempFilePath_ + ".tmp";
    {
        std::ofstream tmpFile(tmpPath);
        ASSERT_TRUE(tmpFile.is_open());
        tmpFile << "stale";
    }
    ASSERT_EQ(chmod(tmpPath.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH), 0);

    ASSERT_TRUE(cache_.FlushToDisk());
    struct stat fileStat {};
    ASSERT_EQ(stat(tempFilePath_.c_str(), &fileStat), 0);
    EXPECT_EQ(fileStat.st_mode & 0777, S_IRUSR | S_IWUSR);
}

/*
 * 用例描述:缓存目录无法创建(父路径为普通文件)时落盘返回失败
 * 测试步骤:
 * 1、SetCacheFilePath 指向普通文件路径下的子文件
 * 2、FlushToDisk
 * 预期结果:
 * 1、返回 false
 */
TEST_F(TestUbseSsuVmCache, FlushToDisk_OpenFailure_ReturnsFalse)
{
    // 用普通文件充当父路径,注入"无法创建缓存目录"的失败场景(父目录缺失已支持自动创建)
    char fileTmpl[] = "/tmp/ssu_vm_cache_ut_file_XXXXXX";
    int fd = mkstemp(fileTmpl);
    ASSERT_GE(fd, 0) << "mkstemp failed";
    close(fd);
    cache_.SetCacheFilePath(std::string(fileTmpl) + "/cache.json");
    EXPECT_FALSE(cache_.FlushToDisk());
    unlink(fileTmpl);
}

/*
 * 用例描述:缓存父目录不存在时自动创建(权限 0700)并成功落盘
 * 测试步骤:
 * 1、SetCacheFilePath 指向不存在的父目录
 * 2、FlushToDisk
 * 预期结果:
 * 1、FlushToDisk 返回 true
 * 2、父目录被创建且权限为 0700
 * 3、缓存文件存在
 */
TEST_F(TestUbseSsuVmCache, FlushToDisk_MissingParentDir_AutoCreated)
{
    cache_.SetCacheFilePath(tempFilePath_ + "/cache.json");
    EXPECT_TRUE(cache_.FlushToDisk());
    struct stat st{};
    ASSERT_EQ(stat(tempFilePath_.c_str(), &st), 0);
    EXPECT_TRUE(S_ISDIR(st.st_mode));
    EXPECT_EQ(st.st_mode & static_cast<mode_t>(0777), static_cast<mode_t>(0700));
    ASSERT_EQ(stat((tempFilePath_ + "/cache.json").c_str(), &st), 0);
    EXPECT_TRUE(S_ISREG(st.st_mode));
    unlink((tempFilePath_ + "/cache.json").c_str());
    rmdir(tempFilePath_.c_str());
}

// ============================================================================
// F. 全量与清空
// ============================================================================

/*
 * 用例描述:清空已填充缓存后所有 map 为空
 * 测试步骤:
 * 1、写入 alloc/connect/attached
 * 2、Clear
 * 3、查询
 * 预期结果:
 * 1、ListAllocInfo 为空
 * 2、GetConnectInfo 返回 NOT_FOUND
 * 3、IsAttached 返回 false
 */
TEST_F(TestUbseSsuVmCache, Clear_AfterPopulated_AllMapsEmpty)
{
    cache_.UpdateAllocInfo(MakeAllocResult("ns1", 4096));
    cache_.UpdateConnectInfo("ns1", MakeConnectList("ns1", 1));
    cache_.MarkAttached("ns1");

    cache_.Clear();

    std::vector<UbseSsuAllocResult> list;
    cache_.ListAllocInfo(list);
    EXPECT_TRUE(list.empty());

    std::vector<UbseSsuConnectInfo> ci;
    EXPECT_EQ(cache_.GetConnectInfo("ns1", ci), UBSE_SSU_ERROR_SPACE_NOT_FOUND);
    EXPECT_FALSE(cache_.IsAttached("ns1"));
}

/*
 * 用例描述:Clear 与并发查询交叉执行,读线程要么读到空缓存、要么读到一致快照,不崩溃
 * 测试步骤:
 * 1、写入 alloc/connect/attached
 * 2、并发执行"重灌 + Clear"与 ListAllocInfo/GetAllocInfo/IsAttached
 * 3、等待所有线程结束
 * 预期结果:
 * 1、不崩溃(读写锁保护下无数据竞争)
 * 2、结束后最后一次操作为 Clear,查询返回空
 */
TEST_F(TestUbseSsuVmCache, Clear_DuringRead_QueryReturnsEmpty)
{
    cache_.UpdateAllocInfo(MakeAllocResult("ns1", 4096));
    cache_.UpdateConnectInfo("ns1", MakeConnectList("ns1", 1));
    cache_.MarkAttached("ns1");

    constexpr int N = 50;
    std::vector<std::thread> threads;

    // Clear 线程:每轮重灌数据后清空,保证结束时缓存为空
    threads.emplace_back([&]() {
        for (int j = 0; j < N; ++j) {
            cache_.UpdateAllocInfo(MakeAllocResult("ns1", 4096));
            cache_.UpdateConnectInfo("ns1", MakeConnectList("ns1", 1));
            cache_.Clear();
        }
    });
    // 读线程:与 Clear 并发查询,只验证不崩溃(读到空或旧快照均合法)
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < N; ++j) {
                std::vector<UbseSsuAllocResult> list;
                cache_.ListAllocInfo(list);
                UbseSsuAllocResult ar;
                cache_.GetAllocInfo("ns1", ar);
                std::vector<UbseSsuConnectInfo> ci;
                cache_.GetConnectInfo("ns1", ci);
                cache_.IsAttached("ns1");
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }

    std::vector<UbseSsuAllocResult> list;
    cache_.ListAllocInfo(list);
    EXPECT_TRUE(list.empty());
}

/*
 * 用例描述:清空后批量更新,所有数据正确填充
 * 测试步骤:
 * 1、Clear
 * 2、批量 UpdateAllocInfo + UpdateConnectInfo
 * 3、ListAllocInfo 校验
 * 预期结果:
 * 1、数据全部填充
 */
TEST_F(TestUbseSsuVmCache, FullSync_PopulateFromEmpty_Success)
{
    cache_.Clear();
    for (int i = 0; i < 5; ++i) {
        auto name = "ns" + std::to_string(i);
        cache_.UpdateAllocInfo(MakeAllocResult(name, 4096 * (i + 1)));
        cache_.UpdateConnectInfo(name, MakeConnectList(name, static_cast<size_t>(i + 1)));
    }

    std::vector<UbseSsuAllocResult> list;
    cache_.ListAllocInfo(list);
    EXPECT_EQ(list.size(), 5u);
}

// ============================================================================
// G. 并发安全
// ============================================================================

/*
 * 用例描述:并发读写无数据竞争(读不阻塞写)
 * 测试步骤:
 * 1、多线程并发 UpdateAllocInfo / GetAllocInfo / ListAllocInfo
 * 2、等待所有线程完成
 * 预期结果:
 * 1、不崩溃
 * 2、最终数据一致
 */
TEST_F(TestUbseSsuVmCache, UpdateAllocInfo_ConcurrentReadWrite_NoRace)
{
    constexpr int N = 100;
    std::vector<std::thread> threads;

    // 写线程
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < N; ++j) {
                cache_.UpdateAllocInfo(MakeAllocResult("w" + std::to_string(i) + "_" + std::to_string(j), 4096));
            }
        });
    }
    // 读线程
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < N; ++j) {
                std::vector<UbseSsuAllocResult> list;
                cache_.ListAllocInfo(list);
                UbseSsuAllocResult ar;
                cache_.GetAllocInfo("w0_0", ar);
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }

    std::vector<UbseSsuAllocResult> list;
    cache_.ListAllocInfo(list);
    EXPECT_EQ(list.size(), static_cast<size_t>(4 * N));
}

/*
 * 用例描述:多线程并发落盘被 flushMutex_ 串行化,文件内容完整
 * 测试步骤:
 * 1、8 个线程各自写入一条分配信息并 FlushToDisk
 * 2、等待线程结束,新实例 LoadFromDisk 校验
 * 预期结果:
 * 1、所有线程落盘返回 true
 * 2、加载后数据完整(8 条分配信息)
 */
TEST_F(TestUbseSsuVmCache, FlushToDisk_Concurrent_Serialized)
{
    constexpr int N = 8;
    std::vector<int> flushResults(N, 0);
    std::vector<std::thread> threads;
    threads.reserve(N);
    for (int i = 0; i < N; ++i) {
        threads.emplace_back([&, i]() {
            cache_.UpdateAllocInfo(MakeAllocResult("ns" + std::to_string(i), 4096));
            flushResults[i] = cache_.FlushToDisk() ? 1 : 0;
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    for (const auto result : flushResults) {
        EXPECT_EQ(result, 1);
    }

    UbseSsuVmCache cache2;
    cache2.SetCacheFilePath(tempFilePath_);
    ASSERT_TRUE(cache2.LoadFromDisk());
    std::vector<UbseSsuAllocResult> allocList;
    ASSERT_EQ(cache2.ListAllocInfo(allocList), UBSE_OK);
    EXPECT_EQ(allocList.size(), static_cast<size_t>(N));
}

} // namespace ubse::ssu::vm_cache::ut
