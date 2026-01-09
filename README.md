# UBS Engine (UB Service Core Engine)

> **软件定义计算 · 资源按需组合与分配**

UBS Engine(UB Service Core Engine, 简称UBS Engine)提供了ubse daemon程序及其相应的SDK开发库。开发者可以利用该SDK访问ubse daemon
提供的服务，从而实现对MEM（内存）等资源的调度与管理，执行关键运维操作。

本项目已在 [openEuler](https://www.openeuler.org/) 社区开源，欢迎贡献与使用！

---

## 📌 快速开始

### 环境要求
- 操作系统：推荐 **openEuler 24.03 LTS SP3或更高版本**
- 编译工具链：CMake ≥ 3.22, GCC ≥ 9.3
- 依赖库：详见 [构建指导文档](./docs/build_install/构建指导.md)

### 获取源码
```bash
git clone https://gitee.com/openeuler/ubs-engine.git
cd ubs-engine
```

### 构建项目
```bash
# 默认 Release 构建（生产环境）
./build.sh

# Debug 构建（含调试信息）
./build.sh -D

# 构建并生成 RPM 包
./build.sh package
```

构建产物位于 `cmake-build-*` 目录下，RPM 包输出至 `output/`。

---

## 🧩 核心功能

- **内存资源池化**：将物理内存抽象为可调度资源池。
- **按需分配与回收**：支持细粒度、低延迟的内存分配策略。
- **SDK 支持**：提供 C/C++ SDK，便于集成到上层应用。
- **高可用与安全机制**：内置 HA 模块、权限控制与加密支持。

> 详细功能说明请参阅 [API 文档](./docs/api/) 和 [设计架构](./docs/design/architecture.md)。

---

## 📂 项目结构概览

```text
## 📌 快速开始
UBSEngine/
├── 3rdparty                    //第三方软件
├── conf                        //配置文件
├── doc                         //文档
├── scripts                     //脚本
├── src                         
│   ├── include                 //全局头文件
│   ├── apiserver               //北向接口暴露
│   ├── cli                     //cli代码
│   │   ├──ubse_cli_framework   //命令注册和结果回显辅助builder类
|   │   └──ubse_cert            //证书相关
│   │   
│   ├── controllers             //控制器
│   │   ├── include             //头文件  
│   │   ├── mem                 //内存池化控制器
│   │   |    ├── algorithm      //mem调度算法
│   │   │    ├── memcontroller  //内存池化控制器---文件
│   │   │    └── memscheduler   //内存池化调度器---文件
│   │   └── node                //内存池化控制器---节点的采集信息
│   │        └── nodecontroller //内存池化控制器---文件
│   │  
│   ├── framwork                //软件框架
│   │   ├── com                 //通信组件，与hcom对接，线程切换，回调
│   │   ├── config              //配置模块
│   │   ├── context             //上下文模块
│   │   ├── event               //事件中心
│   │   ├── ha                  //ha模块
│   │   ├── http                //http组件
│   │   ├── ipc                 //ipc
│   │   ├── log                 //日志组件
│   │   ├── misc                //杂项：智能指针、锁、环形队列、CRC
│   │   ├── plugin_mgr          //插件管理
│   │   ├── security            //安全组件，开源只需提权，商用版本支持使用KMC加解密
│   │   ├── serde               //序列化反序列化
│   │   ├── thread_pool         //线程池操作库，fram自身系统线程池
│   │   ├── timer               //定时器
│   │   └── xml                 //xml解析处理
│   │   
│   ├── message                 //消息
│   ├── node                    //创建数据链路
│   ├── ras                     //故障处理
│   ├── res_plugins
│   │   ├── syssentry           //syssentry对接
│   │   ├── mti                 //UBM对接
│   │   └── mmi                 //内存资源接口
│   │   
│   └── sdk                     //sdk模块
│       ├── include             //sdk的对外接口声明
│       └── sample              //sdk示例
└── test
    ├── IT
    └── UT
```

> 各模块详细说明请查看对应目录下的 `README.md` 或 [docs 目录](./docs/)。

---

## 🧪 开发者测试

项目包含完整的单元测试（UT）与集成测试（IT）：

```bash
# 运行全部测试
./build.sh test

# 仅运行 UT
./build.sh ut

# 运行特定测试用例
./build.sh ut -- --gtest_filter="TestMemScheduler.*"

# 生成代码覆盖率报告（HTML 可视化）
./build.sh ut -C -H
# 启动后终端会输出类似：http://localhost:8000/coverage/index.html
```

> 测试开发指南见 [单元测试开发指南](./docs/test/单元测试开发指南.md)。

---

## 📚 文档索引

所有技术文档位于 [`docs/`](./docs/) 目录，包括：

- **架构设计**：[architecture.md](./docs/design/architecture.md)
- **构建与安装**：[构建指导.md](./docs/build_install/构建指导.md)
- **CLI 使用手册**：[cli/](./docs/cli/)
- **配置说明**：[config/](./docs/config/)
- **API 接口规范**：[api/](./docs/api/)
- **示例代码**：[example/](./docs/example/)
- **测试指南**：[test/](./docs/test/)

---

## 🤝 参与贡献

我们欢迎社区开发者提交 Issue、PR 或参与讨论！  
请先阅读 [贡献指南](./CONTRIBUTING.md)，并遵守 openEuler 社区行为准则。

---

## 📄 许可证

本项目采用 [Mulan PSL v2](https://license.coscl.org.cn/MulanPSL2) 开源许可证。

---

> **UBS Engine —— 让硬件资源像软件一样灵活调度。**  
> 项目主页：https://gitee.com/openeuler/ubs-engine
