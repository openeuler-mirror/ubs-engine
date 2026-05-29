# UBS-VirtAgent

软件定义计算，资源按需组合与分配

# ubs_virt_agent

## 1.项目介绍 Introduction

UBSVirtAgent(UB Service Core Virt Agent)是灵衢（UnifiedBus, UBS）软件栈中面向虚拟化场景的核心组件之一。
提供了虚拟化场景下虚机和容器场景的内存逃生能力。

## 2.目录结构 Contents Structure

```shell
UBSVirtAgent/
├── src
│   ├── addons
│   │   ├──virt_agent                    // UBSVirtAgent代码目录
│   │   │   ├── alarm_handler            // 告警处理
│   │   │   ├── cloud_adapter            // 云管对接
│   │   │   ├── common                   // 公共模块
│   │   │   │   ├── libvirt              // libvirt相关
│   │   │   │   ├── mempooling           // mempooling相关
│   │   │   │   ├── message              // 消息相关
│   │   │   │   │  ├── include           // 内部消息类
│   │   │   │   │  ├── sdk               // sdk相关的消息类
│   │   │   │   ├── util                 // 工具相关
│   │   │   ├── conf                     // 配置
│   │   │   ├── dao                      // 数据库相关
│   │   │   ├── docs                     // 文档资料
│   │   │   ├── default_strategy         // 默认的超分决策策略
│   │   │   ├── export                   // 信息采集
│   │   │   │   ├── libvirt_helper       // libvirt采集
│   │   │   │   ├── os_helper            // os采集
│   │   │   │   ├── resource_export      // 其他采集
│   │   │   ├── include                  // 头文件
│   │   │   ├── mem_fragmentation        // 内存碎片相关
│   │   │   ├── mem_handler              // 内存处理
│   │   │   ├── migrate                  // 虚机迁移
│   │   │   │   ├── util                 // 迁移相关工具
│   │   │   ├── over_commit              // 容器超分
│   │   │   │   ├── container            // 容器对接
│   │   │   │   ├── mem_manager          // 内存管理
│   │   │   ├── router                   // 路由注册
│   │   │   ├── sdk                      // sdk接口
│   │   │   │   ├── case_conf            // 场景设置
│   │   │   │   ├── container            // 容器相关
│   │   │   │   ├── go                   // go接口
│   │   │   │   │  ├── collector         // 采集相关接口
│   │   │   │   │  ├── dlopen            // 加载相关
│   │   │   │   │  ├── escape            // 内存逃生相关接口
│   │   │   │   ├── include              // SDJ头文件
│   │   │   │   ├── mem_fragmentation    // 内存碎片相关
│   │   │   │   ├── python               // python接口
│   │   │   │   │  ├── ffi               // 实现
│   │   │   │   │  ├── models            // 模型
│   │   │   │   │  ├── sample            // 测试脚本
│   │   │   │   ├── vm_migrate           // 虚机迁移相关
│   │   │   ├── status_manager           // 状态管理
│   │   │   ├── strategy                 // 策略对接
│   ├── ...                              // UBSE相关
├── ...                                  // UBSE相关
└── test
    ├── IT
    ├── PT
    └── UT
```

## 3.架构 Architecture

UBSVirtAgent 架构详细介绍请查看：[Architecture](../design/architecture.md)。

## 4.功能 Function

提供了虚拟化场景下虚机和容器场景的内存逃生能力。

## 5.使用指南 Getting Started

UBSVirtAgent作为UBSE中的插件，使用方法均与UBSE一致，不再赘述，详见UBSE的[README.md](../../../../../README.md)文件。
