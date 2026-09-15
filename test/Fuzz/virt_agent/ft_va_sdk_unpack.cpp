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
 * DT Fuzz harness: virt_agent SDK 响应解析攻击面（恶意/损坏的守护进程响应）
 *
 * 攻击面说明：
 *   SDK（libubs-virt-agent.so）收到 ubse 守护进程 IPC 响应后，调用
 *   ubse_*_unpack 系列函数把响应 buffer 解析为 C 结构体。
 *   若守护进程被攻破或响应被篡改，伪造的长度/计数字段将在 unpack 内部
 *   触发越界读、整型溢出（len * sizeof）等问题。本 harness 进程内直接
 *   构造"响应"喂给全部 unpack 接口做白盒攻击测试。
 *
 * 输入布局：
 *   byte[0]    unpack 目标函数选择器
 *   byte[1..]  伪造的响应 buffer
 */

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

#include "ubse_ipc_log.h"
#include "mem_fragmentation_msg.h"
#include "ubs_virt_agent_case_conf_helper.h" // 注意：与 mem_fragmentation helper 头互斥（均定义 unpack_ctx_t）
#include "ubs_virt_agent_mem_fragmentation.h" // vm_domain_info_t

using namespace vm;

// ubs_virt_agent_mem_fragmentation_helper.h 与 case_conf helper 头在同一 TU 中重复定义
// unpack_ctx_t 导致编译冲突，此处仅声明本 harness 用到的 unpack 接口（签名与 helper 头一致）。
virt_agent_ret_t ubse_node_info_unpack(uint8_t* buffer, uint32_t len, numa_info_t** numa_infos, uint32_t* node_cnt);

virt_agent_ret_t ubse_vm_info_unpack(uint8_t* buffer, uint32_t len, vm_domain_info_t** vm_infos, uint32_t* node_cnt);

virt_agent_ret_t ubse_mem_borrow_strategy_msg_unpack(uint8_t* buffer, uint32_t len, borrow_strategy_c* borrow_strategy);

virt_agent_ret_t ubse_mem_borrow_execute_msg_unpack(uint8_t* buffer, uint32_t len, mem_borrow_result_c* result);

virt_agent_ret_t ubse_mem_task_info_query_msg_unpack(uint8_t* buffer, uint32_t len, async_task_info_c* result);

virt_agent_ret_t ubse_mem_migrate_strategy_msg_unpack(uint8_t* buffer, uint32_t len, MemMigrateStrategy* strategy);

namespace {

volatile uint64_t g_sink = 0;

// 消费 unpack 结果，防止优化（sizeof 对任意完整类型均合法）
template <typename T>
void Sink(const T& v)
{
    g_sink ^= *reinterpret_cast<const unsigned char*>(&v);
    g_sink ^= sizeof(T);
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // 静音 IPC 层日志：unpack 失败路径每次执行打 ERROR/INFO（fuzz 输入大量非法，
    // 30M 次产生 ~5GB 日志并拖慢 3 倍）。日志不属于被测攻击面，注册空 sink 丢弃。
    static std::once_flag ipcLogOnce;
    std::call_once(ipcLogOnce, [] { ubse::ipc::UbseIpcLog::SetLogFunc([](uint32_t, const char*) {}); });

    if (size < 2) {
        return 0;
    }
    const uint8_t selector = data[0];
    uint8_t* buffer = const_cast<uint8_t*>(data + 1);
    const uint32_t len = static_cast<uint32_t>(size - 1);

    switch (selector % 8) {
        case 0: {
            case_conf_info_t info{};
            (void)ubse_case_conf_info_unpack(buffer, len, &info);
            g_sink ^= static_cast<uint64_t>(info.index);
            g_sink ^= static_cast<unsigned char>(info.cur_case[0]);
            break;
        }
        case 1: {
            case_conf_set_info_t info{};
            (void)ubse_case_conf_set_unpack(buffer, len, &info);
            g_sink ^= info.ret;
            break;
        }
        case 2: {
            numa_info_t* infos = nullptr;
            uint32_t cnt = 0;
            (void)ubse_node_info_unpack(buffer, len, &infos, &cnt);
            g_sink ^= cnt;
            free(infos); // unpack 内部 calloc，调用方负责释放
            break;
        }
        case 3: {
            vm_domain_info_t* infos = nullptr;
            uint32_t cnt = 0;
            (void)ubse_vm_info_unpack(buffer, len, &infos, &cnt);
            g_sink ^= cnt;
            free(infos); // unpack 内部 calloc，调用方负责释放
            break;
        }
        case 4: {
            borrow_strategy_c strategy{};
            (void)ubse_mem_borrow_strategy_msg_unpack(buffer, len, &strategy);
            Sink(strategy);
            break;
        }
        case 5: {
            mem_borrow_result_c result{};
            (void)ubse_mem_borrow_execute_msg_unpack(buffer, len, &result);
            Sink(result);
            break;
        }
        case 6: {
            async_task_info_c result{};
            (void)ubse_mem_task_info_query_msg_unpack(buffer, len, &result);
            Sink(result);
            break;
        }
        default: {
            MemMigrateStrategy strategy{};
            (void)ubse_mem_migrate_strategy_msg_unpack(buffer, len, &strategy);
            Sink(strategy);
            delete[] strategy.vmInfoList; // unpack 内部 new[]，调用方负责释放
            break;
        }
    }
    return 0;
}

// 语料目录为空时生成种子：响应 TLV 模板（长度/计数/字符串字段边界值）
extern "C" int FtGenSeeds(const char* corpusDir)
{
    static const struct {
        const char* name;
        std::vector<uint8_t> bytes;
    } seeds[] = {
        {"u00_min", {0x00, 0x00, 0x00, 0x00}},
        {"u01_len1", {0x00, 0x00, 0x00, 0x01, 0x61}},
        {"u02_len_max", {0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x01}},
        {"u03_cnt2", {0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x01, 0x02, 0x03, 0x04}},
        {"u04_cnt_max", {0x00, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xff, 0xff}},
        {"u05_strlen8", {0x00, 0x00, 0x00, 0x08, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'}},
        {"u06_strlen_trunc", {0x00, 0x00, 0x00, 0x10, 'a', 'b'}},
        {"u07_sel1", {0x01, 0x00, 0x00, 0x00, 0x04, 'c', 'a', 's', 'e'}},
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
