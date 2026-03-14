[TOC]
# UBS RMRS
## 项目简介

UBS RMRS是一款面向虚拟化场景的节点内资源管理服务插件，作为UBS Engine框架的一部分提供集群级的内存资源调度服务，主要针对虚机内存碎片场景、虚机内存超分场景、容器内存超分场景。

## 目录结构


```
UBS RMRS/
├── 3rdparty                    // 源码三方库
├── benchmark                   // cli工具
├── conf                        //配置文件
├── doc                         //文档
├── scripts                     //项目脚本
├── src                         
│   ├── include                 //头文件
│   ├── common                 //公共模块
│   ├── interface              //对外接口
│   ├── mem_fragment           //虚机碎片源码
│   ├── over_commit            // 虚机/容器超分源码
└── test
	├── UT                     // 测试三方库
        ├── conf           // UT配置文件
        └── testcase           // 测试用例
```

## 约束说明
- 虚机碎片/超分场景仅适用于2M虚机场景


## 项目架构

  - UBS RMRS是一款面向虚拟化场景的节点内资源管理服务插件，作为UBS Engine框架的一部分提供集群级的内存资源调度服务，主要针对虚机内存碎片场景、虚机内存超分场景、容器内存超分场景。

    ![img](https://resource.idp.huawei.com/idpresource/nasshare/editor/image/208584334606/1_zh-cn_image_0000002547490823.png)

    虚机碎片架构分为两层：

    UBS RMRS部署在UBS engine内，各计算节点Agent端仅负责数据采集和消息转发等，主节点侧作为功能入口，提供碎片内存的借用、迁出，内存归还和回滚接口，负责节点间的碎片内存管理。

    RMRS部署在各计算节点的OS Turbo内，基于自身的资源采集，提供碎片场景虚机的迁出/迁回策略，利用SMAP迁移能力执行虚机的迁出和迁回。

    虚机/容器超分场景：

    MemLink动态回收虚机的空余内存或补充内存。在numa内存触发高水位水线时，触发内存借用，并迁出虚机内存，缓解内存压力。在numa内存触发低水位水线时，归还借用内存。

    UBS RMRS提供内存借用、虚机迁移、内存归还接口，提供虚机迁移算法。OS Turbo提供资源采集能力，供迁出算法决策。

# 快速入门

## 前置条件

- UBS RMRS是UBS Engin的插件，安装UBS Engin后需要开启插件配置
- UBS RMRS依赖UB Turbo及其插件进行内存迁移等能力，需要配套使用
- UBS RMRS依赖libvirt采集节点虚机信息


## UBS RMRS编译

在根目录下执行:

```bash
git submodule update --init --recursive
dos2unix build.sh
sh build.sh -ub
```
编译产物：

- 在cmake-build-release/lib下会有以下库文件: `libmempooling.so`

# License说明
本项目采用木兰开源许可，详见项目根目录LICENSE文件