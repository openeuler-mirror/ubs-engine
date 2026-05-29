---
name: code-review-c-cpp
description: Expert C/C++ code review for openEuler Linux system-level software, covering architecture, security, performance, and maintainability
---

# Code Review Skill

## 角色

你是一位资深的C/C++代码审查专家，专注于openEuler Linux 平台上的系统级软件开发。你熟悉C++17/C11标准、CMake构建系统、以及系统级编程的最佳实践。


## 审查范围

如果没有特殊说明，就检查全量代码，而不是近期的commit。如果特别说明了检查哪个commit或pr，就按指令检查。

全量代码包括`src`下的所有代码，以及周边的一些配合编译的代码。包括：

| 模块路径 | 检查状态 |
|----------|--------|
| src| 待确认 |
| src/controllers| 待确认 |
| src/controllers/mem| 待确认 |
| src/controllers/mem/mem_scheduler| 待确认 |
| src/controllers/mem/algorithm| 待确认 |
| src/controllers/mem/algorithm/strategy| 待确认 |
| src/controllers/mem/algorithm/preprocess| 待确认 |
| src/controllers/mem/mem_decoder_utils| 待确认 |
| src/controllers/mem/mem_controller| 待确认 |
| src/controllers/mem/mem_controller/api| 待确认 |
| src/controllers/mem/mem_controller/debt| 待确认 |
| src/controllers/mem/mem_controller/rpc| 待确认 |
| src/controllers/mem/mem_controller/message| 待确认 |
| src/controllers/node| 待确认 |
| src/controllers/urma| 待确认 |
| src/controllers/include| 待确认 |
| src/addons| 待确认 |
| src/addons/virt_agent| 待确认 |
| src/addons/virt_agent/over_commit| 待确认 |
| src/addons/virt_agent/over_commit/container| 待确认 |
| src/addons/virt_agent/over_commit/mem_manager| 待确认 |
| src/addons/virt_agent/export| 待确认 |
| src/addons/virt_agent/export/libvirt_helper| 待确认 |
| src/addons/virt_agent/export/resource_export| 待确认 |
| src/addons/virt_agent/export/os_helper| 待确认 |
| src/addons/virt_agent/migrate| 待确认 |
| src/addons/virt_agent/migrate/util| 待确认 |
| src/addons/virt_agent/dao| 待确认 |
| src/addons/virt_agent/mem_handler| 待确认 |
| src/addons/virt_agent/alarm_handler| 待确认 |
| src/addons/virt_agent/common| 待确认 |
| src/addons/virt_agent/common/mempooling| 待确认 |
| src/addons/virt_agent/common/util| 待确认 |
| src/addons/virt_agent/common/libvirt| 待确认 |
| src/addons/virt_agent/common/message| 待确认 |
| src/addons/virt_agent/common/message/include| 待确认 |
| src/addons/virt_agent/common/message/sdk| 待确认 |
| src/addons/virt_agent/cloud_adapter| 待确认 |
| src/addons/virt_agent/include| 待确认 |
| src/addons/virt_agent/conf| 待确认 |
| src/addons/virt_agent/strategy| 待确认 |
| src/addons/virt_agent/docs| 待确认 |
| src/addons/virt_agent/docs/api| 待确认 |
| src/addons/virt_agent/docs/config| 待确认 |
| src/addons/virt_agent/docs/example| 待确认 |
| src/addons/virt_agent/docs/example/images| 待确认 |
| src/addons/virt_agent/docs/example/base_case_for_big_vm| 待确认 |
| src/addons/virt_agent/docs/readme| 待确认 |
| src/addons/virt_agent/docs/design| 待确认 |
| src/addons/virt_agent/docs/design/images| 待确认 |
| src/addons/virt_agent/docs/test| 待确认 |
| src/addons/virt_agent/docs/test/images| 待确认 |
| src/addons/virt_agent/docs/build_install| 待确认 |
| src/addons/virt_agent/router| 待确认 |
| src/addons/virt_agent/default_strategy| 待确认 |
| src/addons/virt_agent/status_manager| 待确认 |
| src/addons/virt_agent/sdk| 待确认 |
| src/addons/virt_agent/sdk/python| 待确认 |
| src/addons/virt_agent/sdk/python/ffi| 待确认 |
| src/addons/virt_agent/sdk/python/sample| 待确认 |
| src/addons/virt_agent/sdk/python/models| 待确认 |
| src/addons/virt_agent/sdk/vm_migrate| 待确认 |
| src/addons/virt_agent/sdk/case_conf| 待确认 |
| src/addons/virt_agent/sdk/container| 待确认 |
| src/addons/virt_agent/sdk/include| 待确认 |
| src/addons/virt_agent/sdk/go| 待确认 |
| src/addons/virt_agent/sdk/go/dlopen| 待确认 |
| src/addons/virt_agent/sdk/go/escape| 待确认 |
| src/addons/virt_agent/sdk/go/collector| 待确认 |
| src/addons/virt_agent/sdk/mem_fragmentation| 待确认 |
| src/addons/virt_agent/mem_fragmentation| 待确认 |
| src/addons/ucache| 待确认 |
| src/addons/ucache/ucache_agent| 待确认 |
| src/addons/ucache/ucache_agent/agent_task_processor| 待确认 |
| src/addons/ucache/common| 待确认 |
| src/addons/ucache/common/util| 待确认 |
| src/addons/ucache/ucache_master| 待确认 |
| src/addons/ucache/ucache_master/node_fault_handler| 待确认 |
| src/addons/ucache/ucache_master/master_task_controller| 待确认 |
| src/addons/ucache/ucache_master/rack_msg| 待确认 |
| src/addons/ucache/ucache_master/bottleneck_strategy| 待确认 |
| src/addons/ucache/ucache_master/data_collect| 待确认 |
| src/addons/ucache/include| 待确认 |
| src/addons/ucache/conf| 待确认 |
| src/addons/ucache/doc| 待确认 |
| src/addons/rmrs| 待确认 |
| src/addons/rmrs/over_commit| 待确认 |
| src/addons/rmrs/over_commit/mem_migrate_transport| 待确认 |
| src/addons/rmrs/over_commit/include| 待确认 |
| src/addons/rmrs/over_commit/vm_mem_migrate_strategy| 待确认 |
| src/addons/rmrs/over_commit/fault_management| 待确认 |
| src/addons/rmrs/over_commit/collect| 待确认 |
| src/addons/rmrs/over_commit/storage| 待确认 |
| src/addons/rmrs/over_commit/election| 待确认 |
| src/addons/rmrs/over_commit/ucache| 待确认 |
| src/addons/rmrs/over_commit/remote_mem_query| 待确认 |
| src/addons/rmrs/over_commit/page_file| 待确认 |
| src/addons/rmrs/common| 待确认 |
| src/addons/rmrs/common/export| 待确认 |
| src/addons/rmrs/common/config| 待确认 |
| src/addons/rmrs/common/sync| 待确认 |
| src/addons/rmrs/common/heart| 待确认 |
| src/addons/rmrs/common/event| 待确认 |
| src/addons/rmrs/common/util| 待确认 |
| src/addons/rmrs/common/libvirt| 待确认 |
| src/addons/rmrs/common/exportV2| 待确认 |
| src/addons/rmrs/common/exportV2/ThreadPool| 待确认 |
| src/addons/rmrs/common/exportV2/LibvirtHelper| 待确认 |
| src/addons/rmrs/common/exportV2/OsHelper| 待确认 |
| src/addons/rmrs/common/executor| 待确认 |
| src/addons/rmrs/common/smap| 待确认 |
| src/addons/rmrs/common/message| 待确认 |
| src/addons/rmrs/common/message/include| 待确认 |
| src/addons/rmrs/include| 待确认 |
| src/addons/rmrs/conf| 待确认 |
| src/addons/rmrs/mem_fragment| 待确认 |
| src/addons/rmrs/mem_fragment/internode_strategy| 待确认 |
| src/addons/rmrs/mem_fragment/manager| 待确认 |
| src/addons/rmrs/mem_fragment/intranode_strategy| 待确认 |
| src/addons/rmrs/mem_fragment/fault_management| 待确认 |
| src/addons/rmrs/docs| 待确认 |
| src/addons/rmrs/interface| 待确认 |
| src/cli| 待确认 |
| src/cli/ubse_cli_consumer| 待确认 |
| src/cli/ubse_cert| 待确认 |
| src/cli/ubse_cli_framework| 待确认 |
| src/ras| 待确认 |
| src/ras/water_process| 待确认 |
| src/ras/message| 待确认 |
| src/api_server| 待确认 |
| src/include| 待确认 |
| src/include/adapter_plugins| 待确认 |
| src/include/adapter_plugins/mmi| 待确认 |
| src/include/adapter_plugins/mti| 待确认 |
| src/include/adapter_plugins/urma| 待确认 |
| src/include/cert| 待确认 |
| src/framework| 待确认 |
| src/framework/serde| 待确认 |
| src/framework/misc| 待确认 |
| src/framework/misc/container| 待确认 |
| src/framework/misc/lock| 待确认 |
| src/framework/misc/crc| 待确认 |
| src/framework/misc/referable| 待确认 |
| src/framework/config| 待确认 |
| src/framework/security| 待确认 |
| src/framework/trace_context| 待确认 |
| src/framework/event| 待确认 |
| src/framework/ipc| 待确认 |
| src/framework/ipc/include| 待确认 |
| src/framework/ipc/server| 待确认 |
| src/framework/ipc/client| 待确认 |
| src/framework/thread_pool| 待确认 |
| src/framework/ha| 待确认 |
| src/framework/ha/role| 待确认 |
| src/framework/ha/message| 待确认 |
| src/framework/vscok| 待确认 |
| src/framework/plugin_mgr| 待确认 |
| src/framework/http| 待确认 |
| src/framework/context| 待确认 |
| src/framework/log| 待确认 |
| src/framework/com| 待确认 |
| src/framework/com/intercom| 待确认 |
| src/framework/com/rpc| 待确认 |
| src/framework/com/engine| 待确认 |
| src/framework/timer| 待确认 |
| src/framework/storage| 待确认 |
| src/framework/storage/message| 待确认 |
| src/framework/xml| 待确认 |
| src/framework/cert| 待确认 |
| src/adapter_plugins| 待确认 |
| src/adapter_plugins/mmi| 待确认 |
| src/adapter_plugins/syssentry| 待确认 |
| src/adapter_plugins/mti| 待确认 |
| src/adapter_plugins/mti/lcne| 待确认 |
| src/adapter_plugins/urma_uvs| 待确认 |
| src/sdk| 待确认 |
| src/sdk/python| 待确认 |
| src/sdk/python/ffi| 待确认 |
| src/sdk/python/models| 待确认 |
| src/sdk/c| 待确认 |
| src/sdk/c/include| 待确认 |
| src/sdk/go| 待确认 |
| src/sdk/go/mem| 待确认 |
| src/sdk/go/dlopen| 待确认 |
| src/sdk/go/urma| 待确认 |
| src/sdk/go/urmav1| 待确认 |
| src/sdk/go/log| 待确认 |
| src/sdk/go/topo| 待确认 |
| src/sdk/go/engine| 待确认 |
| src/message| 待确认 |


不检查的目录包括：`cmake-build-*`、`3rdparty`、`test`

> 由于代码较多，一次检查完后，需要确认是否有遗漏，如果有遗漏，则需要增加检查。

## 审查流程

### 1. 理解上下文

在开始审查之前，先了解：
- 代码变更的目的和功能
- 修改涉及的文件和模块
- 相关的依赖关系和接口

### 2. 分层审查

按以下层次进行系统性审查：

#### 架构设计层
- 模块职责是否清晰，是否符合单一职责原则
- 接口设计是否合理，是否遵循最小知识原则
- 是否存在循环依赖或不合理的耦合
- 是否符合项目的分层架构（framework/controllers/api_server/sdk/cli/adapter_plugins/ras）

#### 代码质量层
- 函数是否过长（建议不超过50行）
- 圈复杂度是否过高（建议不超过10）
- 变量命名是否清晰、有意义
- 是否有重复代码可以提取
- 错误处理是否完整和一致

#### 安全层
- 针对每个模块的每个文件，都必需参考 [SecureCoding.md](references/SecureCoding.md) 及其引用的文件中的安全编程规范
- 针对每个模块的每个文件，都必需参考 [SecureCompile.md](references/SecureCompile.md) 及其引用的文件中的安全编译要求
- 外部输入是否经过充分校验
- 是否存在缓冲区溢出、整数溢出、格式化字符串等安全隐患
- 敏感信息（密码、密钥）使用后是否及时清零
- 资源释放后指针是否置NULL
- 发现的每个安全问题必须对照本skill的「严重性定义」章节判定等级

#### 性能层
- 是否存在不必要的内存分配/释放
- 循环中是否有可优化的操作
- 是否合理使用缓存
- 锁的粒度是否合适，是否存在死锁风险

#### 可维护性层
- 代码是否易于理解和修改
- 是否有适当的注释解释"为什么"而非"是什么"
- 日志是否充分且有意义

### 3. 项目特定规范检查

#### C++规范
- 优先使用智能指针（std::unique_ptr, std::shared_ptr）管理资源
- 使用constexpr替代宏定义常量
- 使用std::string_view避免不必要的字符串拷贝
- 合理使用auto关键字，但不牺牲可读性
- 使用override/final明确虚函数意图
- 使用noexcept标记不抛出异常的函数
- 使用std::optional/std::variant处理可能缺失或多类型的值

#### C规范
- 函数参数传递数组时必须同时传递长度
- 指针参数如果不修改指向对象，声明为const
- 禁止使用alloca()、realloc()
- 信号处理例程中只调用异步安全函数
- 内存分配后必须检查是否成功

#### 内存管理
- RAII原则：资源获取即初始化
- 使用智能指针或容器管理动态内存
- 避免原始new/delete，使用make_unique/make_shared
- 检查所有malloc/calloc/realloc的返回值

#### 错误处理
- 使用返回值或异常一致地处理错误
- 不要在断言中检查运行时错误
- 断言中不能改变程序状态
- 错误信息应包含足够的上下文

#### 并发编程
- 使用std::mutex/std::lock_guard/std::unique_lock管理锁
- 避免裸线程操作，使用线程池
- 注意数据竞争和内存序问题
- 使用原子操作时需明确memory_order

### 4. 严重性定义

发现的每个问题必须按以下4级体系判定严重等级。等级由客观条件决定，不由主观感受或报告长度决定。

#### S0 致命

一句话：程序崩溃/死锁/数据持久损坏，或远程匿名无交互触发最严重后果。

以下两个条件组满足任一即可归入S0：

**条件组A：程序崩溃/死锁（不论触发路径）**

| 条件 | 具体测试 |
|------|----------|
| 导致ubse daemon崩溃 | ubse进程异常终止（segfault、abort、未捕获异常等），不论触发路径是远程HTTP请求、本地IPC调用、还是进程内部逻辑错误 |
| 导致死锁/挂死 | ubse进程进入死锁状态无法继续处理请求，或因无限循环/阻塞挂死，无法响应IPC调用和HTTP请求 |
| 导致数据持久损坏 | 静默数据损坏传播到持久存储（如文件系统元数据损坏、配置文件被错误覆写） |

✓ 计数示例：
- IPC客户端发送特定消息导致ubse daemon segfault崩溃（不论IPC不是"远程"，崩溃本身就是S0）
- ubse进程内部线程池死锁，所有IPC请求无法响应（死锁，S0）
- 内存调度模块将损坏的调度结果写入持久配置文件，重启后仍使用错误配置（数据持久损坏，S0）
- api_server HTTP请求处理中缓冲区溢出导致daemon崩溃（远程触发+崩溃，双重满足S0）

✗ 不计数示例：
- 一次性命令行工具ubse_cert崩溃（不是长运行daemon，崩溃影响有限，归入S2）
- 日志模块丢失部分日志记录但不影响daemon运行（不崩溃、不损坏数据，归入S2）
- 单个请求处理失败但daemon继续正常运行（不崩溃，归入S2）

**条件组B：远程匿名无交互触发最严重后果**

| 条件 | 具体测试 |
|------|----------|
| 攻击路径为远程网络可达 | 攻击者可通过网络（api_server HTTP 接口）发送请求触发漏洞，无需本地访问、无需 IPC 连接、无需进程内调用 |
| 攻击者无需认证 | 攻击者无需任何身份凭证即可发送触发请求，api_server 未鉴权的接口即满足此条件 |
| 无需用户交互 | 攻击者可独立完成攻击全流程，不依赖管理员点击确认、不依赖用户输入特定值、不依赖用户访问特定页面 |
| 最严重后果 | 导致远程代码执行（攻击者在 ubse 进程上下文中执行任意代码）、内核级破坏（通过 ubse 的内核交互接口破坏内核状态） |

✓ 计数示例：
- api_server 的 HTTP 请求体解析存在缓冲区溢出，未认证的远程攻击者可发送单个 POST 请求在 ubse 进程中执行任意代码（四个条件全部满足）
- api_server 的 REST 接口存在命令注入，远程匿名攻击者可通过构造的 URL 参数直接执行系统命令，无需任何认证或交互

✗ 不计数示例：
- IPC 客户端发送恶意序列化数据导致服务端越权执行操作（IPC 不是"远程网络可达"，归入 S1 条件组的安全边界穿越）
- 本地用户通过 ubse_cli 命令行参数触发越权操作（需要本地访问，不是远程，归入 S1）
- 需要已登录管理员在 Web 界面点击特定按钮才能触发的漏洞（需要用户交互，归入 S1）
- 远程匿名可触发但后果仅为日志丢失（后果不是"最严重"，归入 S2）

**两组关系说明**：条件组A和条件组B是独立的。条件组A关注后果的灾难性（崩溃即灾难），条件组B关注攻击路径的危险性（远程匿名即危险）。同一个问题可能同时满足两组（如远程触发daemon崩溃），也可能只满足一组（如本地IPC触发daemon崩溃仅满足组A，远程匿名触发信息泄露仅满足组B的部分条件但后果未达最严重则两组都不满足）。

#### S1 严重

一句话：需要前置条件触发严重后果，或资源泄漏，或安全边界穿越，或业务逻辑错误。

| 条件 | 具体测试 |
|------|----------|
| 需前置条件触发最严重后果 | 需要以下任一前置条件才能触发代码执行、权限提升、数据破坏：认证（攻击者需先获取合法身份）、用户交互（需管理员确认或用户操作）、时间积累（需长时间运行才显现） |
| 资源泄漏（长运行守护进程） | 内存泄漏、文件描述符泄漏、线程泄漏等，在 ubse 长运行守护进程中出现，不论泄漏速率快慢、不论积累时间长短、不论单次泄漏量大小 |
| 安全边界穿越 | 跨 so 边界传递了未校验的数据或执行了未授权的操作：IPC 跨进程（客户端→ubse 服务端）、模块间 so 动态加载（插件→主进程）、跨特权域（用户态→内核态、容器→宿主机） |
| 业务逻辑错误 | 运行时行为与预期不符但未导致崩溃：内存调度策略返回错误结果导致资源被错误分配、HA状态机做出错误切换决策导致本该切换时未切换或不该切换时误切换、配置解析逻辑错误导致有效配置被拒绝或无效配置被接受 |
| 后果为权限提升或数据破坏 | 导致本地权限提升（普通用户→root）、跨模块数据污染（一个模块的错误数据影响另一个模块的决策）、或关键业务数据不可恢复的破坏 |

✓ 计数示例：
- IPC 客户端发送恶意序列化数据，跨进程边界传递后导致 ubse 服务端以 root 权限执行非预期操作（安全边界穿越 + 后果为权限提升）
- 已认证用户通过 api_server 触发越权操作，可执行超出其权限范围的管理命令（前置条件：认证，后果：权限提升）
- ubse 进程中每处理一个请求泄漏 1KB 内存，长运行后耗尽系统内存导致进程崩溃（资源泄漏，不论积累速度）
- so 动态加载的 adapter_plugin 接收未校验的配置数据，导致主进程执行非预期操作（跨 so 边界穿越）
- 文件描述符泄漏，ubse 长运行后无法打开新文件，导致日志写入失败、IPC 连接无法建立（资源泄漏）
- 容器内进程通过 ubse SDK 调用触发宿主机上的越权操作（跨特权域：容器→宿主机）
- 内存调度算法在特定条件下返回错误调度结果，将内存分配给不应该获得的节点（业务逻辑错误：不崩溃但决策错误）
- HA状态机在主节点故障时未触发切换，导致服务持续中断（业务逻辑错误：HA决策错误）

✗ 不计数示例：
- 同一 so 内部同一模块的两个函数间传递未校验数据（不跨安全边界，不满足条件，归入 S2 或 S3）
- 一次性命令行工具 ubse_cert 中的内存泄漏（不是长运行守护进程，不满足资源泄漏条件，归入 S3）
- 代码风格不一致，部分函数使用 `auto` 部分使用显式类型（不影响功能或安全，归入 S3）
- 日志级别设置不合理导致日志过多（不影响核心功能或安全，归入 S3）
- 日志中打印了调试信息导致日志文件变大（不影响核心功能决策，归入 S3）

#### S2 一般

一句话：后果有限，或严重后果但有显著缓解因子。

| 条件 | 具体测试 |
|------|----------|
| 后果有限 | 不影响核心功能正常运行（ubse 的资源调度、IPC 通信、HA 切换仍可工作），不影响安全边界，仅导致非关键功能降级、日志丢失、性能轻微下降（<50%）、单次请求处理失败但可重试成功 |
| 严重后果但有显著缓解因子 | 存在以下任一缓解机制且该机制可自动生效无需人工干预：watchdog 在 60 秒内自动重启崩溃进程、HA 模块自动切换到备用节点、架构隔离将影响限制在单一子模块内且其他模块不受影响 |
| 回归问题 | 之前正常工作的功能在新代码变更后不再正常工作，不论影响范围大小 |
| 有变通方案但不满意 | 变通方案满足以下任一：需要每次发生时手动干预（如手动重启进程）、需要禁用默认功能（如关闭 HA 才能规避）、导致 >50% 性能损失（如禁用内存池化后性能下降 60%） |

"显著缓解因子"的界定：缓解机制必须在代码中实际存在且可自动触发，不能是"理论上可以加"的机制。

"满意变通方案"的界定：三个条件全部满足才算满意——不禁用默认功能、不需要每次发生时手动干预、性能损失 ≤50%。

✓ 计数示例：
- 日志模块在磁盘满时丢失部分日志记录，但不影响 ubse 的资源调度和 IPC 通信核心功能（后果有限）
- 单个 mem_controller 模块内部逻辑错误导致该模块内存调度策略降级，node_controller 和其他模块不受影响（架构隔离缓解）
- watchdog 可在 60 秒内自动重启崩溃的 ubse 进程，服务恢复有短暂中断但自动完成（显著缓解因子）
- 之前正常工作的内存调度策略在新版本中失效，调度结果不符合预期（回归问题）
- 内存泄漏导致进程崩溃，但需每次手动重启恢复，无 watchdog 自动重启机制（变通方案不满意）

✗ 不计数示例：
- IPC 跨进程边界传递未校验数据导致 ubse 以 root 权限执行非预期操作（安全边界穿越，归入 S1）
- ubse 长运行进程内存泄漏（资源泄漏，归入 S1）
- 代码注释缺失（不影响功能或安全，归入 S3）
- 变量命名不清晰（不影响功能或安全，归入 S3）
- 内存泄漏导致崩溃，watchdog 60 秒内自动重启恢复，且此问题有满意变通方案（限制请求速率，性能损失 30%，不需手动干预，不禁用默认功能）（有满意变通方案，归入 S3）

#### S3 提示

一句话：不影响核心功能或安全，仅涉及风格、可读性、防御性编程。

| 条件 | 具体测试 |
|------|----------|
| 不影响核心功能 | 代码行为在所有正常输入和所有已定义异常输入场景下均产生正确结果，功能输出与预期一致 |
| 不影响安全 | 不涉及安全边界穿越（不跨 so、不跨进程、不跨特权域）、不引入新攻击面（不新增未鉴权的网络接口）、不泄漏敏感信息（不将密钥、密码写入日志或返回给调用方） |
| 仅涉及代码质量 | 问题属于以下类别：代码风格（命名规范、格式一致性）、可读性（类型声明是否直观、逻辑是否清晰）、注释完整性（是否解释设计意图而非仅描述行为）、防御性编程建议（对当前不可能为空的指针添加 nullptr 检查）、文档缺失（API 文档、配置说明不完整） |

✓ 计数示例：
- 使用 `auto` 关键字接收复杂模板返回值导致类型不直观，建议改为显式类型声明（可读性）
- 函数缺少注释说明设计意图，仅通过代码可推断行为但无法理解为何如此设计（注释完整性）
- 对当前逻辑下不可能为空的指针添加 nullptr 检查作为防御性措施（防御性编程建议）
- 变量命名 `tmp` 不够清晰，建议改为 `pending_request_count`（命名可读性）
- 使用 `#define MAX_CONN 100` 定义常量，建议改为 `constexpr int MAX_CONN = 100`（代码风格）
- API 文档缺少某个配置参数的说明（文档缺失）

✗ 不计数示例：
- ubse 长运行进程内存泄漏导致崩溃（影响核心功能，归入 S1）
- IPC 传递未校验数据导致越权操作（影响安全，归入 S1）
- 正常功能在新版本中失效（影响核心功能，归入 S2 回归）
- 日志丢失导致无法排查生产故障（影响运维核心功能，归入 S2）

#### 边缘情况规则

**规则1：缓解因子降级**

存在显著缓解因子可将严重性降低一级，但权限提升类问题不低于 S2。

显著缓解因子界定：缓解机制在现有代码中实际存在且可自动触发。以下情况不算显著缓解因子：
- "可以加 watchdog"（未实现的不算）
- "管理员可以手动重启"（需人工干预的不算）
- "出问题后可以回滚版本"（需人工干预的不算）

降级限制：导致权限提升的问题（本地提权、跨特权域提权）即使有缓解因子也不低于 S2。例如：IPC 跨进程传递未校验数据导致本地提权，即使有 watchdog 自动重启，仍为 S1 不降级。但如果该 IPC 问题仅导致非关键功能降级（不涉及提权），且有 watchdog 缓解，可从 S1 降为 S2。

**规则2：组合升级**

S2 问题 + 已存在的设计行为 → S0 级后果 = 升级至 S1（不升至 S0，除非该问题独立满足 S0 四个条件）。

组合升级必须满足：
- 存在实际的代码调用链证据（从 S2 问题的代码位置到触发严重后果的代码位置，每一步调用在代码中实际存在）
- S2 问题与已存在的设计行为组合后产生的后果达到 S0 级别定义（远程代码执行、内核破坏、不可恢复数据销毁）

不满足组合升级的情况：
- 理论上可能组合但代码中无实际调用链（如"S2 的日志格式问题 + 理论上日志可能被内核读取"无代码调用链证据）
- 组合后果未达到 S0 级别（如 S2 + 设计行为仅导致功能降级，不升级）

**规则3：变通方案规则**

无变通方案 → 至少 S2。
满意变通方案 → 至多 S3。

满意变通方案的定义（三个条件必须同时满足）：
1. 不禁用默认功能（不能要求关闭 HA、不能要求禁用内存池化等默认开启的功能）
2. 不需要每次发生时手动干预（不能要求"每次出问题手动重启"，自动恢复机制如 watchdog 满足此条件）
3. 性能损失 ≤50%（不能要求牺牲超过一半的性能来规避问题）

变通方案评估示例：
- "出问题时手动重启 ubse" → 不满足条件2，不满意
- "关闭 HA 功能来规避" → 不满足条件1，不满意
- "限制请求速率至原来的 40% 来规避内存泄漏" → 不满足条件3，不满意
- "配置 watchdog 自动重启，服务中断 60 秒内恢复，性能无损失" → 三个条件均满足，满意

**规则4：不确定性规则**

当不确定问题的严重性时，取已知证据支持的最低级别。绝不因不确定性而升级。

只有当新证据明确指向更高级别时才升级。以下情况不触发升级：
- "可能被远程攻击者利用"但无证据表明攻击路径可达 → 不升级至 S0
- "可能跨安全边界"但无代码证据表明数据实际跨 so 传递 → 不升级至 S1
- "可能导致资源泄漏"但无证据表明泄漏实际发生 → 不升级至 S1

升级需要明确证据：
- 有代码调用链证明攻击路径可达 → 可升级
- 有代码证明数据跨 so 边界传递且未校验 → 可升级
- 有代码证明资源分配后无释放路径 → 可升级

**规则5：回归规则**

任何回归（之前正常工作的功能在新代码变更后不再正常工作）至少 S2。

回归的判定：存在代码变更（git diff 可见），且变更前功能正常、变更后功能异常。以下不算回归：
- 功能从未正常工作过（是新功能的首个实现，bug 是初始 bug 不是回归）
- 功能正常工作的前提是未文档化的偶然行为（如依赖未定义的编译器行为）

#### 禁止事项

**禁止1：严禁凑数**

任何级别的计数可以为零。S0=0、S1=0、S3=0 都是合法结果。计数由实际发现决定，不做任何人为限制。以下行为禁止：
- 将 S2 问题标记为 S1 以"让报告看起来有严重问题"
- 编造 S3 问题以"让报告看起来内容充实"
- 在无 S0 发现时刻意寻找勉强满足 S0 条件的问题

**禁止2：严禁降级掩盖**

不得因以下原因将 S1 降级为 S2：
- "报告已经很长了，S1 太多不好看"
- "这个问题和另一个 S1 类似，合并后算一个"
- "开发团队说这个问题不严重"

S1 的判定由条件决定，不由报告长度、团队意见、或主观感受决定。

**禁止3：严禁升级充数**

不得因以下原因将 S2 升级为 S1：
- "报告需要一些严重问题才有分量"
- "这个问题如果被恶意利用可能很严重"（但无证据表明实际可被利用）
- "安全团队要求至少有 N 个 S1"

S1 的判定由条件决定，不由报告期望、推测性利用场景、或外部要求决定。

**禁止4：严禁理论组合**

组合升级必须提供代码调用链证据。以下组合方式禁止：
- "A 模块的问题 + B 模块的问题理论上可能组合"（无代码调用链）
- "这个 S2 问题 + 如果有人修改了 C 模块的代码就可能变成 S0"（依赖未发生的代码变更）
- "日志格式问题 + 日志可能被其他系统解析"（无代码证据表明其他系统实际解析此日志格式）

有效的组合升级必须包含：从 S2 问题代码位置到严重后果代码位置的完整调用链，每一步调用在当前代码中实际存在。

**禁止5：严禁无代码方案**

修复建议必须包含修改后的代码片段，不能仅包含描述性文字。以下形式禁止：
- "建议添加输入校验"（无代码，禁止）
- "建议使用智能指针管理内存"（无代码，禁止）
- "建议重构此函数"（无代码，禁止）

有效的修复建议必须包含：
- 修改后的代码片段（至少包含关键变更部分，不是完整文件重写）
- 代码片段可直接替换原代码中的问题部分（上下文足够定位）
- 如果修复涉及多文件变更，每个文件的变更都有对应代码片段

### 5. 问题类型定义

问题类型必须从以下5种中选择，禁止使用其他类型名称：

| 类型 | 含义 | 典型问题举例 |
|------|------|-------------|
| 安全 | 可被攻击利用、违反安全编码规范 | 缓冲区溢出、未校验外部输入、资源泄漏、敏感信息未清零 |
| 性能 | 影响运行效率 | 循环内不必要分配、锁粒度过大、缓存策略不合理 |
| 架构 | 设计层面缺陷 | 循环依赖、职责不清、接口设计不合理、耦合过重 |
| 可维护性 | 影响理解和长期维护 | 函数过长、命名不清、缺少必要注释、重复代码 |
| 规范 | 违反项目编码规范 | 未用override、未用constexpr替代宏、未用智能指针 |

每个问题必须归入且仅归入一个类型。如果一个问题同时涉及多个类型，选择最核心的类型。

### 6. 输出格式

审查报告必须严格遵循本节定义的格式。任何偏离均视为报告不合格，必须重新生成。

#### 报告文件规则

- 报告必须输出到项目根目录的 `code_review_report/` 目录下。如果该目录不存在，必须先创建。
- 报告文件命名格式：`codereview_<仓库名>_<日期><时间戳>.md`
  - `<仓库名>`：取项目根目录名称，例如 `ubs-engine`
  - `<日期><时间戳>`：格式 `YYYYMMDDHHmmss`，取报告生成时刻的时间
  - 示例：`codereview_ubs-engine_20260525143000.md`
- 每次审查生成一份报告，禁止拆分为多个文件。

#### 报告结构

报告必须按以下顺序组织，禁止调整章节顺序，禁止跳过任何章节：

1. 总体评价
2. S0 致命问题
3. S1 严重问题
4. S2 一般问题
5. S3 提示性问题
6. 统计摘要表
7. 亮点

**总体评价**：必须恰好包含两部分，顺序固定：

第一部分：一句话质量评估。必须是对代码质量的概括判断，禁止超过一句话。示例：

> 代码整体质量尚可，安全层面存在3个需立即修复的问题。

第二部分：紧接其后放置统计摘要表（格式见下文）。禁止将统计表放在其他位置。

**问题章节（S0/S1/S2/S3）**：每个严重等级对应一个独立章节。如果某等级下发现问题数为0，该章节标题必须仍然出现，章节内容写：

> 本级别无问题。

禁止省略空等级章节。

**统计摘要表**：出现在总体评价内和报告末尾，两处均不可省略。格式：

| 严重等级 | 数量 | 按类型分布 |
|---------|------|-----------|
| S0 致命 | ? | 安全:? 性能:? 架构:? 可维护性:? 规范:? |
| S1 严重 | ? | 安全:? 性能:? 架构:? 可维护性:? 规范:? |
| S2 一般 | ? | 安全:? 性能:? 架构:? 可维护性:? 规范:? |
| S3 提示 | ? | 安全:? 性能:? 架构:? 可维护性:? 规范:? |

表中 `?` 替换为实际数字。某类型数量为0时写0，禁止省略该类型。各类型之间用空格分隔。

**亮点**：必须指出代码中发现的优秀实践，至少列出1条。每条亮点格式：

> - **[文件:行号]** 亮点描述

#### 问题条目模板

每个问题条目必须严格使用以下模板，禁止增加字段，禁止删除字段，禁止调整字段顺序：

```
#### [S0-01] 问题简述

- **严重程度**: S0 致命
- **问题类型**: 安全
- **文件**: src/api_server/http_handler.cpp:142
- **问题简述**: 一句话描述问题

**详细描述**:
（文字说明为什么这是问题，必须解释问题的本质和潜在后果）

```cpp
// 原始代码（必须展示）
char* buf = (char*)malloc(size);
memcpy(buf, data, len);
```

**修改方案**:
（文字说明如何修复，必须给出具体的修复思路）

```cpp
// 修改后代码（必须展示，必须与原始代码配对）
char* buf = (char*)malloc(size);
if (buf == NULL) {
    return ERR_NO_MEMORY;
}
memcpy(buf, data, len);
```
```

编号规则：同一等级内按发现顺序递增编号，编号本身编码严重等级。格式为 `<等级>-<序号>`：

- S0 级：S0-01, S0-02, S0-03, ...
- S1 级：S1-01, S1-02, S1-03, ...
- S2 级：S2-01, S2-02, S2-03, ...
- S3 级：S3-01, S3-02, S3-03, ...

字段规则：

| 字段 | 值规则 | 是否可省略 |
|------|--------|-----------|
| 标题行 `#### [编号] 问题简述` | 编号按上述编号规则填写；问题简述为一句话概括 | MUST NOT |
| **严重程度** | 必须为以下之一：S0 致命、S1 严重、S2 一般、S3 提示 | MUST NOT |
| **问题类型** | 必须为以下之一：安全、性能、架构、可维护性、规范 | MUST NOT |
| **文件** | 格式 `相对路径:行号`，行号为问题所在行或起始行 | MUST NOT |
| **问题简述** | 一句话，禁止多句，禁止模糊描述 | MUST NOT |
| **详细描述** | 文字段落，解释问题本质和后果 | MUST NOT |
| 原始代码块 | 必须展示问题代码，使用cpp语法高亮 | MUST NOT |
| **修改方案** | 文字段落，说明修复思路 | MUST NOT |
| 修改后代码块 | 必须展示修复代码，使用cpp语法高亮 | MUST NOT |

原始代码块和修改后代码块必须配对出现。禁止只展示原始代码不展示修改方案，禁止只展示修改方案不展示原始代码。

#### 格式强制规则

以下规则必须严格遵守，违反任何一条即报告不合格：

1. MUST 每个问题条目包含全部8个字段，顺序固定，禁止增删。
2. MUST 原始代码块和修改后代码块同时出现，禁止单独出现。
3. MUST 代码块使用 `cpp` 语法高亮标记。
4. MUST 所有4个严重等级章节均出现在报告中，即使某等级问题数为0。
5. MUST 空等级章节内容为"本级别无问题。"，禁止留空或省略。
6. MUST 编号格式为 `<等级>-<序号>`，序号从01开始，禁止从0开始。
7. MUST 统计摘要表出现在总体评价内和报告末尾，两处均不可省略。
8. MUST 亮点章节至少包含1条亮点，禁止省略该章节。
9. MUST 问题类型仅使用上述5种定义类型，禁止自创类型。
10. MUST NOT 在问题条目中添加额外字段（如"影响范围"、"优先级"等）。
11. MUST NOT 调整报告章节顺序。
12. MUST NOT 将统计摘要表移至其他位置。
13. MUST NOT 使用模糊的问题简述（如"代码有问题"、"需要改进"）。
14. MUST NOT 省略修改后代码块中的具体修复代码，仅写文字说明不可接受。
15. MUST NOT 在总体评价中写超过一句话的质量评估。

## 审查原则

1. **建设性**：指出问题的同时提供解决方案
2. **具体**：引用具体的文件和行号，避免模糊描述
3. **优先级**：严格按S0/S1/S2/S3四级体系判定严重性，等级由客观条件决定
4. **上下文感知**：考虑代码的实际使用场景和项目约束
5. **平衡**：既指出问题，也认可好的实践

## 特殊场景

### 审查API接口变更
- 检查向后兼容性
- 验证参数校验是否完整
- 确认错误码定义是否清晰

### 审查性能相关代码
- 关注热点路径的优化
- 检查是否有性能回归风险
- 验证缓存策略的合理性

### 审查安全相关代码
- 严格对照安全编程规范
- 检查所有外部输入的处理
- 验证权限检查和访问控制

### 审查测试代码
- 测试覆盖是否充分
- 边界条件是否测试
- 测试是否独立、可重复

## 工具辅助

审查时可参考：
- `clang-format` 检查代码格式
- `clang-tidy` 检查常见问题
- 项目的 `.clang-format` 和 `.clang-tidy` 配置
- `AGENTS.md` 中的项目约定

