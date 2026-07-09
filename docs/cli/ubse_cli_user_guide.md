# UBSE CLI 使用指南

## ubsectl总体设计原则

UBS提供统一的命令行工具ubsectl，命令行风格整体遵从linux常规的命令行规范（UNIX风格和GNU风格）。

具体命令字格式：

```shell
ubsectl  <命令字>  <操作对象>  [{<参数名1>  [参数值1]}… …{<参数名1>  [参数值1]}]
```

**执行用户**

安装UBSE Engine后自动创建的ubse用户。

root用户可通过如下方式使用ubse用户发起请求。

```shell
sudo -u ubse ubsectl [-h | --help] COMMAND TYPE [-h | --help][OPTIONS] 
```

**命令字**

定义系统能够执行的功能，当前可选命令字如下：

| 关键字     | 关键字说明           |
| ------- | --------------- |
| import  | 向UBSE导入信息       |
| remove  | 从UBSE中移除先前导入的信息 |
| create  | 在UBSE中创建资源      |
| delete  | 从UBSE中删除资源      |
| change  | 修改UBSE中的信息或资源   |
| display | 查看UBSE中的信息或资源   |
| attach  | 在UBSE中绑定资源      |
| detach  | 从UBSE中解绑资源      |

**操作对象**

用于指定命令字的操作对象，由UBSE中的能力决定，比如：memory、topo、cert等

**参数列表**

命令字执行时需要的参数，包括一对或多对参数名和参数值

参数名：可遵从长选项风格（两个连接符“--”后面紧跟完整参数名)，也可遵从短选项风格（单个连接符“-”后面紧跟缩写参数名）

参数值：与参数名用空格分开，表达具体的参数取值；参数名不可以没有参数值；单个参数值最大长度1024字符

**命令行举例**

```bash
# 查询内存借用账本详细信息
ubsectl display memory -t borrow_detail
# 触发内存借用
ubsectl create memory -t numa -s <size> -n <name>
```

**特性不支持错误信息**

当 UB 特性配置关闭对应能力时，相关命令返回固定错误信息。

| 命令范围        | 触发条件                            | 错误信息                                      |
| ----------- | ------------------------------- | ----------------------------------------- |
| memory 相关命令 | 内存借用和内存共享特性均不支持，或命令对应的内存类型特性不支持 | `ERROR: Memory feature is not supported.` |
| URMA 相关命令   | URMA 特性不支持                      | `ERROR: URMA feature is not supported.`   |

## 使能命令补齐

ubse内置ubsectl补全脚本，部署完成后脚本位置为：/etc/bash\_completion.d/cli\_commands.sh

**安装Bash Completion脚本库**

参见[安装Bash Completion脚本库](./ubse_installation.md#可选安装bash-completion脚本库)

**临时加载补全脚本**

该方法适用于希望在当前命令窗口立即使用补全脚本的用户。

```shell
    source /etc/bash_completion.d/cli_commands.sh
```

**永久加载补全脚本**

该方法适用于希望在所有命令窗口使用补全脚本的用户。

在\~/.bashrc文件中添加`source /etc/bash_completion.d/cli_commands.sh`以永久启用脚本补全。

## 证书管理

### 导入证书

**描述**

向UBSE导入证书，用于节点间通信的认证与加密

**用法**

```shell
ubsectl import cert -c <ca-cert-file> -s <server-cert-file> -k <server-key-file> -l <ca-crl-file>
```

**参数**

| 参数                    | 说明                    | 取值  |
| --------------------- | --------------------- | --- |
| -c/--ca-cert-file     | 必选参数，根证书文件完整路径及文件名    | 字符串 |
| -s/--server-cert-file | 必选参数，身份证书文件完整路径及文件名   | 字符串 |
| -k/--server-key-file  | 必选参数，身份证书私钥文件完整路径及文件名 | 字符串 |
| -l/--ca-crl-file      | 可选参数，吊销证书列表文件完整路径及文件名 | 字符串 |

**约束限制**

1. 只允许root和ubse用户使用，原始证书路径必须是ubsectl可访问
2. 导入可信机构颁发的证书时，需交互式输入私钥保护密码，密码不显示，输入完按Enter键确认即可，只支持回退键操作密码
3. 密码长度不能超过1000字符，不支持"\n"和"\b"等不可见字符
4. 文件路径只能是绝对路径（路径只允许大小写字母、数字、"-"、"\_"、"."和"/"），不允许软硬链接
5. 文件大小不超过2MB

**输出信息说明**

执行成功/失败

**示例**

```bash
$ ubsectl import cert -c /usr/cert/trust.pem -s /usr/cert/server.pem -k /usr/cert/server_key.pem -l /usr/cert/crl.pem
Enter certificate password: 
Certificates imported successfully
```

### 移除证书

**描述**

从UBSE中移除证书信息

**用法**

```shell
ubsectl remove cert
```

**参数**

无

**约束限制**

只允许root和ubse用户使用，原始证书路径必须是ubsectl可访问

**输出信息说明**

执行成功/失败

**示例**

```bash
$ ubsectl remove cert
Certificates removed successfully
```

### 更新证书吊销列表

**描述**

向UBSE更新吊销证书信息

**用法**

```shell
ubsectl change cert -l <ca-crl-file>
```

**参数**

| 参数               | 说明                 | 取值  |
| ---------------- | ------------------ | --- |
| -l/--ca-crl-file | 必选参数，根证书文件完整路径及文件名 | 字符串 |

**约束限制**

1. 只允许root和ubse用户使用，原始证书路径必须是ubsectl可访问
2. 文件路径只能是绝对路径（路径只允许大小写字母、数字、"-"、"\_"、"."和"/"），不允许软硬链接
3. 文件大小不超过2MB

**输出信息说明**

执行成功/失败

**示例**

```bash
$ ubsectl change cert -l /usr/cert/crl.pem
Certificate Revocation List changed successfully
```

## 内存池化

### 检查各节点内存池化功能健康状态

**描述**

检查各节点内存池化功能健康状态

**用法**

```shell
ubsectl check memory
```

**输入参数**

无

**约束限制**

ubsectl只能在root，ubse用户中运行

**输出信息说明**

| 字段名    | 字段描述                                                                                                                                                                                                                                                                                       | 取值范围                                                                                                 |
| ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------- |
| node   | 节点信息。例：computer1(1)节点信息由2部分组成：1.括号前部分：主机名2.括号内部分：节点的槽位号                                                                                                                                                                                                                                    | 字符串                                                                                                  |
| status | 节点的内存借用功能健康状态，当各组件健康状态均为ok时，取值ok                                                                                                                                                                                                                                                           | 可选值：\[ ok \| nok ]                                                                                   |
| detail | 内存借用功能所依赖的各个组件的健康状态，包括节点集群状态、obmm 内核模块插入状态以及 sysSentry 服务状态。各状态的判定规则为： 1. 节点集群状态：ok，节点处于 Smoothing 或 Working 状态。  nok：节点处于其他非正常状态 2. obmm 内核模块状态：ok，模块已正确插入；nok：模块未插入；unknown：节点未上报状态，或集群状态为 Unknown（无法获取信息） 3. sysSentry 服务状态：ok，服务运行正常；nok：服务运行异常；unknown：节点未上报状态，或集群状态为 Unknown（无法获取信息） | cluster state: \[ ok \| nok ]; obmm: \[ ok \| nok \| unknown ]; sysSentry: \[ ok \| nok \| unknown ] |

**示例**

#### 场景1：节点重启中，备节点无法连上master节点

- master节点
  ```bash
  $ ubsectl check memory
  -----------------------------------------------------------------------------------------------------
  node                  status                  detail
  -----------------------------------------------------------------------------------------------------
  computer01(1)         ok                      cluster state: ok; obmm: ok; sysSentry: ok
  -(2)                  nok                     cluster state: nok; obmm: unknown; sysSentry: unknown
  -----------------------------------------------------------------------------------------------------
  ```
- 备节点
  ```bash
  $ ubsectl check memory
  ERROR: Failed to obtain memory information
  ```

#### 场景2：节点启动稳定后

- master节点
  ```bash
  $ ubsectl check memory
  -----------------------------------------------------------------------------------------------------
  node                  status                  detail
  -----------------------------------------------------------------------------------------------------
  computer01(1)         ok                      cluster state: ok; obmm: ok; sysSentry: ok
  computer02(2)         ok                      cluster state: ok; obmm: ok; sysSentry: ok
  -----------------------------------------------------------------------------------------------------

  ```
- 备节点
  ```bash
  $ ubsectl check memory
  -----------------------------------------------------------------------------------------------------
  node                  status                  detail
  -----------------------------------------------------------------------------------------------------
  computer01(1)         ok                      cluster state: ok; obmm: ok; sysSentry: ok
  computer02(2)         ok                      cluster state: ok; obmm: ok; sysSentry: ok
  -----------------------------------------------------------------------------------------------------

  ```

#### 场景3：只有主节点启动，备节点不启动

- master节点
  ```bash
  $ ubsectl check memory
  -----------------------------------------------------------------------------------------------------
  node                  status                  detail
  -----------------------------------------------------------------------------------------------------
  computer01(1)         ok                      cluster state: ok; obmm: ok; sysSentry: ok
  -(2)                  nok                     cluster state: nok; obmm: unknown; sysSentry: unknown
  -----------------------------------------------------------------------------------------------------

  ```
- 备节点
  ```bash
  $ ubsectl check memory
  ERROR: Internal error with error code 5

  ```

#### 场景4：稳定后备节点停掉

- master节点
  ```bash
  $ ubsectl check memory
  -----------------------------------------------------------------------------------------------------
  node                  status                  detail
  -----------------------------------------------------------------------------------------------------
  computer01(1)         ok                      cluster state: ok; obmm: ok; sysSentry: ok
  -(2)                  nok                     cluster state: nok; obmm: unknown; sysSentry: unknown
  -----------------------------------------------------------------------------------------------------

  ```
- 备节点
  ```bash
  $ ubsectl check memory
  ERROR: Internal error with error code 5

  ```

#### 场景5：稳定后主节点停掉

- master节点
  ```bash
  $ ubsectl check memory
  -----------------------------------------------------------------------------------------------------
  node                  status                  detail
  -----------------------------------------------------------------------------------------------------
  -(1)                  nok                     cluster state: nok; obmm: unknown; sysSentry: unknown
  computer02(2)         ok                      cluster state: ok; obmm: ok; sysSentry: ok
  -----------------------------------------------------------------------------------------------------

  ```
- 备节点
  ```bash
  $ ubsectl check memory
  ERROR: Internal error with error code 5

  ```

#### 场景6：主节点集群状态、sysSentry服务状态正常，obmm内核模块未插入

- master节点
  ```bash
  $ ubsectl check memory
  -----------------------------------------------------------------------------------------------------
  node                  status                  detail
  -----------------------------------------------------------------------------------------------------
  computer01(1)         nok                     cluster state: ok; obmm: nok; sysSentry: ok
  -(2)                  nok                     cluster state: nok; obmm: unknown; sysSentry: unknown
  -----------------------------------------------------------------------------------------------------

  ```

#### 场景7：主节点集群状态、obmm内核模块正确插入，sysSentry服务状态异常

- master节点
  ```bash
  $ ubsectl check memory
  -----------------------------------------------------------------------------------------------------
  node                  status                  detail
  -----------------------------------------------------------------------------------------------------
  computer01(1)         nok                     cluster state: ok; obmm: ok; sysSentry: nok
  -(2)                  nok                     cluster state: nok; obmm: unknown; sysSentry: unknown
  -----------------------------------------------------------------------------------------------------

  ```

#### 场景8：备节点处于故障状态，obmm内核模块正确插入、sysSentry服务状态正常

- 备节点
  ```bash
  $ ubsectl check memory
  -----------------------------------------------------------------------------------------------------
  node                  status                  detail
  -----------------------------------------------------------------------------------------------------
  computer01(1)         ok                      cluster state: ok; obmm: ok; sysSentry: ok
  computer02(2)         nok                     cluster state: nok; obmm: ok; sysSentry: ok
  -----------------------------------------------------------------------------------------------------

  ```

### 查询各节点内存借入信息

**描述**

查询当前集群中，各节点内存借入信息

**用法**

```shell
ubsectl display memory -t node_borrow
```

**输入参数**

无

**约束限制**

只显示集群中在位的节点，如果脱离集群或未启动，则不显示

ubsectl只能在root，ubse用户中运行，可管理所有内存资源

**输出信息说明**

显示信息中的字段说明：

| 字段名          | 字段描述                                                      | 字段取值 |
| ------------ | --------------------------------------------------------- | ---- |
| borrow\_node | 借入节点信息。例：computer1(1)节点信息由2部分组成：1.括号前部分：主机名2.括号内部分：节点的槽位号 | 字符串  |
| lend\_node   | 借出节点信息。例：computer1(1)节点信息由2部分组成：1.括号前部分：主机名2.括号内部分：节点的槽位号 | 字符串  |
| size         | 借用内存总量，单位: MB                                             | 整数   |

**示例**

```bash
$ ubsectl display memory -t node_borrow
--------------------------------------------
borrow_node   lend_node        size   
--------------------------------------------
node-1(1)     node-2(2)        450   
node-1(1)     node-3(3)        450   
node-4(4)     node-3(3)        100   
--------------------------------------------
```

### 查询各节点内存借出信息

**描述**

查询当前集群中，各节点内存借出信息

**用法**

```shell
ubsectl display memory -t node_lend
```

**输入参数**

无

**约束限制**

只显示集群中在位的节点，如果脱离集群或未启动，则不显示

ubsectl只能在root，ubse用户中运行，可管理所有内存资源

**输出信息说明**

显示信息中的字段说明：

| 字段名          | 字段描述                                                      | 字段取值 |
| ------------ | --------------------------------------------------------- | ---- |
| lend\_node   | 借出节点信息。例：computer1(1)节点信息由2部分组成：1.括号前部分：主机名2.括号内部分：节点的槽位号 | 字符串  |
| borrow\_node | 借入节点信息。例：computer1(1)节点信息由2部分组成：1.括号前部分：主机名2.括号内部分：节点的槽位号 | 字符串  |
| size         | 借用内存总量，单位: MB                                             | 整数   |

**示例**

```bash
$ ubsectl display memory -t node_lend
--------------------------------------------
lend_node     borrow_node      size   
--------------------------------------------
node-1(1)     node-2(2)        450   
node-1(1)     node-3(3)        450   
node-4(4)     node-3(3)        100   
--------------------------------------------
```

### 查询内存借用账本详细信息

**描述**

查询当前集群中，所有内存借用账本详细信息, 并支持根据内存名和内存类型过滤查询账本

**用法**

```shell
ubsectl display memory -t borrow_detail [-bt <type>] [-n <name>]
```

**输入参数**

| 字段名              | 字段描述                         | 字段取值                                        |
| ---------------- | ---------------------------- | ------------------------------------------- |
| -bt--borrow-type | 可选，根据借用类型过滤                  | 可选值：\[ numa \| fd \| share ]                |
| -n--name         | 可选，根据内存名过滤各个节点、各个类型的同名内存都会显示 | 字符串，长度为1\~47，仅可包括大小写字母、数字、"."、":"、"-"以及"\_" |

**约束限制**

只显示集群中在位的节点，如果脱离集群或未启动，则不显示

ubsectl只能在root，ubse用户中运行，可管理所有内存资源

`fd`和`numa`类型的name在本节点唯一，`share`类型的name在集群内唯一

**输出信息说明**

显示信息中的字段说明：

| 字段名          | 字段描述                                                         | 字段取值                                                                                                              |
| ------------ | ------------------------------------------------------------ | ----------------------------------------------------------------------------------------------------------------- |
| name         | 一次内存借用的名称                                                    | 字符串                                                                                                               |
| type         | 内存借用的类型                                                      | 可选值：\[ numa \| fd \| share ]                                                                                      |
| borrow\_node | 借入节点信息。例：computer1(1)节点信息由2部分组成：1.括号前部分：主机名2.括号内部分：节点的槽位号    | 字符串                                                                                                               |
| lend\_node   | 借出节点信息。例：computer1(1)节点信息由2部分组成：1.括号前部分：主机名2.括号内部分：节点的槽位号    | 字符串                                                                                                               |
| lend\_numa   | 借出numa信息。例：1(216)numa信息由2部分组成：1.括号前部分：numaId2.括号内部分：socketId | 字符串                                                                                                               |
| lend\_size   | numa节点的借用内存总量，单位: MB                                         | 整数                                                                                                                |
| status       | 借用状态                                                         | 可选值：done：借用完成exporting：借出节点正在执行导出importing：代入节点正在执行导入unexporting：借出节点正在取消导出unimporting：代入节点正在取消导入fault：当次借用请求出现故障 |
| handle       | 资源操作句柄句柄含义：numa类型：远端numaIdfd、share类型：内存块标识信息集合               | numa-id: 整数mem-ids: 整数数组，用,分隔                                                                                     |

**示例**

查询全量账本

```bash
$ ubsectl display memory -t borrow_detail
------------------------------------------------------------------------------------------------
name         type    borrow_node   lend_node   lend_numa   lend_size       status       handle    
------------------------------------------------------------------------------------------------
memory1      numa    node-1(1)     node-3(3)   1(216)      100             done         5
memory2      share   node-1(1)     node-3(3)   1(216)      200             done         1,2
memory2      share   node-2(2)     node-3(3)   1(216)      1200            done         3,4
memory3      share                 node-3(3)   1(216)      1200            done         -
memory4      numa    node-2(2)     node-3(3)   1(216)      1300            exporting    -
memory5      fd      node-1(1)     node-3(3)   1(216)      1400            importing    -
memory6      numa                  node-3(3)   1(216)      1300            unexporting  -
memory7      fd      node-1(1)     node-3(3)   1(216)      1400            unimporting  -
memory8      numa    node-2(2)                             1500            fault        -
memory9      fd                    node-3(3)   1(216)      1500            fault        -
------------------------------------------------------------------------------------------------
```

根据类型过滤

```bash
$ ubsectl display memory -t borrow_detail -bt numa
--------------------------------------------------------------------------------------
  name    type   borrow_node     lend_node   lend_numa   lend_size   status   handle   
--------------------------------------------------------------------------------------
  test1   numa   controller(1)   node01(2)   0(36)       128         done     2        
                                                                                      
  test2   numa   controller(1)   node01(2)   0(36)       128         done     2        
                                                                                      
  test    numa   controller(1)   node01(2)   0(36)       2048        done     2        
--------------------------------------------------------------------------------------
```

根据内存名过滤

```bash
$ ubsectl display memory -t borrow_detail -n test
--------------------------------------------------------------------------------------
  name    type   borrow_node     lend_node   lend_numa   lend_size   status   handle   
--------------------------------------------------------------------------------------
  test    fd     controller(1)   node01(2)   0(36)       128         done     1      
                                                                                      
  test    numa   controller(1)   node01(2)   0(36)       128         done     2   
                                                                                       
  test    share  controller(1)   node01(2)   0(36)       512         done     4,5,6,7
  
  test    numa   node02(3)       node01(2)   0(36)       128         done     4  
--------------------------------------------------------------------------------------
```

根据类型和内存名过滤

```bash
$ ubsectl display memory -t borrow_detail -bt numa -n test
-------------------------------------------------------------------------------------
  name   type   borrow_node     lend_node   lend_numa   lend_size   status   handle   
-------------------------------------------------------------------------------------
  test   numa   controller(1)   node01(2)   0(36)       2048        done     2        
-------------------------------------------------------------------------------------
```

### 查询numa状态信息

**描述**

查询当前集群中，所有numa设备的信息

**用法**

```shell
ubsectl display memory -t numa_status [-all]
```

**输入参数**

| 字段名     | 字段描述        | 字段取值 |
| ------- | ----------- | ---- |
| --all-a | 可选，查询大页内存信息 | 无    |

**约束限制**

ubsectl只能在root，ubse用户中运行，可管理所有内存资源

**输出信息说明**

- 显示信息中的字段说明：

| 字段名           | 字段描述                                                    | 字段取值 |
| ------------- | ------------------------------------------------------- | ---- |
| node          | 节点信息。例：computer1(1)节点信息由2部分组成：1.括号前部分：主机名2.括号内部分：节点的槽位号 | 字符串  |
| numa          | numa id                                                 | 整数   |
| total         | NUMA的内存总量，单位: MB                                        | 整数   |
| used          | NUMA的内存使用量，单位: MB                                       | 整数   |
| free          | NUMA的内存空闲量，单位: MB                                       | 整数   |
| used\_percent | 内存使用比例, 保留一位小数                                          | 浮点数  |
| 2M\_total     | NUMA的2M大页总数量(4k页环境)                                     | 整数   |
| 2M\_free      | NUMA的2M大页的空闲数量(4k页环境)                                   | 整数   |
| 1G\_total     | NUMA的1G大页总数量(4k页环境)                                     | 整数   |
| 1G\_free      | NUMA的1G大页的空闲数量(4k页环境)                                   | 整数   |
| 512M\_total   | NUMA的512M大页总数量(64k页环境)                                  | 整数   |
| 512M\_free    | NUMA的512M大页的空闲数量(64k页环境)                                | 整数   |

**示例**

```bash
$ ubsectl display memory -t numa_status
-------------------------------------------------------
node         numa    total   used   free   used_percent
node-1(1)    0       1024    128    896    12.5       
node-1(1)    1       8192    256    7936   3.1
node-2(2)    0       1024    128    896    12.5   
```

查询当前集群中，所有numa设备的信息，包括大页内存信息(4k页环境)

```shell
ubsectl display memory -t numa_status --all
-------------------------------------------------------------------------------------------------
node         numa    total   used   free   used_percent   2M_total   2M_free   1G_total   1G_free
node-1(1)    0       1024    128    896    12.5           10         10        0          0
node-1(1)    1       8192    256    7936   3.1            10         10        0          0
node-2(2)    0       1024    128    896    12.5           0          0         5          5
```

查询当前集群中，所有numa设备的信息，包括大页内存信息(64k页环境)

```shell
ubsectl display memory -t numa_status --all
--------------------------------------------------------------------------------
node         numa    total   used   free   used_percent   512M_total   512M_free
node-1(1)    0       1024    128    896    12.5           512        256
node-1(1)    1       8192    256    7936   3.1            1024        1024
node-2(2)    0       1024    128    896    12.5           512         0
```

### 查询全量节点是否可借用内存信息

**描述**

查询全量节点是否可借用内存信息

**用法**

```shell
ubsectl display memory -t config
```

**输入参数**

无

**约束限制**

ubsectl只能在root，ubse用户中运行，可管理所有内存资源

**输出信息说明**

- 显示信息中的字段说明：

| 字段名      | 字段描述                                                    | 字段取值                   |
| -------- | ------------------------------------------------------- | ---------------------- |
| node     | 节点信息。例：computer1(1)节点信息由2部分组成：1.括号前部分：主机名2.括号内部分：节点的槽位号 | 字符串                    |
| isLender | 此节点是否可以作为内存借出方                                          | 可选值：\[ true \| false ] |

**示例**

```bash
$ ubsectl display memory -t config
-------------------------------------------------------
node         isLender    
-------------------------------------------------------
node-1(1)    true       
node-2(2)    false
-------------------------------------------------------       
```

### 触发内存借用

**描述**

在本节点借用指定类型的内存

**用法**

```shell
# NUMA 类型
ubsectl create memory -t numa -s <size> -n <name> [-l <linkInfo>]
# FD 类型
ubsectl create memory -t fd -s <size> -n <name>
# 共享内存类型
ubsectl create memory -t share -s <size> -n <name> [-r <regionList>]
```

**输入参数说明**

- 输入参数说明：

| 字段名         | 字段描述                         | 字段取值                                       |
| ----------- | ---------------------------- | ------------------------------------------ |
| -t--type    | 借用类型                         | 可选值：\[ numa \| fd \| share ]               |
| -n--name    | 本次借用的唯一标识                    | 字符串长度为1\~47，仅可包括大小写字母、数字、"."、":"、"-"以及"\_" |
| -s--size    | 借用内存大小单位: MB/GB，不能小于4MB      | 整数 + M/G                                   |
| -l--link-id | 可选参数, 链路id，仅numa类型支持，其他类型不生效 | 字符串                                        |
| -r--region  | 可选参数，共享域节点列表，仅共享借用支持，其他类型不生效 | 用,分隔的**节点槽位号**                             |

**约束限制**

ubsectl只能在root，ubse用户中运行，可管理所有内存资源

`fd`和`numa`类型的name在本节点唯一，`share`类型的name在集群内唯一

`-r`仅`share`类型有效，`-l`仅`numa`类型有效

**输出信息说明**

- **numa类型：**
  | 字段名         | 字段描述          | 字段取值 |
  | ----------- | ------------- | ---- |
  | name        | 本次借用的唯一标识     | 字符串  |
  | size        | 借用内存大小，单位: MB | 整数   |
  | numa-id     | 远端numaId      | 整数   |
  | import-node | 借入节点id        | 整数   |
  | export-node | 借出节点id        | 整数   |
- **fd类型：**
  | 字段名         | 字段描述          | 字段取值      |
  | ----------- | ------------- | --------- |
  | name        | 本次借用的唯一标识     | 字符串       |
  | size        | 借用内存大小，单位: MB | 整数        |
  | mem-ids     | 内存标识信息        | 整数数组，用,分隔 |
  | import-node | 借入节点id        | 整数        |
  | export-node | 借出节点id        | 整数        |
- **share类型：**
  | 字段名         | 字段描述          | 字段取值      |
  | ----------- | ------------- | --------- |
  | name        | 本次借用的唯一标识     | 字符串       |
  | size        | 借用内存大小，单位: MB | 整数        |
  | export-node | 借出节点id        | 整数        |
  | region      | 共享域           | 整数数组，用,分隔 |

**示例**

- **numa 类型**
  ```bash
  $ ubsectl create memory -t numa -l 1/36/0-2/36/0 -s 128M -n testName
  name:testName
  size:128M
  numa-id:5
  import-node:1
  export-node:2
  ```
- **fd 类型**
  ```bash
  $ ubsectl create memory -t fd -s 128M -n testName
  name:testName
  size:128M
  mem-ids:1,2,3
  import-node:1
  export-node:2
  ```
- **share 类型**
  ```bash
  $ ubsectl create memory -t share -s 128M -n testName -r 1,2
  name:testName
  size:128M
  export-node:2
  region:1,2
  ```

### 删除内存借用

**描述**

删除命令行调用节点的内存借用记录，释放借用的内存。

**用法**

```shell
ubsectl delete memory [-t <borrow-type>] -n <name>
```

**输入参数说明**

- 输入参数说明：
  | 字段名      | 字段描述                                           | 字段取值                                  |
  | -------- | ---------------------------------------------- | ------------------------------------- |
  | -t--type | 借用类型默认值: "numa"                                | 可选值: \[ numa \| fd \| share \| addr ] |
  | -n--name | 借用的唯一标识长度为1\~47，仅可包括大小写字母、数字、"."、":"、"-"以及"\_" | 字符串                                   |

**输出信息说明**

Delete successfully

**示例**

```bash
$ ubsectl delete memory -t share -n testName
Delete successfully
```

### 查询进程内存配置信息

**描述**

查询当前进程中，所有已配置的进程内存阈值信息

**用法**

```shell
ubsectl display process-mem -t <type>
```

**输入参数说明**

| 参数名      | 说明               | 取值     |
| -------- | ---------------- | ------ |
| -t--type | 必选，查询的进程内存配置信息类型 | config |

**约束限制**

1. ubsectl只能在root，ubse用户中运行
2. 需要目标节点的 process\_mem 插件正常启动，否则无法获取已纳管进程信息

**输出信息说明**

| 字段名                  | 字段描述           | 字段取值 |
| -------------------- | -------------- | ---- |
| pid                  | 进程ID           | 整数   |
| evictThreshold       | 迁出阈值(%)        | 整数   |
| targetEvictThreshold | 预期迁出比例(%)      | 整数   |
| reclaimThreshold     | 迁回阈值(%)        | 整数   |
| totalMemoryUsage     | 进程期望总内存大小      | 整数   |
| srcNuma              | 本地NUMA节点ID（可选） | 整数   |

**示例**

```bash
$ ubsectl display process-mem -t config
---------------------------------------------------------------------------------------------------
pid         evictThreshold   targetEvictThreshold   reclaimThreshold   totalMemoryUsage   srcNuma
---------------------------------------------------------------------------------------------------
1234        80               50                     30                 1073741824         1
5678        80               50                     30                 1073741824         N/A
---------------------------------------------------------------------------------------------------
```

### 修改进程内存配置

**描述**

修改指定进程的内存配置信息，包括迁出阈值、预期迁出比例、迁回阈值以及进程的期望内存大小。当进程存在子进程时，子进程会继承父进程的内存使用配置。

**用法**

```shell
ubsectl change process-mem -p <pid> -e <evict-thresh> -t <target-evict-thresh> -r <reclaim-thresh> -s <size> [--src-numa <numa-id>]
```

**输入参数说明**

| 参数名                     | 说明                                     | 取值                                                     |
| ----------------------- | -------------------------------------- | ------------------------------------------------------ |
| -p--pid                 | 必选，目标进程的PID                            | 整数，范围 1-4194304                                        |
| -e--evict-thresh        | 必选，迁出阈值(%)。当进程总内存使用超过此比例时，触发迁出动作       | 整数，范围 1-100                                            |
| -t--target-evict-thresh | 必选，预期迁出比例(%)。迁出后远端内存占总内存的目标比例          | 整数，范围 1-100                                            |
| -r--reclaim-thresh      | 必选，迁回阈值(%)。当进程总内存使用低于此比例时，将远端内存全部迁回并释放 | 整数，范围 1-100，需比--evict-thresh至少小5                       |
| -s--size                | 必选，进程期望总内存大小                           | 格式为数字+单位(B/K/M/G)，支持最多两位小数，范围 128M\~32G，如 512M、1G、1.5G |
| -sn--src-numa           | 可选，本地NUMA节点ID。同平面socket会被选为借出socket    | 整数                                                     |

**约束限制**

1. ubsectl只能在root，ubse用户中运行
2. 需要目标节点的 process\_mem 插件正常启动，否则配置无法生效
3. evict-thresh必须比reclaim-thresh至少大5，避免震荡
4. 当进程已有子进程时，子进程会继承该配置

**输出信息说明**

Set successfully / 错误信息

**示例**

```bash
# 配置PID为1234的进程，迁出阈值80%，预期迁出比例50%，迁回阈值30%，期望内存1G
$ ubsectl change process-mem -p 1234 -e 80 -t 50 -r 30 -s 1G
Set successfully

# 指定本地NUMA节点
$ ubsectl change process-mem -p 1234 -e 80 -t 50 -r 30 -s 1G --src-numa 0
Set successfully

# 参数校验失败
$ ubsectl change process-mem -p 1234 -e 80 -t 50 -r 76 -s 1G
evict-thresh must be at least 5 higher than reclaim-thresh to avoid oscillation
```

### 移除进程内存配置

**描述**

移除指定进程的内存配置信息。移除前会将进程已借用的远端内存全部迁回并归还，配置移除后进程将不再进行内存迁出/迁入管理。

**用法**

```shell
ubsectl remove process-mem -p <pid>
```

**输入参数说明**

| 参数名     | 说明          | 取值              |
| ------- | ----------- | --------------- |
| -p--pid | 必选，目标进程的PID | 整数，范围 1-4194304 |

**约束限制**

1. ubsectl只能在root，ubse用户中运行
2. 需要目标节点的 process\_mem 插件正常启动，否则配置移除操作无法执行

**输出信息说明**

Unset successfully

**示例**

```bash
$ ubsectl remove process-mem -p 1234
Unset successfully
```

### 导入共享内存

**描述**

在本节点导入共享形态的远端内存。

**用法**

```bash
ubsectl attach memory -n <name>
```

**输入参数说明**

| 字段名      | 字段描述    | 字段取值                                       |
| -------- | ------- | ------------------------------------------ |
| -n--name | 借用的唯一标识 | 字符串长度为1\~47，仅可包括大小写字母、数字、"."、":"、"-"以及"\_" |

**输出信息说明**

| 字段名         | 字段描述          | 字段取值      |
| ----------- | ------------- | --------- |
| name        | 本次借用的唯一标识     | 字符串       |
| size        | 借用内存大小，单位: MB | 整数        |
| mem-ids     | 内存标识信息        | 整数数组，用,分隔 |
| import-node | 借入节点id        | 整数        |
| export-node | 借出节点id        | 整数        |
| region      | 共享域           | 整数数组，用,分隔 |

**示例**

```bash
$ ubsectl attach memory -n testName
name:testName
size:128M
mem-ids:1,2,3
import-node:1
export-node:2   
region:1,2
```

### 删除导入共享内存

**描述**

删除本节点导入的共享形态的远端内存。

**用法**

```bash
ubsectl detach memory -n <name>
```

**输入参数说明**

| 字段名      | 字段描述    | 字段取值                                       |
| -------- | ------- | ------------------------------------------ |
| -n--name | 借用的唯一标识 | 字符串长度为1\~47，仅可包括大小写字母、数字、"."、":"、"-"以及"\_" |

**输出信息说明**

```bash
Detach successfully
```

## topo链接

### 查询拓扑链接信息

**描述**

查询集群内所有节点的静态拓扑链接信息

**用法**

```shell
ubsectl display topo -t <OPTIONS>
```

**输入参数**

| 字段名 | 字段描述        | 字段取值         |
| --- | ----------- | ------------ |
| -t  | 必选，根据节点类型过滤 | 可取值：\[ cpu ] |

**约束限制**

1. 当前只支持CPU类型节点的硬件连线关系
2. 该命令只回显UBM上报的静态topo链接关系

**输出信息说明**

1. 规格说明：

   a. 表内每一行表示两个CPU设备之间的链接信息，显示的链接对不重复，排序后展示。

   b. 表内列信息说明如下
   | 表头                                                                        | 说明                                                                                               | 取值     |
   | ------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------ | ------ |
   | link-id                                                                   | 表示一条链接的ID。1.显示为“-”时，表示该链路不可用                                                                     | 字符串    |
   | node                                                                      | 排序较小一端的节点信息。例：computer1(1)，由2部分组成：1.括号前部分：主机名2.括号内部分：节点的槽位号3.显示为“-”时，表示该节点的UBSE尚未被划入本节点UBSE所在的集群 | 字符串    |
   | socket                                                                    | 排序较小一端的节点信息内，OS上标识某一个CPU的socketId值，例：361.当该节点的UBSE上报信息失败时，显示“-”来表示缺乏信息                           | 整数     |
   | port                                                                      | 排序较小一端的节点信息内，CPU硬件上的端口值，例：0 1.当该节点的UBSE上报信息失败时，显示“-”来表示缺乏信息                                      | 整数     |
   | interface-name                                                            | 排序较小一端的端口名称。例：400GUB1/1/1。1.当无法获取到端口名称时，显示"-"来表示缺乏信息；2.此名称与UBM配置命令、告警、性能指标中的端口名称对应               | 字符串    |
   | peer-node                                                                 | 排序较大一端的节点信息。例：computer2(2)，由2部分组成：1.括号前部分：主机名2.括号内部分：节点的槽位号3.显示为“-”时，表示该节点的UBSE尚未被划入本节点UBSE所在的集群 | 字符串    |
   | peer-socket                                                               | 排序较大一端的节点内，OS上标识某一个CPU的socketId值，例：36 1.当该节点的UBSE上报信息失败时，显示“-”来表示缺乏信息                            | 整数     |
   | peer-port                                                                 | 排序较大一端的节点内，CPU硬件上的端口值，例：0 1.当该节点的UBSE上报信息失败时，显示“-”来表示缺乏信息                                        | 整数     |
   | peer-interface-name                                                       | 排序较大一端的端口名称。例：400GUB2/1/1。1.当无法获取到端口名称时，显示"-"来表示缺乏信息；2.此名称与UBM配置命令、告警、性能指标中的端口名称对应               | 字符串    |
   | c. 链接显示完成后，最后两行会显示当前集群的已知总链接数：**Total Links**，和可用链接数：**Available Links**。 | <br />                                                                                           | <br /> |

**示例**

以3节点如下组网场景为例：

![img.png](./images/img-ubse-node.png)

场景1：Ubse能够获取全量各节点信息场景：

```bash
$ ubsectl display topo -t cpu
cpu topo：
------------------------------------------------------------------------------------------------------------------------
link-id         node           socket  port  interface-name    peer-node       peer-socket peer-port peer-interface-name
------------------------------------------------------------------------------------------------------------------------
1/36/0-2/36/0   computer1(1)   36      0     400GUB1/1/1       computer2(2)    36          0          400GUB2/1/1
1/36/1-2/36/1   computer1(1)   36      1     400GUB1/1/2       computer2(2)    36          1          400GUB2/1/2
1/36/2-3/36/0   computer1(1)   36      2     400GUB1/1/3       computer3(3)    36          0          400GUB3/1/1
1/36/3-3/36/1   computer1(1)   36      3     400GUB1/1/4       computer3(3)    36          1          400GUB3/1/2
1/376/0-2/376/0 computer1(1)   376     0     400GUB1/2/1       computer2(2)    376         0          400GUB2/2/1
1/376/1-2/376/1 computer1(1)   376     1     400GUB1/2/2       computer2(2)    376         1          400GUB2/2/2
1/376/2-3/376/0 computer1(1)   376     2     400GUB1/2/3       computer3(3)    376         0          400GUB3/2/1
1/376/3-3/376/1 computer1(1)   376     3     400GUB1/2/4       computer3(3)    376         1          400GUB3/2/2
2/36/2-3/36/2   computer2(2)   36      2     400GUB2/1/3       computer3(3)    36          2          400GUB3/1/3
2/36/3-3/36/3   computer2(2)   36      3     400GUB2/1/4       computer3(3)    36          3          400GUB3/1/4
2/376/2-3/376/2 computer2(2)   376     2     400GUB2/2/3       computer3(3)    376         2          400GUB3/2/3
2/376/3-3/376/3 computer2(2)   376     3     400GUB2/2/4       computer3(3)    376         3          400GUB3/2/4
------------------------------------------------------------------------------------------------------------------------
Total Links: 12
Available Links: 12
```

场景2：当某一节点不在当前集群时，显示为“-”。具体如下，以缺少2节点信息为例。

```sh
$ ubsectl display topo -t cpu
cpu topo：
------------------------------------------------------------------------------------------------------------------------
link-id         node            socket  port  interface-name    peer-node       peer-socket peer-port peer-interface-name
------------------------------------------------------------------------------------------------------------------------
-               computer1(1)    36      0     400GUB1/1/1       -               -           -          -
-               computer1(1)    36      1     400GUB1/1/2       -               -           -          -
1/36/2-3/36/0   computer1(1)    36      2     400GUB1/1/3       computer3(3)    36          0          400GUB3/1/1
1/36/3-3/36/1   computer1(1)    36      3     400GUB1/1/4       computer3(3)    36          1          400GUB3/1/2
-               computer1(1)    376     0     400GUB1/2/1       -               -           -          -
-               computer1(1)    376     1     400GUB1/2/2       -               -           -          -
1/376/2-3/376/0 computer1(1)    376     2     400GUB1/2/3       computer3(3)    376         0          400GUB3/2/1
1/376/3-3/376/1 computer1(1)    376     3     400GUB1/2/4       computer3(3)    376         1          400GUB3/2/2
-               -               -       -     -                 computer3(3)    36          2          400GUB3/1/3
-               -               -       -     -                 computer3(3)    36          3          400GUB3/1/4
-               -               -       -     -                 computer3(3)    376         2          400GUB3/2/3
-               -               -       -     -                 computer3(3)    376         3          400GUB3/2/4
------------------------------------------------------------------------------------------------------------------------
Total Links: 12
Available Links: 4
```

场景3：当链接两端节点上报链接状态不同时，端口状态为down的一端socket、port、interface-name显示为“-”。具体如下，以1、2节点上报链接状态不同为例。

```bash
$ ubsectl display topo -t cpu
cpu topo：
------------------------------------------------------------------------------------------------------------------------
link-id         node           socket  port  interface-name    peer-node       peer-socket peer-port peer-interface-name
------------------------------------------------------------------------------------------------------------------------
-               computer1(1)   -       -     -                 computer2(2)    36          0          400GUB2/1/1
-               computer1(1)   36      1     400GUB1/1/2       computer2(2)    -           -          -          
1/36/2-3/36/0   computer1(1)   36      2     400GUB1/1/3       computer3(3)    36          0          400GUB3/1/1
1/36/3-3/36/1   computer1(1)   36      3     400GUB1/1/4       computer3(3)    36          1          400GUB3/1/2
1/376/0-2/376/0 computer1(1)   376     0     400GUB1/2/1       computer2(2)    376         0          400GUB2/2/1
1/376/1-2/376/1 computer1(1)   376     1     400GUB1/2/2       computer2(2)    376         1          400GUB2/2/2
1/376/2-3/376/0 computer1(1)   376     2     400GUB1/2/3       computer3(3)    376         0          400GUB3/2/1
1/376/3-3/376/1 computer1(1)   376     3     400GUB1/2/4       computer3(3)    376         1          400GUB3/2/2
2/36/2-3/36/2   computer2(2)   36      2     400GUB2/1/3       computer3(3)    36          2          400GUB3/1/3
2/36/3-3/36/3   computer2(2)   36      3     400GUB2/1/4       computer3(3)    36          3          400GUB3/1/4
2/376/2-3/376/2 computer2(2)   376     2     400GUB2/2/3       computer3(3)    376         2          400GUB3/2/3
2/376/3-3/376/3 computer2(2)   376     3     400GUB2/2/4       computer3(3)    376         3          400GUB3/2/4
------------------------------------------------------------------------------------------------------------------------
Total Links: 12
Available Links: 10
```

### 查询集群内所有节点的信息

**功能**

查询集群内所有节点的信息

**用法**

```shell
ubsectl display cluster
```

**输入参数**

无

**约束限制**

无

**输出信息说明**

| 字段名         | 字段描述                                                    | 取值范围                                |
| ----------- | ------------------------------------------------------- | ----------------------------------- |
| node        | 节点信息。例：computer1(1)节点信息由2部分组成：1.括号前部分：主机名2.括号内部分：节点的槽位号 | 字符串                                 |
| role        | 节点在集群中的角色                                               | 可选值：\[ master \| standby \| agent ] |
| bonding-eid | 节点bonding配置的eid                                         | 字符串                                 |
| guid        | 节点全局唯一标识符                                               | 字符串                                 |

**示例**

#### 场景1：集群各节点健康状态

```bash
$ ubsectl display cluster
---------------------------------------------------------------------------------------------------------------------
node                  role          bonding-eid                               guid
---------------------------------------------------------------------------------------------------------------------
computer01(1)         master        4245:4944:0000:0000:0000:0000:0100:0000   CC08-A000-0-0-000000-0000000800800100
computer02(2)         standby       4245:4944:0000:0000:0000:0000:0200:0000   CC08-A000-0-0-000000-0000000801800100
computer03(3)         agent         4245:4944:0000:0000:0000:0000:0300:0000   CC08-A000-0-0-000000-0000000802000000
---------------------------------------------------------------------------------------------------------------------

```

#### 场景2：节点重启中，备节点无法连上master节点

- master节点
  ```bash
  $ ubsectl display cluster
  ---------------------------------------------------------------------------------------------------------------------
  node                  role          bonding-eid                               guid
  ---------------------------------------------------------------------------------------------------------------------
  computer01(1)         master        4245:4944:0000:0000:0000:0000:0100:0000   CC08-A000-0-0-000000-0000000800800100
  -(2)                  -             4245:4944:0000:0000:0000:0000:0200:0000   -
  ---------------------------------------------------------------------------------------------------------------------
  ```
- 备节点
  ```bash
  $ ubsectl display cluster
  ERROR: Failed to obtain cluster information
  ```

#### 场景3：节点启动稳定后

- master节点
  ```bash
  $ ubsectl display cluster
  ---------------------------------------------------------------------------------------------------------------------
  node                  role          bonding-eid                               guid
  ---------------------------------------------------------------------------------------------------------------------
  computer01(1)         master        4245:4944:0000:0000:0000:0000:0100:0000   CC08-A000-0-0-000000-0000000800800100
  computer01(2)         standby       4245:4944:0000:0000:0000:0000:0200:0000   CC08-A000-0-0-000000-0000000801800100
  ---------------------------------------------------------------------------------------------------------------------

  ```
- 备节点
  ```bash
  $ ubsectl display cluster
  ---------------------------------------------------------------------------------------------------------------------
  node                  role          bonding-eid                               guid
  ---------------------------------------------------------------------------------------------------------------------
  computer01(1)         master        4245:4944:0000:0000:0000:0000:0100:0000   CC08-A000-0-0-000000-0000000800800100
  computer01(2)         standby       4245:4944:0000:0000:0000:0000:0200:0000   CC08-A000-0-0-000000-0000000801800100
  ---------------------------------------------------------------------------------------------------------------------
  ```

#### 场景4：只有主节点启动，备节点不启动

- master节点
  ```bash
  $ ubsectl display cluster
  ---------------------------------------------------------------------------------------------------------------------
  node                  role          bonding-eid                               guid
  ---------------------------------------------------------------------------------------------------------------------
  computer01(1)         master        4245:4944:0000:0000:0000:0000:0100:0000   CC08-A000-0-0-000000-0000000800800100
  -(2)                  -             4245:4944:0000:0000:0000:0000:0200:0000   -
  ---------------------------------------------------------------------------------------------------------------------

  ```
- 备节点
  ```bash
  $ ubsectl display cluster
  ERROR: Internal error with error code xx

  ```

#### 场景5：稳定后备节点停掉

- master节点
  ```bash
  $ ubsectl display cluster
  ---------------------------------------------------------------------------------------------------------------------
  node                  role          bonding-eid                               guid
  ---------------------------------------------------------------------------------------------------------------------
  computer01(1)         master        4245:4944:0000:0000:0000:0000:0100:0000   CC08-A000-0-0-000000-0000000800800100
  -(2)                  -             4245:4944:0000:0000:0000:0000:0200:0000   -
  ---------------------------------------------------------------------------------------------------------------------

  ```
- 备节点
  ```bash
  $ ubsectl display cluster
  ERROR: Internal error with error code xx

  ```

#### 场景6：稳定后主节点停掉

- master节点
  ```bash
  $ ubsectl display cluster
  ---------------------------------------------------------------------------------------------------------------------
  node                  role          bonding-eid                               guid
  ---------------------------------------------------------------------------------------------------------------------
  -(1)                  -             4245:4944:0000:0000:0000:0000:0100:0000   -
  computer02(2)         master        4245:4944:0000:0000:0000:0000:0200:0000   CC08-A000-0-0-000000-0000000801800100
  ---------------------------------------------------------------------------------------------------------------------

  ```
- 备节点
  ```bash
  $ ubsectl display cluster
  ERROR: Internal error with error code xx

  ```

### 查询集群中节点的详细信息

**功能**

查询集群中指定节点的详细信息，当不指定参数时显示当前节点的信息。

**用法**

```shell
# 显示节点的详细信息
ubsectl display node [-n <node_id>]
```

**输入参数**

| 参数名        | 必填 | 参数描述       | 取值范围          |
| ---------- | -- | ---------- | ------------- |
| -n, --node | 否  | 指定要查询的节点ID | 数字，取值范围 1-255 |

**约束限制**

1. 当指定 -n参数时，必须确保指定的节点ID存在于集群中
2. 该命令需要节点间网络通信正常才能获取完整的节点信息
3. 在故障场景下，如果目标节点不可达，命令将返回相应的错误信息
4. 每次只能查询一个节点的信息，需要查询多个节点时请使用 display cluster命令

**输出信息说明**

1. 规格说明
   该命令返回的数据是 ubsectl display cluster命令的子集，包含以下字段：
   | 字段名         | 字段描述                                                    | 取值范围                                |
   | ----------- | ------------------------------------------------------- | ----------------------------------- |
   | node        | 节点信息。例：computer1(1)节点信息由2部分组成：1.括号前部分：主机名2.括号内部分：节点的槽位号 | 字符串                                 |
   | role        | 节点在集群中的角色                                               | 可选值：\[ master \| standby \| agent ] |
   | bonding-eid | 节点bonding配置的eid                                         | 字符串                                 |
   | guid        | 节点全局唯一标识符                                               | 字符串                                 |
2. 注意事项
   a. 该命令是 display cluster命令的精简版，专注于单个节点的详细信息
   b. 当目标节点不可达时，命令会尝试返回最近已知的节点信息（包括bonding-eid）
   c. 返回的节点名称格式与 display cluster命令保持一致
   d. 错误处理逻辑与集群其他命令保持一致

**示例**

#### 场景1：查询当前节点的信息

```bash
$ ubsectl display node
---------------------------------------------------------------------------------------------------------------------
node                  role          bonding-eid                               guid
---------------------------------------------------------------------------------------------------------------------
computer01(1)         master        4245:4944:0000:0000:0000:0000:0100:0000   CC08-A000-0-0-000000-0000000800800100
---------------------------------------------------------------------------------------------------------------------
 
```

#### 场景2：查询指定节点的信息（节点正常）

```bash
$ ubsectl display node -n 2
---------------------------------------------------------------------------------------------------------------------
node                  role          bonding-eid                               guid
---------------------------------------------------------------------------------------------------------------------
computer02(2)         standby       4245:4944:0000:0000:0000:0000:0200:0000   CC08-A000-0-0-000000-0000000801800100
---------------------------------------------------------------------------------------------------------------------

$ ubsectl display node -n 3
---------------------------------------------------------------------------------------------------------------------
node                  role          bonding-eid                               guid
---------------------------------------------------------------------------------------------------------------------
computer03(3)         agent         4245:4944:0000:0000:0000:0000:0300:0000   CC08-A000-0-0-000000-0000000802000000
---------------------------------------------------------------------------------------------------------------------
 
```

#### 场景3：查询当前节点的信息（当前节点是standby）

```bash
$ ubsectl display node
---------------------------------------------------------------------------------------------------------------------
node                  role          bonding-eid                               guid
---------------------------------------------------------------------------------------------------------------------
computer02(2)         standby       4245:4944:0000:0000:0000:0000:0200:0000   CC08-A000-0-0-000000-0000000801800100
---------------------------------------------------------------------------------------------------------------------
 
```

#### 场景4：指定不存在的节点

```bash
$ ubsectl display node -n 99
ERROR: Node 99 does not exist in the cluster
 
```

#### 场景5：只有主节点启动，主节点查询备节点信息

```bash
$ ubsectl display node -n 2
ERROR: Node 2 is not active
---------------------------------------------------------------------------------------------------------------------
node                  role          bonding-eid                               guid
---------------------------------------------------------------------------------------------------------------------
-(2)                  -             4245:4944:0000:0000:0000:0000:0200:0000   -
---------------------------------------------------------------------------------------------------------------------
 
```

#### 场景6：稳定后备节点停掉，主节点查询备节点信息

```bash
$ ubsectl display node -n 2
ERROR: Node 2 is not responding
---------------------------------------------------------------------------------------------------------------------
node                  role          bonding-eid                               guid
---------------------------------------------------------------------------------------------------------------------
-(2)                  -             4245:4944:0000:0000:0000:0000:0200:0000   -
---------------------------------------------------------------------------------------------------------------------
 
```

#### 场景7：稳定后主节点停掉，备节点（现为主节点）查询原主节点信息

```bash
$ ubsectl display node -n 1
ERROR: Node 1 is not responding
---------------------------------------------------------------------------------------------------------------------
node                  role          bonding-eid                               guid
---------------------------------------------------------------------------------------------------------------------
-(1)                  -             4245:4944:0000:0000:0000:0000:0100:0000   -
---------------------------------------------------------------------------------------------------------------------
 
```

## URMA

### 提供查询URMA设备

- **用法**

```shell
ubsectl display urma [--node <node-id>] [--dev <urma_name>]
```

- **输入参数**

| 字段名      | 字段描述          | 字段取值                             |
| -------- | ------------- | -------------------------------- |
| --node-n | 可选，指定要查询的节点ID | 数字，取值范围 1-255                    |
| --dev-d  | 可选，设备名称       | 字符串列表，批量查询用逗号分隔，如urma\_1,urma\_2 |

- **输出信息说明**
  支持分页

| 字段名        | 字段描述                | 取值范围                         |
| ---------- | ------------------- | ---------------------------- |
| urma-name  | bonding设备的名称        | 字符串                          |
| dev-eid    | bonding设备的 Eid      | 字符串                          |
| dev1\_name | bonding设备绑定的fe名称    | 字符串                          |
| dev2\_name | bonding设备绑定的fe名称    | 字符串                          |
| dev1\_eid  | bonding设备绑定的fe1 Eid | 字符串                          |
| dev2\_eid  | bonding设备绑定的fe2 Eid | 字符串                          |
| status     | urma设备状态            | 可选值： \[ active \| inactive ] |

- **示例**

查询指定节点的所有URMA设备：

```shell
$ ubsectl display urma --node 1
----------------------------------------------------------------------------------------------------------
urma-name      dev-eid     dev1-name         dev2-name         dev1-eid         dev2-eid        status 
----------------------------------------------------------------------------------------------------------
urma_1         eid0         udma1            udma49             eid1            eid2            active
urma_2         eid3         udma2            udma50             eid4            eid5            inactive
urma_3         eid6         udma3            udma51             eid7            eid8            active
...
----------------------------------------------------------------------------------------------------------
```

查询指定名称的URMA设备：

```shell
$ ubsectl display urma --dev urma_1,urma_2
----------------------------------------------------------------------------------------------------------
urma-name      dev-eid     dev1-name         dev2-name         dev1-eid         dev2-eid        status 
----------------------------------------------------------------------------------------------------------
urma_1         eid0         udma1            udma49             eid1            eid2            active
urma_2         eid3         udma2            udma50             eid4            eid5            inactive
----------------------------------------------------------------------------------------------------------
```

### 创建ETS QoS配置

**描述**

创建ETS (Enhanced Transmission Selection) QoS优先级组配置，为指定优先级分配带宽。

**用法**

```shell
ubsectl create urma-qos -p <priority> -c <cir>
```

**输入参数**

| 字段名     | 字段描述          | 字段取值                      |
| ------- | ------------- | ------------------------- |
| -p--pri | 必选，优先级级别      | 数字，可选值：0、1。支持逗号分隔列表，如 0,1 |
| -c--cir | 必选，承诺信息速率（带宽） | 非负整数，单位：Gbps。支持逗号分隔列表     |

**约束限制**

1. ubsectl只能在root、ubse用户中运行
2. \--pri和--cir参数必须同时提供，且数量必须匹配
3. 优先级配置数量最多为2组（即最多两个优先级）
4. 不允许配置重复的优先级
5. CLOS组网模式不支持-n参数
6. 不支持修改，需执行`ubsectl delete urma-qos`删除后重新创建

**输出信息说明**

执行成功返回：`create successfully`

**错误信息说明**

| 错误信息                                                                                   | 说明                         |
| -------------------------------------------------------------------------------------- | -------------------------- |
| ERROR: CLOS networking does not support -n option.                                     | CLOS组网模式不支持指定节点            |
| ERROR: Both --pri and --cir are required.                                              | --pri和--cir参数必须同时提供        |
| ERROR: Invalid priority or bandwidth value.                                            | 优先级或带宽值无效                  |
| ERROR: Priority must be 0 or 1.                                                        | 优先级必须是0或1                  |
| ERROR: --pri and --cir count mismatch.                                                 | --pri和--cir参数数量不匹配         |
| ERROR: QoS config count exceeds limit (max 2).                                         | QoS配置数量超过限制（最多2组）          |
| ERROR: Duplicate priorities are not allowed.                                           | 不允许配置重复的优先级                |
| ERROR: Failed to access UBM interface, please check UBM service status.                | 无法访问UBM接口，请检查UBM服务状态       |
| ERROR: ETS QoS priority group already exists, please delete existing QoS config first. | QoS配置已存在，请先删除现有配置          |
| ERROR: Failed to create ETS QoS template, No ETS QoS template exists.                  | 创建ETS QoS模板失败，不存在ETS QoS模板 |

**示例**

创建单个优先级配置：

```shell
$ ubsectl create urma-qos -p 0 -c 10
create successfully
```

创建多个优先级配置：

```shell
$ ubsectl create urma-qos -p 0,1 -c 10,20
create successfully
```

参数数量不匹配的错误示例：

```shell
$ ubsectl create urma-qos -p 0,1 -c 10
ERROR: --pri and --cir count mismatch.
```

### 删除ETS QoS配置

**描述**

删除ETS QoS优先级组配置，释放已分配的带宽资源。

**用法**

```shell
ubsectl delete urma-qos
```

**输入参数**

无

**约束限制**

1. ubsectl只能在root、ubse用户中运行
2. CLOS组网模式不支持-n参数

**输出信息说明**

执行成功返回：`delete successfully`

**错误信息说明**

| 错误信息                                                                    | 说明                   |
| ----------------------------------------------------------------------- | -------------------- |
| ERROR: CLOS networking does not support -n option.                      | CLOS组网模式不支持指定节点      |
| ERROR: Failed to access UBM interface, please check UBM service status. | 无法访问UBM接口，请检查UBM服务状态 |

**示例**

```shell
$ ubsectl delete urma-qos
delete successfully
```

### 查询ETS QoS配置

**描述**

查询已创建的ETS QoS优先级组配置信息。

**用法**

```shell
ubsectl display urma-qos
```

**输入参数**

无

**约束限制**

1. ubsectl只能在root、ubse用户中运行
2. CLOS组网模式不支持-n参数

**输出信息说明**

| 字段名             | 字段描述   | 字段取值         |
| --------------- | ------ | ------------ |
| priority        | 优先级级别  | 数字：0 或 1     |
| bandwidth(Gbps) | 分配的带宽值 | 非负整数，单位：Gbps |

**错误信息说明**

| 错误信息                                                                                     | 说明                         |
| ---------------------------------------------------------------------------------------- | -------------------------- |
| ERROR: CLOS networking does not support -n option.                                       | CLOS组网模式不支持指定节点            |
| ERROR: No ETS QoS priority groups has been created, please run: ubsectl create urma-qos. | 未创建ETS QoS优先级组，请先执行创建命令    |
| ERROR: Failed to access UBM interface, please check UBM service status.                  | 无法访问UBM接口，请检查UBM服务状态       |
| ERROR: Failed to create ETS QoS template, No ETS QoS template exists.                    | 创建ETS QoS模板失败，不存在ETS QoS模板 |

**示例**

查询存在的ETS QoS配置：

```shell
$ ubsectl display urma-qos
------------------------------
priority    bandwidth(Gbps)
------------------------------
0           10
1           20
------------------------------
```

未创建配置时的错误示例：

```shell
$ ubsectl display urma-qos
ERROR: No ETS QoS priority groups has been created, please run: ubsectl create urma-qos.
```

## SSU存储分配

### 查询SSU分配摘要

**描述**

查询当前用户可见的SSU存储空间分配摘要，包括分配名称、总容量和分配策略。

**用法**

```shell
ubsectl-ssu display -t alloc_summary
```

**输入参数**

| 参数        | 说明                 | 取值            |
| --------- | ------------------ | ------------- |
| -t/--type | **必选**，SSU查询类型 | `alloc_summary` |

**约束限制**

ubsectl-ssu只能在root、ubse用户中运行。

**输出信息说明**

| 字段名     | 字段描述                  | 字段取值                     |
| ------- | --------------------- | ------------------------ |
| name    | 分配空间标识                | 字符串，与创建SSU分配时的name参数一致 |
| size    | 分配空间下所有命名空间容量之和       | GiB整数，统一以`G`为单位显示       |
| strategy | 分配策略                  | 可选值：\[ Linear \| Striped ] |

**错误信息说明**

| 错误信息                                           | 说明                 |
| ---------------------------------------------- | ------------------ |
| INFO: No SSU allocation information found.     | 未查询到任何SSU分配信息      |
| ERROR: The option -t or --type is required.    | 缺少必选参数type          |
| ERROR: Invalid type. The value must be alloc_summary or alloc_detail. | type取值不合法 |
| ERROR: Internal error with error code \<code>. | 服务端内部错误，code为具体错误码 |
| ERROR: Deserialization failed in client.       | 客户端反序列化响应数据失败      |
| ERROR: Serialization failed in client.         | 客户端序列化请求数据失败       |

**示例**

```shell
$ ubsectl-ssu display -t alloc_summary
------------------------------------------------
name            size    strategy
------------------------------------------------
alloc-space-1   10G     Linear
alloc-space-2   20G     Striped
------------------------------------------------
```

### 查询SSU分配详情

**描述**

查询指定SSU存储空间的详细分配信息，包括命名空间列表、目标端信息、设备路径、容量和LBA格式。需指定分配名称。

**用法**

```shell
ubsectl-ssu display -t alloc_detail -n <name>
```

**输入参数**

| 参数        | 说明                 | 取值                                           |
| --------- | ------------------ | -------------------------------------------- |
| -t/--type | **必选**，SSU查询类型 | `alloc_detail`                               |
| -n/--name | **必选**，按分配名称查询 | 字符串，1-48字符，仅支持大小写字母、数字、`.`、`:`、`-`和`_` |

**约束限制**

1. ubsectl-ssu只能在root、ubse用户中运行
2. 必须指定-n参数，返回指定名称的分配空间详情

**输出信息说明**

表头包含分配名称和策略，表体为该分配空间下的命名空间列表。

| 字段名         | 字段描述                        | 字段取值                         |
| ----------- | --------------------------- | ---------------------------- |
| Name        | 分配空间标识，展示在表头上方              | 字符串，与创建SSU分配时的name参数一致     |
| Strategy    | 分配策略，展示在表头上方                | 可选值：\[ Linear \| Striped ]   |
| ns_uuid     | 命名空间对应的物理设备UUID             | 字符串，格式为UUID                  |
| tgt_eid     | Target EID                  | 字符串                          |
| tgt_nqn     | Target NQN                  | 字符串                          |
| namespace_id | 命名空间ID                     | 正整数                          |
| ns_dev_path | 命名空间设备路径                   | 字符串                          |
| ns_size     | 命名空间容量                     | GiB整数，统一以`G`为单位显示           |
| lba_format  | LBA格式                       | 可选值：\[ 512B \| 4K ]          |

**错误信息说明**

| 错误信息                                           | 说明                 |
| ---------------------------------------------- | ------------------ |
| INFO: No SSU allocation information found.     | 未查询到任何SSU分配信息      |
| ERROR: The option -t or --type is required.    | 缺少必选参数type          |
| ERROR: Invalid type. The value must be alloc_summary or alloc_detail. | type取值不合法 |
| ERROR: The option -n or --name is required.    | 缺少必选参数name          |
| ERROR: Invalid name. The value must be 1-48 characters and contain only letters, digits, '.', ':', '-' or '_'. | name格式不合法 |
| ERROR: Internal error with error code \<code>. | 服务端内部错误，code为具体错误码 |
| ERROR: Deserialization failed in client.       | 客户端反序列化响应数据失败      |
| ERROR: Serialization failed in client.         | 客户端序列化请求数据失败       |

**示例**

查询指定分配空间的详情：

```shell
$ ubsectl-ssu display -t alloc_detail -n alloc-space-1
----------------------------------------------------------------------------------------------------------------
Name: alloc-space-1  Strategy:Linear
----------------------------------------------------------------------------------------------------------------
ns_uuid    tgt_eid    tgt_nqn             namespace_id    ns_dev_path     ns_size    lba_format
----------------------------------------------------------------------------------------------------------------
uuid-aa    e2         nqn.2024-01:target  1               /dev/nvme0n1    10G        4K
uuid-bb    e3         nqn.2024-01:target  2               /dev/nvme1n1    10G        4K
----------------------------------------------------------------------------------------------------------------
```

### 分配SSU存储空间

**描述**

分配SSU存储空间，创建指定数量和容量的命名空间。支持线性分配和条带化分配两种策略。

**用法**

```shell
ubsectl-ssu create -n <name> -s <size> [-l <lba>] [-m <num>] [-r <strategy>]
```

**输入参数**

| 参数             | 说明                  | 取值                                                                 |
| -------------- | ------------------- | ------------------------------------------------------------------ |
| -n/--name      | **必选**，分配空间标识名称 | 字符串，1-48字符，仅支持大小写字母、数字、`.`、`:`、`-`和`_`                         |
| -s/--size      | **必选**，申请总容量      | 大写`G`后缀的GiB整数，如`10G`；最小`1G`                                |
| -l/--lba       | 可选，LBA格式          | 可选值：`512B`、`4K`；默认为`512B`                                      |
| -m/--ns\_num   | 可选，切分的命名空间数量     | 正整数，范围1-128；默认为1                                                |
| -r/--strategy  | 可选，分配策略           | 可选值：`Linear`、`Striped`；默认为`Linear`。当`ns_num`为1时，strategy参数不生效 |

**约束限制**

1. ubsectl-ssu只能在root、ubse用户中运行
2. name在系统中必须唯一，重复name将返回错误
3. size只接受大写`G`后缀，不接受小写`g`或无后缀格式
4. LBA格式必须使用`512B`或`4K`，不接受裸数字`512`
5. strategy必须使用`Linear`或`Striped`，不接受全小写格式
6. 当ns_num为1时，strategy参数不生效

**输出信息说明**

分配成功后，以表格形式展示分配结果，格式与查询详情命令一致。

| 字段名         | 字段描述                        | 字段取值                       |
| ----------- | --------------------------- | -------------------------- |
| Name        | 分配空间标识，展示在表头上方              | 字符串                        |
| Strategy    | 分配策略，展示在表头上方                | 可选值：\[ Linear \| Striped ] |
| ns_uuid     | 命名空间对应的物理设备UUID             | 字符串，格式为UUID                |
| tgt_eid     | Target EID                  | 字符串                        |
| tgt_nqn     | Target NQN                  | 字符串                        |
| namespace_id | 命名空间ID                     | 正整数                        |
| ns_dev_path | 命名空间设备路径                   | 字符串                        |
| ns_size     | 命名空间容量                     | GiB整数，统一以`G`为单位显示         |
| lba_format  | LBA格式                       | 可选值：\[ 512B \| 4K ]        |

**错误信息说明**

| 错误信息                                           | 说明                 |
| ---------------------------------------------- | ------------------ |
| INFO: No SSU allocation information found.     | 服务端返回的命名空间列表为空    |
| ERROR: The option -n or --name is required.    | 缺少必选参数name          |
| ERROR: The option -s or --size is required.    | 缺少必选参数size          |
| ERROR: Invalid name. The value must be 1-48 characters and contain only letters, digits, '.', ':', '-' or '_'. | name格式不合法 |
| ERROR: Invalid size. The value must be an integer number of GiB with uppercase G suffix, minimum 1G. | size格式不合法, 必须为大写G后缀的GiB整数, 最小1G |
| ERROR: Invalid ns_num. The value must be an integer in range 1-128. | ns_num不在1-128范围内 |
| ERROR: Invalid lba. The value must be 512B or 4K. | lba格式不合法, 必须为512B或4K |
| ERROR: Invalid strategy. The value must be Linear or Striped. | strategy格式不合法, 必须为Linear或Striped |
| ERROR: Internal error with error code \<code>. | 服务端内部错误，code为具体错误码 |
| ERROR: Deserialization failed in client.       | 客户端反序列化响应数据失败      |
| ERROR: Serialization failed in client.         | 客户端序列化请求数据失败       |

**示例**

分配一个10G命名空间，使用默认LBA格式和默认线性策略：

```shell
$ ubsectl-ssu create -n alloc-space-1 -s 10G
----------------------------------------------------------------------------------------------------------------
Name: alloc-space-1  Strategy:Linear
----------------------------------------------------------------------------------------------------------------
ns_uuid    tgt_eid    tgt_nqn             namespace_id    ns_dev_path     ns_size    lba_format
----------------------------------------------------------------------------------------------------------------
uuid-aa    e2         nqn.2024-01:target  1               /dev/nvme0n1    10G        512B
----------------------------------------------------------------------------------------------------------------
```

分配2个共20G的命名空间，使用条带化策略：

```shell
$ ubsectl-ssu create -n alloc-space-2 -s 20G -m 2 -l 4K -r Striped
----------------------------------------------------------------------------------------------------------------
Name: alloc-space-2  Strategy:Striped
----------------------------------------------------------------------------------------------------------------
ns_uuid    tgt_eid    tgt_nqn             namespace_id    ns_dev_path     ns_size    lba_format
----------------------------------------------------------------------------------------------------------------
uuid-aa    e2         nqn.2024-01:target  1               /dev/nvme0n1    10G        4K
uuid-bb    e3         nqn.2024-01:target  2               /dev/nvme1n1    10G        4K
----------------------------------------------------------------------------------------------------------------
```

### 挂载已分配的存储空间

**描述**

将指定的存储空间挂载到系统，仅执行命名空间挂载，返回命名空间设备路径列表。

**用法**

```shell
ubsectl-ssu attach -n <name> [-q <host_nqn>] [-e <src_eid>]
```

**输入参数**

| 参数             | 说明               | 取值                                         |
| -------------- | ---------------- | ------------------------------------------ |
| -n/--name      | **必选**，分配空间标识名称 | 字符串，1-48字符，仅支持大小写字母、数字、`.`、`:`、`-`和`_` |
| -q/--host\_nqn | 可选参数，Host NQN | 字符串，1-68字符；不指定时，使用创建namespace时默认分配的NQN |
| -e/--src\_eid  | 可选参数，源端EID    | 字符串，1-16字符；不指定时，使用host侧特定FE的EID          |

**约束限制**

1. ubsectl-ssu只能在root、ubse用户中运行
2. name必须对应已分配的SSU存储空间
3. 该命令仅执行命名空间挂载，不支持type、dev_name、level和chunk_size参数

**输出信息说明**

| 字段名        | 字段描述       | 字段取值                 |
| ----------- | ---------- | ---------------------- |
| ns_dev_paths | 命名空间设备路径列表 | 字符串列表，字符串之间使用`,`分隔 |

**错误信息说明**

| 错误信息                                           | 说明                 |
| ---------------------------------------------- | ------------------ |
| ERROR: The option -n or --name is required.    | 缺少必选参数name          |
| ERROR: Invalid name. The value must be 1-48 characters and contain only letters, digits, '.', ':', '-' or '_'. | name格式不合法 |
| ERROR: Invalid host_nqn. The value must be 1-68 characters. | host_nqn格式不合法 |
| ERROR: Invalid src_eid. The value must be 1-16 characters. | src_eid格式不合法 |
| ERROR: The option --dev_name, --level or --chunk_size requires --type. | 未指定type时携带了聚合参数 |
| ERROR: Internal error with error code \<code>. | 服务端内部错误，code为具体错误码 |
| ERROR: Deserialization failed in client.       | 客户端反序列化响应数据失败      |
| ERROR: Serialization failed in client.         | 客户端序列化请求数据失败       |

**示例**

```shell
$ ubsectl-ssu attach -n alloc-space-1 -q nqn.2024-01:host -e e1
ns_dev_paths: /dev/nvme0n1,/dev/nvme1n1
```

### 卸载已分配的存储空间

**描述**

将指定的存储空间从系统卸载，释放设备占用。

**用法**

```shell
ubsectl-ssu detach -n <name> [-q <host_nqn>]
```

**输入参数**

| 参数             | 说明               | 取值                                         |
| -------------- | ---------------- | ------------------------------------------ |
| -n/--name      | **必选**，分配空间标识名称 | 字符串，1-48字符，仅支持大小写字母、数字、`.`、`:`、`-`和`_` |
| -q/--host\_nqn | 可选参数，Host NQN | 字符串，1-68字符；attach时指定了host_nqn，则detach时必须指定相同host_nqn；attach时未指定，则detach时可以不指定 |

**约束限制**

1. ubsectl-ssu只能在root、ubse用户中运行
2. name必须对应已分配且已挂载的SSU存储空间
3. 卸载前需确保没有进程正在使用该存储空间
4. host_nqn必须与attach时传入的host_nqn一致；attach时未传入host_nqn，则detach时可以不传入host_nqn
5. 该命令仅执行命名空间卸载，不支持type、dev_name、level和chunk_size参数

**输出信息说明**

执行成功无额外输出；执行失败返回错误信息。

**错误信息说明**

| 错误信息                                           | 说明                 |
| ---------------------------------------------- | ------------------ |
| ERROR: The option -n or --name is required.    | 缺少必选参数name          |
| ERROR: Invalid name. The value must be 1-48 characters and contain only letters, digits, '.', ':', '-' or '_'. | name格式不合法 |
| ERROR: Invalid host_nqn. The value must be 1-68 characters. | host_nqn格式不合法 |
| ERROR: The option --dev_name requires --type. | 未指定type时携带了聚合设备名称参数 |
| ERROR: Internal error with error code \<code>. | 服务端内部错误，code为具体错误码 |
| ERROR: Deserialization failed in client.       | 客户端反序列化响应数据失败      |
| ERROR: Serialization failed in client.         | 客户端序列化请求数据失败       |

**示例**

```shell
$ ubsectl-ssu detach -n alloc-space-1 -q nqn.2024-01:host
```

### 挂载线性编址的存储空间

**描述**

将多个命名空间设备以线性拼接方式聚合为一个逻辑块设备并挂载，返回聚合设备路径和命名空间设备路径列表。

**用法**

```shell
ubsectl-ssu attach -t Linear -n <name> -d <dev_name> [-q <host_nqn>] [-e <src_eid>]
```

**输入参数**

| 参数             | 说明               | 取值                                         |
| -------------- | ---------------- | ------------------------------------------ |
| -t/--type      | **必选**，挂载类型      | 固定取值：`Linear`                             |
| -n/--name      | **必选**，分配空间标识名称 | 字符串，1-48字符，仅支持大小写字母、数字、`.`、`:`、`-`和`_` |
| -d/--dev\_name | **必选**，聚合设备名称    | 字符串，1-32字符，仅支持大小写字母、数字、`_`、`-`和`.` |
| -q/--host\_nqn | 可选参数，Host NQN | 字符串，1-68字符；不指定时，使用创建namespace时默认分配的NQN |
| -e/--src\_eid  | 可选参数，源端EID    | 字符串，1-16字符；不指定时，使用host侧特定FE的EID          |

**约束限制**

1. ubsectl-ssu只能在root、ubse用户中运行
2. name必须对应已分配的SSU存储空间
3. type必须使用`Linear`，不接受全小写格式
4. 必须指定dev_name
5. 线性编址挂载不支持level和chunk_size参数

**输出信息说明**

| 字段名        | 字段描述       | 字段取值                 |
| ----------- | ---------- | ---------------------- |
| ns_dev_paths | 命名空间设备路径列表 | 字符串列表，字符串之间使用`,`分隔 |
| dev_path    | 聚合设备路径     | 字符串                  |

**错误信息说明**

| 错误信息                                           | 说明                 |
| ---------------------------------------------- | ------------------ |
| ERROR: The option -n or --name is required.    | 缺少必选参数name          |
| ERROR: Invalid name. The value must be 1-48 characters and contain only letters, digits, '.', ':', '-' or '_'. | name格式不合法 |
| ERROR: Invalid host_nqn. The value must be 1-68 characters. | host_nqn格式不合法 |
| ERROR: Invalid src_eid. The value must be 1-16 characters. | src_eid格式不合法 |
| ERROR: Invalid type. The value must be Linear or Striped. | type取值不合法 |
| ERROR: The option -d or --dev_name is required when --type is Linear. | Linear模式缺少dev_name |
| ERROR: Invalid dev_name. The value must be 1-32 characters and contain only letters, digits, '_', '-' or '.'. | dev_name格式不合法 |
| ERROR: The option --level or --chunk_size is only valid when --type is Striped. | Linear模式携带了条带参数 |
| ERROR: Internal error with error code \<code>. | 服务端内部错误，code为具体错误码 |
| ERROR: Deserialization failed in client.       | 客户端反序列化响应数据失败      |
| ERROR: Serialization failed in client.         | 客户端序列化请求数据失败       |

**示例**

```shell
$ ubsectl-ssu attach -t Linear -n alloc-space-1 -d ssu_linear0 -q nqn.2024-01:host -e e1
ns_dev_paths: /dev/nvme0n1,/dev/nvme1n1
dev_path: /dev/ssu_linear0
```

### 卸载线性编址的存储空间

**描述**

将线性聚合的块设备卸载并释放。

**用法**

```shell
ubsectl-ssu detach -t Linear -n <name> -d <dev_name> [-q <host_nqn>]
```

**输入参数**

| 参数             | 说明               | 取值                                         |
| -------------- | ---------------- | ------------------------------------------ |
| -t/--type      | **必选**，卸载类型      | 固定取值：`Linear`                             |
| -n/--name      | **必选**，分配空间标识名称 | 字符串，1-48字符，仅支持大小写字母、数字、`.`、`:`、`-`和`_` |
| -d/--dev\_name | **必选**，聚合设备名称    | 字符串，1-32字符，仅支持大小写字母、数字、`_`、`-`和`.` |
| -q/--host\_nqn | 可选参数，Host NQN | 字符串，1-68字符；attach时指定了host_nqn，则detach时必须指定相同host_nqn；attach时未指定，则detach时可以不指定 |

**约束限制**

1. ubsectl-ssu只能在root、ubse用户中运行
2. name必须对应已分配且已挂载的SSU存储空间
3. type必须使用`Linear`，不接受全小写格式
4. 必须指定dev_name，且与挂载时指定的聚合设备名称一致
5. 卸载前需确保没有进程正在使用该聚合设备
6. host_nqn必须与attach时传入的host_nqn一致；attach时未传入host_nqn，则detach时可以不传入host_nqn
7. 线性编址卸载不支持level和chunk_size参数

**输出信息说明**

执行成功无额外输出；执行失败返回错误信息。

**错误信息说明**

| 错误信息                                           | 说明                 |
| ---------------------------------------------- | ------------------ |
| ERROR: The option -n or --name is required.    | 缺少必选参数name          |
| ERROR: Invalid name. The value must be 1-48 characters and contain only letters, digits, '.', ':', '-' or '_'. | name格式不合法 |
| ERROR: Invalid host_nqn. The value must be 1-68 characters. | host_nqn格式不合法 |
| ERROR: Invalid type. The value must be Linear or Striped. | type取值不合法 |
| ERROR: The option -d or --dev_name is required when --type is Linear. | Linear模式缺少dev_name |
| ERROR: Invalid dev_name. The value must be 1-32 characters and contain only letters, digits, '_', '-' or '.'. | dev_name格式不合法 |
| ERROR: Internal error with error code \<code>. | 服务端内部错误，code为具体错误码 |
| ERROR: Deserialization failed in client.       | 客户端反序列化响应数据失败      |
| ERROR: Serialization failed in client.         | 客户端序列化请求数据失败       |

**示例**

```shell
$ ubsectl-ssu detach -t Linear -n alloc-space-1 -d ssu_linear0 -q nqn.2024-01:host
```

### 挂载条带化编址的存储空间

**描述**

将多个命名空间设备以条带化方式聚合为一个逻辑块设备并挂载，返回聚合设备路径和命名空间设备路径列表。

**用法**

```shell
ubsectl-ssu attach -t Striped -n <name> -d <dev_name> -l <level> -c <chunk_size> [-q <host_nqn>] [-e <src_eid>]
```

**输入参数**

| 参数                | 说明                | 取值                                                                 |
| ----------------- | ----------------- | ------------------------------------------------------------------ |
| -t/--type         | **必选**，挂载类型       | 固定取值：`Striped`                                                    |
| -n/--name         | **必选**，分配空间标识名称  | 字符串，1-48字符，仅支持大小写字母、数字、`.`、`:`、`-`和`_`                         |
| -d/--dev\_name    | **必选**，聚合设备名称     | 字符串，1-32字符，仅支持大小写字母、数字、`_`、`-`和`.`                                 |
| -l/--level        | **必选**，RAID级别     | 可选值：`raid0`、`raid5`                                                |
| -c/--chunk\_size  | **必选**，条带Chunk大小 | 可选值：`4K`、`16K`、`32K`、`64K`、`128K`、`256K`、`512K`                   |
| -q/--host\_nqn    | 可选参数，Host NQN  | 字符串，1-68字符；不指定时，使用创建namespace时默认分配的NQN                         |
| -e/--src\_eid     | 可选参数，源端EID     | 字符串，1-16字符；不指定时，使用host侧特定FE的EID                                |

**约束限制**

1. ubsectl-ssu只能在root、ubse用户中运行
2. name必须对应已分配的SSU存储空间
3. type必须使用`Striped`，不接受全小写格式
4. 必须指定dev_name、level和chunk_size
5. level必须使用`raid0`或`raid5`
6. chunk_size必须使用支持列表中的大写`K`格式，不接受小写`k`或裸数字

**输出信息说明**

| 字段名        | 字段描述       | 字段取值                 |
| ----------- | ---------- | ---------------------- |
| ns_dev_paths | 命名空间设备路径列表 | 字符串列表，字符串之间使用`,`分隔 |
| dev_path    | 聚合设备路径     | 字符串                  |

**错误信息说明**

| 错误信息                                           | 说明                 |
| ---------------------------------------------- | ------------------ |
| ERROR: The option -n or --name is required.    | 缺少必选参数name          |
| ERROR: Invalid name. The value must be 1-48 characters and contain only letters, digits, '.', ':', '-' or '_'. | name格式不合法 |
| ERROR: Invalid host_nqn. The value must be 1-68 characters. | host_nqn格式不合法 |
| ERROR: Invalid src_eid. The value must be 1-16 characters. | src_eid格式不合法 |
| ERROR: Invalid type. The value must be Linear or Striped. | type取值不合法 |
| ERROR: The option -d or --dev_name is required when --type is Striped. | Striped模式缺少dev_name |
| ERROR: Invalid dev_name. The value must be 1-32 characters and contain only letters, digits, '_', '-' or '.'. | dev_name格式不合法 |
| ERROR: The option -l or --level is required when --type is Striped. | Striped模式缺少level |
| ERROR: The option -c or --chunk_size is required when --type is Striped. | Striped模式缺少chunk_size |
| ERROR: Invalid level. The value must be raid0 or raid5. | level取值不合法 |
| ERROR: Invalid chunk_size. The value must be 4K, 16K, 32K, 64K, 128K, 256K or 512K. | chunk_size取值不合法 |
| ERROR: Internal error with error code \<code>. | 服务端内部错误，code为具体错误码 |
| ERROR: Deserialization failed in client.       | 客户端反序列化响应数据失败      |
| ERROR: Serialization failed in client.         | 客户端序列化请求数据失败       |

**示例**

```shell
$ ubsectl-ssu attach -t Striped -n alloc-space-2 -d ssu_striped0 -l raid0 -c 64K -q nqn.2024-01:host -e e1
ns_dev_paths: /dev/nvme0n1,/dev/nvme1n1
dev_path: /dev/ssu_striped0
```

### 卸载条带化编址的存储空间

**描述**

将条带化聚合的块设备卸载并释放。

**用法**

```shell
ubsectl-ssu detach -t Striped -n <name> -d <dev_name> [-q <host_nqn>]
```

**输入参数**

| 参数             | 说明               | 取值                                         |
| -------------- | ---------------- | ------------------------------------------ |
| -t/--type      | **必选**，卸载类型      | 固定取值：`Striped`                            |
| -n/--name      | **必选**，分配空间标识名称 | 字符串，1-48字符，仅支持大小写字母、数字、`.`、`:`、`-`和`_` |
| -d/--dev\_name | **必选**，聚合设备名称    | 字符串，1-32字符，仅支持大小写字母、数字、`_`、`-`和`.` |
| -q/--host\_nqn | 可选参数，Host NQN | 字符串，1-68字符；attach时指定了host_nqn，则detach时必须指定相同host_nqn；attach时未指定，则detach时可以不指定 |

**约束限制**

1. ubsectl-ssu只能在root、ubse用户中运行
2. name必须对应已分配且已挂载的SSU存储空间
3. type必须使用`Striped`，不接受全小写格式
4. 必须指定dev_name，且与挂载时指定的聚合设备名称一致
5. 卸载前需确保没有进程正在使用该聚合设备
6. host_nqn必须与attach时传入的host_nqn一致；attach时未传入host_nqn，则detach时可以不传入host_nqn
7. 条带化编址卸载不需要level和chunk_size参数

**输出信息说明**

执行成功无额外输出；执行失败返回错误信息。

**错误信息说明**

| 错误信息                                           | 说明                 |
| ---------------------------------------------- | ------------------ |
| ERROR: The option -n or --name is required.    | 缺少必选参数name          |
| ERROR: Invalid name. The value must be 1-48 characters and contain only letters, digits, '.', ':', '-' or '_'. | name格式不合法 |
| ERROR: Invalid host_nqn. The value must be 1-68 characters. | host_nqn格式不合法 |
| ERROR: Invalid type. The value must be Linear or Striped. | type取值不合法 |
| ERROR: The option -d or --dev_name is required when --type is Striped. | Striped模式缺少dev_name |
| ERROR: Invalid dev_name. The value must be 1-32 characters and contain only letters, digits, '_', '-' or '.'. | dev_name格式不合法 |
| ERROR: Internal error with error code \<code>. | 服务端内部错误，code为具体错误码 |
| ERROR: Deserialization failed in client.       | 客户端反序列化响应数据失败      |
| ERROR: Serialization failed in client.         | 客户端序列化请求数据失败       |

**示例**

```shell
$ ubsectl-ssu detach -t Striped -n alloc-space-2 -d ssu_striped0 -q nqn.2024-01:host
```
