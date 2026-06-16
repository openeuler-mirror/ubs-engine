# URMA

## 1. 提供查询URMA设备

- **用法**

```shell
ubsectl display urma [--node <node-id>] [--dev <urma_name>]
```

- **输入参数**

| 字段名           | 字段描述              | 字段取值                           |
|---------------|-------------------|--------------------------------|
| --node<br/>-n | 指定要查询的节点ID        | 1~max_id                       |
| --dev<br/>-d  | 设备名称              | 字符串列表，批量查询用逗号分隔，如urma_1,urma_2 |

- **输出信息说明**
支持分页

| 字段名       | 字段描述                     | 取值范围                  |
|-----------| ---------------------------- |-----------------------|
| urma-name | bonding设备的名称           | 字符串                   |
| dev-eid   | bonding设备的 Eid            | 字符串                   |
| dev1_name | bonding设备绑定的fe名称     | 字符串                   |
| dev2_name | bonding设备绑定的fe名称     | 字符串                   |
| dev1_eid  | bonding设备绑定的fe1 Eid    | 字符串                   |
| dev2_eid  | bonding设备绑定的fe2 Eid    | 字符串                   |
| status    | urma设备状态                 | 可选值： [ active \| inactive ] |

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

| 字段名           | 字段描述                     | 字段取值                                   |
|---------------|--------------------------|------------------------------------------|
| -p<br/>--pri  | 必选，优先级级别                 | 数字，可选值：0、1。支持逗号分隔列表，如 0,1              |
| -c<br/>--cir  | 必选，承诺信息速率（带宽）            | 非负整数，单位：Gbps。支持逗号分隔列表                   |

**约束限制**

1. ubsectl只能在root、ubse用户中运行
2. --pri和--cir参数必须同时提供，且数量必须匹配
3. 优先级配置数量最多为2组（即最多两个优先级）
4. 不允许配置重复的优先级
5. CLOS组网模式不支持-n参数
6. 不支持修改，需执行`ubsectl delete urma-qos`删除后重新创建

**输出信息说明**

执行成功返回：`create successfully`

**错误信息说明**

| 错误信息 | 说明 |
|---------|------|
| ERROR: CLOS networking does not support -n option. | CLOS组网模式不支持指定节点 |
| ERROR: Both --pri and --cir are required. | --pri和--cir参数必须同时提供 |
| ERROR: Invalid priority or bandwidth value. | 优先级或带宽值无效 |
| ERROR: Priority must be 0 or 1. | 优先级必须是0或1 |
| ERROR: --pri and --cir count mismatch. | --pri和--cir参数数量不匹配 |
| ERROR: QoS config count exceeds limit (max 2). | QoS配置数量超过限制（最多2组） |
| ERROR: Duplicate priorities are not allowed. | 不允许配置重复的优先级 |
| ERROR: Failed to access UBM interface, please check UBM service status. | 无法访问UBM接口，请检查UBM服务状态 |
| ERROR: ETS QoS priority group already exists, please delete existing QoS config first. | QoS配置已存在，请先删除现有配置 |
| ERROR: Failed to create ETS QoS template, No ETS QoS template exists. | 创建ETS QoS模板失败，不存在ETS QoS模板 |

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

| 错误信息 | 说明 |
|---------|------|
| ERROR: CLOS networking does not support -n option. | CLOS组网模式不支持指定节点 |
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

| 字段名      | 字段描述       | 字段取值      |
|----------|------------|-----------|
| priority | 优先级级别      | 数字：0 或 1  |
| bandwidth(Gbps) | 分配的带宽值     | 非负整数，单位：Gbps |

**错误信息说明**

| 错误信息 | 说明 |
|---------|------|
| ERROR: CLOS networking does not support -n option. | CLOS组网模式不支持指定节点 |
| ERROR: No ETS QoS priority groups has been created, please run: ubsectl create urma-qos. | 未创建ETS QoS优先级组，请先执行创建命令 |
| ERROR: Failed to access UBM interface, please check UBM service status. | 无法访问UBM接口，请检查UBM服务状态 |
| ERROR: Failed to create ETS QoS template, No ETS QoS template exists. | 创建ETS QoS模板失败，不存在ETS QoS模板 |

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