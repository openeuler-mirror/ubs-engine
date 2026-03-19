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

#include "main_test_fuzz.h"

#include <filesystem>
#include <mockcpp/mockcpp.hpp>

#include "engine/ubse_com_engine.h"
#include "ubse_file_util.h"
#include "ubse_http_server.h"
#include "ubse_lcne_module.h"
#include "adapter_plugins/mti/ubse_topology_interface.h"
#include "ubse_uds_server.h"
#include "ubse_ipc_log.h"

namespace ubse::fuzz {
const int ARGC = 1;
const int g_fuzzCount = 1; // 用例执行的轮数
char **g_argv;

UbseContext &g_ubseContext = UbseContext::GetInstance();

void SetArgv(char **argv)
{
    g_argv = argv;
}

char **GetArgv()
{
    return g_argv;
}

int GetFuzzCount()
{
    return g_fuzzCount;
}

void ClearDir(const std::string &dirPath)
{
    std::filesystem::path path(dirPath);
    if (!exists(path)) {
        return;
    }

    remove_all(path);
}

int32_t TestServerStart()
{
    std::cout << "----------Server Creating----------" << std::endl;

    auto now = std::chrono::system_clock::now();

    MOCKER(&ubse::mti::UbseLcneModule::GetLcneData).stubs().will(returnValue(UBSE_OK));
    MOCKER(&ubse::mti::UbseLcneModule::FillNodeComInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(&ubse::mti::UbseLcneModule::SetUvsComInfo).stubs().will(returnValue(UBSE_OK));

    MOCKER(&ipc::UbseUDSServer::Start).stubs().will(returnValue(UBSE_OK));

    MOCKER(&ubse::com::UbseComEngineManager::CreateEngine).stubs().will(returnValue(UBSE_OK));
    MOCKER(&context::UbseContext::InitModule).stubs().will(returnValue(UBSE_OK));
    MOCKER(&context::UbseContext::StartModule).stubs().will(returnValue(UBSE_OK));

    MOCKER(&ubse::http::UbseHttpServer::Start).stubs().will(returnValue(true));

    ubse::mti::MtiNodeInfo fuzzUbseNodeInfo{"1", "192.168.100.100"};
    MOCKER(&ubse::mti::UbseGetLocalNodeInfo).stubs().with(outBound(fuzzUbseNodeInfo)).will(returnValue(UBSE_OK));

    MOCKER(&ubse::utils::UbseFileUtil::CreateAndChmodDirectory).stubs().will(returnValue(UBSE_OK));
    ubse::ipc::UbseIpcLog::SetLogFunc([]([[maybe_unused]]uint32_t level, [[maybe_unused]]const char *message) {});

    auto res = g_ubseContext.Run(ARGC, GetArgv());
    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double, std::milli> elapsed_seconds = end - now;
    std::cout << "Server start time: " << elapsed_seconds.count() << "ms" << std::endl;
    return res;
}

int32_t TestServerStop()
{
    std::cout << "----------Server Stopping----------" << std::endl;
    // 停止Master
    g_ubseContext.Stop();
    std::cout << "----------Server stop success----------" << std::endl;
    return UBSE_OK;
}
} // namespace ubse::fuzz
