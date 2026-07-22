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

#include "main_test_pt.h"

#include <execinfo.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "ubse_pt_dir.h"
#include "test_pt_utils.h"

namespace ubse::pt {
using namespace ubse::pt::utils;
const int STACK_CALL_START_FLOOR = 2;
const int STACK_CALL_MAX_FLOOR = 20;
const int TEST_MAX_PROCESS_NUM = 130;
const std::string CONFIG_DEFAULT_DIR = "/conf";
const std::string BINARY_PATH = "/bin";
const std::string CONFIG_FILE = "/ubse.conf";
const std::string TEMP_CONFIG_FILE = "/ubse.conf.bak";
const std::string PLUGIN_CONFIG_FILE = "/ubse_plugin_admission.conf";
const std::string TEMP_PLUGIN_CONFIG_FILE = "/ubse_plugin_admission.conf.bak";
const uint32_t SLEEP_TIME = 1;
const int ARGC = 1;
const std::string STORE_PATH = std::string(PT_DIRECTORY) + "/store/";
const std::string MASTER_LOG_PATH = std::string(PT_DIRECTORY) + "/manager_log/";
const std::string CLI_LOG_PATH = std::string(PT_DIRECTORY) + "/cli_log/";
const std::string UDS_ADDRESS = std::string(PT_DIRECTORY) + "/uds/ubseAgentUds.socket";
UbseCmdAndFunc* g_pstCmdFuncServer = nullptr;
UbseCmdAndFunc* g_pstCmdFuncCli = nullptr;
UbseContext& g_ubseContext = UbseContext::GetInstance();
std::string g_configFilePath{};
std::string g_backupFilePath{};
std::string g_pluginConfigFilePath{};
std::string g_pluginBackupFilePath{};

// 备份文件
int32_t BackupFile(const std::string& originalFile, const std::string& backupFile)
{
    std::ifstream src(originalFile, std::ios::binary);
    std::ofstream dst(backupFile, std::ios::binary);

    if (!src.is_open() || !dst.is_open()) {
        std::cerr << "Unable to open file: " << originalFile << std::endl;
        return UBSE_ERROR;
    }

    dst << src.rdbuf();
    return UBSE_OK;
}

void ReplaceConfigItem(std::string& content, const std::string& key, const std::string& newValue)
{
    size_t pos = content.find(key + "=");
    if (pos == std::string::npos) {
        std::cerr << "configuration item not found: " << key << std::endl;
        return;
    }

    size_t endPos = content.find('\n', pos);
    if (endPos == std::string::npos) {
        endPos = content.length();
    }

    size_t valueStartPos = pos + key.length() + 1; // 跳过key和等号
    content.replace(valueStartPos, endPos - valueStartPos, newValue);
}

void InsertConfigItem(std::string& content, const std::string& section, const std::string& key,
                      const std::string& value)
{
    size_t pos = content.find("[" + section + "]");
    if (pos == std::string::npos) {
        std::cerr << "section not found: " << section << std::endl;
        return;
    }
    size_t endPos = content.find('\n', pos);
    if (endPos == std::string::npos) {
        endPos = content.length();
    }

    content.insert(endPos, "\n" + key + "=" + value + "\n");
}

void ReplaceSectionConfigItem(std::string& content, const std::string& section, const std::string& key,
                              const std::string& newValue)
{
    size_t pos = content.find("[" + section + "]");
    if (pos == std::string::npos) {
        std::cerr << "section not found: " << section << std::endl;
        return;
    }

    size_t keyPos = content.find(key + "=", pos);
    if (keyPos == std::string::npos) {
        std::cout << "configuration item not found: " << key << std::endl;
        std::cout << "Inserting new configuration item: " << key << std::endl;
        InsertConfigItem(content, section, key, newValue);
        return;
    }

    size_t endPos = content.find('\n', keyPos);
    if (endPos == std::string::npos) {
        endPos = content.length();
    }

    size_t valueStartPos = keyPos + key.length() + 1; // 跳过key和等号
    content.replace(valueStartPos, endPos - valueStartPos, newValue);
}

void DeleteConfigItem(std::string& content, const std::string& key)
{
    size_t pos = content.find(key + "=");
    if (pos == std::string::npos) {
        std::cerr << "configuration item not found: " << key << std::endl;
        return;
    }

    // 删除配置项
    size_t endPos = content.find('\n', pos);
    if (endPos == std::string::npos) {
        endPos = content.length();
    }
    content.erase(pos, endPos - pos + 1);
}

void CreateDir(const std::string& dirPath)
{
    std::filesystem::path path(dirPath);
    if (!std::filesystem::is_directory(path)) {
        if (!std::filesystem::create_directories(path)) {
            std::cerr << "Failed to create directory: " << dirPath << std::endl;
        }
    } else {
        std::cout << "Directory already exists: " << dirPath << std::endl;
    }
}

void ClearDir(const std::string& dirPath)
{
    std::filesystem::path path(dirPath);
    if (!std::filesystem::exists(path)) {
        return;
    }

    std::filesystem::remove_all(path);
}

// 更改配置文件
int32_t UpdateMasterConfig(const std::string& configPath)
{
    // 读取文件内容
    std::ifstream inFile(configPath);
    if (!inFile.is_open()) {
        std::cerr << "Error: Unable to open file: " << configPath << std::endl;
        return UBSE_ERROR;
    }
    std::stringstream buffer;
    buffer << inFile.rdbuf();
    inFile.close();
    std::string content = buffer.str();
    std::string logPath;
    // 文件内容替换
    ReplaceConfigItem(content, "nodeId", PT_NODE_ID);
    ReplaceConfigItem(content, "db.isMaster", DB_IS_MASTER);

    ReplaceConfigItem(content, "log.path", MASTER_LOG_PATH);
    ReplaceConfigItem(content, "db.store.dir", STORE_PATH);
    ReplaceSectionConfigItem(content, "ubse.common", "udsAddress", UDS_ADDRESS);

    // 写入文件
    std::ofstream outFile(configPath);
    if (!outFile.is_open()) {
        std::cerr << "Error: Unable to open file: " << configPath << std::endl;
        return UBSE_ERROR;
    }
    outFile << content;
    outFile.close();
    return UBSE_OK;
}

int32_t UpdateCliConfig(const std::string& configPath)
{
    // 读取文件内容
    std::ifstream inFile(configPath);
    if (!inFile.is_open()) {
        std::cerr << "Error: Unable to open file: " << configPath << std::endl;
        return UBSE_ERROR;
    }
    std::stringstream buffer;
    buffer << inFile.rdbuf();
    inFile.close();
    std::string content = buffer.str();
    // 文件内容替换
    ReplaceConfigItem(content, "nodeId", PT_NODE_ID);
    ReplaceConfigItem(content, "db.isMaster", DB_IS_MASTER);

    // 写入文件
    std::ofstream outFile(configPath);
    if (!outFile.is_open()) {
        std::cerr << "Error: Unable to open file: " << configPath << std::endl;
        return UBSE_ERROR;
    }
    outFile << content;
    outFile.close();
    return UBSE_OK;
}

int32_t UpdatePluginConfig(const std::string& configPath)
{
    // 读取文件内容
    std::ifstream inFile(configPath);
    if (!inFile.is_open()) {
        std::cerr << "Error: Unable to open file: " << configPath << std::endl;
        return UBSE_ERROR;
    }
    std::stringstream buffer;
    buffer << inFile.rdbuf();
    inFile.close();
    std::string content = buffer.str();
    // 删除插件配置行
    DeleteConfigItem(content, "vm");

    // 写入文件
    std::ofstream outFile(configPath);
    if (!outFile.is_open()) {
        std::cerr << "Error: Unable to open file: " << configPath << std::endl;
        return UBSE_ERROR;
    }
    outFile << content;
    outFile.close();
    return UBSE_OK;
}

// 恢复文件
int32_t RestoreFile(const std::string& originalFile, const std::string& backupFile)
{
    if (std::remove(originalFile.c_str()) != 0) {
        std::cerr << "Error: Unable to remove file: " << originalFile << std::endl;
        return UBSE_ERROR;
    }

    if (std::rename(backupFile.c_str(), originalFile.c_str()) != 0) {
        std::cerr << "Error: Unable to rename file: " << backupFile << std::endl;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

ProcessMmap* MemoryMapping()
{
    ProcessMmap* pMmap = nullptr;
    /* mmap匿名映射一段内存 */
    pMmap = static_cast<ProcessMmap*>(
        mmap(nullptr, sizeof(ProcessMmap), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0));
    if (pMmap == MAP_FAILED) {
        std::cout << "Set mmap failed, errno " << errno << std::endl;
        return nullptr;
    }
    new (pMmap) ProcessMmap();
    return pMmap;
}

// 初始化整个资源结构
void PTestResourceInit()
{
    // 说明已经申请过资源
    if (g_pstCmdFuncCli != nullptr) {
        return;
    }

    // 申请内存，代用一个大数组存放cmd和对应的处理函数func
    g_pstCmdFuncCli = new UbseCmdAndFunc[CMD_MAX];
    g_pstCmdFuncServer = new UbseCmdAndFunc[CMD_MAX];

    // 配置文件备份
    std::string absolutePath = GetArgv()[0];
    size_t pos = absolutePath.find(BINARY_PATH);
    g_configFilePath = absolutePath.substr(0, pos) + CONFIG_DEFAULT_DIR + CONFIG_FILE;
    g_backupFilePath = absolutePath.substr(0, pos) + CONFIG_DEFAULT_DIR + TEMP_CONFIG_FILE;
    BackupFile(g_configFilePath, g_backupFilePath);

    // 插件准入配置文件修改
    g_pluginConfigFilePath = absolutePath.substr(0, pos) + CONFIG_DEFAULT_DIR + PLUGIN_CONFIG_FILE;
    g_pluginBackupFilePath = absolutePath.substr(0, pos) + CONFIG_DEFAULT_DIR + TEMP_PLUGIN_CONFIG_FILE;
    BackupFile(g_pluginConfigFilePath, g_pluginBackupFilePath);
    UpdatePluginConfig(g_pluginConfigFilePath);

    // 创建文件夹
    CreateDir(MASTER_LOG_PATH);
    CreateDir(CLI_LOG_PATH);
}

unsigned int TestShowCallStack()
{
    void* array[STACK_CALL_MAX_FLOOR];
    int stackNum = backtrace(array, STACK_CALL_MAX_FLOOR);
    char** stackTrace;
    int uiLoop;
    int iPid = getpid();

    stackTrace = backtrace_symbols(array, stackNum);
    std::cout << "thread:" << syscall(SYS_gettid) << std::endl;
    for (uiLoop = STACK_CALL_START_FLOOR; uiLoop < stackNum; ++uiLoop) {
        std::cout << "[ERROR] callback:" << stackTrace[uiLoop] << std::endl;
    }
    return static_cast<unsigned int>(iPid);
}

/* 调用栈打印 */
void TestSigSegvProc(int iSigNo)
{
    unsigned int iPid;

    if (iSigNo == SIGSEGV) {
        std::cout << "=============================Call Stack============================" << std::endl;
        iPid = TestShowCallStack();
        std::cout << "=============================Process" << iPid << "quit========================" << std::endl;
        signal(SIGSEGV, SIG_DFL);
    }

    /* 执行abort生成core dump文件 */
    abort();
}

/* 在服务端进程中执行特定的命令，并处理这些命令的结果 */
void PTestServerStart(ProcessMmap* pMmap)
{
    int32_t iRet = -1;
    uint32_t uiMsgDeal = 0;
    pMmap->stServerInfo.bFlag = true;
    std::cout << "ServerStart" << std::endl;

    if (pMmap->stServerInfo.acProcName[0] != 0) {
        std::cout << "ServerInfo.acProcName:" << pMmap->stServerInfo.acProcName << std::endl;
        execl(pMmap->stServerInfo.acProcName, pMmap->stServerInfo.acProcName, "child", "suite_UbseServer_start",
              nullptr);
    }

    (void)prctl(PR_SET_NAME, "UbseServer");
    if (signal(SIGSEGV, TestSigSegvProc) == SIG_ERR) {
        std::cout << "SIG_ERR" << std::endl;
        exit(EXIT_FAILURE);
    }
    while (pMmap->bFlag && pMmap->stServerInfo.bFlag) {
        uiMsgDeal = 1;
        if (pMmap->stServerInfo.uiCmd != CMD_MAX && pMmap->stServerInfo.uiCmd != CMD_INIT &&
            g_pstCmdFuncServer[pMmap->stServerInfo.uiCmd].enCmd == pMmap->stServerInfo.uiCmd) {
            std::cout << "ServerStart running" << std::endl;
            iRet = g_pstCmdFuncServer[pMmap->stServerInfo.uiCmd].pFunc(pMmap);
            std::cout << "ServeriRet=" << iRet << std::endl;
        } else {
            sleep(1);
            uiMsgDeal = 0;
        }
        if (uiMsgDeal) {
            pMmap->stServerInfo.uiCmd = CMD_MAX;
            pMmap->iStatus = iRet;
            pMmap->stServerInfo.iStatus = iRet;
        }
    }
    pMmap->stServerInfo.bFlag = false;
    return;
}

/* 在客户端进程中执行特定的命令，并处理这些命令的结果 */
void PTestCliStart(ProcessMmap* pMmap)
{
    int32_t iRet = -1;
    uint32_t uiMsgDeal = 0;
    pMmap->stCliInfo.bFlag = true;
    std::cout << "CliStart" << std::endl;

    if (pMmap->stCliInfo.acProcName[0] != 0) {
        std::cout << "CliInfo.acProcName:" << pMmap->stCliInfo.acProcName << std::endl;
        execl(pMmap->stCliInfo.acProcName, pMmap->stCliInfo.acProcName, "child", "suite_UbseCli_start", nullptr);
    }

    (void)prctl(PR_SET_NAME, "UbseCli");
    if (signal(SIGSEGV, TestSigSegvProc) == SIG_ERR) {
        std::cout << "SIG_ERR" << std::endl;
        exit(EXIT_FAILURE);
    }
    while (pMmap->bFlag && pMmap->stCliInfo.bFlag) {
        uiMsgDeal = 1;
        if (pMmap->stCliInfo.uiCmd != CMD_MAX && pMmap->stCliInfo.uiCmd != CMD_INIT &&
            g_pstCmdFuncCli[pMmap->stCliInfo.uiCmd].enCmd == pMmap->stCliInfo.uiCmd) {
            std::cout << "CliStart running" << std::endl;
            iRet = g_pstCmdFuncCli[pMmap->stCliInfo.uiCmd].pFunc(pMmap);
            std::cout << "CliiRet=" << iRet << std::endl;
        } else {
            sleep(1);
            uiMsgDeal = 0;
        }

        if (uiMsgDeal) {
            pMmap->stCliInfo.uiCmd = CMD_MAX;
            pMmap->iStatus = iRet;
            pMmap->stCliInfo.iStatus = iRet;
        }
    }
    pMmap->stCliInfo.bFlag = false;
    return;
}

/* 创建一个子进程，并在子进程中执行一个特定的函数。 */
uint32_t PTestSetSubProcess(FuncPtr* opt, ProcessMmap* pMmap, pid_t* pid, const std::string& procName)
{
    pid_t fpid;
    /* 调用fork()函数创建一个子进程 */
    fpid = fork();
    if (fpid < 0) {
        ::std::cout << "Fork fail" << std::endl;
        return UBSE_ERROR;
    } else if (fpid == 0) {
        /* 如果procName不为空，则调用execl()函数执行procName指定的程序 */
        if (!procName.empty()) {
            execl(procName.c_str(), procName.c_str(), "child", "suite_create_process", nullptr);
        }
        std::cout << "Fork success" << std::endl;
        /* 在子进程中调用opt函数，并传入pMmap参数 */
        (*opt)(pMmap);
        /* 执行完成后退出 */
        exit(0);
    }
    std::cout << "pid for " << procName << " is " << fpid << std::endl;
    sleep(SLEEP_TIME);
    *pid = fpid;
    return UBSE_OK;
}

void SignalHandler(int32_t signo)
{
    pid_t pid = getpid();

    if (signo == SIGCHLD) {
        std::cout << "SIGCHLD pid " << pid << std::endl;
    }
    return;
}

/* 创建一个子进程 */
uint32_t PTestSetProcess(ProcessMmap* pMmap)
{
    uint32_t uiRet;
    uint32_t i;
    uint32_t uiMaxDelay = 50;
    pMmap->stServerInfo.iStatus = -1;
    pMmap->stCliInfo.iStatus = -1;
    pMmap->iStatus = -1;

    FuncPtr opt[2] = {PTestServerStart, PTestCliStart};
    std::string procName[2] = {"test_server", "test_cli"};

    signal(SIGCHLD, SignalHandler);

    pMmap->stServerInfo.uiCmd = CMD_MAX;
    pMmap->stCliInfo.uiCmd = CMD_MAX;
    pMmap->bFlag = true;
    /* 循环调用ITest_SetSubProcess函数创建服务器进程和客户端进程，并将创建的进程ID存储在pMmap->fpid数组中 */
    for (i = 0; i < NO_2; i++) {
        uiRet = PTestSetSubProcess(&opt[i], pMmap, &pMmap->fpid[i], procName[i]);
        if (uiRet != UBSE_OK) {
            std::cout << "Set sub process fail" << std::endl;
            return UBSE_ERROR;
        } else {
            std::cout << "Set sub process success" << std::endl;
        }
    }

    /* 在uiMaxDelay时间内，服务器进程和客户端进程都成功创建，则退出循环。如果超过了uiMaxDelay时间，则返回错误。 */
    i = 0;
    while (((!pMmap->stCliInfo.bFlag) || (!pMmap->stServerInfo.bFlag)) && (i++ < uiMaxDelay)) {
        /* 尽量保证monitor成功拉起 */
        sleep(1);
    }
    if (i >= uiMaxDelay) {
        std::cout << "fork child process timeout" << std::endl;
        return UBSE_ERROR;
    }

    return UBSE_OK;
}

int32_t PTestCmdServerStart(ProcessMmap* pMmap)
{
    std::cout << "----------UbseMaster Creating----------" << std::endl;
    // 清除数据库
    ClearDir(STORE_PATH);
    CreateDir(STORE_PATH);
    // 配置文件修改
    UpdateMasterConfig(g_configFilePath);
    // 启动Master
    auto now = std::chrono::system_clock::now();
    UbseResult res = g_ubseContext.Run(ARGC, GetArgv());
    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double, std::milli> elapsed_seconds = end - now;
    std::cout << "UbseMaster start time: " << elapsed_seconds.count() << "ms" << std::endl;
    std::cout << "----------UbseMaster start success----------" << std::endl;
    return res;
}

void PTestCmdAndFuncRegister(TagTestCmdInfo enCmd, FuncPtrp pFunc, ProcessMode processMode)
{
    /* 注册类型为Cli，将函数（pFunc）注册到客户端命令函数数组（g_pstCmdFuncCli） */
    if (ProcessMode::CLI == processMode) {
        g_pstCmdFuncCli[enCmd].enCmd = enCmd;
        g_pstCmdFuncCli[enCmd].pFunc = pFunc;
    } else {
        /* 注册类型为Server，将函数（pFunc）注册到服务器命令函数数组（g_pstCmdFuncServer） */
        g_pstCmdFuncServer[enCmd].enCmd = enCmd;
        g_pstCmdFuncServer[enCmd].pFunc = pFunc;
    }
}

void PTestGetCliResult(ProcessMmap* pMmap, uint32_t uiTime, int32_t* psiStatus)
{
    uint32_t uiTimeTmp = uiTime;
    while (pMmap->stCliInfo.iStatus == -1 && uiTimeTmp > 0) {
        sleep(1);
        uiTimeTmp--;
    }

    *psiStatus = pMmap->stCliInfo.iStatus;
    pMmap->stCliInfo.iStatus = -1;
    return;
}

void PTestGetServerResult(ProcessMmap* pMmap, uint32_t uiTime, int32_t* psiStatus)
{
    uint32_t uiTimeTmp = uiTime;
    while (pMmap->stServerInfo.iStatus == -1 && uiTimeTmp > 0) {
        sleep(1);
        uiTimeTmp--;
    }

    *psiStatus = pMmap->stServerInfo.iStatus;
    pMmap->stServerInfo.iStatus = -1;
    return;
}

int32_t PTestCmdCliStart(ProcessMmap* pMmap)
{
    std::cout << "----------UbseCli Creating----------" << std::endl;
    // 配置文件修改
    UpdateCliConfig(g_configFilePath);
    // 启动Cli
    UbseResult res = g_ubseContext.Run(ARGC, GetArgv(), ProcessMode::CLI);
    std::cout << "----------UbseCli start success----------" << std::endl;
    return res;
}

int32_t PTestCmdServerStop(ProcessMmap* pMmap)
{
    std::cout << "----------UbseMaster Stopping----------" << std::endl;
    // 停止Master
    g_ubseContext.Stop();
    std::cout << "----------UbseMaster stop success----------" << std::endl;
    return UBSE_OK;
}

int32_t PTestCmdCliStop(ProcessMmap* pMmap)
{
    std::cout << "----------UbseCli Stopping----------" << std::endl;
    // 停止Cli
    g_ubseContext.Stop();
    std::cout << "----------UbseCli stop success----------" << std::endl;
    return UBSE_OK;
}

void PTestQuitProcess(ProcessMmap* pMmap)
{
    uint32_t t = 0;
    uint32_t uiMaxDelay = 20;

    pMmap->bFlag = false;

    while (((pMmap->stCliInfo.bFlag) || (pMmap->stServerInfo.bFlag)) && (t++ < uiMaxDelay)) {
        /* 尽量保证monitor成功拉起 */
        sleep(1);
    }

    // 资源清理
    for (int i = 0; i < TEST_MAX_PROCESS_NUM; i++) {
        if (pMmap->fpid[i] > 0) {
            std::cout << "\r****kill subrprocess" << pMmap->fpid[i] << "****\r" << std::endl;
            kill(pMmap->fpid[i], SIGTERM);
            waitpid(pMmap->fpid[i], nullptr, 0);
        }
    }
    return;
}

void PTestClearResource(ProcessMmap* pMmap)
{
    munmap(pMmap, sizeof(ProcessMmap));
    delete[] g_pstCmdFuncCli;
    delete[] g_pstCmdFuncServer;
    // 恢复配置文件
    RestoreFile(g_configFilePath, g_backupFilePath);
    RestoreFile(g_pluginConfigFilePath, g_pluginBackupFilePath);

    // 清除数据库
    ClearDir(STORE_PATH);
}
} // namespace ubse::pt