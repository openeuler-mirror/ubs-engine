# 版本说明

## 修改记录

|文档版本|发布日期|修改说明|
|---|---|---|
|01|2026-06-30|第一次正式发布|

## 版本配套说明

### 产品版本信息

|产品名称|版本|
|--|--|
|UBS Engine|master|
|RMRS|master|
|Ucache|master|
|virt_agent|master|

### 软件版本配套说明

**UBS Engine**

|软件名称|软件版本|
|--|--|
|OS|openEuler 24.03 LTS 或更高版本|

**RMRS**

N/A

**Ucache**

N/A

**virt_agent**

N/A

### 硬件版本配套说明

**UBS Engine**

|部件名称|硬件要求|
|--|--|
|CPU架构|aarch64|
|内存|64GB及以上|
|磁盘|SSD，吞吐量不低于500MB/s|
|芯片互联|UB|
|网卡|可选依赖（可选使用TCP辅助UB建链，默认采用UB自举建链）|

**RMRS**

N/A

**Ucache**

N/A

**virt_agent**

N/A

## UBS Engine
 
### 更新说明

* 支持1D FM电互联超节点容器化部署，最多支持192容器。
* 借用算法支持配置带宽优先模式。
* 支持管控面独立部署和身份校验消除安全风险。
* 链路故障时，UBSE需要设置SEI降级以及受影响端口，避免Atomic指令访问导致借入节点复位。
* UBSE支持基于链路状态设置/清除远端NUMA故障标志位，确保业务正常访问。

### 已解决问题

无

### 遗留问题

无

## RMRS
 
### 更新说明

无

### 已解决问题

无

### 遗留问题

无

## Ucache
 
### 更新说明

无

### 已解决问题

无

### 遗留问题

无

## virt_agent
 
### 更新说明

无

### 已解决问题

无

### 遗留问题

无

## 版本配套文档

**UBS Engine**

|文档名称|内容简介|
|----|---|
|《[UBS Engine API 参考指南](ubse_api_reference.md)》|本文档主要介绍UBS Engine SDK对外提供的API。|
|《[UBS Engine CLI 使用指南](./ubse_cli_user_guide.md)》|本文档介绍了UBS Engine CLI总体设计、命令行操作、证书管理、内存池化及节点健康检查等核心功能。|
|《[UBS Engine配置说明](./ubse_configuration_instructions.md)》|本文档介绍了UBS Engine系统配置说明，包含日志、HA、RPC、UBFM、内存池化等模块的配置原则与示例。|
|《[UBS Engine安装指南](./ubse_installation.md)》|本文档介绍了UBS Engine安装指南，包含环境要求、节点规划、安装步骤、配置修改及部署验证。|
|《[UBS Engine 安全管理与加固](./ubse_security_description.md)》|本文档提供了UBS Engine系统安全设计与加固指南，包含安全架构、最小特权、权限控制及通信安全等内容。|

**RMRS**

|文档名称|内容简介|
|----|---|
|《[RMRS API参考](../zh/addons/rmrs/rmrs_api_reference.md)》|本文档主要介绍RMRS对外提供的API。|
|《[RMRS 用户指南](./addons/rmrs/rmrs_user_guide.md)》|本文档介绍了UBS Engine内的RMRS插件，用于集群内存资源调度。|

**Ucache**

|文档名称|内容简介|
|----|---|
|《[Ucache 用户指南](../zh/addons/ucache/ucache_user_guide.md)》|本文档描述了Ucache用户指南，介绍基于灵衢总线构建全局PageCache池，优化I/O敏感应用读取性能。|

**virt_agent**

|文档名称|内容简介|
|----|---|
|《[virt_agent API 参考](./addons/virt_agent/virt_agent_api_reference.md)》|本文档主要介绍virt_agent对外提供的API。|
|《[virt_agent 配置项说明](../zh/addons/virt_agent/virt_agent_configuration_instructions.md)》|本文档介绍了UBS Virtagent配置说明，包含配置原则、ini文件格式、节点独立配置及重启要求。|
|《[virt_agent 安全设计](../zh/addons/virt_agent/virt_agent_security_description.md)》|本文档描述了UBS Virtagent安全设计，包含安全架构、权限最小化、暴露面安全及安全编译等内容。|
