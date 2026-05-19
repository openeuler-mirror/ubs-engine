# UBS Virt Agent大规格虚拟机特性最佳实践

> 本文档旨在针对UBS Virt Agent提供创建大规格虚拟机特性的最佳实践. 允许使用者在集群中, 创建超过单节点内存上线的虚拟机

## 前置准备

- 请参考RMRS UBSE等依赖组件配置, 进行场景配置



## 最小成功路径

### 1、虚拟机规划

**EXP: 环境内存规格**

- 4物理节点
    - nodeId依次为`node0` ~ `node3`
    - 每个节点均包含4个numa, 依次为`NUMA0` ~ `NUMA3`
- 主节点物理可用大页内存大小: 3TB
    - 内存均匀分布于4个NUMA, 平均一个NUMA内存, 为0.75T
- 其余节点物理可用大页内存大小: 1TB

**注意:** 环境实际大页内存可以通过接口`ubs_virt_agent_mem_fragmentation_node_info_list`获取

```c++
node_info_list_s *node_info_list = null;
const virt_agent_ret_t ret = ubs_virt_agent_mem_fragmentation_node_info_list(node_info_list);
if (ret != 0) {
    return ret;
}
//获得结果json示例如下:
//node_info_list {
//    "node_info_s": [
//        {
//            "node_id": "0",
//            "numa_infos": [
//                {
//                    "timestamp": 1234567,
//                    "node_id": "0",
//                    "host_name": "node0",
//                    "numa_id": 0,
//                    "socket_id": 0,
//                    "is_local": 1,
//                    "mem_total": 3221225472, // 单位KB
//                    "mem_free": 3221225472, // 单位KB
//                    "huge_page_data": [
//                        {
//                            "pageSize": 2048, // 大页大小, 单位KB
//                            "hugePageTotal": 1572864,  // 大页数量
//                            "hugePageFree": 1572864, // 大页数量
//                        }    
//                    ]
//                    "numaPageInfoCount": 1
//                }, ...
//            ],
//            "numa_len": 4,
//            "is_current": True, // 是否为当前查询节点
//        }, ...
//    ],
//    "node_len": 4
//}
```

在当前集群环境中, 用户规划创建一个大规格内存虚拟机

- 虚拟机物理内存: 5T

根据节点实际内存情况, 可以做出如下规划

- 本地规划使用内存2TB大页内存, 其余各节点借用1TB大页内存

### 2、内存借用

根据规划, 需要在主节点, 调用`ubs_virt_agent_mem_borrow`内存借用接口, 借用3T内存

```c++
mem_borrow_param_s *param = (mem_borrow_param_s *)malloc(sizeof(mem_borrow_param_s));
param -> src_nid = "node0";
param -> numa_len = 4;
param -> borrow_size = 3145728; // 单位MB, 此处借用3TB内存
numa_meta_info_s *numa_meta_infos = (numa_meta_info_s *)malloc(sizeof(numa_meta_info_s) * param -> numa_len);

// 根据实际情况填写 or 根据ubs_virt_agent_mem_fragmentation_node_info_list查询结果填写
// 此处按照如下numa关系配置
// NUMA0 - socketId: 0; NUMA1 - socketId: 0; NUMA2 - socketId: 1; NUMA3 - socketId: 1
for (int i = 0; i < param -> numa_len; ++i) {
    numa_meta_infos[i].socket_id = i < 2 ? 0 : 1;
    numa_meta_infos[i].numa_id = i;
}
mem_borrow_result_s* result = null;
const virt_agent_ret_t ret = ubs_virt_agent_mem_borrow(param, true, result);
if (ret != 0) {
    return ret;
}
//获得结果json示例如下:
//result {
//    "mem_borrow_result_list" : [  
//        {
//            "borrow_ids_ptr": [],
//            "present_numa_ids_ptr": [],
//            "borrow_ids_size": 0,
//            "present_numa_ids_size": 0,
//            "task_id": "mem_fragmentation_borrow_1",
//        }, ...
//    ],
//    "mem_borrow_result_list_len": 4,
//}
// 注意: 返回的借用任务顺序, 与入参中 numa_meta_infos中numa顺序一一对应; 本例中, 入参中numa_meta_infos第一个元素为numaId 0;
// 则返回任务中, mem_borrow_result_list中第一个任务, 即为numaId 0的借用任务; 由于是异步借用任务, 所以返回结果中, 不包含借用结果, 仅返回任务ID
// 需要通过task query接口查询任务执行结果
```

### 3、查询借用结果

在调用借用接口后, 返回结果中会获取任务ID(即task_id); 需要根据task_id调用查询接口`ubs_virt_agent_sync_task_query`查询借用结果

```c++
// 根据借用请求可知, node0包含4个numa, 一共会有四个借用任务
//下面, 将以一个任务举例 "mem_fragmentation_borrow_1"
char *task_id = "mem_fragmentation_borrow_1";
uint32_t task_id_len;
async_task_info_c *result = null;
const virt_agent_ret_t ret = ubs_virt_agent_sync_task_query(task_id, 26,  result);
//获得结果json示例如下:
//result {
//    "task_id": "mem_fragmentation_borrow_1",
//    "status": 2,
//    "resultCode": 0,
//    "memBorrowResult": {
//        "borrow_ids_ptr": [
//            "borrow_id_1",
//        ],
//        "present_numa_ids_ptr": [
//            4  
//        ],
//        "borrow_ids_size": 1,
//        "present_numa_ids_size": 1,
//        "task_id": "mem_fragmentation_borrow_1"
//    }
//}
//如示例所示, numa0借用映射至本地的远端numa编号为4
```

### 4、规划虚拟机xml

待所有的借用任务都成功结束后, 假设查询获得以下借用结果:
NUMA0 -- NUMA4; NUMA0 本地内存 0.75TB; NUMA4 借用内存 0.75T
NUMA1 -- NUMA5; NUMA1 本地内存 0.75TB; NUMA5 借用内存 0.75T
NUMA2 -- NUMA6; NUMA2 本地内存 0.75TB; NUMA6 借用内存 0.75T
NUMA3 -- NUMA7; NUMA3 本地内存 0.75TB; NUMA7 借用内存 0.75T

此时, 规划虚拟机内存的xml配置: 要求将包含借用关系的numa内存, 配置于同一个guest numa中; 例如

```xml
<!-- 当前规划2个guest numa -->
<numatune>
  <memnode cellid='0' mode='preferred' proportion='786432-node0;786432-node4;786432-node1;786432-node5'/>
  <memnode cellid='1' mode='preferred' proportion='786432-node2;786432-node6;786432-node3;786432-node7'/>
</numatune>
```

### 5、创建虚拟机

规划好虚拟机numa配置后, 用户可以根据使用生态, 调用libvirt 接口或者virsh指令创建虚拟机, 此处不过多做赘述

### 6、使能冷热页流动

为了提高整体的内存读写性能, 需要调用``开启冷热页流动, 允许本地冷页内存, 流动至远端. 根据当前guest远端内存规划方式

应该构造如下的冷热页流动的使能请求`ubs_virt_agent_page_swap_enable`

```c++
const pid_t pid = 123; // 虚拟机实际进程号
page_swap_enable_s *page_swap_enable = (page_swap_enable_s *)malloc(sizeof(page_swap_enable));
page_swap_enable -> 
virt_agent_ret_t ret = ubs_virt_agent_page_swap_enable(pid, page_swap_enable);
```

### 7、虚拟机删除

当前需要销毁虚拟机的时候，需要在确认虚拟机销毁（即借用内存不再使用后），执行归还内存`ubs_virt_agent_mem_return`接口，归还借用内存。

## 可靠性防护

### 内存借用失败场景

如果出现内存借用失败

---