# UBS-Engine Code Wiki

> **UBSE (UB Service Core Engine)** —— 软件定义计算、资源按需组合与分配引擎。
> C++17 / C11 CMake 项目，目标平台 openEuler Linux (ARM64)。
>
> 本文档由源码分析自动整理，覆盖项目整体架构、主要模块职责、关键类与函数、依赖关系以及运行方式。

---

## 目录

- [1. 项目概览](#1-项目概览)
- [2. 整体架构](#2-整体架构)
- [3. 目录结构](#3-目录结构)
- [4. 构建与运行](#4-构建与运行)
- [5. 核心启动流程](#5-核心启动流程)
- [6. Framework 框架层](#6-framework-框架层)
  - [6.1 context 上下文](#61-context-上下文)
  - [6.2 config 配置](#62-config-配置)
  - [6.3 log 日志与审计](#63-log-日志与审计)
  - [6.4 com 通信层](#64-com-通信层)
  - [6.5 ha 高可用选举](#65-ha-高可用选举)
  - [6.6 event 事件分发](#66-event-事件分发)
  - [6.7 http 组件](#67-http-组件)
  - [6.8 ipc 通信](#68-ipc-通信)
  - [6.9 plugin_mgr 插件管理](#69-plugin_mgr-插件管理)
  - [6.10 security 安全](#610-security-安全)
  - [6.11 serde 序列化](#611-serde-序列化)
  - [6.12 thread_pool / timer / trace_context](#612-thread_pool--timer--trace_context)
  - [6.13 node_mgr 节点管理](#613-node_mgr-节点管理)
  - [6.14 cert / vip / vscok / xml / storage](#614-cert--vip--vscok--xml--storage)
  - [6.15 misc 工具集](#615-misc-工具集)
- [7. Controllers 控制器层](#7-controllers-控制器层)
  - [7.1 mem 内存池化控制器](#71-mem-内存池化控制器)
  - [7.2 node 节点控制器](#72-node-节点控制器)
  - [7.3 urma URMA 控制器](#73-urma-urma-控制器)
  - [7.4 ssu SSU 控制器](#74-ssu-ssu-控制器)
  - [7.5 npu NPU 控制器](#75-npu-npu-控制器)
- [8. API Server 北向 API](#8-api-server-北向-api)
- [9. SDK 多语言客户端](#9-sdk-多语言客户端)
  - [9.1 C SDK](#91-c-sdk)
  - [9.2 Go SDK](#92-go-sdk)
  - [9.3 Python SDK](#93-python-sdk)
- [10. CLI 命令行](#10-cli-命令行)
- [11. Adapter Plugins 适配器插件](#11-adapter-plugins-适配器插件)
- [12. Addons Rack 级插件](#12-addons-rack-级插件)
- [13. RAS 故障处理](#13-ras-故障处理)
- [14. Message 消息基类](#14-message-消息基类)
- [15. 测试体系](#15-测试体系)
- [16. 第三方依赖](#16-第三方依赖)
- [17. 配置文件](#17-配置文件)
- [18. 模块依赖关系总览](#18-模块依赖关系总览)
- [19. 关键设计要点](#19-关键设计要点)

---

## 1. 项目概览

UBSE 提供了 `ubse` daemon 程序及其相应的 SDK 开发库。开发者可以利用 SDK 访问 daemon 提供的服务，从而实现对 **MEM（内存）**、**Node（节点拓扑）**、**URMA（远程内存访问设备）**、**SSU（共享存储）** 等资源的调度与管理，执行关键运维操作。

**核心能力**：

- 内存池化与按需借用（FD / NUMA / Addr / SHM 四种形态）
- 分布式选主与高可用故障切换
- 节点拓扑发现与多跳路由通信
- 北向 IPC（UDS）+ HTTP 双通道对外暴露
- 插件化扩展（节点级适配器 + Rack 级业务插件）
- 故障处理（OOM 紧急迁移、BMC 下电、内存 CE/UCE 等）

**技术栈**：C++17 / C11，CMake 3.22+，命名空间统一为 `ubse::*`，模块基类 `ubse::module::UbseModule`。

---

## 2. 整体架构

UBSE 采用分层架构，自底向上分为五层：

```
┌─────────────────────────────────────────────────────────────┐
│  外部消费方：CLI(ubsectl) / C SDK / Go SDK / Python SDK      │
└───────────────┬──────────────────────────────┬──────────────┘
                │ UDS IPC                       │ HTTP
┌───────────────▼──────────────────────────────▼──────────────┐
│  API Server 层：UbseApiServerModule (UDS + 权限校验)         │
└───────────────┬─────────────────────────────────────────────┘
                │
┌───────────────▼─────────────────────────────────────────────┐
│  Controllers 层：mem / node / npu / ssu / urma 资源控制器    │
└───────────────┬─────────────────────────────────────────────┘
                │
┌───────────────▼─────────────────────────────────────────────┐
│  Framework 层：com / config / event / ha / http / ipc /      │
│                log / plugin_mgr / security / serde / ...     │
└───────────────┬─────────────────────────────────────────────┘
                │
┌───────────────▼─────────────────────────────────────────────┐
│  Adapter Plugins 层：mmi / mti / ssu / syssentry /           │
│                      urma_uvs / bandbridge(.ko)              │
├─────────────────────────────────────────────────────────────┤
│  Addons 层(Rack 插件)：rmrs / ucache / virt_agent /          │
│                        process_mem                           │
├─────────────────────────────────────────────────────────────┤
│  RAS 故障处理 / Message 消息基类                              │
└─────────────────────────────────────────────────────────────┘
```

**两条对外通道**：

1. **UDS IPC 通道**（主通道）：SDK/CLI 通过 Unix Domain Socket (`/var/run/ubse/ubse.sock`) 与 daemon 通信，由 `api_server` 模块承载，权限校验后分发到各控制器注册的 IPC handler。
2. **HTTP 通道**：由具体控制器（如 ssu）通过 `framework/http` 注册 REST 路由；VIP 模块在主节点额外启动独立 HTTP server 对外服务。

**模块化机制**：所有模块继承 `UbseModule`，通过宏 `CORE_MODULE_IMPL(Module, Dep...)` / `OPTIONAL_MODULE_IMPL(Module, Dep...)` 声明依赖，由 `UbseContext` 拓扑排序后按序激活。模块间通过 `UbseContext::GetModule<T>()` 获取句柄。

---

## 3. 目录结构

```
ubs-engine/
├── 3rdparty/              # 第三方依赖（cpp-httplib, hcom, libxml2, mockcpp, rapidjson, securec）
├── conf/                  # 配置文件（ubse.conf, ubse_auth_default.conf, ubse_plugin_admission.conf, plugin_process_mem.conf, topo/）
├── docs/                  # 文档（zh, build_install, conventions, design, images, test）
├── example/               # SDK 示例（c / go / python）
├── scripts/               # 脚本（build/, cmake/, command_completion/, rpm/, toolchain/）
├── src/
│   ├── ubse_main.cpp      # daemon 主入口
│   ├── include/           # 全局共享头文件
│   ├── framework/         # 核心框架（21 个子模块）
│   ├── controllers/       # 资源控制器（mem / node / npu / ssu / urma）
│   ├── api_server/        # 北向 API 暴露层
│   ├── sdk/               # 多语言 SDK（c / go / python）
│   ├── cli/               # CLI 命令行（ubsectl + ubsectl-ssu）
│   ├── adapter_plugins/   # 节点级适配器插件
│   ├── addons/            # Rack 级业务插件（rmrs / ucache / virt_agent / process_mem）
│   ├── container/         # 容器化辅助（ubse_helper）
│   ├── ras/               # 故障处理
│   └── message/           # 消息基类
├── test/                  # 测试（UT / IT / PT + mock_libs / tools）
├── build.sh               # 构建脚本
├── CMakeLists.txt         # 顶层 CMake
└── config.h.in            # 自动生成 config.h 的模板
```

---

## 4. 构建与运行

### 4.1 构建命令

```shell
bash build.sh              # Release 构建（默认）
bash build.sh -D           # Debug 构建
bash build.sh -T RelWithDebInfo  # Release with debug info
bash build.sh package      # 构建并打包为 RPM 到 output/
bash build.sh ut           # 构建并运行所有 UT
bash build.sh ut -- --gtest_filter="TestSuite.*"   # 运行指定用例
bash build.sh ut -C        # 生成覆盖率报告（cmake-build-debug/coverage）
bash build.sh ut -C -H     # 覆盖率 + 启动 HTTP 服务器查看报告
```

**常用选项**：`-D`(Debug) `-T <type>`(构建类型) `-C`(覆盖率) `-c`(清理) `-j <n>`(并发数) `--asan/--lsan/--tsan/--ubsan`(Sanitizer) `--hccs`(关闭 UB 优化) `--ninja/--make`(生成器) `-V <ver>`(发布版本) `--skip-run-tests`(只构建不执行)。

### 4.2 构建产物

- 共享库 → `cmake-build-*/lib/`
- 可执行文件 → `cmake-build-*/bin/`
- 自动生成头 → `cmake-build-*/include/`（`config.h`、`register_xalarm.h`）
- 配置文件 → 构建时拷贝到 `cmake-build-*/conf/`
- 主 daemon 可执行：`ubse`（由 [src/ubse_main.cpp](../../../src/ubse_main.cpp) 构建）

### 4.3 构建类型说明

| 类型 | 用途 | 特点 |
|---|---|---|
| Release（默认） | 生产包 | 代码优化，不含调试信息 |
| Debug | 调试 | 无优化，含调试信息；测试自动切换到此类型 |
| RelWithDebInfo | 调试优化代码 | 优化 + 调试信息 |
| MinSizeRel | 最小生产包 | 最小二进制，不含调试信息 |

### 4.4 CMake 模块（scripts/cmake/）

| 文件 | 作用 |
|---|---|
| `building.cmake` | 全局编译/链接选项（栈保护、FORTIFY_SOURCE、RELRO、NX 栈） |
| `ccache.cmake` | ccache 加速重复构建 |
| `gtest.cmake` | GTest/GMock 引入（系统包 → FetchContent fallback） |
| `doxygen.cmake` | Doxygen 文档生成（`docs` target） |
| `iwyu.cmake` | Include-What-You-Use 检查与自动修复 |
| `libboundscheck.cmake` | libboundscheck（securec）引入 |
| `mockcpp.cmake` | mockcpp 源码构建（v2.7 + ARM64 补丁） |
| `utils.cmake` | 通用宏：`add_ut`、`setup_coverage`、`build_message`(simpo)、`install_lib` 等 |

### 4.5 注意事项

- 默认构建类型为 Release；构建测试目标时自动切换为 Debug 以获取完整覆盖率
- 本地构建优先使用 Ninja（若已安装），否则回退 Unix Makefiles
- CI 构建通过 `is_build_project=true` 环境变量跳过本地优化
- UB 优化默认开启（`ENABLE_UB=ON`），通过 `--hccs` 关闭
- LSP 配置：`compile_commands.json` 位于对应构建目录

---

## 5. 核心启动流程

daemon 入口位于 [src/ubse_main.cpp](../../../src/ubse_main.cpp)：

1. `InitializeAuditFunctions()` —— 动态加载 libaudit，初始化审计日志
2. 注册信号处理（SIGINT/SIGTERM/SIGPIPE），实现优雅退出
3. `UbseContext::GetInstance().Run(argc, argv)` —— 启动上下文，内部完成：
   - 创建核心模块与插件模块
   - 基于 `CORE_MODULE_IMPL` 声明做拓扑排序
   - 按序 `InitModule` → `StartModule`
4. 主循环 `while (!g_globalStop)` 每 100ms 轮询，等待停止信号
5. 收到退出信号后 `g_ubseContext.Stop()` → `AuditDestroy()`

**模块激活顺序**（由依赖关系驱动）：`config` → `log` → `trace_context` → `thread_pool` → `event` → `security` → `com` → `ha(election)` → `node_mgr` → `storage` → `http` → `vip` → `plugin_mgr` → 业务模块（controllers / api_server / ras）。

---

## 6. Framework 框架层

`src/framework/` 包含 21 个子模块，构成整个系统的基石。模块间通过 `UbseContext::GetModule<T>()` 互相获取句柄。

### 6.1 context 上下文

**职责**：全局单例上下文，管理所有模块的注册、拓扑排序、生命周期（Init/Start/Stop/Destroy），提供进程参数解析与运行模式判定。

**关键类**：`class UbseContext`（[src/framework/context/ubse_context.h](../../../src/framework/context/ubse_context.h)）

```cpp
UbseResult Run(int argc, char* argv[], ProcessMode = MANAGER);
void Stop();
template<typename T> std::shared_ptr<T> GetModule();   // 按 T::kModuleName 取模块
UbseResult GetArgStr(argName, argValue&);
ProcessMode GetProcessMode();                          // MANAGER / CLI / DEFAULT
UbseResult SetWorkReadiness(status); uint8_t GetWorkReadiness(); bool IsAllModulesReady();
```

全局变量：`g_globalStop`（atomic）、`g_globalCv`（condition_variable_any）。私有方法 `TopologicalSort` / `ResolveActivation` / `ActivateWithDependencies` 实现依赖排序与按序激活；`CheckDependencyType<T>()` 编译期校验依赖类型完整。

### 6.2 config 配置

**职责**：从 INI 风格配置文件读取配置（按 section/key 组织），提供类型安全的 get 接口与按前缀批量读取。

**关键类**：`class UbseConfigManager`（单例）、`class UbseConfModule : public UbseModule`

```cpp
UbseResult Init(const std::string& confDir, const std::string& filePrefix="");
UbseResult GetConf(section, configKey, std::string& configVal);
template<typename T> UbseResult GetConf(section, configKey, T& configVal);  // uint32/64, float, bool, string
UbseResult GetAllConfigWithPrefix(sectionPrefix, configVals);
```

C 接口（`ubse_conf.h`）：`UbseGetUInt / UbseGetFloat / UbseGetStr / UbseGetBool / UbseGetULong`。通过 `struct ConfigItem` + `class RegisterConfigHelper` 实现配置项静态注册。**基础模块，无 framework 内部依赖。**

### 6.3 log 日志与审计

**职责**：运行日志（多级别、文件轮转、ringbuffer 异步落盘、syslog 转发）与系统审计日志（基于 libaudit 动态加载，支持操作/资源分配释放/安全四类审计事件）。

**关键类**：
- `enum class UbseLogLevel { DEBUG, INFO, WARN, ERROR, CRIT }`
- `class UbseLoggerEntry` —— 单条日志条目，支持 `operator<<` 流式写入，自动捕获 file/func/line/timestamp
- `class UbseLoggerManager`（单例）—— `Init(LoggerOptions&, UbseLoggerWriter*)` / `Push(entry&&)` / `Pop()` / `LogToSyslog(entry&)`
- `class UbseLoggerWriter`（抽象）—— 子类 `UbseDefaultLoggerWriter`(cout)、`UbseLoggerFilesink`(文件轮转)
- `class LogBuffer` —— ringbuffer 异步缓冲

**日志宏**：`UBSE_DEFINE_THIS_MODULE(name)` 定义模块名；`UBSE_LOG_DEBUG/INFO/WARN/ERROR/CRIT` 流式日志。

**审计日志**（[src/include/ubse_logger_audit.h](../../../src/include/ubse_logger_audit.h)）：
- `enum class RecordType { AUDIT_OPERATE, AUDIT_RUNTIME_ALLOC, AUDIT_RUNTIME_DEALLOC, AUDIT_SECURITY }`
- 宏：`UBSE_AUDIT_OPERATE(interface)` / `UBSE_AUDIT_RUNTIME_ALLOC` / `UBSE_AUDIT_RUNTIME_DEALLOC` / `UBSE_AUDIT_SECURITY(interface)`
- `InitializeAuditFunctions() / AuditDestroy()` 动态 `dlopen("libaudit.so")` 加载审计 API
- 审计事件：`AUDIT_USER_CMD(1123)`、`AUDIT_DEV_ALLOC(2307)`、`AUDIT_DEV_DEALLOC(2308)`、`AUDIT_CRYPTO_PARAM_CHANGE_USER(2401)`

依赖：`config`、`context`、`trace_context`、libuuid、libaudit。

### 6.4 com 通信层

**职责**：基于 HCOM（华为自研网络库）提供节点间 RPC 通信，支持 TCP / UDS / HCCS / UB 多协议、Server/Client 双角色、TLS 认证、多跳路由转发与节点内消息队列回环。

**关键类**：
- `class UbseComBase : public Referable` —— 通信基类
  ```cpp
  virtual UbseResult Start() / Stop() / Connect() / ConnectWithOption(ConnectOption, std::string&);
  template<class TReq, class TRsp> UbseResult Send(const SendParam&, TReq&, TRsp&, bool withCopy=false);
  template<class TReq> UbseResult AsyncSend(const SendParam&, TReq&, const UbseComCallback&);
  static UbseResult ReplyMsg(UbseComMessageCtx&, const UbseComDataDesc&);
  std::vector<UbseLinkInfo> GetAllLinkInfo();
  UbseResult AddRoute(const RouteEntry&) / void DelRoute(const std::string&);
  ```
- `class UbseComModule : public UbseModule` —— Module 适配，`IsCurrentNode(nodeId)` 判断节点内路由；`RpcSend` 自动选择节点内 queue 或跨节点 RPC
- `class UbseComEngine` —— 单引擎实例，`RegUbseComMsgHandler` / `CreateChannel` / `GetMessageHandler(moduleCode, opCode)`
- `class UbseCommunication` —— 引擎静态门面：`UbseComRpcConnect` / `UbseComMsgSend`(同步) / `UbseComMsgAsyncSend`(异步) / `UbseComMsgReply`
- `class UbseRouteTable` —— 多跳路由：`Lookup(dstNodeId)` 按 priority 升序匹配精确/分组/默认路由
- `class UbseRpcServer : public UbseComBase` —— 跨节点 RPC Server
- `class UbseInterCom` —— 节点内（同 nodeId）消息队列回环
- `struct RouteEntry { dstNodeId; capacity; priority; nextHopNodeId; }` —— 路由表项

**路由设计**：三种类型——精确（capacity=0）、分组（capacity=2^n，将 dstNodeId-1 低 n 位置 0 做分组匹配，适合机柜级转发）、默认（dstNodeId="0", capacity=UINT32_MAX）。priority=0 保留给直连路由。

**对外 C 风格 API**：`UbseRegRpcService` / `UbseRpcSend` / `UbseRpcAsyncSend` —— 根据当前角色（master/agent/standby）自动选择直发或转发到 master。

依赖：`election`（角色查询决定转发目标）、`node_mgr`、`log`、`trace_context`、`misc/*`、外部库 `hcom`、`securec`。

### 6.5 ha 高可用选举

**职责**：分布式选主算法（master/standby/agent 三角色 + global 多组级联），通过心跳与选举报文完成主备自动选举、故障切换、组间拓扑同步与角色变更通知。

**关键类**：
- `class UbseElectionModule : public UbseModule`
  ```cpp
  UbseResult UbseGetAllNodes(Node& master, Node& standby, std::vector<Node>& agent);
  UbseResult GetCurNodeGlobalTopoInfo(HaTopologyInfo&);
  bool IsLeader();
  void TimerTaskElection() / TimerTaskCom();   // 定时选举与通信
  void SwitchMasterFromStandby() / SwitchAgentFromMaster();   // 故障切换
  ```
- `class ElectionRole`（抽象基类，[src/framework/ha/role/ubse_election_role.h](../../../src/framework/ha/role/ubse_election_role.h)）
  ```cpp
  virtual void ProcTimer() = 0;          // 定时周期处理
  virtual uint32_t RecvPkt(srcID, rcvPkt, reply&) = 0;   // 收到选举报文
  virtual void RecvInterGroupInfo(rcvInfo, replyInfo);    // 跨组信息交换
  virtual UBSE_ID_TYPE GetMasterNode() / GetStandbyNode() / GetGlobalMasterNode();
  virtual RoleType GetRoleType() = 0;    // MASTER/STANDBY/AGENT/INITIALIZER
  virtual GlobalRoleType GetGlobalRoleType();  // GLOBAL_*
  virtual uint64_t GetTurnId() = 0;      // 选举任期
  ```
  子类：`Initializer / Master / Standby / Agent / GlobalMaster / GlobalStandby / GlobalAgent / GlobalInitializer`
- `class RoleMgr`（单例）—— `GetRole()` / `SwitchRole(RoleType, RoleContext&)` / `RoleChangeAttach(type, handler)` / `RoleChangeNotify(type, newId)` / `GetAllGroupStates()`
- `class UbseElectionCommMgr : public UbseComBase` —— 选举专用通信：`Connect(dstIp)` / `SendElectionPkt(destID, pkt, reply&)` / `ElectionSubEvent()` 订阅链路状态
- `class UbseElectionPktHandler` —— 注册到 com 层的选举报文 RPC 处理器（moduleCode=ELECTION, opCode=ELECTION_PKT）

**选举办法**：`SendElectionPkt(myselfID, myselfIp)` 主动发起；`ElectWhenLowest(myselfID, allNodes)` ID 最小者发起；`SendGlobalElectionPkt` 全局选举；`IsSmallestNode` / `IsSecondSmallestNode` 判定；`AcceptNewMaster` 接受新主；`ForceElection` 强制重选。

**事件枚举** `UbseElectionEventType`：13 种事件（CHANGE_TO_MASTER、STANDBY_CHANGE_TO_MASTER、MASTER_ONLINE_NOTIFICATION、NODE_UP/DOWN、GLOBAL_* 等）。`struct UbseElectionHandler` + `class UbseElectionHandlerBuilder` 链式构造。

依赖：`com`（收发选举报文）、`event`（订阅链路通断、节点故障）、`node_mgr`（集群节点列表）、`log`、`misc/*`。

### 6.6 event 事件分发

**职责**：发布/订阅模式事件总线，按 HIGH/MEDIUM/LOW 三级优先级分线程池分发，支持队列拥塞监控。

**关键类**：
- `class UbseEventModule : public UbseModule`
  ```cpp
  UbseResult UbseSubEvent(eventId, registerFunc, priority=MEDIUM);
  UbseResult UbseUnSubEvent(eventId, registerFunc);
  UbseResult UbsePubEvent(eventId, eventMessage);
  ```
- `class UbseEventDistribute : public Referable` —— `RegisterSubscribe` / `PubEvent` / `EventDistribute` / `MonitorCongestion`
- `class UbseEventQueue` —— 生产者-消费者阻塞队列
- `enum UbseEventPriority { HIGH, MEDIUM, LOW }`
- `using UbseEventHandler = std::function<uint32_t(std::string&, std::string&)>`
- `struct EventTask { eventId; eventMessage; priority; registerFunc; traceId; }`

依赖：`config`、`log`、`thread_pool`、`misc/referable`。

### 6.7 http 组件

**职责**：基于 cpp-httplib 提供北向 HTTP/HTTPS 服务端与客户端，支持 TCP/UDS 监听、SSL 证书、路由注册。

**关键类**：
- `class UbseHttpServer` —— `struct Config { name; listenAddr; port; useUds; udsPath; useSsl; certPaths; }`；`Start()` / `Stop()` / `RegisterRoute(path, method, handler)` / `HandleRequest(req, res)`
- `class UbseHttpModule : public UbseModule`
  ```cpp
  static UbseResult RegHttpService(UbseHttpMethod method, url, func);
  static UbseResult HttpSend(UbseHttpRequest&, UbseHttpResponse&);
  static UbseResult UbseHttpPostJsonRequest(path, body, jsonRsp&);
  static UbseHttpServer* GetServer();
  ```
- `enum class UbseHttpMethod` —— GET/HEAD/POST/PUT/DELETE/TRACE/CONNECT/PATCH
- `struct UbseHttpRequest { method; path; params; headers; body; UbseHttpPeerCert peerCert; }`（含 mTLS 对端证书 cn/ou）
- `using UbseHttpHandlerFunc = std::function<uint32_t(const UbseHttpRequest&, UbseHttpResponse&)>`

依赖：`cert`、`misc/ubse_secure_buffer`、外部库 `httplib`。

### 6.8 ipc 通信

**职责**：基于 Unix Domain Socket + epoll 提供 SDK ↔ daemon 的 IPC 通信，支持短连接（请求-响应）与长连接（事件订阅/异步通知）、按用户限额的连接管理、同步等待响应。

**关键类**：
- `class UbseIpcServer` —— `explicit UbseIpcServer(const UbseUDSConfig&)`；`Start()` / `Stop()` / `RegisterHandler(moduleCode, opCode, handler)` / `SendResponse(statusCode, requestId, msg&)` / `AsyncSendLongLink(...)` 推送
- `class UbseUDSServer`
  ```cpp
  struct UbseUDSConfig {
      socketPath; socketPermissions=0660;
      threadPoolSize=8; threadPoolQueueSize=16;
      maxTransientConnections=16; maxTransientConnectionsPerUser=8;
      maxPersistentConnections=256; maxPersistentConnectionsPerUser=128;
  };
  ```
  内部 `enum SessionState { CONNECT, READING, PROCESSING, READY_TO_WRITE, WRITING, CLOSING }`、`enum SessionType { PENDING, TRANSIENT, PERSISTENT }`、`struct ClientSession { fd; clientInfo; connType; state; readBuffer; writeBuffer; }`
- `class UbseUDSClient`（单例）—— `Connect()` / `Send(request, response, totalTimeout)` 同步短连接；`PerSistentConnect()` 长连接；`RegisterLongLinkNotify(moduleCode, opCode)` 订阅事件；`RegisterClientRequestHandler(handler)` 处理服务端反向请求
- `class UbseSyncReq`（单例）—— 长连接模式下同步等待响应：`RegisterRequest(reqId)` / `WaitForResp(reqId, timeout, msg&)` / `StoreResp(reqId, msg)`
- 底层：`SendMsg(fd, buffer, length, timeoutMs)` / `RecvMsg(fd, buffer, length, timeoutMs)` 保证写/读完整长度

**消息定义**：`struct UbseRequestHeader { moduleCode; opCode; bodyLen; clientRequestId; }`、`struct UbseResponseHeader { statusCode; bodyLen; clientRequestId; }`、`UbseRequestMessage / UbseResponseMessage`（含 `freeFunc` 释放回调）。

**C 风格 SDK 接口**：`ubse_socket_path_set` / `ubse_invoke_call(module_code, op_code, request_data, response_data)` / `ubse_long_link_connect()` / `ubse_shm_fault_register(handler)`。

依赖：`thread_pool`、`misc/ubse_map_util`(PairMap)、`misc/ubse_request_id_util`、`api_server`、SDK 头 `ubs_engine_mem.h`。

### 6.9 plugin_mgr 插件管理

**职责**：动态加载 `.so` 插件，按准入配置（admission）过滤允许加载的插件，调用插件导出的 `Init/DeInit` 完成生命周期管理，维护插件就绪状态。

**关键类**：
- `class UbsePluginModule : public UbseModule` —— `GetPluginLoaded(name)` / `GetPluginReadyStatus(name)` / `NotifyPluginReadyStatus(name, flag)`
- `class UbsePluginManager` —— `Start()` 读取准入+插件配置，加载并初始化；`LoadAndInitPlugin(name, fileName)` / `LoadPluginModule(name, fileName)` / `DeInitializePlugin(name)`
  - `using UbsePluginInitFunc = uint32_t(*)(const uint16_t moduleCode)` / `using UbsePluginDeInitFunc = void(*)()`
- `class UbsePluginLoader`（单例）—— `DiscoverAndLoad()` / `UnloadAll()`
- `class UbsePluginAdmission` —— `LoadAdmissionConfig()` / `GetAllowedPlugins()` 返回允许加载列表 + 分配的 moduleCode
- `class UbsePluginConfig` —— `LoadPluginConfigs()` / `GetAllPluginConfigs()`

对外 C 接口：`GetPluginInitRes(name)` / `GetPluginReadyStatus(name)` / `NotifyPluginReadyStatus(name, status)`（供插件回调）。

依赖：`context`、`config`、`misc/lock`、外部 `dlopen`。

### 6.10 security 安全

**职责**：基于 Linux capability 机制管理 ubse 进程的有效权限集，启动时设置初始 capability，运行时按需增删。

**关键类**：
- `class UbseSecurityModule final : public UbseModule` —— `static UbseResult ModifyEffectiveCapabilities(std::vector<__u32>& caps, bool isAdd)`
- `class UbseSecurityManager` —— `GetCapabilities()` / `SetInitialCapabilities()` / `ModifyEffectiveCapabilities(caps, isAdd)`
- `uint32_t ChangeOverrideCapability(bool isAdd)` —— 便捷函数，添加/移除 `CAP_DAC_OVERRIDE`

依赖：Linux 内核头 `<linux/capability.h>`。

### 6.11 serde 序列化

**职责**：自研二进制序列化框架，支持基础类型/字符串/vector/map/嵌套结构的类型安全序列化与反序列化，带类型标签与对齐控制。

**关键类**：
- `class UbseSerialization` —— `operator<<` 重载基础类型/string/vector/map/嵌套；`GetBuffer()` / `GetLength()` / `Check()`
- `class UbseDeSerialization` —— `operator>>` 重载；`Set(buf, len)` / `Check()` / `SetFail()`
- 辅助类型：`struct addr_len<T>`（定长数组）、`struct array_len_insert`、`struct right_v<T>`、`struct enum_v<T>`
- 类型 ID 系统：`template<typename T> serial_type GetTypeId() / GetTypePointerId()`，`enum class allowed_type` 列举 37 种类型
- 头格式：`serial_head = (type << LEN_CTRL_OFFSET) | len`，类型块占高 8 位，长度占低 24 位
- 容量上限：`MAX_CAPACITY = 2^24`，单次添加上限 `ONCE_LIMIT_LEN = MAX_CAPACITY >> 8`

**独立模块，无 framework 内部依赖**（仅依赖 securec）。

### 6.12 thread_pool / timer / trace_context

**thread_pool**：通用任务执行器（线程池），支持 CPU 亲和性绑定、命名线程、阻塞队列。
- `class UbseRunnable : public Referable` —— `virtual void Run()`
- `class UbseTaskExecutor : public Referable` —— `static Ref<UbseTaskExecutor> Create(name, threadNum, queueCapacity=1000)` / `Start()` / `Stop()` / `Execute(RunnablePtr)` / `Execute(std::function<void()>)` / `SetCpuSetStartIndex(idx)`
- `class UbseTaskExecutorModule : public UbseModule`
- 依赖：`misc/container/ubse_ring_buffer.h`、`misc/lock`、`log`

**timer**：定时器控制器，注册周期性任务独立线程回调。
- `using UbseTimerHandler = std::function<UbseResult()>`
- `uint32_t UbseTimerHandlerRegister(name, handler, interval)` / `void UbseTimerHandlerUnregister(name)`
- `class UbseTimerController` —— `Start(interval_ms, callback, name)` / `Stop()`（析构自动调用）

**trace_context**：线程本地 traceId 管理，跨 RPC 调用传递，基于 libuuid 生成 UUID。
- `class TraceContext` —— `static uint32_t InitUuid()`（动态 dlopen libuuid） / `static void SetTraceId(string)` / `static std::string GetTraceId()` / `static void Clear()`
- `thread_local char tls_traceId[TRACE_ID_SIZE]`

### 6.13 node_mgr 节点管理

**职责**：节点静态信息管理与节点发现，支持 5 种发现模式。

**关键类**：
- `class UbseNodeMgrModule : public UbseModule`（`OPTIONAL_MODULE_IMPL(UbseNodeMgrModule, UbseUrmaUvsModule)`）
- `class UbseNodeStaticInfoMgr`（单例）
  ```cpp
  UbseResult Init(); NodeMap GetAllNodes(); GetNodesByGroupId(groupId);
  GetUbseNodeByAddr(ip); GetUbseNodeById(id); GetCurrentNode();
  NodeDiscoveryMode GetNodeDiscoveryMode(); bool IsUrma(); bool IsClos();
  GetRootIpList(); GetClusterIpList(); GetPodCapability(); ApplyUrmaDev();
  ```
- `enum class NodeDiscoveryMode { TCP_CONFIG_CLOS_MODE, TCP_CONFIG_FULL_MESH_MODE, TCP_ROOT_MODE, URMA_CLOS_MODE, URMA_FULL_MESH_MODE }`

C 接口：`GetCurrentNode()` / `GetAllNodes()` / `GetAllNodesStoredByGroup()` / `GetUbseNodeById(nodeId)` / `IsUrma()` / `GetRootIpList()` / `GetNodesByGroupId(groupId)` / `GetCurPhysicalLinkInfo(allLinkInfo&)` / `ApplyUrmaDev()`。

子目录：`conf_mode/`、`root_mode/`、`static_mode/` 三种发现模式实现。依赖 `urma_uvs_module`（可选）、`ubse_node_controller.h`。

### 6.14 cert / vip / vscok / xml / storage

**cert**：基于 OpenSSL 的证书/密钥加载与 SSL 校验。
- `class UbseSslValidator` —— `explicit UbseSslValidator(UbseCertPaths paths)`；`SecureBuffer LoadPassword()` / `bool CheckAllFileExist()`（校验 serverCert/serverKey/trustCert/password 四文件）
- 依赖：`misc/ubse_file_util`、OpenSSL

**vip**：主节点绑定 VIP（L2 ARP 广播），并在 VIP 上启动独立 HTTP server 对外提供北向接口；随选举角色变更自动 bind/unbind。
- `class UbseVipManager`（单例）—— `struct UbseVipConfig { enable; listenIp(CIDR); listenPort=10002; arpCount=5; arpInterval=200; address; prefix=24; interface; }`；`Init(config)` / `BindVip()` / `UnbindVip()` / `RegisterRoute(path, method, handler)`
- 私有：`BindVipL2 / UnbindVipL2 / AddIpAddress / DelIpAddress / SendGratuitousArp / StartHttpServer / ResolveInterface`
- `class UbseVipModule` —— 注册选举事件处理器 `HandleChangeToMaster / HandleStandbyChangeToMaster / HandleChangeToStandby / HandleChangeToAgent`
- 常量：`kIfaceFilePath = "/var/run/ubse/ubse_iface"`（K8s helper container 写入的接口名）
- 依赖：`http`、`election`、`misc/ubse_secure_buffer`

**vscok**：通过 AF_VSOCK 向信任环（trust ring）服务请求对数据签名，基于 SSL 双向认证。
- `class UbseVsockClient` —— `uint32_t UbseVsockSend(UbseSignReq& req, UbseSignRsp& rsp)`
- `struct UbseSignReq { id; type; payload; }` / `struct UbseSignRsp { signedData; }`
- `struct MsgHeader { id; version=0xffff0400; type; len; char data[0]; }` —— 柔性数组消息头
- 依赖：`cert`、`misc/ubse_secure_buffer`、OpenSSL

**xml**：基于 libxml2 的 XML 文档解析/遍历/修改/序列化，维护游标路径链表支持回溯。
- `class UbseXml : public std::enable_shared_from_this<UbseXml>` —— `Parse()` / `Printer(outputString&, includeXmlDeclaration=true)` / `Next(name, num=0)` / `Previous()` / `Back()` / `Child(name, num=0)` / `Text()` / `Name()` / `Attr(key)` / `AddNode(name)` / `DeleteNode(name, num)` / `GetDeepth()`
- `enum class UbseXmlError { OK, NULLPTR, NOT_FOUND, ERROR_XML_PARSE }`
- `class ListNode { xmlNode* node; shared_ptr<ListNode> next; int depth; }`

**storage**：基于本地 KV 数据库（默认 `/var/lib/ubse/data`）提供精确/模糊查询、删除，并通过 com 层 RPC 实现主节点远程查询代理。
- `class UbseStorageModule : public UbseModule` —— `Put(dbName, key, value, valueLen)` / `Get(dbName, key, KV&)` / `RemoteGet(dbName, key, RemoteGetHandler handler)` / `Delete(dbName, key)`
- `using RemoteGetHandler = std::function<UbseResult(const std::vector<KV>&)>`
- `class UbseStorageReqHandler : public UbseComBaseMessageHandler` —— 注册到 com 层（moduleCode=STORAGE, opCode=STORAGE_REQ）
- 常量：`DEFAULT_DB_NAME = "default"`、`DB_STORE_DIR = "/var/lib/ubse/data"`
- 依赖：`com`

### 6.15 misc 工具集

`src/framework/misc/` 提供基础工具，**均无 framework 内部依赖**（下表为主要条目，另含 `ubse_auto_serial_util.hpp`、`ubse_auto_simpo.hpp`、`ubse_env_util`、`ubse_math_util`、`ubse_pack_util`、`ubse_logging_lock_guard` 等）：

| 文件 | 职责 | 关键类/函数 |
|---|---|---|
| `ubse_file_util` | 文件/目录工具 | `UbseFileUtil::GetFileInfo / ListFiles(regex) / IsDirectory / CreateAndChmodDirectory(dir, 0755) / CheckFileExists / SetFileOwnership(path, uid, gid)` |
| `ubse_json_util` | JSON 解析（rapidjson） | `UbseJsonUtil::GetStrFromJsonPtr / GetUintFromJsonPtr / GetBoolFromJsonPtr / GetArrayFromJsonPtr / GetObjectFromJsonPtr` |
| `ubse_net_util` | 网络工具 | `UbseNetUtil::ParseIpList / FindLocalIpByRemote / ValidIpv4Addr / IpV4ToInt / ParseIpRangeToList` |
| `ubse_os_util` | OS 工具 | `UbseOsUtil::GetUserNameById(uid, name) / Exec(cmd, res) / GetUidByName(name, uid) / GetNumaIdByPid(pid, numaId) / ReadFileContent(path, res)` |
| `ubse_str_util` | 字符串工具 | `Split / ConvertStrToInt/Long/Uint / Trim / GenerateRandomStr(size) / HexPairToByte` |
| `ubse_topo_util` | 拓扑工具 | `struct ChipLocation { slotId; chipId; }`、`IsMultiPortTopo(UbseCpuInfo&)` |
| `ubse_dl_manager` | 动态库加载（RAII） | `UbseDlManager::Open() / Close() / GetSymbol(name)` |
| `ubse_future_mgr` | 异步请求结果（std::any） | `UbseFutureMgr::CreateInstance(requestId) / SetResult(requestId, any) / GetResult() / Wait()` |
| `ubse_fs` | 根目录隔离文件读写 | `UbseFs::DeleteFile / ReadFile / WriteFile`（按文件名加锁） |
| `crc` | CRC32 校验 | `CrcUtil::SoftCrc32(data, dataLen, initValue=0xFFFFFFFF)`（256 项查找表） |
| `lock` | 锁原语 | `Lock`(mutex)、`RecursiveLock`、`ReadWriteLock`(pthread_rwlock)、`WriteLocker/ReadLocker`(RAII) |
| `referable` | 引用计数基类 | `Referable::IncreaseRef/DecreaseRef/GetRef`（原子），`Ref<T>` 侵入式智能指针 |
| `ubse_secure_buffer` | 安全内存缓冲 | 析构清零 |
| `ubse_pointer_process` | 安全删除 | `SafeDelete / SafeDeleteArray / SafeFree / SafeMakeShared` |
| `ubse_map_util` | 双键 map | `PairMap<K1, K2, V>` |
| `ubse_request_id_util` | 请求 ID 生成 | `UbseRequestIdUtil` 按类型生成唯一 ID |
| `container/ubse_ring_buffer` | 环形阻塞队列 | 线程池使用 |

---

## 7. Controllers 控制器层

`src/controllers/` 实现五类资源的调度与控制，每个控制器对外通过 IPC handler 注册到 `api_server`。

### 7.1 mem 内存池化控制器

**职责**：内存资源的池化、借用（FD/NUMA/Addr/SHM 四种形态）、对账（ledger）、调度算法。

**公共定义** [src/controllers/include/ubse_mem_def.h](../../../src/controllers/include/ubse_mem_def.h)：提供 C 风格公共定义（extern "C"），含关键常量 `UBSE_MEM_RESOURCE_ID_MAX_LEN=48`、`UBSE_MEM_NUMA_MAX_NUM=8`、`UBSE_MEM_HIGH_WATERMARK_DEFAULT=88`、`UBSE_MEM_LOW_WATERMARK_DEFAULT=11`；借用请求类型 `UBSE_MEM_APP_BORROW_REQUEST` / `UBSE_MEM_SHARE_REQUEST` / `UBSE_MEM_WATER_BORROW_REQUEST` / `UBSE_MEM_APP_PRI_REQUEST`；枚举 `UbseMemShmRegionType`(ALL2ALL_SHARE/ONE2ALL_SHARE)、`UbseMemPerfLevel`(L0直连/L1一跳/L2多跳)、`UbseMemQueryType`。

**对外 API**（[src/include/ubse_mem_controller.h](../../../src/include/ubse_mem_controller.h)，命名空间 `ubse::mem::controller`）：

```cpp
enum class UbseMemStage { UBSE_NOT_EXIST, CREATING, DELETING, EXIST, ... };
enum class UbseMemBorrowType { FD_BORROW, NUMA_BORROW, ADDR_BORROW, SHM_BORROW, SHM_ATTACH };

// NUMA 借用
UbseResult UbseMemNumaCreate(name, borrower, opt, desc&);
UbseResult UbseMemNumaCreateWithLender(...);          // 指定借出方（碎片场景）
UbseResult UbseMemNumaCreateWithCandidate(...);       // 候选节点范围
UbseResult UbseMemNumaDelete(name, borrower);
// Addr 借用
UbseResult UbseMemAddrCreate(...) / UbseMemAddrDelete(...);
// 账本查询
UbseResult UbseGetNumaMemDebtInfoWithNode(nodeId, debtInfos&);
UbseResult UbseGetAddrMemDebtInfoWithNode(...);
UbseResult UbseGetAllNodeNumaInfo(...) / UbseGetNodeNumaInfoByNodeId(...);
UbseResult UbseMemDebtCircleCheck(srcNodeId, dstNodeId, isCircle&);   // 成环检查
uint32_t UbseQueryResult(name, result&, borrowType);
```

**目录组织**：

- `mem_controller/` 核心控制器
  - `ubse_mem_controller_module.{h,cpp}` —— `class UbseMemControllerModule : public UbseModule`，模块入口，持有 `UbseMemServiceImpl`
  - `ubse_mem_controller_api.{h,cpp}` —— 控制器对外 API 与初始化（`Init/UnInit/Stop/CreateTaskExecutor/RegisterNodeCtlNotify/LoadLocalAllObjs`）
  - `ubse_mem_controller.{cpp}` / `ubse_mem_controller_handler` / `ubse_mem_controller_dispatcher` —— 请求分发与处理
  - `ubse_mem_controller_ledger`(+`_filter`)、`ubse_mem_global_ledger_report` / `ubse_mem_global_ledger_summary` / `ubse_mem_global_ledger_summary_store` —— 账本管理与全局汇报
  - `ubse_mem_controller_master_online_handler` / `ubse_mem_controller_pre_online` —— master 在线处理
  - `ubse_mem_controller_fault_handle` —— 故障处理
  - `ubse_mem_service_impl` —— 服务实现类
  - `api/` 借用 API 子目录：`ubse_mem_controller_fd_api`(fd 借用) / `_numa_api` / `_addr_api` / `_share_api`(+`_capabilities`+`_forward`+`_store`)，及 `api_common` / `helper`
  - `debt/` 账本查询：`ubse_mem_debt_info`(+`_partial_fetch`+`_query`+`_query_addr/fd/numa/share`)
  - `message/` 所有消息的序列化（`*_simpo` + `*_serial`，覆盖 borrow_req/exportobj/importobj/return_req/share_attach_req 等）
  - `rpc/` RPC 处理器注册：`ubse_mem_controller_rpc_register`、`ubse_mem_debt_info_query_handler`、`ubse_mem_get_opt_result_handler`
- `mem_decoder_utils/` —— `ubse_mem_decoder_utils`、`ubse_mem_prehandle_manager`（消息解码与预处理）
- `mem_scheduler/` 调度器（命名空间 `ubse::mem::scheduler`）
  - `ubse_mem_scheduler_impl` —— 调度主实现
  - `ubse_mem_scheduler_account_manager` / `_filter_manager` / `_node_manager` / `_score_manager` —— 账户/过滤/节点/评分管理
  - `ubse_mem_scheduler_request`（请求处理）、`ubse_mem_scheduler_sub_health_mode`（亚健康模式）
  - 子目录 `scheduler_cache/` / `scheduler_filter/` / `scheduler_score/`，公共定义 `ubse_mem_types.h`

### 7.2 node 节点控制器

**职责**：集群节点拓扑采集、Master/Agent 之间节点信息上报下发、CPU 拓扑、节点状态机管理。

**关键类**（[src/controllers/node/](../../../src/controllers/node/)）：

- `class UbseNodeControllerModule : public UbseModule` —— 模块入口，依赖 `UbseConfModule`/`UbseLoggerModule`/`UbseElection`/`UbseTimer`/`ubse::mti::UbseTopologyInterface`
- `class UbseNodeControllerMaster`（单例）—— Master 端核心
  - `UbseNodeReportHandler(nodeInfo)` 处理从节点周期上报
  - `UbseLcneTopologyChangeHandler` 处理 LCNE 拓扑变化
  - `UbseCabinetReportHandler` / `UbseGlobalReportHandler` / `UbseSingleNodeReportHandler` 多级上报
  - 故障平滑：`ExecuteNodeSmoothing(nodeId)`、`ProcessFaultCounter`，阈值 `FAULT_REPORT_THRESHOLD = 150`
- `class UbseNodeControllerAgent`（单例）—— Agent 端核心
  - 定时器：`UbseNodeInfoReportTimerHandler` / `UbseCabinetInfoReportTimerHandler` / `UbseGlobalInfoReportTimerHandler`
  - 上报：`UbseNodeReportNodeInfo` / `LcneChangeReportNodeInfo` / `UbseCabinetReportSingleNode/FullInfo` / `UbseGlobalReportSingleNode/FullInfo`
  - RPC handler：`CollectNodeInfoHandler`、`nodeChangeHandler`
- `class UbseNodeController` —— `GetInstance()` 单例，`GetAllNodes()` 提供全量节点信息
- `ubse_node_controller_collector` / `ubse_node_com_urma_collector` —— 节点信息采集器

**北向 API** [src/controllers/node/ubse_node_api.h](../../../src/controllers/node/ubse_node_api.h)（命名空间 `ubse::node::api`）：
- `class UbseNodeApi` —— `static UbseResult Register()` 注册 IPC handler
- handler：`UbseQueryTopologyInfoHandle`(拓扑查询) / `UbseServerNodeGet`/`UbseServerNodeList`(节点获取/列表) / `UbseServerCpuTopoList`(CPU 拓扑列表) / `UbseServerNodeNumaMemGet`(NUMA 内存获取) / `UbseQueryClusterInfo` / `UbseQueryCpuTopo` / `UbseQueryNodeInfo`

### 7.3 urma URMA 控制器

**职责**：URMA（Unified Remote Memory Access）设备管理、QoS 带宽设置、设备分配/释放、拓扑变更处理。

**关键定义** [src/controllers/urma/ubse_urma_def.h](../../../src/controllers/urma/ubse_urma_def.h)（命名空间 `ubse::urma`）：
- `struct UrmaQosProfile { profileName; minBandWidth; maxBandWidth; }`
- `enum class UrmaDevType { UNIQUE(独占), SHARED(共享) }`
- `enum class UrmaDevState { ACTIVED, INACTIVED, UNKNOWN }`
- `enum class FeType { PHYSICAL_TYPE(pfe0), VIRTUAL_TYPE(vfe1) }`
- `struct UbseFeInfo { name; slotId; ubpuId; iouId; entityId; fetype; }`
- `struct EidGroup { primaryEid; portEids map; feInfo shared_ptr; }`
- `struct UbseUrmaInfo { subPath; urmaDevEid; eidGroups; urmaQosProfile; urmaDevType; state; hwResId; }`
- `struct UbseUrmaNodeInfo { nodeId; urmaList map; updateTimeStamp; }`
- `struct UrmaNameCompare` —— 按名称末尾数字排序

**关键类**：
- `class UrmaController`（单例，[src/controllers/urma/ubse_urma_controller.h](../../../src/controllers/urma/ubse_urma_controller.h)）
  - QoS：`UbseUrmaBandWidthSet/Get/Reset/Update`
  - SDK：`UbseGetLocalUrmaDevInfo` / `UbseAllocUrmaDev` / `UbseFreeUrmaDev`
  - 节点级：`UbseGetUrmaDevInfoByNodeId` / `UbseUrmaCliDevActivate`
  - 事件：`static UbseTopoLinkChangeHandler` / `static UbseNodeJoinHandler`
  - 恢复：`RecoverUrmaDeviceForOneNode` / `ActivateSpecifyUrmaBonding`
- `class UbseUrmaControllerApi` —— 北向 IPC API 入口，`static UbseResult Register()`，handler 包括 `UbseUrmaBandWidthSet/Get/CliGet/Reset` / `UbseUrmaDevGet/CliDevGet/CliDevActivate/DevAlloc/DevFree`
- `ubse_urma_controller_module` —— `UbseUrmaControllerModule : UbseModule` 模块入口
- 全局函数：`GetDirConnectInfo()` / `UbseUrmaControllerSetUvsInfo(current_slot_id, allLinkInfo, bondingInfo)` / `QueryUrmaInfoStateFromUrma(nodeId, urmaName="")` / `IsUdmaDevHealthy(feEid)` / `QueryAllPortsDown(isAllPortDown&)`

### 7.4 ssu SSU 控制器

**职责**：SSU（Storage Service Unit）存储资源分配/释放/挂载/卸载、NVMe 命名空间管理、FE/VFE 设备管理、责任链调度算法。

**核心服务实现** `class UbseSsuServiceImp : public UbseSsuService`（[src/controllers/ssu/ubse_ssu_service_imp.h](../../../src/controllers/ssu/ubse_ssu_service_imp.h)，单例，命名空间 `ubse::ssu::service`）：

```cpp
// 生命周期
StartCollecting() / StopCollecting() / StartClearTimer() / StopClearTimer() / RebuildLedgerFromDevList();
// 主接口
AllocSpace(req, identity, result&) / FreeSpace(name, identity);
AttachSpace(req, nsDevPaths&) / DetachSpace(req);
AttachLinearSpace(req, nsDevPaths&, devPath&) / DetachLinearSpace(req);
AttachStripedSpace(req, nsDevPaths&, devPath&) / DetachStripedSpace(req);
AddAccessPermission(name, nqn, identity) / RemoveAccessPermission(name, nqn, identity);
GetNsStats(name, statsList&, identity) / ListAllocInfo(result&, identity) / GetAllocInfoByName(...) / GetConnectInfo(name, vfe, list&, identity);
GetFeDeviceList(feList&) / FeDeviceAlloc(upi, vfe, busInstanceGuid&) / FeDeviceFree(upi, vfe);
VerifyAttachDetachIdentity(name, identity, nsVerifyList&);   // master 端 identity 验证
```

**收集器** `class UbseSsuCollector` —— 定时设备列表收集与缓存；`GetCachedDevList/Map` / `GetDevListWithReservations`；预留管理 `AddReserveSpace/AddReleasedSpace/ReleaseReservation/OnReserveBegin/OnReserveEnd`。

**调度器**（命名空间 `ubse::ssu::scheduler`，责任链模式）：
- `struct UbseSsuAllocRequest { allocSize; nsNum; lbaSize; addressingType; tenant; }`
- `enum class UbseSsuAllocRetCode { OK, INVALID_PARAM, INSUFFICIENT_SPACE, INTERNAL_ERROR }`
- `class UbseSsuAllocationContext` —— 封装 devices/request/result/selectedDevs
- Filter 实现：`PreCheckHandler`(预检查) / `UbseSsuTenantIsolationFilter`(租户隔离) / `UbseSsuAllocateAlgorithmHandler`(默认算法：容量大优先、namespace 数量少次优先)
- `enum class UbseSsuFilterGroupId { PRE_CHECK, ALGORITHM, POST_CHECK }`
- `class UbseSsuScheduler` —— `Execute(ctx&)` / `AddFilter(groupId, filter)`

**工具** `ubse_ssu_utils`（命名空间 `ubse::ssu::utils`）：`GetSsuExecutor()` / `StrToHex(id)` / `GenerateHostNqn()`（格式 `nqn.2024-01.org.nvmexpress:uuid:xxx`）。

**HTTP 路由** `ubse_ssu_http_handler`（命名空间 `ubse::ssu::http_handler`）：`RegisterSsuHttpHandlers()` 注册 `POST /ubse/v1/ssu/spaces -> AllocSpace`、`DELETE /ubse/v1/ssu/spaces/{name} -> FreeSpace`。

**IPC handler**（`ipc/handler/`，17 个，每个对应一个 SSU 操作）：`ubse_alloc_space_handler` / `ubse_free_space_handler` / `ubse_attach_space_handler` / `ubse_detach_space_handler` / `ubse_attach_linear_space_handler` / `ubse_detach_linear_space_handler` / `ubse_attach_striped_space_handler` / `ubse_detach_striped_space_handler` / `ubse_add_access_permission_handler` / `ubse_remove_access_permission_handler` / `ubse_get_ns_stats_handler` / `ubse_list_alloc_info_handler` / `ubse_get_alloc_info_by_name_handler` / `ubse_get_connect_info_handler` / `ubse_fe_device_alloc_handler` / `ubse_fe_device_free_handler` / `ubse_get_fe_device_list_handler`。

**IPC 抽象基类** `class UbseSsuHandler` —— `Execute(buffer, context)` 模板方法，子类实现 `Unpack`/`Handle`/`Pack` 三阶段；`RegisterSdkDispatcher()` 注册 SDK dispatcher。

### 7.5 npu NPU 控制器

**职责**：NPU 资源管理与虚机状态监控（[src/controllers/npu/](../../../src/controllers/npu/)）。

**关键文件**：
- `ubse_npu_controller_module.{h,cpp}` —— 模块入口与生命周期
- `ubse_npu_controller_dispatcher.{h,cpp}` —— 请求分发
- `ubse_npu_controller_process.{h,cpp}` —— 业务处理
- `ubse_npu_manager_api.{h,cpp}` —— 管理对外 API
- `ubse_npu_msg_execute.{h,cpp}` —— 消息执行
- `ubse_npu_source_def.{h,cpp}` —— 资源定义
- `resource_collection/` —— NPU 资源采集
- `vm_state_monitor/` —— 虚机状态监控

> npu 控制器暂未提供 `plugin_services/` 对外服务契约。

---

## 8. API Server 北向 API

**职责**：daemon 统一北向入口，承载 UDS IPC 服务器、权限校验、对象映射、IPC handler 注册分发。

**关键类** [src/api_server/ubse_api_server_module.h](../../../src/api_server/ubse_api_server_module.h)（命名空间 `api::server`）：

```cpp
class UbseApiServerModule final : public UbseModule {
public:
    static constexpr const char* kModuleName = "UbseApiServerModule";
    UbseResult Initialize() / UnInitialize() / Start() / Stop() override;
    UbseResult RegisterIpcHandler(uint16_t moduleCode, uint16_t opCode, UbseIpcHandler handler,
                                  const std::string &object = "");
    uint32_t SendResponse(uint32_t statusCode, uint64_t requestId, UbseIpcMessage &response);
    uint32_t AsyncSendLongLink(UbseRequestMessage requestMessage, void *ctx,
                               UbseAsyncResponseHandler handler, std::vector<uint64_t> &reqList);
private:
    std::unique_ptr<ubse::ipc::UbseIpcServer> ipcServer_{};
    std::vector<HandlerRegistration> pendingHandlers_;
    std::shared_ptr<UbseApiServerAuthManager> authManager_;
};
```

- 模块依赖：`CORE_MODULE_IMPL(UbseApiServerModule, UbseConfModule, UbseLoggerModule)`
- 常量：`UDS_PERM=0660`、`THREAD_POOL_SIZE=8`、`THREAD_POOL_QUEUE_SIZE=16`
- UDS 路径：`UBSE_UDS_SOCKET_PATH`（默认 `/var/run/ubse/ubse.sock`，与 SDK 端一致）
- **handler 装饰器** `DecorateHandlerWithReadnessCheck`：在原始 handler 外包裹 1) `IsAllModulesReady()` 检查 2) 通过 uid 获取 username 3) `authManager_->CheckPermission(userName, moduleCode, opCode)` 权限校验
- `RegisterIpcHandler` 支持延迟注册（push 到 `pendingHandlers_`），启动后批量注册

**全局 C 风格入口** [src/api_server/ubse_api_server.cpp](../../../src/api_server/ubse_api_server.cpp)：`RegisterIpcHandler(...)` / `SendResponse(...)`，内部委托 `UbseContext::GetInstance().GetModule<UbseApiServerModule>()`。

**权限管理** `class UbseApiServerAuthManager`：`LoadAuthConfig()` 加载用户/角色映射；`bool CheckPermission(username, moduleCode, opCode)` 先通过 `objectManager_` 将 (moduleCode, opCode) 映射到 object 字符串，再校验用户角色是否包含该 object。内置 `ADMIN_ROLE`、`ALL_OBJECT`、`BUILTIN_USERS`，支持 `AUTH_USER`/`AUTH_ROLE`/`AUTH_USER_DEFAULT`/`AUTH_ROLE_DEFAULT` 配置 section。

**对象映射** `class UbseApiServerObjectManager`：`AddObjectMapping(moduleCode, opCode, object)` / `GetObjectString(moduleCode, opCode)`，使用 `PairMap` + `shared_mutex` 并发安全。

> **归档位置**：`UbseApiServerAuthManager` 与 `UbseApiServerObjectManager` 实现位于 [src/framework/ipc/auth/](../../../src/framework/ipc/auth/)，对外头文件在 `src/framework/ipc/include/`，不在 `src/api_server/` 目录内。

> api_server 主要承载 UDS IPC 通道；HTTP 路由由具体控制器自行通过 `framework/http` 注册。两条通道并行存在。

---

## 9. SDK 多语言客户端

SDK 提供对 daemon 服务的访问能力，支持 **C / Go / Python** 三种语言。通信方式：

| 语言 | 通信方式 | 组织方式 |
|---|---|---|
| C | UDS 直连 daemon | 头文件 `include/` + 实现按模块拆分 + `libubse_helper` 内部 pack/unpack |
| Go | 双路：`engine/` 通过 dlopen 调 C 库；其余通过 `ipc/` 直接走 UDS | 按模块分目录，每目录一个 `.go`，`urmav1/` 兼容旧版本 |
| Python | 双路：`ubse_engine.py` 用 ctypes 加载 `libubse-client.so.1`；`ipc/` 直接走 UDS | `ffi/`(底层绑定) + `ipc/` + `models/` + 顶层业务封装 |

UDS 默认路径三端一致：`/var/run/ubse/ubse.sock`，最大消息 10MB，默认超时 30s。

### 9.1 C SDK

**头文件** [src/sdk/c/include/](../../../src/sdk/c/include/)：

- `ubs_engine.h`：客户端生命周期 `int32_t ubs_engine_client_initialize(const char *uds_path)` / `void ubs_engine_client_finalize(void)`
- `ubs_engine_mem.h`：内存借用全套 C 接口（30+ 函数）
  - NUMA：`ubs_mem_numa_create` / `_create_with_lender` / `_create_with_candidate` / `_get` / `_list` / `_delete` / `ubs_mem_numastat_get`
  - FD：`ubs_mem_fd_create` / `_create_with_lender` / `_create_with_candidate` / `_permission` / `_get` / `_list` / `_delete`
  - SHM：`ubs_mem_shm_create` / `_create_with_affinity` / `_create_with_lender` / `_attach` / `_get` / `_list` / `_list_with_prefix` / `_detach` / `_delete` / `_fault_get` / `_fault_register`
  - memid 映射：`ubs_mem_fd_get_memid_by_import` / `ubs_mem_numa_get_memid_by_import` / `ubs_mem_shm_get_memid_by_import`
  - 关键常量：`UBS_MEM_MAX_SLOT_NUM=16`、`UBS_MEM_MAX_MEMID_NUM=2048`、`UBS_MEM_MAX_NAME_LENGTH=48`、`UBS_MEM_MIN_SIZE=4MB`、`UBS_MEM_MAX_LENDER_CNT=4`、`UBS_MEM_FLAG_NO_WR_DELAY=0x1`、`UBS_MEM_FLAG_SHM_ANONYMOUS=0x2`、`UBS_MEM_FLAG_CACHEABLE=0x4`
  - 故障类型枚举 `ubs_mem_fault_type_t`：约 18 种故障 + `UB_MEM_HEALTHY=1000`
- `ubs_engine_topo.h`：`ubs_topo_node_list(node_list**, cnt*)` / `ubs_topo_node_local_get(node*)` / `ubs_topo_link_list(cpu_links**, cnt*)`
- `ubs_engine_urma.h`：`ubs_urma_dev_get(devices**, cnt*)` / `ubs_urma_dev_alloc(name, dev_info*)` / `ubs_urma_dev_free(name)`
- `ubs_engine_ssu.h`：约 20 个函数
  - 分配/释放：`ubs_ssu_space_alloc(req, result*)` / `ubs_ssu_space_free(name)`
  - 权限：`ubs_ssu_access_permission_add(name, nqn)` / `_remove`
  - 挂载：`ubs_ssu_space_attach`/`_detach`、`ubs_ssu_linear_space_attach`/`_detach`、`ubs_ssu_striped_space_attach`/`_detach`
  - 查询：`ubs_ssu_alloc_info_list` / `ubs_ssu_ns_stats_get` / `ubs_ssu_connect_info_get`
  - FE 设备：`ubs_ssu_fe_device_list` / `ubs_ssu_fe_device_alloc` / `ubs_ssu_fe_device_free`
  - 类型：`ubs_ssu_lba_format_t`(512/4K) / `ubs_ssu_raid_level_t`(RAID0/RAID5) / `ubs_ssu_chunk_size_t`(4K~512K) / `ubs_ssu_alloc_strategy_t`(STRIPED/LINEAR)
- `ubs_engine_log.h`：`ubs_engine_log_callback_register(handler)` 注册日志回调
- `ubs_error.h` / `ubs_engine_object_def.h`：错误码与对象定义

**实现**：`ubs_engine.c` / `ubs_engine_mem.cpp` / `ubs_engine_topo.cpp` / `ubs_engine_urma.cpp` / `ubs_engine_log.cpp` / `ubs_error.c`；`libubse_helper.{h,cpp}` 内部辅助层（错误映射、请求校验、缓冲区计算、pack/unpack）；`util/ubs_engine_pack_util.{h,cpp}` 通用打包工具；`ssu/ubs_ssu_validate.{h,cpp}`（命名空间 `ubs::ssu`）SSU 请求参数校验。

### 9.2 Go SDK

[src/sdk/go/](../../../src/sdk/go/) 模块组织：

- `engine/ubs_engine.go`：客户端生命周期，**通过 dlopen 调用 C 库**（`ubs_engine_client_initialize`/`_finalize`），cgo 桥接
- `dlopen/dl.go`：动态库加载封装（`GetLibHandle` / `LoadFunc` / `CloseLibHandle`）
- `errcode/`：错误码定义
- `ipc/ipc.go`：**直接 UDS IPC** 通信层
  - 常量：`UbseIpcSocketPath="/var/run/ubse/ubse.sock"`、`MaxMessageSize=10MB`、`DefaultTimeout=30s`
  - `SetSocketPath(path)` / `GetSocketPath()` / `ConnectToUnixSocket()` / `SendRequest(conn, moduleCode, opCode, request)` / `ReceiveResponse(conn)` / `InvokeCall(moduleCode, opCode, request)`
  - 消息格式：`isResp(1) + header(16) + body`；使用 `sync.Pool` 重用 4KB 缓冲区
- `mem/ubs_engine_mem.go`、`topo/ubs_engine_topo.go`、`urma/ubs_engine_urma.go` + `urmav1/`(兼容旧版本，含测试)
- `ssu/`：`ubs_engine_ssu.go` + `ubs_engine_ssu_pack.go` + `ubs_engine_ssu_validation.go` + `constants.go`
  - 结构体：`UbsSsuAllocSpaceReq`/`UbsSsuAllocResult`/`UbsSsuNamespaceInfo`/`UbsSsuSpaceReq`/`UbsSsuLinearSpaceReq`/`UbsSsuStripedSpaceReq`/`UbsUbVfe`/`UbsSsuFe`/`UbsSsuConnectInfo`/`UbsSsuNsStats`
  - 函数（通过 `ipc.InvokeCall(UbseModuleCode, OpCode, request)`）：`UbsSsuListAllocInfo`/`AllocSpace`/`FreeSpace`/`AddAccessPermission`/`RemoveAccessPermission`/`AttachSpace`/`DetachSpace`/`AttachLinearSpace`/`DetachLinearSpace`/`AttachStripedSpace`/`DetachStripedSpace`/`GetNsStats`/`GetConnectInfo`/`GetFeDeviceList`/`FeDeviceAlloc`/`FeDeviceFree`
  - 列表上限保护：`maxAllocResults=1024`
- `pack/ubs_binary_codec.go`：二进制编解码器（`NewBinaryPacker().PackString(s, maxLen).Bytes()`）
- `log/ubs_engine_log.go`：日志接口

### 9.3 Python SDK

[src/sdk/python/](../../../src/sdk/python/)（包名 `ubse`）：

- `ubse_engine.py`：顶层入口 `ubs_engine_client_initialize(uds_path)` / `ubs_engine_client_finalize()`，委托 `UbsEngineBindingBase`
- `ubs_engine_mem.py` / `ubs_engine_topo.py` / `ubs_engine_ssu.py` / `ubs_engine_log.py`：业务模块封装
- `ffi/`：底层绑定层
  - `ubs_engine_binding_base.py`：`ctypes.CDLL('/usr/lib64/libubse-client.so.1')` 加载 C 库
  - `ubs_engine_binding_{mem,topo,ssu,log}.py`：各业务模块 ctypes 绑定
  - `ubs_binary_codec.py`、`ubs_engine_types.py`、`ubs_error_registry.py`（`raise_for_status(module_code, status_code, op_code)` 按模块路由异常）
- `ipc/`：直接 UDS IPC 层
  - `ubs_engine_ipc.py`：常量 `UBSE_IPC_SOCKET_PATH="/var/run/ubse/ubse.sock"`、`MAX_MESSAGE_SIZE=10MB`、`DEFAULT_TIMEOUT=30`；`set_socket_path` / `get_socket_path` / `connect_to_unix_socket` / `send_request(sock, module_code, op_code, request)` / `receive_response(sock, module_code, op_code)` / `invoke_call(module_code, op_code, request)`；`struct.pack('<B HH I', ...)` 构造消息头
  - `ubs_engine_ipc_codes.py`：IPC 模块码/操作码常量
- `models/`：数据模型（`ubs_engine_model_mem.py` / `ubs_engine_model_ssu.py` / `ubs_engine_model_topo.py`）

---

## 10. CLI 命令行

**主入口** [src/cli/ubse_cli_main.cpp](../../../src/cli/ubse_cli_main.cpp)：

```cpp
int main(int argc, char *argv[]) {
    UbseCliValidateStartupConditions(argc, argv);
    auto &registry = UbseCliModuleRegistry::GetInstance();
    registry.UbseCliCallAllModuleSignUp();          // 触发所有模块注册
    auto args = UbseCliBuildArgs(argc, argv);
    UbseCliValidateArgsByWhitelist(args);            // 白名单校验
    registry.UbseCliHelpInfoParse(args);
    UbseCliValidateCommandArgsCount(args, ...);
    registry.UbseCliGetParseTool().UbseCliArgsParse(args);   // 参数解析
    UbseCliStartTimeoutTimer();
    UbseCliDisableIpcLog();
    UbseCliExecuteMatchedCommand(registry);          // 执行匹配命令
    UbseCliStopTimeoutTimer();
}
```

**核心框架类**（`ubse_cli_framework/`，命名空间 `ubse::cli::reg` / `ubse::cli::framework`）：

- `class UbseCliParse`：参数解析器，`UbseCliArgsParse(const std::vector<std::string>&)` 解析 `-x`/`--xxx` 选项
- `class UbseCliModuleRegistry`（单例）：模块注册中心
  - `UbseCliLoadedModule(module_name, creator)` 注册创建器
  - `UbseCliCallAllModuleSignUp()` 触发所有模块 `UbseCliSignUp()`
  - `UbseCliRegister(commands_info)` 批量注册命令
  - `UbseCliSetProgramContext(program_name, show_type_in_usage)`（默认 `ubsectl`）
  - 常量：`UBSE_MAX_CMD_NUM=20`、`UBSE_MAX_CMD_OR_TYPE_LENGTH=64`、`UBSE_MAX_OPTIONS_NUM=10`
- `class UbseCliRegModule`：模块注册基类，`virtual void UbseCliSignUp() = 0`；宏 `UBSE_CLI_REGISTER_MODULE(name, type)` 在全局作用域声明静态 helper 完成自注册
- `class UbseCliRegBuilder`：Builder 模式构建命令信息
  - `UbseCliSetCommand(command)` / `UbseCliSetType(type)` / `UbseCliAddOption(short, long, desc)` / `UbseCliSetFunc(func)` / `UbseCliBuild()`
  - `using UbseCliCommandFunc = std::shared_ptr<UbseCliResultEcho> (*)(const std::map<std::string, std::string>&)`
- `ubse_cli_res_builder`：结果回显（`UbseCliResultEcho`/`UbseCliStringEcho`/`UbseCliVariableCellEcho`）
- `ubse_cli_main_utils`：入口工具（`UbseCliValidateStartupConditions` / `UbseCliBuildArgs` / `UbseCliValidateArgsByWhitelist` / `UbseCliExecuteMatchedCommand` 等）

**命令注册机制**：静态自注册 → SignUp 触发 → Builder 构建 → 解析与执行 → 结果回显。

**命令分组**（每组继承 `UbseCliRegModule`）：

| 命令组 | 文件 | 主要命令 |
|---|---|---|
| mem | `ubse_cli_mem_cmd_reg.{h,cpp}` | `UbseCliQueryMem` / `UbseCliCheckMemoryStatus` / `CreateMemory` / `DeleteMemory` / `ShmMemoryAttach` / `ShmMemoryDetach` |
| node | `ubse_cli_node_cmd_reg.{h,cpp}` | `UbseCliQueryClusterInfo` / `UbseCliQueryCpuTopology` / `UbseCliQueryNodeInfo` |
| urma | `ubse_cli_urma_cmd_reg.{h,cpp}` | `UbseCliQueryUrmaQos` / `UbseCliQueryUrmaDevInfo` / `UbseCliActivateUrmaDevInfo` |
| cert | `ubse_cli_cert_cmd_reg.{h,cpp}` | `UbseCliChangeCaCrl`(CA CRL 导入) / `UbseCliCreateCert`(证书导入) / `UbseCliRemoveCert`(证书删除) |
| ssu | `ssu/` | `UbseCliDisplaySsu`(信息展示) / `UbseCliCreateSsu` / `UbseCliAttachSsu` / `UbseCliDetachSsu` |

**cert 工具** `ubse_cert/`（命名空间 `ubse::cli::cert`）：`ImportCertSet(serverCert, trustCert, serverKey, caCrl, errMsg&)` / `ImportCert(...)` / `ImportCaCrl(...)` / `DeleteCertSet(errMsg&)` / `CopyFileContents(src, dst, errMsg&)` / `CheckFilePathValid(path, isFile, errMsg&)`。

**SSU 专用入口** `ssu/ubse_cli_ssu_main.cpp`：程序名 `ubsectl-ssu`，默认 type 为 `ssu`，`UbseCliSetProgramContext("ubsectl-ssu", false)` 隐藏 type 显示。`ubse_cli_ssu_struct.{h,cpp}` 定义与 SSU handler 严格对齐的 wire 契约结构（请求/响应/元素），每个含 `Serialize/Deserialize`。`ubse_cli_ssu_limits.h` 定义默认值（`SSU_CLI_MIN_SIZE_BYTES` / `SSU_CLI_DEFAULT_NS_NUM` / `SSU_CLI_DEFAULT_HOST_NQN` 等）。

**mem consumer 子目录** `ubse_cli_consumer/`：独立内存 consumer CLI（`ubse_cli_mem_{create,attach,detach,query}` / `ubse_cli_mem_struct` / `ubse_cli_buffer_guard`）。

---

## 11. Adapter Plugins 适配器插件

`src/adapter_plugins/` 注册 6 个节点级适配器插件，以静态库形式被主体框架链接（bandbridge 为独立内核模块）。每个插件继承 `UbseModule`，由 `UbseContext` 加载。

### 11.1 mmi 内存管理适配器

**用途**：UBSE 与底层 OBMM（Out-of-Band Memory Manager，带外内存管理器）的适配层，把上层"内存借用"语义（fd/numa/shm/addr）翻译成对 obmm 设备的 import/export/unimport/unexport 操作；支持"预上线远端内存"。

**关键类** `class UbseMmiModule : public UbseModule`（命名空间 `ubse::mmi`）：
- 生命周期：`Initialize / UnInitialize / Start / Stop`
- 对象恢复：`UbseMemGetObjData(NodeMemDebtInfo&)`
- 借用执行器（按借用类型×操作组合）：
  - fd：`UbseMemFdImportPermissionExecutor` / `UbseMemFdImportExecutor` / `UbseMemFdUnImportExecutor` / `UbseMemFdExportExecutor` / `UbseMemFdUnExportExecutor`
  - numa：`UbseMemNumaImportExecutor / UnImport / Export / UnExport`
  - shm：`UbseMemShmImportExecutor / UnImport / Export / UnExport`
  - addr：`UbseMemAddrImportExecutor / UnImport / Export / UnExport`
- 预上线：`UbseMmiPreOnline(cnaTopoInfos, preImportSize)` / `UbseMmiUnPreOnline()`

**关键类型**（`ubse_mem_types.h`、`ubse_obmm_utils.h`）：
- `enum class UbseBorrowType { FD_BORROW=0, NUMA_BORROW=1, SHARE_BORROW=2, ADDR_BORROW=3 }`
- `enum class UbseObmmType { IMPORT=0, EXPORT=1 }`
- `struct UbseMemLocalObmmCustomMeta`（512 字节内 obmm 自定义存储，含 uid/gid/pid/username/导入导出节点/socket/numa/chipId/portId）—— 对账与故障恢复核心结构
- `class RmObmmUtils`（单例）：`GetPreOnlineInfo` / `GetPreOnlineSwitch`

依赖：`ubse_context / ubse_config / ubse_security / ubse_utils / ubse_com / ubse_http / ubse_election / ubse_controller`。

### 11.2 mti 管理与拓扑适配器

**用途**：通过 LCNE（Local Compute Node Engine）感知本节点和集群节点信息（CPU/Socket/Numa/CNA/EID）；通过带外 CtrlQ 与 1825 芯片/D2H 内存/URMA 设备通信获取芯片拓扑、EID、Bus Instance；通过 SMBIOS 区分 Clos vs FullMesh 组网。

**关键类** `class UbseLcneModule : public UbseModule`（命名空间 `ubse::mti`）：
- `UbseGetLocalNodeInfo(UbseMtiNodeInfo&)` / `UbseGetAllNodeInfos(vector<UbseMtiNodeInfo>&)`
- `UbseGetDevTopology(UbseDevTopology&)`
- `GetUbseLcneOSInfo()` / `GetCurSlotId()` / `GetMtiComEid()` / `GetLocalBoardIOInfo()` / `GetClusterIpList()` / `GetClusterLocalIp()`

子目录：`lcne/`（带内拓扑客户端）、`out_of_band/`（带外 ctrl-q 消息代理）、`smbios/`（SMBIOS 解析）。单例：`UbseMti1825::GetInstance()` / `UbseMtiUrma::GetInstance()`（实际为 `OutOfBand` 子类）。`GetCurNodeId(slotId, nodeId&)` 在 Clos 模式下借助 `UbseSmbios::GetServerIdx` 推导 nodeId。

依赖：`mti_out_of_band / ubse_election / ubse_xml / ubse_utils / ubse_com / ubse_context / ubse_log / ubse_http / ubse_config / ubse_smbios`。

### 11.3 ssu 共享存储适配器

**用途**：UBSE 与底层 SSU（共享存储/NVMe-oF 设备）适配层。提供设备发现 `GetDevList`、Namespace 增删 `CreateDevNameSpace/DeleteDevNameSpace`、Namespace 主机 Attach/Detach、Namespace 访问控制 `AddNameSpaceAllowHost/RemoveNameSpaceAllowHost`、块设备组合 `CreateBlockDevice/DeleteBlockDevice`(Linear/Striped)。

**关键类** `class UbseSsuAdapterImpl : public UbseSsuAdapterInterface`（单例，命名空间 `ubse::adapter_plugins::ssu::def`）：
- 通过 `UbseDlManager` 运行时 `dlopen("libssu.so")` 加载底层 SSU 库
- 函数指针：`AcquireDevInfoFunc / CreateNamespaceFunc / DeleteNamespaceFunc / AttachNamespaceFunc / DetachNamespaceFunc / AddNamespaceAllowHostFunc / RemoveNamespaceAllowHostFunc / GetNamespaceAllowHostsFunc / FreeAllowHostsMemFunc`
- 数据结构：`DevEidT`(16 字节 EID)、`enum DevStatusT`、`struct DevAddrT`(src/tgt EID + IP + subNqn + jettyId + useUb)、`struct NamespaceBaseAttrT`、`struct DevNamespaceInfoT`、`struct DevInfoT`(最多 64 个 namespace)

外部依赖：`libblockdev`(blockdev/bd_lvm/bd_mdraid)、`glib-2.0`。链接 `ubse_log / ubse_utils / ubse_config`。

### 11.4 syssentry 系统哨兵适配器

**用途**：UBSE 与 SysSentry（系统哨兵内核故障上报子系统）适配层。通过 `dlopen` 加载 xalarm 库（`XalarmRegisterFunc / XalarmGetEventFunc / XalarmUnRegisterFunc`），监听内核上报的故障事件并转发给 `UbseRasHandler`。

**关键类**：
- `class SysSentryModule : public UbseModule`（`OPTIONAL_MODULE_IMPL(SysSentryModule, UbseRasModule)`，依赖 RAS）—— 启动 `UbseRasObserver`、注册定时器查询 `sentry_msg_monitor` 状态、向 SysSentry 配置事件开关与广播 EID
- `class UbseRasObserver`（单例，`sentry_observer.h`）：`Start/Stop`、`UbseConfigSysSentry/WithRetry`、`RegQueryMsgMonitorTimer`、内部线程 `SentryEventListen`

### 11.5 urma_uvs URMA UVS 适配器

**用途**：通过 dlopen 加载 UVS（UB Verbs Service）库，向 ubcore 设置拓扑、按 EID 查询设备名、创建/删除聚合设备。函数指针：`UvsSetTopoInfo / UvsGetDeviceNameByUrmaEid / UvsCreateAggrDev / UvsDeleteAggrDev`。关键结构：`UbcoreTopoFe`、`UbcoreTopoAggrDev`、`UbcoreTopoLink`。

### 11.6 bandbridge 带宽桥接内核模块

**用途**：**Linux 内核模块**（.ko，非用户态），在内核态提供 UBSE 与硬件之间的"带宽桥接"驱动，通过 CtrlQ 与设备交互（与 mti/out_of_band 的用户态 ctrl_q 消息对应）。

**构建方式**：`Makefile` 使用 KBuild（`obj-m := bandbridge.o`），`make -C /lib/modules/$(uname -r)/build M=$(PWD) modules`；CMakeLists 仅在 `aarch64` 且存在 KDIR 时通过 `add_custom_target(bandbridge_ko ALL ...)` 触发构建，输出 `${CMAKE_BINARY_DIR}/modules/bandbridge.ko`。源文件：`bandbridge_{main,device,ctrlq,log,sample}.{c,h}`。协议 GPL v2。提供 install/uninstall/debug 目标（`insmod/rmmod`）。

---

## 12. Addons Rack 级插件

`src/addons/` 注册 4 个 **Rack 级插件**（区别于 adapter_plugins 的节点级适配器），以 **共享库** 形式构建，导出 C 入口 `UbsePluginInit(uint16_t modCode) / UbsePluginDeInit(void)`，由 `plugin_mgr` 通过 `dlopen` 加载。modCode 由 `conf/ubse_plugin_admission.conf` 分配。

### 12.1 rmrs 机架级内存资源调度

**职责**：机架级"内存超分"（over commit）—— 把集群内节点的空闲内存池化后，按虚机 NUMA 维度借给内存紧张节点；包含虚机内存跨节点迁移、SMAP 远端 NUMA 设置、UCache 策略对接、故障管理、选主、节点间消息传输。

- 库名：`mempooling` SHARED（+ `mempooling_static` 测试用），modCode 默认 777
- 链接：`rapidjson`、`ubse_utils`、libvirt
- C 入口：`rack_mempooling_plugin.h` 的 `UbsePluginInit(modCode) / UbsePluginDeInit()`

**关键类**：
- `class MpSubModule`（`include/mp_module.h`，基类）—— `Init/DeInit/Name()`，命名空间 `mempooling`
- `include/mp_error.h`：错误码体系，`MpResult = uint32_t`、`VmResult = uint32_t`；MID 高两字节 + ERRNO 低两字节
- `include/numa_info.h`：核心数据结构 `NumaMetaData`(nodeId/numaId/socketId/memTotal/memFree/hugePageTotal/cpuIds...)、`NumaVmInfo`(虚机内存/CPU 绑定)、`NumaInfo`、`NodeInfo`(节点+numa 列表)
- `include/rmrs_serialize.h`：自定义二进制序列化框架 `class RmrsOutStream` / `class RmrsInStream`，支持 vector/set/map/string，通过 `CountMember<T>` + `MemberTupleHelper<T, N>` + 宏 `RMRS_MEMBER_TUPLE_HELPER(n)` 实现聚合体反射式逐字段序列化（C++17 结构化绑定，最多 16 成员）
- `common/exportV2/exporter.h`：`class Exporter`（命名空间 `mempooling::exportV2`）—— 并行采集虚机与 NUMA 信息（`GetVmInfoImmediately / GetNumaInfoImmediately`），内置线程池 `ThreadPool`、`StartGate` 同步栅栏、`parallel_collect` 模板
- `common/util/mp_json_util.h`：`JsonUtil`（rapidjson）类型安全 JSON 字段读取
- `over_commit/` 核心调度逻辑子模块：
  - `collect/`：`collect_util` / `numa_memInfo_send` / `over_commit_serializer` —— NUMA 内存信息采集与发送
  - `election/`：`interface_guard` / `over_commit_election_handler` —— 选主保护
  - `fault_management/`：`over_commit_fault_management_handler` / `fault_memid_helper/module` / `fault_node_module` —— 故障处理
  - `mem_migrate_transport/`：`over_commit_msg(_handler)` / `set_smap_remote_numa_info_send` / `smap_remove_send` —— 内存迁移消息传输
  - `page_file/`：`page_file_helper` —— 大页文件辅助
  - `remote_mem_query/`：`smap_query_process_send` —— 远端内存查询
  - `storage/`：`over_commit_storage` —— 持久化
  - `ucache/`：`over_commit_ucache_strategy` —— UCache 策略对接
  - `vm_mem_migrate_strategy/`：`vm_mem_migrate_strategy` —— 虚机内存迁移策略

### 12.2 ucache 统一缓存插件

**职责**：基于应用瓶颈识别（短/长窗口阈值）和借用策略，在 master 节点统一调度集群借用动作；agent 节点执行借用/迁移；与选主事件联动（主备切换时停止/重启 master）。

- 库名：`ucache_plugin` SHARED，modCode 默认 778
- 链接 `rapidjson`、`ubse_utils`
- 配置：`src/addons/ucache/conf/plugin_ucache.conf`（构建时拷贝到 `${CMAKE_BINARY_DIR}/expand_conf`）

**关键类**：
- `rack_ucache_plugin.{h,cpp}`：C 入口 + 选主事件处理 `HandleSwitchOver / HandleStandbyToMaster / HandleChangeToMaster / HandleChangeToStandby / HandleChangeToAgent`（调用 `ucache::master::Exit / Init`）；`RegEvent / DeregEvent` 通过 `UbseElectionHandlerBuilder` 注册选主事件
- `common/ucache_config.h`：`class UcacheConfig`（单例）
  - `Initialize(modCode)` 加载 `LoadConfig / LoadStrategyConfig / LoadMasterConfig / LoadBottleneckConfig`
  - 配置项：`exportInterval`、`borrowSize`、`maxActionSize`、`balanceThreshold`、`scarcityThreshold`、`bottleneckThreshold`、`bottleneckShortSize/Threshold`、`bottleneckLongSize/Threshold`、`maxReliableTimes`、`masterInterval`、`masterMaxBorrowSize`、`nodeIds`
  - 约束：`MIN_BORROW_SIZE=128MB`、`MAX_BORROW_SIZE=4GB`、`MIN_BALANCE_THRESHOLD=1.0`
- `ucache_master/` 子模块：`borrow_action / borrow_strategy / mem_borrow / migrate_strategy / bottleneck_strategy / data_collect / master_task_controller / node_fault_handler / rack_msg`
- `ucache_agent/`：`agent_task_processor`

### 12.3 virt_agent 虚拟化代理插件

**职责**：虚拟化场景代理，覆盖虚机内存超分借用、虚机内存碎片整理、虚机确定性热迁移（HAM）、容器场景内存管理；提供 C/Go/Python 三套 SDK；通过 router 注册 SDK server 接口。

- 库名：`virtagent` SHARED，modCode 默认 205
- 链接 `rapidjson`、`boundscheck`；配置 `src/addons/virt_agent/conf/plugin_virt_agent.conf`、`src/addons/virt_agent/conf/auth-virt_agent.conf`

**关键类**：
- `rack_vm_plugin.{h,cpp}`：C 入口，按场景分发：
  - `VmConfiguration::Initialize(modCode)` → `CommonInit()`（动态加载 RMRS `MempoolingModule::Init`）→ 根据 `GetVirtSceneType()` 分发到 `VmSceneInit()` 或 `ContainerSceneInit()`
  - 解初始化顺序：`MemHandler::Terminate` → `FragmentationVmCollection::FragTerminate` → `StopThread` → `LibvirtHelper::DeInit` → `EscapeAlgorithmHelper::Close` → `MempoolingModule::DeInit`
- `router/router.{h,cpp}`：SDK 服务器注册入口
  - `VMCommonSdkServerInit()`：注册 `VmMigrate` + `VirtMemFragSdk::QueryRegister` + `VirtCaseConfSdk::Register`
  - `VMMemFragSdkServerInit()`：非超分场景，注册 `VirtMemFragSdk::Register`
  - `ContainerSdkServerInit()`：注册 `VirtContainerSdk::Register`
- `include/vm_sdk_def.h`：模块码与 op 码枚举（`UBS_VA_VM_MIGRATE=0x1001`、`UBS_VA_QUERY=0x1002`、`UBS_VA_MEM_FRAGMENTATION=0x1003`、`UBS_VA_CASE_CONF=0x1004`、`UBS_VA_CONTAINER=0x1005`）
- `include/vm_info.h`：`enum class VmEventType {UNKNOWN, START, SHUTDOWN, MIGRATION, MIGRATION_START, MIGRATION_END, OOM}`、`struct VmDomainInfo`(uuid/name/state/maxMem/usedMem/pid/numaMemInfo...)
- `mem_handler/mem_handler.h`：高低水线事件 ID（`RegisterMemEventNotifyFunc_high/low/clear`）、`enum WatermarkWarningType`、`struct Notify`
- `common/mempooling/mempooling_module.h`：动态加载 RMRS 的桥接模块
- `sdk/go/dlopen/dl.go`：Go SDK 通过 cgo `dlopen("/usr/lib64/libubs-virt-agent.so.1")` 加载 C SDK

主要子目录：`alarm_handler/`、`cloud_adapter/`(case_conf + vm_migrate)、`dao/`(ham_migrate_vm_info_storage / migrate_state_storage)、`export/`(libvirt_helper / os_helper / resource_export)、`libvirt_handler/`(fast_recovery)、`mem_fragmentation/`(fragmentation_vm_collection / hugepage_handler / mem_task_manager)、`mem_handler/`、`migrate/`(ham_migrate / migrate_strategy / vm_migrate_handler)、`over_commit/`(container + mem_manager + vm_mem_adapter)、`status_manager/`(resource_collect / status_manager / vm_task_counter)、`strategy/`(escape_algorithm_helper/module)。

### 12.4 process_mem 进程内存插件

**职责**：进程级内存管理，按 PID 维度采集与管控进程内存占用；配置由顶层 `conf/plugin_process_mem.conf` 提供（modCode 默认 234）。

**关键文件**：
- `process_mem_plugin.{h,cpp}`：插件 C 入口（`UbsePluginInit / UbsePluginDeInit`）
- `process_mem_pid_bridge.{h,cpp}`：PID 桥接（进程标识与宿主信息关联）
- `process_mem_pid_collect.{h,cpp}`：进程内存信息采集
- `process_mem_pid_config_manager.{h,cpp}`：配置管理
- `process_mem_pid_info_manager.{h,cpp}`：进程内存信息管理
- `process_mem_pid_manager_def.h`：公共定义

---

## 13. RAS 故障处理

**职责**：UBSE 的故障处理中枢，对接 SysSentry 上报的硬件/内核故障事件，按故障类型分发到注册的处理器；处理 OOM、BMC 下电、PANIC、内核 Reboot、内存 CE/UCE、网络故障；支持节点级状态回调（pre-fault、fault、clear）；通过 com 模块跨节点同步故障信息。

### 13.1 顶层接口

[src/include/ubse_ras.h](../../../src/include/ubse_ras.h)：
- 故障类型常量：`ALARM_REBOOT_EVENT=1003`(BMC 下电)、`ALARM_OOM_EVENT=1005`、`ALARM_PANIC_EVENT=1007`、`ALARM_KERNEL_REBOOT_EVENT=1009`、`ALARM_MEM_FAULT=1013`、`ALARM_MEM_UCE_PREDICT_EVENT=101`、`ALARM_MEM_UCE_EVENT=103`、`ALARM_NET_FAULT=110`
- `using AlarmFaultHandler = std::function<uint32_t(ALARM_FAULT_TYPE, std::string faultInfo)>`
- `enum class AlarmHandlerPriority { HIGH=0, MEDIUM=1, LOW=2 }`
- `struct AlarmHandler { alarmFaultEvent; name; handler; priority; }`
- C API：`RegisterAlarmFaultHandler(AlarmHandler)` / `RegisterAlarmFaultHandler(type, name, handler, priority)` / `UnRegisterAlarmFaultHandler(type, name&)`

### 13.2 关键类

**`UbseRasHandler`**（[src/ras/ubse_ras_handler.h](../../../src/ras/ubse_ras_handler.h)，单例）：
- `StartRasHandler()` —— 初始化
- `NodeFaultHandle(alarm_msg*)` —— 节点级故障回调入口
- `RegisterAlarmFaultHandler / UnRegisterAlarmFaultHandler` —— 注册按 `(faultType, priority, name)` 索引的处理器映射 `unordered_map<ALARM_FAULT_TYPE, map<AlarmHandlerPriority, HandlerMap>>`
- `RegisterNodeHandler(NodeHandlerType, NodeHandler)` / `CallNodeHandle(type, nodeId)` —— 节点控制器回调（4 种：`PRE_FAULT_STATE / PRE_FAULT_STATE_FAIL / NODE_FAULT_STATE / NODE_FAULT_STATE_CLEAR`）
- `AddProcessedMsgId / ClearAllMsgId / MsgIdHasBeenProcessed` —— 故障 msgId 去重
- 私有：`HandleBMCFault`、`HandleOomFault`、`HandlePanicAndRebootFault`、`HandleMemoryFault`、`ExecuteFaultHandler`
- 自由函数：`IsMemInitFinished`、`HandleCnaAndEidMsg`、`QueryNodeIdByEid`、`ReportAckToSysSentry`、`LogMemDebtInfoWithNode`
- 字符校验：`SPECIAL_CHAR_WHITE_LIST`、`IsAllowedSpecialChar`、`hasInvalidChars`（故障信息字段白名单防注入）

**`UbseRasComHandler` 系列**（跨节点通信处理器，继承 `UbseComBaseMessageHandler`）：
- `class UbseRasComHandler`（op=`UBSE_RAS_BMC_REBOOT`，module=`RAS`）
- `class UbseRasSwitchRoleHandler`（op=`UBSE_RAS_SWITCH_ROLE`）—— BMC 重启后角色切换
- `class UbseOomHandler`（op=`UBSE_RAS_OOM`）—— OOM 跨节点同步

**`UbseRasModule`**（[src/ras/ubse_ras_module.cpp](../../../src/ras/ubse_ras_module.cpp)）：
- `OPTIONAL_MODULE_IMPL(UbseRasModule, UbseElectionModule, UbseComModule, UbseNodeControllerModule)` —— 可选模块，依赖选主、com、节点控制器
- `Start()` 调用 `UbseRasHandler::GetInstance().StartRasHandler()`

### 13.3 OOM 处理流程

[src/ras/ubse_ras_oom_handler.cpp](../../../src/ras/ubse_ras_oom_handler.cpp) + [message/ubse_ras_oom_message.h](../../../src/ras/message/ubse_ras_oom_message.h)。入口 `OomHandler(ALARM_FAULT_TYPE, std::string faultInfo)` / `InitOomHandler()`。

**事件消息格式**（SysSentry 上报）：
```
{msgId}_{nr_nid:1,nid:[0,-1,...],sync:1,timeout:30000,reason:2,timesec:...,timeusec:...}
```
例如 `"1650_{nr_nid:1,nid:[0,-1,-1,-1,-1,-1,-1,-1],sync:1,timeout:30000,reason:2,timesec:1741057335,timeusec:469389}"`。

**处理流程**：
1. **`ParseEventMessage`**（正则 `^(\d+)_\{nr_nid:(\d+),nid:\[(-?\d+(?:,-?\d+)*)\],sync:(\d+),timeout:(\d+),reason:(\d+),timesec:(\d+),timeusec:(\d+)\}`）解析到 variant map
2. **`CheckCommonParam`**：校验 `reason`（0=KSWAPD、1=DIRECT_RECLAIM、2=HUGEPAGE_RECLAIM），校验 `nid` 数量 ≤ `MAX_NUMA_NUM=8`
3. **分流**：小页 OOM（`ProcessSmallpageOom`，当前仅打日志、ack）/ 大页 OOM（`CheckHugePageOomParam`，虚拟化场景，强制 `nr_nid=1` 且 `sync=1`）
4. **读取 NUMA 状态**：`IsRemoteNuma(numaId, isRemote&)`（读 `/sys/devices/system/node/nodeN/remote`）、`GetFreeHugepages(numaId, memFree&)`
5. **触发紧急迁移**：通过 `dlopen` 加载 smap 库符号 `SmapUrgentMigrateOutPtr`（`void(*)(uint64_t size)`），调用 `smapUrgentMigrateOutFunc(URGENT_SIZE=128MB)` 紧急迁出
6. **跨节点同步**：构造 `UbseRasOomMessage(nodeId, memNeed, oomNumaId)` 通过 com 通道发给 master/相关借出节点
7. **频率限制**：`FREQ_LIMIT=10`、`MAX_OOM_TIMEOUT_MS=3600000`（1 小时）
8. **配置感知**：读取 `ubse_plugin_admission` 段的 `virt_agent` 开关

**OOM 消息类**：
- `class UbseRasOomMessage : public UbseBaseMessage`：成员 `nodeId / memNeed / oomNumaId`，实现 `Serialize/Deserialize`
- `class UbseRasMessage : public UbseBaseMessage`：通用 RAS 消息，内含 `struct RasData { msgId; data; result; }`

### 13.4 水线处理

水线告警处理已随虚机内存超分能力迁移至 virt_agent 插件：`enum class WatermarkWarningType { NO_WARN=0, HIGH_WATERMARK, LOW_WATERMARK }` 与高低水线触发后的处理逻辑（增加/减少借用账本）现位于 `src/addons/virt_agent/mem_handler/`（见 `mem_handler.cpp` 的 `MemHandler::WaterNotifyEvent`）。

`water_process/ubse_mem_json_def.h`：`struct UbseMemEventNotifyBorrowItem`（借用条目）、`struct UbseMemEventNotifyMessage`（oomEventFlag/borrowItemNum/allNumaInfo/notifyNumaLoc/borrowItem 数组），均带 `ToJson()` 序列化，用于与 MMI 事件通知对接。

### 13.5 RAS 模块依赖

```
SysSentry(adapter_plugins/syssentry)
    └── dlopen xalarm → SentryEventListen 线程
            └── UbseRasHandler.HandleOomFault / HandleBMCFault / HandlePanicAndRebootFault / HandleMemoryFault
                    ├── UbseRasComHandler / UbseRasSwitchRoleHandler / UbseOomHandler (跨节点 com)
                    ├── OomHandler → ParseEventMessage → CheckHugePageOomParam → SmapUrgentMigrateOut
                    └── WaterWarningProcess (virt_agent/mem_handler)
```

---

## 14. Message 消息基类

**`class UbseBaseMessage : public Referable`**（命名空间 `ubse::message`，[src/message/ubse_base_message.cpp](../../../src/message/ubse_base_message.cpp) + [src/include/ubse_base_message.h](../../../src/include/ubse_base_message.h)）—— 所有跨进程/跨节点 IPC 消息的基类：

- 输入侧：`SetInputRawData(uint8_t*, size, copy=false)`、`SetInputRawDataFromShared(shared_ptr<uint8_t[]>, size)`、`InputRawData() / InputRawDataSize()`（最大 10MB：`MAX_IPC_DATA_PACKAGE_LEN=10485760`）
- 输出侧：`SerializedData() / SerializedDataSize()`、`GetSharedOutputData()`（转移所有权为 `shared_ptr`）
- 纯虚接口：`Serialize() / Deserialize()` 由子类实现
- 错误码：`SetErrCode / GetErrCode / CheckMsgBody`
- 类型转换模板：`Convert<Child>`（子类 → 基类 Ref）、`DeConvert<Parent>`（基类 → 父类 Ref）
- 静态成员：`gNullPtr`（空引用哨兵）

实现要点：使用 `securec` 的 `memcpy_s` 安全拷贝；`SafeMakeShared` 分配共享缓冲；`UBSE_DEFINE_THIS_MODULE("ubse")` 标记日志模块。子类示例：`UbseRasMessage`、`UbseRasOomMessage`（RAS）、virt_agent 的 `global_borrow_map_message` 等。

---

## 15. 测试体系

测试位于 [test/](../../../test/)，框架为 GoogleTest + GoogleMock（`test/CMakeLists.txt` 三级 fallback：系统 CONFIG → 系统 MODULE → FetchContent 拉取）。所有测试目标通过 `gtest_add_tests / gtest_discover_tests` 注册到 CTest，输出 JUnit XML 到 `${CMAKE_BINARY_DIR}/coverage/`。

**Mock 机制**：
- **mockcpp**（`3rdparty/mockcpp/`，源码构建 v2.7 + ARM64 补丁）—— C++ 函数/方法级 mock
- **手写桩类**：如 `test/UT/com/test_ubse_com_mock.h` 的 `TestChannel : public UBSHcomChannel` 实现 hcom 通道桩
- **mock_libs/ssu/**：SSU 适配器的 mock 动态库
- **IT/stubs/**：IT 桩模块（`capability_stub.cpp`、`mock_plugin/`、`mti/`、`net_util/`、`node_controller/`、`npu/` 等）

### 15.1 UT 单元测试

主二进制 `${CMAKE_PROJECT_NAME}_ut`，编译选项 `-fno-access-control`（打破访问权限测试私有成员）、`-w`（抑制警告）、`-rdynamic`（允许动态库访问可执行程序全局符号）。

按模块组织（`add_subdirectory` + `add_ut(module)` 宏为每模块创建独立 UT 可执行）：

| 类别 | 覆盖模块 |
|---|---|
| framework/（镜像 src/framework/） | vip、vscok（历史平铺目录 com/config/context/election/event/http/ipc/log 等仍位于顶层，待按镜像规则迁移） |
| controllers/（镜像 src/controllers/） | npu、ssu |
| 顶层历史目录 | mem_controller、mem_scheduler、node_controller、urma_controller、com、config、context、election、event、http、ipc、log、cert、cli、security、serial_tool、storage、thread_pool、timer、utils、node_mgr、plugin_mgr、api_server、ras、sdk、npu |
| adapter_plugins | mmi、mti、ssu、syssentry、smbios、urma_uvs、bandbridge（bandbridge 通过 OBJECT 库单独编译，不编入大 UT） |
| exclusive_executable | 单独执行的 UT 模块（对应 src/addons/，待迁移为 addons/） |

### 15.2 IT 集成测试

主二进制 `${CMAKE_PROJECT_NAME}_it`，链接核心模块。运行前 `sudo setcap cap_chown,cap_dac_override,cap_fowner,cap_sys_ptrace,cap_sys_nice=ep` 赋权。环境变量：`UBSE_UDS_ADDRESS / UBSE_REMOTE_ADDRESS / UBSE_IPC_UDS_PATH_PREFIX`。

**目录组织**：
- `controllers/`：控制器 IT（ssu）
- `infra/`：测试基础设施（client / cluster / config / helper / launcher / mock / node / process / spec / test_infra）
- `resource/`：测试资源（证书、CA 等）
- `scenarios/`：多节点组网场景（`tongsuan_1d_*` / `tongsuan_clos_*` / `zhisuan_npu_*`）
- `stubs/`：桩模块（`capability_stub.cpp`、`mock_plugin/`、`mti/`、`net_util/`、`node_controller/`、`npu/`）
- `tests/`：集成测试用例（client / election / fault / mem_borrow / npu / smoke / topo / urma）
- `IT_P0_CATALOG.md`：P0 用例目录清单

### 15.3 PT 性能测试

主二进制 `${CMAKE_PROJECT_NAME}_pt`，链接 `ubse_context / ubse_mock / ubse_config / ubse_plugin`，直接编译 `src/framework/log/ubse_logger.cpp`、`ubse_conf.cpp`、`ubse_com.cpp` 进入可执行。构建后拷贝 `conf/`，`INSTALL_RPATH=$ORIGIN/../lib`，输出 `pt_detail.xml`。覆盖 log/config/com 基线性能。

**PT 框架**：申请资源 → 创建进程 → 注册操作函数 → fork 子进程 → 执行 CMD 命令 → 终止子进程 → 清理资源。CMD 枚举 `TagTestCmdInfo { CMD_INIT, CMD_SERVER_START, CMD_CLI_START, CMD_SERVER_STOP, CMD_CLI_STOP, CMD_TELEMETRY_REPORT, CMD_MAX }`，命令函数签名 `int32_t Func(ProcessMmap*)`，通过 `PTestCmdAndFuncRegister(cmd, func, ProcessMode::MANAGER|CLI)` 注册（`main_test_pt.h`）。

### 15.4 FUZZ 模糊测试（已移除）

`test/FUZZ/` 目录当前已不存在。历史设计方案（基于 secodefuzz + asan，主二进制 `${CMAKE_PROJECT_NAME}_fuzz`，`DT_SetEnableFork / DT_SetCheckOutOfMemory / DT_Set_TimeOut_Second` 子进程隔离执行）已废弃；如需恢复模糊测试，按 [目录规范文档 3.4](../../conventions/目录规范文档.md) 的目标结构重建。

---

## 16. 第三方依赖

`3rdparty/`（入口 [3rdparty/CMakeLists.txt](../../../3rdparty/CMakeLists.txt)）：

| 依赖 | 用途 | 引入方式 |
|---|---|---|
| **cpp-httplib** | 轻量 C++ HTTP/HTTPS 库，供 `ubse_http` 提供 REST API 与 UBM/UBFM 通信 | `find_path + find_library`，要求 `cpp-httplib-devel >= 0.40.0`；`SOURCE_COMPILING=on` 时 FetchContent v0.40.0 |
| **hcom** | 华为 UBS 通信库（ubs-comm），节点间高/低层通信通道（HcomChannel、HLC、Put/Get/Recv），是 `ubse_com` 底层传输 | `find_path + find_library`，要求 `ubs-comm-devel >= 1.0.1-7`；`SOURCE_COMPILING=on` 时从 gitcode `openeuler/ubs-comm` 拉取自构建 |
| **libxml2** | XML 解析库，供 `ubse_xml` 解析配置/拓扑 XML | `find_package(LibXml2 REQUIRED)`，要求 `libxml2-devel >= 2.11.9` |
| **mockcpp** | C++ Mock 框架，仅 `BUILD_TESTS=ON` 引入，用于 UT/IT 函数/方法级 mock | `scripts/cmake/mockcpp.cmake` FetchContent v2.7 + ARM64 补丁，安装到 `deps/mockcpp/` |
| **rapidjson** | 高性能 JSON 头文件库，被 `ubse_json_util`、rmrs `JsonUtil`、ucache、virt_agent 广泛使用 | `find_package(RapidJSON QUIET)`，要求 `rapidjson-devel >= 1.1.0` |
| **securec** | 华为安全 C 运行时（libboundscheck），提供 `memcpy_s / memset_s / strcpy_s` 等安全函数；几乎所有源文件都包含 `securec.h` | `find_path + find_library`，要求 `libboundscheck-v1.1.11`；`libboundscheck INTERFACE` 目标 |

其他系统依赖：libuuid（trace_context）、libaudit（审计日志，动态加载）、libblockdev + glib-2.0（ssu 适配器）、libvirt（rmrs）、OpenSSL（cert/vscok/http TLS）。

---

## 17. 配置文件

`conf/`（构建时拷贝到 `cmake-build-*/conf/`，发布时打包并通过部署工具修改）：含 `ubse.conf`、`ubse_auth_default.conf`、`ubse_plugin_admission.conf`、`plugin_process_mem.conf`（process_mem 插件配置）与 `topo/`（`hccs-cross.json` / `non-cross.json` 拓扑配置）。

### 17.1 `ubse.conf` 主配置

按 INI 段组织：
- `[ubse.log]`：日志级别（DEBUG/INFO/WARN/ERROR/CRIT）、单文件大小（2-20MB）、文件数（1-200）、缓存队列（64-4096）、syslog 开关与类型
- `[ubse.election]`：选主心跳间隔（1000-60000ms）、丢失阈值（3-20）、是否参与候选、是否等待
- `[ubse.rpc]`：RPC 超时（0-65535s，默认 60）；TCP 模式集群 IP 列表（支持 `192.168.100.100-103` 段）；Clos 组网逻辑柜配置（2-4096）；Clos root 节点列表；`cert.use` TLS 证书开关
- `[ubse.ubfm]`：HTTPS TCP server 端口（默认 8082）、UBM 端口（默认 8799）、UBM vsock cid、TLS 主机名校验
- `[ubse.memory]`：内存块大小（4-4096MB，2 的幂）、`isLender` 是否可借出、`ubse.preonline.enable/size` 预上线开关与大小（128-262144MB，对齐 128）、`api.timeout` 借用 API 超时（1-3600s）、`lender.balance` 借出方 NUMA 负载均衡、`group` 借用分组、`provider` 专用借出节点
- `[ubse.vip]`：VIP 管理（默认关闭）、VIP HTTP 监听 IP/端口、Gratuitous ARP 数量/间隔
- `[ubse.ssu]`：SSU 管理 NVMe Qualified Name（Master 节点用于设备发现/namespace 管理/访问控制）

### 17.2 `ubse_auth_default.conf` 默认认证

定义默认用户-角色、角色-权限映射：
- `[auth.user.default]`：`ubsmd = default_ubsm`、`ubs-scheduler = default_op`、`matrixplugin = default_k8s`
- `[auth.role.default]`：
  - `default_ubsm = mem.fd,mem.numa,mem.shm,mem.stat,topo`（UBSM 管理员：fd/numa/shm 借用 + 内存统计 + 拓扑）
  - `default_op = topo`（运维：仅拓扑）
  - `default_k8s = mem.numa,topo`（K8s 插件：numa 借用 + 拓扑）

### 17.3 `ubse_plugin_admission.conf` 插件准入

分配各 Rack 插件的模块代码（modCode），用于 com 模块消息路由：
- `virt_agent = 205`（默认，当前注释）
- `mempooling = 777`（默认，当前注释）
- `ucache = 778`（默认，当前注释）
- `mem_plugin = 779`（当前唯一启用的项）

---

## 18. 模块依赖关系总览

```
context ── 管理 ──> 所有 *Module（拓扑排序激活）
config ── 被 ──> log/event/com(部分)/plugin/...（基础模块，无内部依赖）
log ── 依赖 ──> config, context, trace_context, libuuid, libaudit
trace_context ── 被 ──> com/event/ipc/log
thread_pool ── 被 ──> event/ipc/com(intercom)
com ── 依赖 ──> election(角色查询决定转发), node_mgr, log, misc, hcom, securec
event ── 依赖 ──> config, log, thread_pool
ha(election) ── 依赖 ──> com(收发选举报文), event(订阅链路/故障), node_mgr(集群节点), log
http ── 依赖 ──> cert, misc/secure_buffer, httplib
ipc ── 依赖 ──> thread_pool, api_server, misc/map_util, misc/request_id_util
plugin_mgr ── 依赖 ──> context, config, misc/lock, dlopen
security ── 依赖 ──> Linux kernel cap
storage ── 依赖 ──> com(RPC handler)
vip ── 依赖 ──> http, election(角色变更), misc/secure_buffer
node_mgr ── 依赖 ──> urma_uvs_module(可选), node_controller
vscok ── 依赖 ──> cert, misc/secure_buffer, OpenSSL
cert ── 依赖 ──> misc/file_util, OpenSSL
serde ── 无 framework 内部依赖（独立，仅 securec）
xml ── 无 framework 内部依赖（libxml2）
misc/* ── 无 framework 内部依赖（基础工具）

# Controllers 层
mem_controller ── 依赖 ──> mmi(执行器), framework核心, com, event
node_controller ── 依赖 ──> election, timer, mti(UbseTopologyInterface)
npu_controller ── 依赖 ──> framework核心, com
urma_controller ── 依赖 ──> urma_uvs, com, node_controller
ssu_controller ── 依赖 ──> ssu_adapter(libssu.so), com, ipc, http

# Adapter Plugins
mmi ── 依赖 ──> ubse_controller, com, http, election, security, config
mti ── 依赖 ──> mti_out_of_band, election, xml, com, smbios
ssu ── 依赖 ──> libblockdev, glib-2.0, log, utils, config
syssentry ── 依赖 ──> ras(OPTIONAL), com, http, xalarm
urma_uvs ── 依赖 ──> dlopen libuvs.so
bandbridge ── 内核模块，独立 KBuild

# Addons (Rack 插件，dlopen 加载)
rmrs ── 依赖 ──> rapidjson, ubse_utils, libvirt, election, com
ucache ── 依赖 ──> rapidjson, ubse_utils, election
virt_agent ── 依赖 ──> rapidjson, boundscheck, rmrs(动态加载), election
process_mem ── 依赖 ──> framework核心, election

# RAS
ras ── 依赖 ──> election, com, node_controller(OPTIONAL)
```

**跨层协作速览**：

```
[内核] bandbridge.ko ←─ctrlq─→ mti/out_of_band
                                  │
                                  ▼
[适配层 adapter_plugins]  mti ──┐
                              mmi ──→ obmm 设备（借用执行器）
                              ssu ──→ libssu.so（NVMe namespace）
                              syssentry ──→ xalarm（故障上报）
                              urma_uvs ──→ libuvs.so（URMA 拓扑）
                                  │
                                  ▼
[framework]  com/config/event/ha/http/ipc/log/plugin_mgr/security/serde/...
                                  │  plugin_mgr dlopen
                                  ▼
[addons Rack 插件]  rmrs(mempooling.so, 777) ──┐
                       ucache(ucache_plugin.so, 778) ──┼──→ 通过 com 跨节点
                       virt_agent(virtagent.so, 205) ──┤
                       process_mem(process_mem.so, 234)┘
                                  │
                                  ▼
[ras]  UbseRasHandler ──→ OomHandler ──→ SmapUrgentMigrateOut
                                  │
                                  ▼
[message]  UbseBaseMessage (IPC 消息基类，10MB 上限)
                                  │
                                  ▼
[test]  UT(按模块 add_ut) / IT(多节点组网场景) / PT(基线)
                                  │
                                  ▼
[3rdparty]  cpp-httplib / hcom / libxml2 / rapidjson / securec / mockcpp(测试)
                                  │
                                  ▼
[scripts/cmake]  building / ccache / gtest / doxygen / iwyu / libboundscheck / mockcpp / utils
```

---

## 19. 关键设计要点

### 19.1 模块化与拓扑激活

所有模块继承 `UbseModule`，通过 `CORE_MODULE_IMPL(Module, Dep...)` / `OPTIONAL_MODULE_IMPL(Module, Dep...)` 宏声明依赖关系，`UbseContext` 在启动期做拓扑排序后按序 `Init → Start`，停止时逆序 `Stop → Destroy`。`CheckDependencyType<T>()` 在编译期校验依赖类型完整、是 `UbseModule` 派生且非抽象。模块间通过 `UbseContext::GetModule<T>()` 获取句柄，避免硬编码单例耦合。

### 19.2 多跳路由与角色感知通信

`UbseRouteTable` 支持精确（capacity=0）、分组（capacity=2^n，将 dstNodeId-1 低 n 位置 0 做机柜级分组匹配）、默认（capacity=UINT32_MAX）三种路由。`com` 层 `RpcSend` 根据当前节点角色（master/agent/standby，来自 election 模块）自动选择直发或转发到 master，对业务透明。

### 19.3 高可用选主

`ha` 模块实现 master/standby/agent 三角色 + global 多组级联选主。`ElectionRole` 抽象基类派生 8 个子类，`RoleMgr` 单例管理角色切换。故障时通过 `SwitchMasterFromStandby / SwitchAgentFromMaster` 自动切换，并通过 `UbseElectionChangeAttachHandler` 通知所有订阅者（如 vip 模块 bind/unbind VIP、ucache/rmrs 停止/重启 master）。选举任期 `GetTurnId()` 防止脑裂。

### 19.4 内存借用四形态与账本

内存池化支持 FD / NUMA / Addr / SHM 四种借用形态，由 `mem_controller` 统一调度。`mmi` 适配器把借用语义翻译成对 obmm 设备的 import/export/unimport/unexport 操作。每次借用维护 `UbseMemLocalObmmCustomMeta`（512 字节内含 uid/gid/pid/username/节点/socket/numa 等）作为对账与故障恢复核心结构。`UbseMemDebtCircleCheck` 防止借用成环。高低水线（默认 88/11）触发借用增减。

### 19.5 UDS IPC 双连接模式

`ipc` 模块支持：
- **短连接**（TRANSIENT）：每次请求-响应建立连接，按用户限额（默认每用户 8 个瞬态连接）
- **长连接**（PERSISTENT）：建立后保持，用于事件订阅与服务端反向通知，`AsyncSendLongLink` 推送，`UbseSyncReq` 同步等待响应

`api_server` 的 handler 装饰器在调用前做 `IsAllModulesReady()` 检查 + uid→username 映射 + `CheckPermission` 权限校验，实现统一鉴权。

### 19.6 插件化扩展

两层插件机制：
- **adapter_plugins**（节点级，静态库链接）：mmi/mti/ssu/syssentry/urma_uvs 适配底层硬件与子系统，bandbridge 为内核模块
- **addons**（Rack 级，共享库 dlopen）：rmrs/ucache/virt_agent/process_mem 实现机架级业务，由 `plugin_mgr` 按准入配置加载，通过 `UbsePluginInit(modCode) / UbsePluginDeInit()` 管理生命周期，`NotifyPluginReadyStatus` 回报就绪

### 19.7 故障处理与紧急迁移

RAS 模块作为故障中枢，对接 SysSentry 上报的硬件/内核故障。OOM 处理流程：解析事件消息 → 校验参数 → 读 NUMA 状态 → dlopen smap 库调用 `SmapUrgentMigrateOut(128MB)` 紧急迁出 → 跨节点 com 同步 → 频率限制（1 小时窗口）。BMC 下电通过 `UbseRasSwitchRoleHandler` 跨节点同步角色切换。故障 msgId 去重防止内核重插导致重复处理。

### 19.8 安全构建

`building.cmake` 默认开启：`-fstack-protector-strong`（栈保护）、`-D_FORTIFY_SOURCE=2`（非 Debug）、`-ftrapv`（Debug 整数溢出检测）、`-Wl,-z,noexecstack`（NX 栈）、`-Wl,-z,relro,-z,now`（GOT 只读）、`-fPIE/-fPIC`。所有源文件使用 `securec` 的 `memcpy_s/memset_s` 替代不安全 CRT 函数。`security` 模块基于 Linux capability 管理进程权限集。mTLS 双向认证用于 http/vscok。

### 19.9 多语言 SDK 一致性

C/Go/Python 三套 SDK 通过统一的 UDS 协议（消息格式 `isResp(1) + header(16) + body`，路径 `/var/run/ubse/ubse.sock`，10MB 上限，30s 超时）保持一致性。Go/Python SDK 既有 ctypes/cgo 调用 C 库的路径，也有直接走 UDS 的独立实现，便于不同场景选型。

---

> **文档说明**：本文档基于源码静态分析整理，覆盖项目整体架构、21 个框架子模块、5 个资源控制器（含 npu）、API Server、三语言 SDK、CLI、6 个适配器插件、4 个 Rack 插件（含 process_mem）、RAS 故障处理、消息基类、测试体系、第三方依赖、配置文件与模块依赖关系。所有文件引用均为相对路径，可直接定位源码。
