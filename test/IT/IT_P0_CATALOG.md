# IT P0 用例清单

> P0 = 接口自身正确性：功能通不通、字段对不对、异常处不处理、边界值对不对。

---

## 一、场景一览

| 场景目录 | 场景类 | 节点 | 组网 |
|----------|--------|------|------|
| `tongsuan_1d_single_node` | `Tongsuan1dFullMeshSingleNodeScenario` | 1 | CLOS (MeshType=8) |
| `tongsuan_1d_two_nodes` | `Tongsuan1dFullMeshTwoNodesScenario` | 2 | CLOS |
| `tongsuan_1d_four_nodes` | `Tongsuan1dFullMeshFourNodesScenario` | 4 | CLOS |
| `zhisuan_npu_single_node` | `ZhisuanNpuSingleNodeScenario` | 1 | 智算 NPU |

---

## 二、SDK 接口 P0 用例

### 2.1 Client

#### ubs_engine_client_initialize

```
int32_t ubs_engine_client_initialize(const char* ubs_engine_uds_path)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 入参 | `ubs_engine_uds_path` | UDS 路径，NULL 走默认，长度 ≤ 107 |
| 返回 | `int32_t` | 错误码 |

| 编号 | 用例名 | 场景 | 入参 | 预期 |
|------|--------|------|------|------|
| P0-Init-Ok-01 | 默认路径初始化 | 单节点 | path=NULL | 返回 `UBS_SUCCESS` |
| P0-Init-Ok-02 | 指定路径初始化 | 单节点 | path=有效UDS | 返回 `UBS_SUCCESS` |
| P0-Init-OverLen-01 | 路径超长 | 单节点 | path长度>107 | `OUT_OF_RANGE` |
| P0-Init-BadParam-01 | 路径不存在 | 单节点 | 无效路径 | init 返回 SUCCESS（延迟连接），后续 SDK 调用连接失败 |

#### ubs_engine_client_finalize

```
void ubs_engine_client_finalize(void)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 入参 | — | 无 |
| 返回 | `void` | — |

| 编号 | 用例名 | 场景 | 入参 | 预期 |
|------|--------|------|------|------|
| P0-Fin-Ok-01 | 正常销毁 | 单节点 | — | 调用不崩溃，之后 SDK 接口返回未初始化错误 |

---

### 2.2 Topo

#### ubs_topo_node_list

```
int32_t ubs_topo_node_list(ubs_topo_node_t** node_list, uint32_t* node_cnt)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 入参 | — | 无 |
| 出参 | `node_list` | `ubs_topo_node_t**`，接口内 malloc，调用方释放 |
| 出参 | `node_cnt` | `uint32_t*`，填充节点数量 |
| 返回 | `int32_t` | 错误码 |

**ubs_topo_node_t** (出参结构体):

| 字段 | 类型 | 说明 |
|------|------|------|
| `slot_id` | `uint32_t` | 节点唯一标识 |
| `host_name` | `char[]` | 主机名 |
| `socket_id[2]` | `uint32_t[2]` | socket id 数组 |
| `numa_ids[2][4]` | `uint32_t[2][4]` | 每 socket 下的 NUMA id |
| `ips[50]` | `ubs_topo_ip_address_t[50]` | IP 地址列表 (af/ipv4/ipv6) |

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-NodeList-Ok-01 | 单节点查询 | 单节点 | `*node_cnt==1`; `slot_id>0`; `host_name` 非空; `socket_id`/`numa_ids` 与 LCNE 一致; `ips` 中至少有一个合法 IP（IPv4: `s_addr != 0`，IPv6: 非全零） | `UBS_SUCCESS` |
| P0-NodeList-Ok-02 | 多节点查询 | 双/四节点 | `*node_cnt==N`; 各节点 `slot_id` 互不相同; `socket_id`/`numa_ids` 与 LCNE 一致，`ips` 中至少有一个合法 IP（IPv4: `s_addr != 0`，IPv6: 非全零） | `UBS_SUCCESS` |
| P0-NodeList-NullPtr-01 | 空指针 | 单节点 | node_list=NULL | `NULL_POINTER` |

#### ubs_topo_node_local_get

```
int32_t ubs_topo_node_local_get(ubs_topo_node_t* node)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 入参 | — | 无 |
| 出参 | `node` | `ubs_topo_node_t*`，调用方分配，接口填充 |
| 返回 | `int32_t` | 错误码 |

出参结构体同 `ubs_topo_node_t`，额外要求 `slot_id == 本节点`。

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-LocalGet-Ok-01 | 查询本节点 | 单/四节点 | `slot_id`/`host_name`/`socket_id` 与 LCNE 一致 | `UBS_SUCCESS` |
| P0-LocalGet-NullPtr-01 | 空指针 | 单节点 | node=NULL | `NULL_POINTER` |

#### ubs_topo_link_list

```
int32_t ubs_topo_link_list(ubs_topo_link_t** cpu_links, uint32_t* cpu_link_cnt)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 入参 | — | 无 |
| 出参 | `cpu_links` | `ubs_topo_link_t**`，接口内 malloc，调用方释放 |
| 出参 | `cpu_link_cnt` | `uint32_t*`，填充链路数量 |
| 返回 | `int32_t` | 错误码 |

**ubs_topo_link_t** (出参结构体):

| 字段 | 类型 | 说明 |
|------|------|------|
| `slot_id` | `uint32_t` | 本端节点 |
| `socket_id` | `uint32_t` | 本端 socket |
| `port_id` | `uint32_t` | 本端端口 |
| `peer_slot_id` | `uint32_t` | 对端节点 |
| `peer_socket_id` | `uint32_t` | 对端 socket |
| `peer_port_id` | `uint32_t` | 对端端口 |

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-LinkList-Ok-01 | 链路查询 | 双/四节点 | `*cpu_link_cnt>0`; `slot_id`/`peer_slot_id` 有效; 与 LCNE 一致 | `UBS_SUCCESS` |
| P0-LinkList-Fld-01 | 字段校验 | 双节点 | `socket_id`/`port_id`/`peer_socket_id`/`peer_port_id` 非空 | `UBS_SUCCESS` |
| P0-LinkList-NullPtr-01 | 空指针 | 双节点 | cpu_links=NULL | `NULL_POINTER` |

---

### 2.3 Mem FD

#### ubs_mem_fd_create

```
int32_t ubs_mem_fd_create(const char* name, uint64_t size, const ubs_mem_fd_owner_t* owner,
                          mode_t mode, ubs_mem_distance_t distance, ubs_mem_fd_desc_t* fd_desc)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 入参 | `name` | 借用标识，长度 < 48，仅字母数字 `.:-_` |
| 入参 | `size` | 借用大小，≥ 4MB |
| 入参 | `owner` | 属主信息 (uid/gid/pid)，可为 NULL |
| 入参 | `mode` | 权限位 |
| 入参 | `distance` | 距离级别 L0/L1/L2 |
| 出参 | `fd_desc` | `ubs_mem_fd_desc_t*`，调用方分配 |

**ubs_mem_fd_desc_t**:

| 字段 | 类型 | 校验方式 |
|------|------|----------|
| `name[48]` | `char[48]` | strcmp == 输入 name |
| `memid_cnt` | `uint32_t` | == ceil(mem_size / unit_size) |
| `memids[2048]` | `uint64_t[2048]` | 非空 |
| `mem_size` | `uint64_t` | == 输入 size |
| `unit_size` | `size_t` | > 0 |
| `export_node` | `ubs_topo_node_t` | slot_id 有效且 ≠ import_node.slot_id（ips 字段无效） |
| `import_node` | `ubs_topo_node_t` | slot_id == 本节点 |
| `mem_stage` | `ubs_mem_stage` | ∈ {UBSE_CREATING(1), UBSE_EXIST(3)} |

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-FdCreate-Ok-01 | 标准创建 | 双/四节点 | owner=NULL, mode=0, distance=L0, size=129MB; `mem_stage∈{CREATING,EXIST}`, `mem_size==129MB`, `memid_cnt==ceil(mem_size/unit_size)`, `memids` 非零, `unit_size>0`, `import_node.slot_id==本节点`, `export_node.slot_id>0`, `export_node.slot_id≠import_node.slot_id` | `UBS_SUCCESS` |
| P0-FdCreate-OverLen-01 | name 超长 | 双节点 | name ≥ 48 | `UBS_ERR_INVALID_ARG` |
| P0-FdCreate-InvalidVal-01 | size < 4MB | 双节点 | size=1MB | `UBS_ENGINE_ERR_OUT_OF_RANGE` |
| P0-FdCreate-InvalidVal-02 | size > 256GB | 双/四节点 | size=257GB; 超过 `UBS_MEM_MAX_MEMID_NUM * 128MB` | `UBS_ENGINE_ERR_ALLOCATE` |
| P0-FdCreate-NullPtr-01 | 空指针 | 双节点 | name=NULL 或 fd_desc=NULL | `UBS_ERR_NULL_POINTER` |
| P0-FdCreate-Dup-01 | 同名重复 | 双节点 | 同名再调一次 | `UBS_ENGINE_ERR_EXISTED` |
| P0-FdCreate-BoundMin-01 | size=4MB | 双节点 | size=4MB; `mem_stage∈{CREATING,EXIST}`, `mem_size==4MB`, `memid_cnt==ceil(mem_size/unit_size)`, `import_node.slot_id==本节点` | `UBS_SUCCESS` |
| P0-FdCreate-BoundMax-01 | name=47字节 | 双节点 | name=47 字节; `mem_stage∈{CREATING,EXIST}`, `name==输入`, `import_node.slot_id==本节点` | `UBS_SUCCESS` |

#### ubs_mem_fd_create_with_lender

```
int32_t ubs_mem_fd_create_with_lender(const char* name, const ubs_mem_fd_owner_t* owner,
    mode_t mode, const ubs_mem_lender_t* lender, uint32_t lender_cnt, ubs_mem_fd_desc_t* fd_desc)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 入参 | `name` | 借用标识 |
| 入参 | `owner` | 属主，可为 NULL |
| 入参 | `mode` | 权限位 |
| 入参 | `lender` | `ubs_mem_lender_t*`，借出方描述数组，字段见下表 |
| 入参 | `lender_cnt` | 借出方数量，1~4 |
| 出参 | `fd_desc` | `ubs_mem_fd_desc_t*`，字段校验同 ubs_mem_fd_create，额外要求 `export_node.slot_id == lender.slot_id` |

**`ubs_mem_lender_t` 字段说明**

| 字段 | 类型 | 说明 |
|------|------|------|
| `lender_size` | `uint64_t` | 借出内存大小，≥ 4MB |
| `slot_id` | `uint32_t` | 借出节点 slot_id |
| `socket_id` | `uint32_t` | 借出节点 socket_id，通过 TopoLinkList 获取 peer_socket_id |
| `numa_id` | `uint32_t` | 借出节点 numa_id，通过 TopoNodeList 匹配 socket 后取 numa_ids |
| `port_id` | `uint32_t` | 借出链路 port_id，通过 TopoLinkList 获取 peer_port_id |

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-FdCreateLender-Ok-01 | 指定借出节点 | 双/四节点 | owner=NULL, mode=0, lender={slot_id=非本节点, socket_id/numa_id/port_id由topo获取, size=129MB}, lender_cnt=1; `mem_stage∈{CREATING,EXIST}`, `mem_size==129MB`, `memid_cnt==ceil(mem_size/unit_size)`, `memids` 非零, `unit_size>0`, `export_node.slot_id==lender.slot_id`, `import_node.slot_id==本节点`, `export_node.slot_id≠import_node.slot_id` | `UBS_SUCCESS` |
| P0-FdCreateLender-OverLen-01 | name 超长 | 双节点 | name ≥ 48 | `UBS_ERR_INVALID_ARG` |
| P0-FdCreateLender-InvalidVal-01 | lender_size < 4MB | 双节点 | lender.lender_size=1 | `UBS_ENGINE_ERR_OUT_OF_RANGE` |
| P0-FdCreateLender-InvalidVal-02 | lender_size > 256GB | 双/四节点 | lender.lender_size=257GB | `UBS_ENGINE_ERR_ALLOCATE` |
| P0-FdCreateLender-NullPtr-01 | lender=NULL | 双节点 | lender=NULL, lender_cnt=1 | `UBS_ERR_NULL_POINTER` |
| P0-FdCreateLender-NullPtr-02 | name/fd_desc=NULL | 双节点 | name=NULL 或 fd_desc=NULL | `UBS_ERR_NULL_POINTER` |
| P0-FdCreateLender-BadParam-01 | 不存在的 slot_id | 双节点 | lender.slot_id=999 | `UBS_ENGINE_ERR_ALLOCATE` |
| P0-FdCreateLender-Dup-01 | 同名重复 | 双节点 | 同名再调 | `UBS_ENGINE_ERR_EXISTED` |

#### ubs_mem_fd_create_with_candidate

```
int32_t ubs_mem_fd_create_with_candidate(const char* name, uint64_t size,
    const ubs_mem_fd_owner_t* owner, mode_t mode, const uint32_t* slot_ids, uint32_t slot_cnt,
    ubs_mem_fd_desc_t* fd_desc)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 入参 | `name` | 借用标识 |
| 入参 | `size` | 借用大小 |
| 入参 | `owner` | 属主，可为 NULL |
| 入参 | `mode` | 权限位 |
| 入参 | `slot_ids` | 候选节点 slot_id 数组 |
| 入参 | `slot_cnt` | 候选节点数量 |
| 出参 | `fd_desc` | `ubs_mem_fd_desc_t*`，额外要求 `export_node.slot_id ∈ slot_ids[]` |

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-FdCreateCandidate-Ok-01 | 指定候选节点 | 双/四节点 | owner=NULL, mode=0, size=129MB, slot_ids={双:2 / 四:3,4}; `mem_stage∈{CREATING,EXIST}`, `mem_size==129MB`, `memid_cnt==ceil(mem_size/unit_size)`, `memids` 非零, `unit_size>0`, `export_node.slot_id∈slot_ids`, `import_node.slot_id==本节点`, `export_node.slot_id≠import_node.slot_id` | `UBS_SUCCESS` |
| P0-FdCreateCandidate-OverLen-01 | name 超长 | 双节点 | name ≥ 48 | `UBS_ERR_INVALID_ARG` |
| P0-FdCreateCandidate-InvalidVal-01 | size < 4MB | 双节点 | size=1 | `UBS_ENGINE_ERR_OUT_OF_RANGE` |
| P0-FdCreateCandidate-InvalidVal-02 | size > 256GB | 双/四节点 | size=257GB | `UBS_ENGINE_ERR_ALLOCATE` |
| P0-FdCreateCandidate-NullPtr-01 | 空指针 | 双节点 | name=NULL 或 fd_desc=NULL | `UBS_ERR_NULL_POINTER` |
| P0-FdCreateCandidate-BadParam-01 | 不存在的 slot_id | 双节点 | slot_ids={999} | `UBS_ENGINE_ERR_ALLOCATE` |
| P0-FdCreateCandidate-Dup-01 | 同名重复 | 双/四节点 | 同名再调；四节点候选为节点3和4 | `UBS_ENGINE_ERR_EXISTED` |

#### ubs_mem_fd_permission

```
int32_t ubs_mem_fd_permission(const char* name, const ubs_mem_fd_owner_t* owner, mode_t mode)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 入参 | `name` | 借用标识 |
| 入参 | `owner` | 新属主信息 (uid/gid/pid) |
| 入参 | `mode` | 新权限位 |
| 返回 | `int32_t` | 错误码 |

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-FdPerm-NotExist-01 | name 不存在 | 双节点 | 不存在的 name | `UBS_ENGINE_ERR_NOT_EXIST` |


#### ubs_mem_fd_get

```
int32_t ubs_mem_fd_get(const char* name, ubs_mem_fd_desc_t* fd_desc)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 入参 | `name` | 借用标识 |
| 出参 | `fd_desc` | `ubs_mem_fd_desc_t*`，字段同上 `fd_create` 出参 |

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-FdGet-NotExist-01 | 查询不存在 | 双节点 | 不存在的 name | `UBS_ENGINE_ERR_NOT_EXIST` |
| P0-FdGet-NullPtr-01 | 空指针 | 双节点 | fd_desc=NULL | `UBS_ERR_NULL_POINTER` |

#### ubs_mem_fd_list

```
int32_t ubs_mem_fd_list(ubs_mem_fd_desc_t** descs, uint32_t* fd_cnt)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 出参 | `descs` | `ubs_mem_fd_desc_t[]`，接口内 malloc |
| 出参 | `fd_cnt` | `uint32_t*`，fd 数量 |

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-FdList-Ok-01 | 空/有 fd 时 list + 字段校验 | 双节点 | ① 空 list 验证接口可达；② 创建2个FD后 list，逐条校验 `mem_stage∈{CREATING,EXIST}`、`mem_size`、`memid_cnt==ceil(mem_size/unit_size)`、`unit_size>0`、`import_node.slot_id==本节点`、`export_node.slot_id>0`、`export≠import`；③ 验证创建的FD均在列表中且 `mem_size` 一致 | `UBS_SUCCESS`; 空/有均可调通；`fd_cnt≥2`; 字段正确 |
| P0-FdList-NullPtr-01 | 空指针 | 双节点 | descs=NULL | `UBS_ERR_NULL_POINTER` |

#### ubs_mem_fd_delete

```
int32_t ubs_mem_fd_delete(const char* name)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 入参 | `name` | 借用标识 |
| 返回 | `int32_t` | 错误码 |

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-FdDel-Ok-01 | 创建后删除 | 双节点 | 正常 name | `UBS_SUCCESS`; 删除后 get 返回不存在 |
| P0-FdDel-NotExist-01 | 删除不存在 | 双节点 | 不存在的 name | `UBS_ENGINE_ERR_NOT_EXIST` |
| P0-FdDel-Dup-01 | 重复删除 | 双节点 | 同名再删 | `UBS_ENGINE_ERR_NOT_EXIST` |
| P0-FdDel-OverLen-01 | name 超长 | 双节点 | name ≥ 48 | `UBS_ERR_INVALID_ARG` |

#### ubs_mem_fd_get_memid_by_import

```
int32_t ubs_mem_fd_get_memid_by_import(const char* name, uint64_t import_memid,
                                       ubs_mem_export_memid_t* export_memid)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 入参 | `name` | 借用标识 |
| 入参 | `import_memid` | 导入端 memid |
| 出参 | `export_memid` | `ubs_mem_export_memid_t*` |

**ubs_mem_export_memid_t**:

| 字段 | 类型 | 说明 |
|------|------|------|
| `export_slot_id` | `uint32_t` | 导出节点 slot_id |
| `export_memid` | `uint64_t` | 导出端内存块标识 |

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-FdMemidByImport-Fld-01 | 创建后查询 | 双节点 | 正常 name+import_memid; `export_slot_id>0`、`export_slot_id==创建时export_node.slot_id`、`export_memid>0` | `UBS_SUCCESS` |
| P0-FdMemidByImport-NotExist-01 | name 不存在 | 双节点 | 不存在的 name | `UBS_ENGINE_ERR_NOT_EXIST` |

#### ubs_mem_fd_fault_register

```
int32_t ubs_mem_fd_fault_register(ubs_mem_fd_fault_cb_t handler)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 入参 | `handler` | 故障回调函数指针，可为 NULL |
| 返回 | `int32_t` | 错误码 |

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-FdFaultReg-NullPtr-01 | NULL handler | 单节点 | handler=NULL | 不崩溃 |

---

### 2.4 Mem NUMA

#### ubs_mem_numastat_get

```
int32_t ubs_mem_numastat_get(uint32_t slot_id, ubs_mem_numastat_t** numa_mems, uint32_t* numa_mem_cnt)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 入参 | `slot_id` | 目标节点 slot_id |
| 出参 | `numa_mems` | `ubs_mem_numastat_t[]`，接口内 malloc，调用方需 free |
| 出参 | `numa_mem_cnt` | `uint32_t*`，numa 信息数量，范围 [0, UBS_TOPO_NUMA_NUM] |

**ubs_mem_numastat_t**:

| 字段 | 类型 | 校验方式 |
|------|------|----------|
| `slot_id` | `uint32_t` | == 入参 |
| `socket_id` | `uint32_t` | 有效值 |
| `numa_id` | `uint32_t` | 有效值 |
| `numa_type` | `ubs_mem_numa_type_t` | NUMA_LOCAL / NUMA_REMOTE |
| `mem_lend_ratio` | `uint32_t` | 0~100 |
| `mem_total` | `uint64_t` | > 0 |
| `mem_free` | `uint64_t` | ≤ mem_total |
| `huge_pages_2M` | `uint32_t` | ≥ 0 |
| `free_huge_pages_2M` | `uint32_t` | ≤ huge_pages_2M |
| `huge_pages_1G` | `uint32_t` | ≥ 0 |
| `free_huge_pages_1G` | `uint32_t` | ≤ huge_pages_1G |
| `mem_borrow` | `uint64_t` | ≥ 0 |
| `mem_lend` | `uint64_t` | ≥ 0 |

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-NumaStatGet-Fld-01 | 字段校验 | 单节点 | `slot_id=本节点`; 出参 `numa_mems!=NULL`、`numa_mem_cnt>0`; 逐条校验 `slot_id==入参`、`socket_id`有效、`numa_id`有效、`numa_type∈{NUMA_LOCAL,NUMA_REMOTE}`、`mem_lend_ratio∈[0,100]`、`mem_total>0`、`mem_free≤mem_total`、`free_huge_pages_2M≤huge_pages_2M`、`free_huge_pages_1G≤huge_pages_1G`、`mem_borrow≥0`、`mem_lend≥0` | `UBS_SUCCESS` |
| P0-NumaStatGet-NotExist-01 | 不存在的 slot_id | 单节点 | `slot_id=9999` | `UBS_ENGINE_ERR_NODE_NOT_EXISTS` |
| P0-NumaStatGet-NullPtr-01 | 空指针 | 单节点 | `numa_mems=NULL`; `numa_mem_cnt=NULL` | `UBS_ERR_NULL_POINTER` |

#### ubs_mem_numa_create

```
int32_t ubs_mem_numa_create(const char* name, uint64_t size, ubs_mem_distance_t distance,
                            ubs_mem_numa_desc_t* numa_desc)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 入参 | `name` | 借用标识，长度 < 48 |
| 入参 | `size` | 借用大小，≥ 4MB |
| 入参 | `distance` | 距离级别 |
| 出参 | `numa_desc` | `ubs_mem_numa_desc_t*` |

**ubs_mem_numa_desc_t**:

| 字段 | 类型 | 校验方式 |
|------|------|----------|
| `name[48]` | `char[48]` | strcmp == 输入 name |
| `numaid` | `int64_t` | ≥ 0 |
| `export_node` | `ubs_topo_node_t` | slot_id 有效且 ≠ import_node.slot_id（ips 字段无效） |
| `import_node` | `ubs_topo_node_t` | slot_id == 本节点 |
| `size` | `uint64_t` | == 输入 size |
| `mem_stage` | `ubs_mem_stage` | ∈ {UBSE_CREATING(1), UBSE_EXIST(3)} |
| `usrInfo[32]` | `uint8_t[32]` | — |

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-NumaCreate-Ok-01 | 标准创建 | 双/四节点 | distance=L0, size=129MB; `mem_stage∈{CREATING,EXIST}`, `size==129MB`, `numaid≥0`, `import_node.slot_id==本节点`, `export_node.slot_id>0`, `export_node.slot_id≠import_node.slot_id`, `name==输入` | `UBS_SUCCESS` |
| P0-NumaCreate-OverLen-01 | name 超长 | 双节点 | name ≥ 48 | `UBS_ERR_INVALID_ARG` |
| P0-NumaCreate-InvalidVal-01 | size < 4MB | 双节点 | size=1 | `UBS_ENGINE_ERR_OUT_OF_RANGE` |
| P0-NumaCreate-Dup-01 | 同名重复 | 双节点 | 同名再调 | `UBS_ENGINE_ERR_EXISTED` |
| P0-NumaCreate-NullPtr-01 | 空指针 | 双节点 | name=NULL 或 numa_desc=NULL | `UBS_ERR_NULL_POINTER` |
| P0-NumaCreate-BoundMin-01 | size=4MB | 双节点 | size=4MB; `mem_stage∈{CREATING,EXIST}`, `size==4MB`, `import_node.slot_id==本节点` | `UBS_SUCCESS` |
| P0-NumaCreate-BoundMax-01 | name=47字节 | 双节点 | name=47 字节; `mem_stage∈{CREATING,EXIST}`, `name==输入`, `import_node.slot_id==本节点` | `UBS_SUCCESS` |

#### ubs_mem_numa_create_with_lender

```
int32_t ubs_mem_numa_create_with_lender(const char* name, const ubs_mem_lender_t* lender,
    uint32_t lender_cnt, ubs_mem_numa_desc_t* numa_desc)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 入参 | `name` | 借用标识，长度 < 48 |
| 入参 | `lender` | `ubs_mem_lender_t*`，借出方描述数组，字段见下表 |
| 入参 | `lender_cnt` | 借出方数量，1~4 |
| 出参 | `numa_desc` | `ubs_mem_numa_desc_t*`，额外要求 `export_node.slot_id == lender.slot_id`，`export_node.socket_id 包含 lender.socket_id` |

**`ubs_mem_lender_t` 字段说明**

| 字段 | 类型 | 说明 |
|------|------|------|
| `lender_size` | `uint64_t` | 借出内存大小，≥ 4MB |
| `slot_id` | `uint32_t` | 借出节点 slot_id |
| `socket_id` | `uint32_t` | 借出节点 socket_id，通过 TopoLinkList 获取 peer_socket_id |
| `numa_id` | `uint32_t` | 借出节点 numa_id，通过 TopoNodeList 匹配 socket 后取 numa_ids |
| `port_id` | `uint32_t` | 借出链路 port_id，通过 TopoLinkList 获取 peer_port_id |

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-NumaCreateLender-Ok-01 | 指定借出节点 | 双/四节点 | lender={slot_id=非本节点, socket_id/numa_id/port_id由topo获取, size=129MB}, lender_cnt=1; `mem_stage∈{CREATING,EXIST}`, `size==129MB`, `numaid≥0`, `export_node.slot_id==lender.slot_id`, `export_node.socket_id包含lender.socket_id`, `import_node.slot_id==本节点`, `export_node.slot_id≠import_node.slot_id`, `name==输入` | `UBS_SUCCESS` |
| P0-NumaCreateLender-OverLen-01 | name 超长 | 双节点 | name ≥ 48 | `UBS_ERR_INVALID_ARG` |
| P0-NumaCreateLender-InvalidVal-01 | lender_size < 4MB | 双节点 | lender.lender_size=1 | `UBS_ENGINE_ERR_OUT_OF_RANGE` |
| P0-NumaCreateLender-NullPtr-01 | lender=NULL | 双节点 | lender=NULL, lender_cnt=1 | `UBS_ERR_NULL_POINTER` |
| P0-NumaCreateLender-NullPtr-02 | name/numa_desc=NULL | 双节点 | name=NULL 或 numa_desc=NULL | `UBS_ERR_NULL_POINTER` |
| P0-NumaCreateLender-BadParam-01 | 不存在的 slot_id | 双节点 | lender.slot_id=999 | `UBS_ENGINE_ERR_LINK_NOT_EXIST` |
| P0-NumaCreateLender-Dup-01 | 同名重复 | 双节点 | 同名再调 | `UBS_ENGINE_ERR_EXISTED` |
| P0-NumaCreateLender-BoundMax-01 | lender_cnt=4 | 双节点 | lender_cnt=4; `export_node.slot_id==lender.slot_id`, `import_node.slot_id==本节点` | `UBS_SUCCESS` |

#### ubs_mem_numa_create_with_candidate

```
int32_t ubs_mem_numa_create_with_candidate(const char* name, uint64_t size,
    const uint32_t* slot_ids, uint32_t slot_cnt, ubs_mem_numa_desc_t* numa_desc)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 入参 | `name` | 借用标识，长度 < 48 |
| 入参 | `size` | 借用大小，≥ 4MB |
| 入参 | `slot_ids` | 候选节点 slot_id 数组 |
| 入参 | `slot_cnt` | 候选节点数量 |
| 出参 | `numa_desc` | `ubs_mem_numa_desc_t*`，额外要求 `export_node.slot_id ∈ slot_ids[]` |

**ubs_mem_numa_desc_t**:

| 字段 | 类型 | 校验方式 |
|------|------|----------|
| `name[48]` | `char[48]` | strcmp == 输入 name |
| `numaid` | `int64_t` | ≥ 0 |
| `export_node` | `ubs_topo_node_t` | slot_id 有效且 ≠ import_node.slot_id（ips 字段无效） |
| `import_node` | `ubs_topo_node_t` | slot_id == 本节点 |
| `size` | `uint64_t` | == 输入 size |
| `mem_stage` | `ubs_mem_stage` | ∈ {UBSE_CREATING(1), UBSE_EXIST(3)} |
| `usrInfo[32]` | `uint8_t[32]` | — |

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-NumaCreateCandidate-Ok-01 | 指定候选节点 | 双节点 | size=129MB, slot_ids={双:2 / 四:3,4}; `mem_stage∈{CREATING,EXIST}`, `size==129MB`, `numaid≥0`, `export_node.slot_id∈slot_ids`, `import_node.slot_id==本节点`, `export_node.slot_id≠import_node.slot_id`, `name==输入` | `UBS_SUCCESS` |
| P0-NumaCreateCandidate-OverLen-01 | name 超长 | 双节点 | name ≥ 48 | `UBS_ERR_INVALID_ARG` |
| P0-NumaCreateCandidate-InvalidVal-01 | size < 4MB | 双节点 | size=1 | `UBS_ENGINE_ERR_OUT_OF_RANGE` |
| P0-NumaCreateCandidate-NullPtr-01 | 空指针 | 双节点 | name=NULL 或 numa_desc=NULL | `UBS_ERR_NULL_POINTER` |
| P0-NumaCreateCandidate-BadParam-01 | 不存在的 slot_id | 双节点 | slot_ids={999} | `UBS_ENGINE_ERR_ALLOCATE` |
| P0-NumaCreateCandidate-Dup-01 | 同名重复 | 双/四节点 | 同名再调；四节点候选为节点3和4 | `UBS_ENGINE_ERR_EXISTED` |

#### ubs_mem_numa_get

```
int32_t ubs_mem_numa_get(const char* name, ubs_mem_numa_desc_t* numa_desc)
```

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-NumaGet-NotExist-01 | 查询不存在 | 单节点 | 不存在的 name | `NOT_EXIST` |
| P0-NumaGet-NullPtr-01 | 空指针 | 单节点 | numa_desc=NULL | `NULL_POINTER` |

#### ubs_mem_numa_list

```
int32_t ubs_mem_numa_list(ubs_mem_numa_desc_t** descs, uint32_t* numa_cnt)
```

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-NumaList-Ok-01 | 空时查询+创建后查询 | 双节点 | 空时调用验证接口可用；创建2个NUMA后查询，逐条校验 `mem_stage∈{CREATING,EXIST}`, `size>0`, `numaid≥0`, `import_node.slot_id==本节点`, `export_node.slot_id>0`, `export_node.slot_id≠import_node.slot_id`；创建的2个NUMA在列表中且size一致 | `UBS_SUCCESS` |
| P0-NumaList-NullPtr-01 | 空指针 | 双节点 | descs=NULL 或 cnt=NULL | `UBS_ERR_NULL_POINTER` |

#### ubs_mem_numa_delete

```
int32_t ubs_mem_numa_delete(const char* name)
```

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-NumaDel-Ok-01 | 创建后删除 | 双节点 | 正常 name | `UBS_SUCCESS`; 删除后 get 返回不存在 |
| P0-NumaDel-NotExist-01 | 删除不存在 | 单节点 | 不存在的 name | `NOT_EXIST` |
| P0-NumaDel-Dup-01 | 重复删除 | 双节点 | 同名再删 | `NOT_EXIST` |
| P0-NumaDel-OverLen-01 | name 超长 | 单节点 | name ≥ 48 | `OUT_OF_RANGE` |

#### ubs_mem_numa_get_memid_by_import

签名同 fd_get_memid_by_import，出参同 `ubs_mem_export_memid_t`。

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-NumaMemidByImport-Fld-01 | 创建后查询 | 双节点 | `export_memid` 与创建时一致 | `UBS_SUCCESS` |
| P0-NumaMemidByImport-NotExist-01 | name 不存在 | 单节点 | 不存在的 name | `NOT_EXIST` |

#### ubs_mem_numa_fault_register

签名同 fd_fault_register。

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-NumaFaultReg-NullPtr-01 | NULL handler | 单节点 | handler=NULL | 不崩溃 |

---

### 2.5 Mem SHM

#### ubs_mem_shm_create

```
int32_t ubs_mem_shm_create(const char* name, uint64_t size, const ubs_mem_fd_owner_t* owner,
                           mode_t mode, const char* region, ubs_mem_shm_desc_t* shm_desc)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 入参 | `name` | 借用标识 |
| 入参 | `size` | 借用大小 |
| 入参 | `owner` | 属主 |
| 入参 | `mode` | 权限 |
| 入参 | `region` | 可见域，"nodes" 按 slot_id 限定 |
| 出参 | `shm_desc` | `ubs_mem_shm_desc_t*` |

**ubs_mem_shm_desc_t**:

| 字段 | 类型 | 校验方式 |
|------|------|----------|
| `name[48]` | `char[48]` | strcmp == 输入 name |
| `mem_size` | `uint64_t` | == 输入 size |
| `unit_size` | `size_t` | > 0 |
| `export_node` | `ubs_topo_node_t` | slot_id 有效 |
| `usr_info[32]` | `uint8_t[32]` | — |
| `import_desc_cnt` | `uint32_t` | 每个 attach 的节点 +1 |
| `mem_stage` | `ubs_mem_stage` | == UBSE_EXIST(3) |
| `import_desc` | `ubs_mem_shm_import_desc_t*` | 导入描述符数组 |

**ubs_mem_shm_import_desc_t** (每个 attach 节点一条):

| 字段 | 类型 | 说明 |
|------|------|------|
| `memid_cnt` | `uint32_t` | 内存块数量 |
| `memids[2048]` | `uint64_t[2048]` | 内存块标识 |
| `import_node` | `ubs_topo_node_t` | 借入节点 |
| `mem_stage` | `ubs_mem_stage` | 内存状态 |

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-ShmCreate-Ok-01 | 标准创建 | 双节点 | 正常 name+size | `UBS_SUCCESS` |
| P0-ShmCreate-OverLen-01 | name 超长 | 单节点 | name ≥ 48 | `OUT_OF_RANGE` |
| P0-ShmCreate-InvalidVal-01 | size < 4MB | 单节点 | size < 4MB | `OUT_OF_RANGE` |
| P0-ShmCreate-Dup-01 | 同名重复 | 双节点 | 同名再调 | `EXISTED` |
| P0-ShmCreate-BigSize-01 | size=1GB | 双节点 | size=1GB | `UBS_SUCCESS` |

#### ubs_mem_shm_create_with_affinity

```
int32_t ubs_mem_shm_create_with_affinity(const char* name, uint64_t size,
    const ubs_mem_fd_owner_t* owner, mode_t mode, const char* region,
    const uint32_t* socket_ids, uint32_t socket_cnt, ubs_mem_shm_desc_t* shm_desc)
```

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-ShmCreateAffinity-BadParam-01 | 不存在的 socket_id | 双节点 | socket_ids 包含不存在的值 | 错误 |

#### ubs_mem_shm_create_with_lender

签名同 fd_create_with_lender，出参为 `ubs_mem_shm_desc_t`。

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-ShmCreateLender-NullPtr-01 | lender=NULL | 双节点 | lender=NULL | `NULL_POINTER` |

#### ubs_mem_shm_attach

```
int32_t ubs_mem_shm_attach(const char* name, ubs_mem_shm_desc_t* shm_desc)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 入参 | `name` | 借用标识 |
| 出参 | `shm_desc` | `ubs_mem_shm_desc_t*`，填充当前 attach 节点的 import_desc |

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-ShmAttach-NotReady-01 | 未创建时 attach | 单节点 | 不存在的 name | 错误 |
| P0-ShmAttach-Dup-01 | 重复 attach | 双节点 | 已 attach 再调 | 错误 |

#### ubs_mem_shm_get

```
int32_t ubs_mem_shm_get(const char* name, ubs_mem_shm_desc_t* shm_desc)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 入参 | `name` | 借用标识 |
| 出参 | `shm_desc` | `ubs_mem_shm_desc_t*`，字段同上 `shm_create` 出参 |

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-ShmGet-NotExist-01 | 查询不存在 | 单节点 | 不存在的 name | `NOT_EXIST` |

#### ubs_mem_shm_list / ubs_mem_shm_list_with_prefix

```
int32_t ubs_mem_shm_list(ubs_mem_shm_desc_t** descs, uint32_t* shm_cnt)
int32_t ubs_mem_shm_list_with_prefix(const char* prefix, ubs_mem_shm_desc_t** descs, uint32_t* shm_cnt)
```

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-ShmList-Ok-01 | 空 list | 单节点 | 无 shm 时 `*shm_cnt==0` | `UBS_SUCCESS` |
| P0-ShmList-NullPtr-01 | 空指针 | 单节点 | descs=NULL | `NULL_POINTER` |
| P0-ShmListPrefix-Ok-01 | 无匹配前缀 | 单节点 | prefix 无匹配 | `*shm_cnt==0` |

#### ubs_mem_shm_detach

```
int32_t ubs_mem_shm_detach(const char* name)
```

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-ShmDetach-NotReady-01 | 未 attach 时 detach | 单节点 | 未 attach 的 name | `NOT_EXIST` |

#### ubs_mem_shm_delete

```
int32_t ubs_mem_shm_delete(const char* name)
```

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-ShmDel-NotExist-01 | 删除不存在 | 单节点 | 不存在的 name | `NOT_EXIST` |

#### ubs_mem_shm_fault_register

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-ShmFaultReg-NullPtr-01 | NULL handler | 单节点 | handler=NULL | 不崩溃 |

#### ubs_mem_shm_get_memid_by_import

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-ShmMemidByImport-NotExist-01 | name 不存在 | 单节点 | 不存在的 name | `NOT_EXIST` |
| P0-ShmMemidByImport-InvalidVal-01 | import_memid 无效 | 单节点 | 不存在的 import_memid | 错误 |

---

### 2.6 URMA QoS

#### ubs_urma_qos_create

```
uint32_t ubs_urma_qos_create(const ubs_urma_qos_config_t* configs, uint32_t count)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 入参 | `configs` | `ubs_urma_qos_config_t[]`，priority∈{0,1}，bandwidth>0 (Gbps) |
| 入参 | `count` | 配置数量，1~2 |
| 返回 | `uint32_t` | 错误码 |

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-UrmaQosCreate-Ok-01 | 创建单优先级 | 单节点 | count=1; priority=0; bandwidth=100 | `UBS_SUCCESS` |

#### ubs_urma_qos_get

```
uint32_t ubs_urma_qos_get(ubs_urma_qos_config_t** configs, uint32_t* count)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 出参 | `configs` | `ubs_urma_qos_config_t[]`，接口内 malloc |
| 出参 | `count` | `uint32_t*`，配置数量 |

（P0 级用例仅空指针校验，创建→查询一致性属 P1）

#### ubs_urma_qos_delete

```
uint32_t ubs_urma_qos_delete(void)
```

（无 P0 用例，删除前置条件校验属 P1）

---

### 2.7 NPU

#### ubs_npu_device_list_query

```
int32_t ubs_npu_device_list_query(ubs_npu_device_info_t** device_list, uint32_t* count)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 出参 | `device_list` | `ubs_npu_device_info_t[]` |
| 出参 | `count` | `uint32_t*` |

#### ubs_npu_device_alloc

```
int32_t ubs_npu_device_alloc(const char* device_name, ubs_npu_device_info_t* dev_info)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 入参 | `device_name` | 设备名 |
| 出参 | `dev_info` | `ubs_npu_device_info_t*` |

#### ubs_npu_device_free

```
int32_t ubs_npu_device_free(const char* device_name)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 入参 | `device_name` | 要释放的设备名 |
| 返回 | `int32_t` | 错误码 |

#### ubs_uba_tid_size_query

```
int32_t ubs_uba_tid_size_query(uint32_t* tid_size)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 出参 | `tid_size` | `uint32_t*` |

NPU 用例由 `zhisuan_npu_single_node` 承载，未按编号命名：

| 用例名 | 场景 | 被验证接口 | 入参/出参校验 |
|--------|------|-----------|--------------|
| `DeviceListQuery` | NPU 单节点 | `ubs_npu_device_list_query` | `*count>0`; 设备字段非空 |
| `DeviceAllocFreeLifecycle` | NPU 单节点 | `ubs_npu_device_alloc` + `ubs_npu_device_free` | 分配→释放→再分配，完整生命周期间无泄漏 |
| `UbaTidSizeQueryAfterAlloc` | NPU 单节点 | `ubs_uba_tid_size_query` | 分配后 `*tid_size` 与预期一致 |
| `RepeatAllocAndFree` | NPU 单节点 | `ubs_npu_device_alloc` + `ubs_npu_device_free` | 反复分配释放不崩溃 |
| `RepeatDealloc` | NPU 单节点 | `ubs_npu_device_free` | 重复释放正确处理 |
| `PreemptDevice` | NPU 单节点 | `ubs_npu_device_alloc` | 抢占已分配设备正确处理 |
| `ConcurrentSuccess` | NPU 单节点 | `ubs_npu_device_alloc` + `ubs_npu_device_free` | 并发调用最终一致 |

---

### 2.8 Error

#### ubs_error_name / ubs_error_string

```
const char* ubs_error_name(int32_t error_code)
const char* ubs_error_string(int32_t error_code)
```

| 类型 | 参数 | 说明 |
|------|------|------|
| 入参 | `error_code` | 错误码 |
| 返回 | `const char*` | 错误码的名称/描述字符串 |

| 编号 | 用例名 | 场景 | 入参/出参校验 | 预期 |
|------|--------|------|--------------|------|
| P0-ErrName-Ok-01 | 已知错误码 | 单节点 | error_code=0 | 返回 `"UBS_SUCCESS"` |
| P0-ErrName-BadParam-01 | 未知错误码 | 单节点 | 非法 error_code | 返回非空字符串，不崩溃 |

---

## 三、CLI 命令 P0 用例

### 3.1 Node / Topo

#### display node / display cluster / display topo -t cpu

| 编号 | 用例名 | 场景 | CLI 命令 | 入参/校验点 | 预期 |
|------|--------|------|----------|------------|------|
| P0-CliNode-Ok-01 | 查询本节点 | 单节点 | `display node` | 无参数; 输出含 nodeId/role/state 非空 | 成功 |
| P0-CliNode-BadParam-01 | 不存在的 nodeId | 单节点 | `display node -n 999` | -n 非法值 | 报错 |
| P0-CliCluster-Ok-01 | 查询集群 GUID | 四节点 | `display cluster` | 无参数; 输出含 clusterGuid 与 LCNE 一致 | 成功 |
| P0-CliTopoCpu-Ok-01 | CPU 拓扑 | 双节点 | `display topo -t cpu` | 指定 nodeId; 输出含 link 信息 | 成功 |

### 3.2 Mem

| 编号 | 用例名 | 场景 | CLI 命令 | 入参/校验点 | 预期 |
|------|--------|------|----------|------------|------|
| P0-CliCheckMem-Ok-01 | 内存健康检查 | 双节点 | `check memory` | 无参数; 输出含各节点内存状态 | 成功 |
| P0-CliMemConfig-Ok-01 | 查询内存配置 | 双节点 | `display memory -t config` | 无参数; 输出含池化配置 | 成功 |
| P0-CliNumaStatus-Ok-01 | NUMA 状态 | 双节点 | `display memory -t numa_status` | 无参数; 输出含 numa_id/mem_total/mem_free | 成功 |
| P0-CliBorrowDetail-Ok-01 | 无借用时查询 | 单节点 | `display memory -t borrow_detail` | 无参数; 输出为空 | 成功 |
| P0-CliCreateNuma-Ok-01 | 创建 numa | 双节点 | `create memory -t numa -n <name> -s <size>` | 正常 name+size; display 可查 | 成功 |
| P0-CliCreateNuma-InvalidChar-01 | 非法 name | 单节点 | `create memory -t numa -n "bad!name"` | 非法字符 | 报错 |
| P0-CliCreateNuma-Dup-01 | 重复创建 | 双节点 | `create memory -t numa` 二次 | 同名 | 报错 EXISTED |
| P0-CliCreateFd-Ok-01 | 创建 fd | 双节点 | `create memory -t fd -n <name> -s <size>` | 正常参数 | 成功 |
| P0-CliCreateFd-InvalidVal-01 | size=0 | 单节点 | `create memory -t fd -s 0` | size=0 | 报错 |
| P0-CliCreateShare-Ok-01 | 创建 share | 四节点 | `create memory -t share -n <name> -s <size>` | 正常参数 | 成功 |
| P0-CliCreateShare-OverLen-01 | name 超长 | 单节点 | `create memory -t share -n <超长>` | name 超长 | 报错 |
| P0-CliDelMem-NotExist-01 | 删除不存在 | 单节点 | `delete memory <name>` | 不存在的 name | 报错 |
| P0-CliAttachMem-NotReady-01 | attach 不存在 | 双节点 | `attach memory <name>` | 不存在的 name | 报错 |
| P0-CliDetachMem-NotReady-01 | detach 未 attach | 单节点 | `detach memory <name>` | 未 attach | 报错 |

### 3.3 URMA QoS

| 编号 | 用例名 | 场景 | CLI 命令 | 入参/校验点 | 预期 |
|------|--------|------|----------|------------|------|
| P0-CliCreateQos-Ok-01 | 创建单优先级 | 单节点 | `create urma-qos --pri 0 --cir 100` | 正常参数 | 成功 |
| P0-CliCreateQos-BadParam-01 | 缺少参数 | 单节点 | `create urma-qos`（无参数） | 缺参数 | 报错 |
| P0-CliCreateQos-BadParam-02 | 参数数量不匹配 | 单节点 | `create urma-qos --pri 0,1 --cir 100` | pri 2个 cir 1个 | 报错 |
| P0-CliCreateQos-InvalidVal-01 | 无效 priority | 单节点 | `create urma-qos --pri 5 --cir 100` | priority ∉ {0,1} | 报错 |
| P0-CliCreateQos-Dup-01 | 重复 priority | 单节点 | `create urma-qos` 二次 | 已存在同名 | 报错 QOS_EXIST |
| P0-CliCreateQos-OverCnt-01 | count 超上限 | 单节点 | count > 2 | 超限 | 报错 |
| P0-CliDisplayQos-Ok-01 | 未创建时查询 | 单节点 | `display urma-qos` | 无配置; 输出为空 | 成功 |
| P0-CliDelQos-NotReady-01 | 未创建时删除 | 单节点 | `delete urma-qos` | 无配置 | 报错 |

---

## 四、统计

### 按模块

| 模块 | SDK P0 | CLI P0 | 合计 |
|------|--------|--------|------|
| Client | 5 | — | 5 |
| Topo | 8 | 4 | 12 |
| Mem FD | 35 | 3 | 38 |
| Mem NUMA | 28 | 7 | 35 |
| Mem SHM | 18 | 3 | 21 |
| NPU | 7 | — | 7 |
| URMA QoS | 1 | 8 | 9 |
| Error | 2 | — | 2 |
| **合计** | **104** | **25** | **129** |

### 按场景

| 场景 | SDK P0 | CLI P0 | 合计 |
|------|--------|--------|------|
| 单节点 | 46 | 15 | 61 |
| 双节点 | 56 | 8 | 64 |
| 四节点 | 15 | 2 | 17 |
| NPU 单节点 | 7 | — | 7 |
