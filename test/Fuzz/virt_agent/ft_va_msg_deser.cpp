/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

/*
 * DT Fuzz harness: virt_agent 服务端 IPC 消息反序列化攻击面
 *
 * 攻击面说明：
 *   ubse 守护进程通过 UDS/vsock 收到 SDK 请求后，将 IPC body 直接交给
 *   BaseMessage 子类的 Deserialize() 解析（VmSerialization TLV 流）。
 *   恶意/损坏的 IPC body 可伪造长度字段、计数、字符串超长等，
 *   本 harness 进程内直接调用各消息类的 Deserialize()/getter 做白盒攻击测试。
 *
 * 输入布局：
 *   byte[0]      消息类型选择器（0..32，其他值走 ResponseInfoMessage）
 *   byte[1..]    IPC body（交给对应消息类解析）
 *
 * 单接口测试（满足"单接口 3000 万次"口径）：
 *   FT_MSG_SELECTOR=<N> ./ft_va_msg_deser --runs=30000000 ...
 *   设置后 byte[0] 被固定为 N（只攻击第 N 号消息类），变异仍作用于全部字节。
 */

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "base_message.h"
#include "case_conf_msg.h"
#include "global_borrow_map_message.h"
#include "ham_make_decision_msg.h"
#include "ham_migrate_dst_info_message.h"
#include "ham_migrate_vm_info_message.h"
#include "mem_container_msg.h"
#include "mem_fragmentation_msg.h"
#include "mem_migrate_msg.h"
#include "migrate_state_map_message.h"
#include "response_info_message.h"

using namespace vm;
// mem_fragmentation 子命名空间中的消息类
using vm::mem_fragmentation::MemFragmentationMemBorrowParamMsg;
using vm::mem_fragmentation::MemFragmentationMemBorrowResultMsg;
using vm::mem_fragmentation::MemFragmentationNodeInfoListMsg;
using vm::mem_fragmentation::MemFragmentationPageSwapEnableMsg;

namespace {

// 消费 getter 结果，防止编译器把数据搬运优化掉（StringToC 定长拷贝路径必须真实执行）
volatile uint64_t g_sink = 0;

void SinkBytes(const void* p, size_t n)
{
    const auto* b = static_cast<const uint8_t*>(p);
    uint64_t acc = 0;
    if (n > 256) { // 大定长数组只消费头部，避免拖慢 fuzz 主循环
        n = 256;
    }
    for (size_t i = 0; i < n; ++i) {
        acc = acc * 31 + b[i];
    }
    g_sink ^= acc;
}

// 统一执行：构造消息（内部 SetInputRawData，不拷贝）-> 反序列化 -> 调用 getter 深度消费数据
template <typename MsgT, typename GetFn>
void RunMsg(const uint8_t* data, size_t size, GetFn getFn)
{
    MsgT msg(const_cast<uint8_t*>(data), static_cast<uint32_t>(size));
    (void)msg.Deserialize();
    getFn(msg);
}

} // namespace

// FT_MSG_SELECTOR 环境变量：固定消息类型选择器，实现单接口持续攻击。
// 解析失败或未设置返回 -1（保持 byte[0] 由 fuzz 数据驱动）。
static int GetFixedSelector()
{
    static int cached = []() -> int {
        const char* env = getenv("FT_MSG_SELECTOR");
        if (env == nullptr || env[0] == '\0') {
            return -1;
        }
        char* end = nullptr;
        const long v = strtol(env, &end, 10);
        if (end == env || v < 0 || v > 255) {
            fprintf(stderr, "[ft] invalid FT_MSG_SELECTOR='%s', ignoring\n", env);
            return -1;
        }
        return static_cast<int>(v);
    }();
    return cached;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 2) { // 至少 1 字节选择器 + 1 字节 body
        return 0;
    }
    // 单接口模式：byte[0] 由环境变量固定，fuzz 数据仅驱动 body
    const uint8_t selector = (GetFixedSelector() >= 0) ? static_cast<uint8_t>(GetFixedSelector()) : data[0];
    const uint8_t* body = data + 1;
    const uint32_t bodyLen = static_cast<uint32_t>(size - 1);

    switch (selector) {
        case 0:
            RunMsg<MemTaskResultQueryMsg>(body, bodyLen, [](auto& m) {
                auto r = m.GetTaskResult();
                SinkBytes(&r, sizeof(r));
            });
            break;
        case 1:
            RunMsg<MemBorrowExecuteResultMsg>(body, bodyLen, [](auto& m) {
                auto r = m.GetBorrowResult();
                SinkBytes(&r, sizeof(r));
            });
            break;
        case 2:
            RunMsg<MemFragmentationMsg>(body, bodyLen, [](auto& m) {
                std::vector<numa_info_t> v;
                (void)m.GetNumaInfo(v);
                g_sink ^= v.size();
            });
            break;
        case 3:
            RunMsg<MemFragmentationVmInfoMsg>(body, bodyLen, [](auto& m) {
                auto v = m.GetVmInfo();
                g_sink ^= v.size();
            });
            break;
        case 4:
            RunMsg<MemFragmentationMemBorrowStrategyInputMsg>(body, bodyLen, [](auto& m) {
                auto r = m.GetInputMsg();
                SinkBytes(&r, sizeof(r));
            });
            break;
        case 5:
            RunMsg<MemBorrowSettingMsg>(body, bodyLen, [](auto& m) {
                auto r = m.GetMemBorrowSettingMsg();
                SinkBytes(&r, sizeof(r));
            });
            break;
        case 6:
            RunMsg<MemFragmentationMemBorrowStrategyOutputMsg>(body, bodyLen, [](auto& m) {
                auto r = m.GetMemBorrowStrategyOutputMsg();
                SinkBytes(&r, sizeof(r));
            });
            break;
        case 7:
            RunMsg<MemFragmentationMemBorrowExecuteOutputMsg>(body, bodyLen, [](auto& m) {
                auto r = m.GetMemBorrowExecuteOutputMsg();
                g_sink ^= r.borrowIds.size() ^ r.presentNumaIds.size();
            });
            break;
        case 8:
            RunMsg<MemFragmentationMemMigrateStrategyInputMsg>(body, bodyLen, [](auto& m) {
                auto r = m.GetInputMsg();
                SinkBytes(&r, sizeof(r));
            });
            break;
        case 9:
            RunMsg<MemRollbackMsg>(body, bodyLen, [](auto& m) {
                auto r = m.GetRollbackParams();
                g_sink ^= r.node_id.size() ^ r.borrow_id_list.size() ^ r.borrow_id_size;
            });
            break;
        case 10:
            RunMsg<MemFragmentationMemMigrateStrategyOutputMsg>(body, bodyLen, [](auto& m) {
                auto r = m.GetOutputMsg();
                g_sink ^= r.vmInfoListSize ^ r.waitingTime;
            });
            break;
        case 11:
            RunMsg<MemFragmentationMemMigrateExecuteInputMsg>(body, bodyLen, [](auto& m) {
                auto r = m.GetInputMsg();
                SinkBytes(&r, sizeof(r));
            });
            break;
        case 12:
            RunMsg<MemFragmentationNodeInfoListMsg>(body, bodyLen, [](auto& m) {
                auto v = m.GetNodeInfoList();
                g_sink ^= v.size();
            });
            break;
        case 13:
            RunMsg<MemFragmentationMemBorrowParamMsg>(body, bodyLen, [](auto& m) {
                auto r = m.GetBorrowParam();
                g_sink ^= r.nodeId.size() ^ r.numaMetaInfos.size() ^ r.borrowSize ^
                          static_cast<uint64_t>(m.GetIsAsync());
            });
            break;
        case 14:
            RunMsg<MemFragmentationMemBorrowResultMsg>(body, bodyLen, [](auto& m) {
                auto v = m.GetMemBorrowResultList();
                g_sink ^= v.size();
            });
            break;
        case 15:
            RunMsg<MemFragmentationPageSwapEnableMsg>(body, bodyLen, [](auto& m) {
                auto v = m.GetPageSwapPair();
                g_sink ^= v.size();
            });
            break;
        case 16:
            RunMsg<CaseConfGetMsg>(body, bodyLen, [](auto& m) {
                auto r = m.GetCaseConf();
                SinkBytes(&r, sizeof(r));
            });
            break;
        case 17:
            RunMsg<CaseConfSetMsg>(body, bodyLen, [](auto& m) {
                auto r = m.GetCaseConf();
                SinkBytes(&r, sizeof(r));
            });
            break;
        case 18:
            RunMsg<MemMigrateMsg>(body, bodyLen, [](auto& m) {
                auto r = m.GetInputParams();
                g_sink ^= r.opt.size() ^ r.uuid.size();
            });
            break;
        case 19:
            RunMsg<HamMakeDecisionMsg>(body, bodyLen, [](auto&) {});
            break;
        case 20:
            RunMsg<MemContainerPidMemInfoInputMsg>(body, bodyLen, [](auto& m) {
                auto v = m.GetPids();
                g_sink ^= v.size();
            });
            break;
        case 21:
            RunMsg<MemContainerPidMemInfoOutputMsg>(body, bodyLen, [](auto& m) {
                auto v = m.GetPidInfos();
                g_sink ^= v.size();
            });
            break;
        case 22:
            RunMsg<UpdateWaterLineForCInputMsg>(body, bodyLen, [](auto& m) {
                auto r = m.GetWaterMark();
                SinkBytes(&r, sizeof(r));
            });
            break;
        case 23:
            RunMsg<ContainerIdListForCInputMsg>(body, bodyLen, [](auto& m) {
                auto r = m.GetContainerPidInfo();
                SinkBytes(&r, sizeof(r));
            });
            break;
        case 24:
            RunMsg<ContainerPidsForCInputMsg>(body, bodyLen, [](auto& m) {
                auto v = m.GetContainerPidInfos();
                g_sink ^= v.size();
            });
            break;
        case 25:
            RunMsg<MemContainerWaterLineMemBorrowInputMsg>(body, bodyLen, [](auto& m) {
                NodeLocInfo nodeLocInfo{};
                std::vector<uint64_t> borrowSizes;
                WaterMark waterMark{};
                (void)m.GetParams(nodeLocInfo, borrowSizes, waterMark);
                g_sink ^= borrowSizes.size() ^ waterMark.highWaterMark ^ waterMark.lowWaterMark;
            });
            break;
        case 26:
            RunMsg<MemContainerWaterLineMemBorrowOutputMsg>(body, bodyLen, [](auto&) {});
            break;
        case 27:
            RunMsg<MemContainerWaterLineMemMigrateInputMsg>(body, bodyLen, [](auto&) {});
            break;
        case 28:
            RunMsg<MemContainerWaterLineMemReturnInputMsg>(body, bodyLen, [](auto&) {});
            break;
        case 29:
            RunMsg<GlobalBorrowMapMessage>(body, bodyLen, [](auto&) {});
            break;
        case 30:
            RunMsg<HamMigrateVmInfoMessage>(body, bodyLen, [](auto& m) {
                auto v = m.GetData();
                g_sink ^= v.size();
            });
            break;
        case 31:
            RunMsg<MigrateStateMapMessage>(body, bodyLen, [](auto& m) {
                auto v = m.GetData();
                g_sink ^= v.size();
            });
            break;
        case 32:
            RunMsg<HamMigrateDstInfoMessage>(body, bodyLen, [](auto& m) {
                auto r = m.GetHamMigrateDstInfo();
                SinkBytes(&r, sizeof(r));
            });
            break;
        default:
            RunMsg<ResponseInfoMessage>(body, bodyLen, [](auto& m) {
                auto r = m.GetResponseInfo();
                SinkBytes(&r, sizeof(r));
            });
            break;
    }
    return 0;
}

// 语料目录为空时生成种子：典型 TLV 结构猜测 + 边界值模板
extern "C" int FtGenSeeds(const char* corpusDir)
{
    static const struct {
        const char* name;
        std::vector<uint8_t> bytes;
    } seeds[] = {
        {"s00_empty_len", {0x00, 0x00, 0x00, 0x00}},
        {"s01_len1", {0x00, 0x00, 0x00, 0x01, 0x41}},
        {"s02_len_big", {0xff, 0xff, 0xff, 0xff, 0x41, 0x42, 0x43, 0x44}},
        {"s03_len64k", {0x00, 0x01, 0x00, 0x00, 0x41}},
        {"s04_cnt0", {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {"s05_cnt2", {0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x08, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38}},
        {"s06_sel1_body", {0x01, 0x00, 0x00, 0x00, 0x04, 't', 'e', 's', 't'}},
        {"s07_sel16_body", {0x10, 0x00, 0x00, 0x00, 0x0a, '{', '"', 'k', '"', ':', '1', '}'}},
    };
    int written = 0;
    for (const auto& s : seeds) {
        const std::string path = std::string(corpusDir) + "/" + s.name;
        FILE* f = fopen(path.c_str(), "wb");
        if (f == nullptr) {
            continue;
        }
        if (fwrite(s.bytes.data(), 1, s.bytes.size(), f) == s.bytes.size()) {
            ++written;
        }
        fclose(f);
    }
    return written;
}
