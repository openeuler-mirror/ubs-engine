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
