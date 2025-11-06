## 1. 检查各节点内存池化功能健康状态

### 描述

检查各节点内存池化功能健康状态

### 用法

```shell
ubsectl check memory
```

### 输入参数

无

### 约束限制

ubsectl只能在root，ubse用户中运行

### 输出信息说明

| 字段名 | 字段描述                                                     | 取值范围          |
| ------ | ------------------------------------------------------------ | ----------------- |
| node   | 节点信息。例：computer1(1)<br/>节点信息由2部分组成：<br/>1.括号前部分：主机名<br/>2.括号内部分：节点的槽位号 | 字符串            |
| status | 查询集群内所有节点的内存借用功能健康状态                     | 可选值：[ok\|nok] |

### 示例

#### 场景1：节点重启中，备节点无法连上master节点

- master节点

``` 
$ ubsectl check memory
------------------------------
node                  status  
------------------------------      
computer01(1)         ok        
-(2)                  nok
------------------------------
```

- 备节点

``` 
$ ubsectl check memory
ERROR: Failed to obtain memory information
```

#### 场景2：节点启动稳定后

- master节点

``` 
$ ubsectl check memory
------------------------------
node                  status  
------------------------------      
computer01(1)         ok        
computer02(2)         ok
------------------------------

```

- 备节点

``` 
$ ubsectl check memory
------------------------------
node                  status  
------------------------------      
computer01(1)         ok        
computer02(2)         ok
------------------------------

```

#### 场景3：只有主节点启动，备节点不启动

- master节点

``` 
$ ubsectl check memory
------------------------------
node                  status  
------------------------------      
computer01(1)         ok        
-(2)                  nok
------------------------------

```

- 备节点

``` 
$ ubsectl check memory
ERROR: Internal error with error code xx

```

#### 场景4：稳定后备节点停掉

- master节点

``` 
$ ubsectl check memory
------------------------------
node                  status  
------------------------------      
computer01(1)         ok        
-(2)                  nok
------------------------------

```

- 备节点

``` 
$ ubsectl check memory
ERROR: Internal error with error code xx

```

#### 场景5：稳定后主节点停掉

- master节点

``` 
$ ubsectl check memory
------------------------------
node                  status  
------------------------------      
-(1)                   nok        
computer02(2)          ok
------------------------------

```

- 备节点

``` 
$ ubsectl check memory
ERROR: Internal error with error code xx

```



## 2. 查询各节点内存借入信息

### 描述

查询当前集群中，各节点内存借入信息

### 用法

```shell
ubsectl display memory -t [OPTIONS]
```

### 输入参数

| 参数      | 参数说明     | 取值                                                |
| --------- | ------------ | --------------------------------------------------- |
| [OPTIONS] | 节点内存借用 | node_borrow: 以节点为粒度，展示各节点的内存借入信息 |

### 约束限制

只显示集群中在位的节点，如果脱离集群或未启动，则不显示

ubsectl只能在root，ubse用户中运行，可管理所有内存资源

### 输出信息说明

- 显示信息中的字段说明：

| 字段名  | 字段描述                                                     | 字段取值 |
| ------ | ------------------------------------------------------------ | -------- |
| node   | 借入节点信息。例：computer1(1)<br>节点信息由2部分组成：<br>1.括号前部分：主机名<br>2.括号内部分：节点的槽位号    | 字符串   |
| from   | 借出节点信息。例：computer1(1)<br/>节点信息由2部分组成：<br/>1.括号前部分：主机名<br/>2.括号内部分：节点的槽位号 | 字符串   |
| size   | 借用内存总量，单位: MB                                       | 整数     |

### 示例

```
$ ubsectl display memory -t node_borrow
--------------------------------------------
node          from             size   
--------------------------------------------
node-1(1)     node-2(2)        450   
node-1(1)     node-3(3)        450   
node-4(4)     node-3(3)        100   
--------------------------------------------
```



## 3. 查询内存借用账本详细信息

### 描述

查询当前集群中，所有内存借用账本详细信息

### 用法

```shell
ubsectl display memory -t [OPTIONS]
```

### 输入参数

| 参数      | 参数说明     | 取值                               |
| --------- | ----------- | ---------------------------------- |
| [OPTIONS] | 节点内存借用 | borrow_detail: 基于分类的内存账本详细信息|

### 约束限制

只显示集群中在位的节点，如果脱离集群或未启动，则不显示

ubsectl只能在root，ubse用户中运行，可管理所有内存资源

### 输出信息说明

- 显示信息中的字段说明：

| 字段名       | 字段描述                                                     | 字段取值                                                      |
| ----------- | ------------------------------------------------------------ | ------------------------------------------------------------ |
| name        | 一次内存借用的名称                                             | 字符串                                                       |
| type        | 内存借用的类型                                                | 可选值：numa、fd                                              |
| borrow_node | 借入节点信息。例：computer1(1)<br>节点信息由2部分组成：<br>1.括号前部分：主机名<br>2.括号内部分：节点的槽位号 | 字符串              |
| borrow_size | 借入内存总量，单位: MB                                       | 整数                                                           |
| lend_node   | 借出节点信息。例：computer1(1)<br/>节点信息由2部分组成：<br/>1.括号前部分：主机名<br/>2.括号内部分：节点的槽位号 | 字符串           |
| lend_size   | 借用内存总量，单位: MB                                       | 整数                                                           |
| status      | 借用状态                                                    | 可选值：<br>done：借用完成<br>exporting：借出节点正在执行导出<br>importing：代入节点正在执行导入<br>unexporting：借出节点正在取消导出<br>unimporting：代入节点正在取消导入<br>fault：当次借用请求出现故障 |

### 示例

```
$ ubsectl display memory -t borrow_detail
------------------------------------------------------------------------------------
name         type    borrow_node   borrow_size   lend_node   lend_size       status
------------------------------------------------------------------------------------
memory1      numa    node-1(1)         100        node-3(3)   100            done
memory2      shm     node-1(1)         200        node-3(3)   200            done
memory2      shm     node-2(2)         200        node-3(3)   200            done
memory3      shm                       200        node-3(3)   200            done
memory4      numa    node-2(2)         300        node-3(3)   300            exporting
memory5      fd      node-1(1)         400        node-3(3)   400            importing
memory6      numa                      300        node-3(3)   300            unexporting
memory7      fd      node-1(1)         400        node-3(3)   400            unimporting
memory8      numa    node-2(2)         500                    500            fault
memory9      fd                        500        node-3(3)   500            fault
------------------------------------------------------------------------------------
```
