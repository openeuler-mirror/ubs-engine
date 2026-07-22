# 大规格虚机创建/删除最佳实践
本文档主要介绍在虚机场景下，如何使用内存借用、冷热页流动等接口来创建超大规格虚机（以在4节点集群硬件环境的节点1创建5T规格虚机为例），包括各种前置操作、配置检查、大页分配和虚机的创建/删除等。
## 0.前置步骤
参考部署指导，安装好UBSE、RMRS和Virt Agent，并修改各组件的配置文件为大规格虚机场景所需要的配置。
## 1.检查配置
## 1.1 配置文件
*当前章节命令需要在所有节点执行*

`/etc/ubse/ubse_plugin_admission.conf` 取消 virt_agent和mempooling 的注释，示例如下：
```
# virt_agent plugin. Default value: 205
virt_agent=205
# mempooling plugin. Default value: 777
mempooling=777
# ucache plugin. Default value: 778
# ucache=778
```
## 1.2 切换1G大页
*当前章节命令需要在所有节点执行*
cat /sys/module/obmm/parameters/mempool_allocator
如果结果是hugetlb_pud，则跳过当前步骤。
否则按如下步骤配置：
1) 添加脚本 `vi /etc/modprobe.d/disable-ummu.conf`,禁止udev自动加载ummu，确保ummu按照`/opt/modprobe.sh`中的参数插入。
```
blacklist ummu
```
2) 按顺序确认rc-local中调用的rc.local中是否使用/opt/modprobe.sh脚本
- 执行`systemctl cat rc-local`查看`[Service]`下是否存在`ExecStart=/etc/rc.d/rc.local start`字样
- 执行`cat /etc/rc.d/rc.local`查看是否包含`sh /opt/modprobe.sh`字样
3) 执行`vi /opt/modprobe.sh`,进入文件内修改脚本内容
- 将含`modprobe ummu`的行替换为`modprobe ummu ubm_granule=4`
- 将含`modprobe obmm`的行替换为`modprobe obmm mempool_allocator=hugetlb_pud`
随后将节点重新上下电，确保新的配置生效。
## 1.3 场景信息
*当前章节命令需要在所有节点执行*

执行 `python /usr/lib/python3.11/site-packages/ubse/ubs_virt_case_conf.py`来查询当前场景信息，

如果结果中包含’memFragmentation‘字样，则说明环境已经是碎片场景，跳过当前步骤。

否则,按以下操作执行：
- 将下列内容上传到环境上作为test_case_conf_set.py，脚本用来后续注入碎片场景
```
from ubse.ffi.ubs_virt_agent_case_conf_set import UbsVirtAgentCaseConfSet


def set_case_conf():
    case_conf_set = UbsVirtAgentCaseConfSet()
    try:
        case_conf_set.ubs_virt_agent_initialize()
        param = {
            "caseType": "memFragmentation",
            "overCommitment": 1.00
        }
        return case_conf_set.ubs_case_conf_set(param)
    except Exception as e:
        msg = f"Failed to set_case_conf, error: {e}"
        print(msg)
        raise RuntimeError(msg) from e
    finally:
        case_conf_set.ubs_virt_agent_finalize()


if __name__ == '__main__':
    ret_code = set_case_conf()

    print(f"{ret_code = }")
```
- 然后执行以下命令来注入正确的场景信息
```
rm -rf /var/lib/ubse/data/* # 清空ubse数据目录，确保场景信息被正确注入
python test_case_conf_set.py # 执行脚本注入碎片场景
systemctl restart ubse # 重启ubse服务，确保场景信息被正确注入
```
等待ubse服务启动成功之后，再次查询 `python /usr/lib/python3.11/site-packages/ubse/ubs_virt_case_conf.py`，结果出现'memFragmentation'字样则代表成功。

## 2.大页分配
以4个节点（每个节点4个numa）环境上创建5T虚机为例。
**此处需要保证集群总内存不少于5T，且创建虚机的本地节点（本文档中为节点1）的内存不少于虚机大小的一半，即2.5T。**

此处按平均分配的逻辑，节点1每个numa分配330990(需要借用的情况要再预留1%用来进行冷热页流动)个2MB大页，节点2、3、4每个numa分配215个1GB大页。

注意，硬件环境上每个numa上的内存总数不一定相同，例如节点1可能有的numa内存数量较少，无法分配330990个2MB大页，只能分配小于330990个2MB大页,那就可以给内存少的numa少分一点大页，内存多的numa多分配一些大页。只要分配的总大页内存不少于5T，并且节点1（本地）大页内存不少于2.5T（均不包括预留的部分）。
- *节点1（本地）执行*
```
echo 330990 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
echo 330990 > /sys/devices/system/node/node1/hugepages/hugepages-2048kB/nr_hugepages
echo 330990 > /sys/devices/system/node/node2/hugepages/hugepages-2048kB/nr_hugepages
echo 330990 > /sys/devices/system/node/node3/hugepages/hugepages-2048kB/nr_hugepages
```
- *节点2、3、4（远端）执行*
```
echo 215 > /sys/devices/system/node/node0/hugepages/hugepages-1048576kB/nr_hugepages
echo 215 > /sys/devices/system/node/node1/hugepages/hugepages-1048576kB/nr_hugepages
echo 215 > /sys/devices/system/node/node2/hugepages/hugepages-1048576kB/nr_hugepages
echo 215 > /sys/devices/system/node/node3/hugepages/hugepages-1048576kB/nr_hugepages
```

## 3.创建虚机
*当前章节命令需要在节点1执行*
- 找一个qcow2格式（脚本推荐格式）的镜像，作为虚机的镜像，可以从[openEuler-24.03-LTS-SP3](https://mirrors.huaweicloud.com/openeuler/openEuler-24.03-LTS-SP3/virtual_machine_img/aarch64/)下载后解压。
- 执行`virsh list --all`确保节点1上并无任何虚机。
- 将create_vm.py脚本上传到节点1上。
- create_vm.py中的IMAGE_BASE_PATH和IMAGE_NAME分别为镜像路径和名称，请根据实际情况修改。 
- 执行`python create_vm.py`创建虚机.
- 日志出现"VM vm1 created successfully"，则代表虚机创建成功。
## 4.删除虚机
*当前章节命令需要在节点1执行*
- 将delete_vm.py脚本上传到节点1上。
- 执行`python delete_vm.py`删除虚机。
- 日志出现"VM vm1 deleted successfully"，则代表虚机删除成功。
## 5.脚本说明
## 5.1 create_vm.py
创建虚机主要流程为：
- 使用ubs_mem_fragmentation_node_info_list接口查询集群节点信息
- 计算创建当前虚机是否需要借用内存
- 使用ubs_mem_borrow（异步）接口借用内存（如需要），并使用ubs_task_result_query查询任务结果
- 再次使用ubs_mem_fragmentation_node_info_list接口查询集群节点信息
- 整理出当前节点各numa的信息，用以后续生成xml文件和冷热页流动
- 创建虚机
- 使用ubs_page_swap_enable接口开启冷热页流动（如需要）

## 5.2 delete_vm.py
删除虚机主要流程为：
- 删除虚机
- 不断调用ubs_mem_fragmentation_node_info_list接口查询集群节点信息，直到查询到虚机已释放所有内存
- 使用ubs_mem_return（异步）接口归还内存（如需要），并使用ubs_task_result_query查询任务结果


## 6. 接口说明
## 6.1 ubs_mem_fragmentation_node_info_list
### 6.1.1 接口说明
`def ubs_mem_fragmentation_node_info_list() -> List['NodeInfoT']`

获取集群中的所有节点的numa信息
### 6.1.2 参数与返回值说明
```
参数：无

返回值：
class NumaInfoHugePageDataT:
 	     page_size: int # 大页大小（字节）
 	     huge_page_total: int # 总大页数量
 	     huge_page_free: int # 可用大页数量
 	     
class NumaInfoT:
         host_id: str # 主机ID
 	     hostname: str # 主机名
 	     numa_id: str # NUMA ID
 	     socket_id: int # 套接字ID
 	     is_local: int # 是否为本地NUMA
 	     mem_total: int # 总内存（字节）
 	     mem_free: int # 可用内存（字节）
 	     numa_huge_page_info: Dict[int, NumaInfoHugePageDataT] # 大页信息字典，键为大页大小（字节），值为大页信息
 	     
class NodeInfoT:
 	     node_id: str # 节点ID
 	     numa_infos: List[NumaInfoT] # 当前节点NUMA信息列表
 	     is_current: bool # 是否为当前节点
```
### 6.1.3 调用示例
详见[test_node_info_list.py](../../../sdk/python/sample/test_node_info_list.py)
## 6.2 ubs_mem_borrow
### 6.2.1 接口说明
`def ubs_mem_borrow(param: 'BorrowParamT', is_async: bool = False) -> List['MemBorrowResultT']`

根据参数执行同步/异步的内存借用操作
### 6.2.2 参数与返回值说明
```
参数：
class NumaMetaInfoT:
 	     socket_id: int # 套接字ID
 	     numa_id: int # NUMA ID
 	     
class BorrowParamT:
 	     node_id: str # 节点id
 	     numa_meta_infos: List[NumaMetaInfoT] # numa元信息列表
 	     borrow_size: int # 借用内存大小，单位：MB

is_async # 是否异步执行借用

返回值：
class MemBorrowResultT:
 	     borrow_ids: List[str] # 借用ID列表
 	     present_numa_ids: List[int] # 当前NUMA ID列表
 	     task_id: str # 异步任务ID
```
### 6.2.3 调用示例
详见[test_mem_borrow.py](../../../sdk/python/sample/test_mem_borrow.py)
## 6.3 ubs_page_swap_enable
### 6.3.1 接口说明 
`def ubs_page_swap_enable(pid: int, page_swap_enable: List['PageSwapPairT']) -> int`

根据参数开启冷热页流动
### 6.3.2 参数与返回值说明
```
参数：
pid # 虚机PID

class NumaQuotaT:
 	     numa_id: int # numa ID
 	     quota: int  # numa上允许使用内存，单位：MB
 	     
class PageSwapPairT:
 	     local_numa_quotas: List[NumaQuotaT] # 本地NUMA的列表
 	     remote_numa_quotas: List[NumaQuotaT] # 远端NUMA的列表
 	     
返回值：
int # 0 成功，其他值失败
```
### 6.3.3 调用示例
详见[test_page_swap_enable.py](../../../sdk/python/sample/test_page_swap_enable.py)
## 6.4 ubs_mem_return
### 6.4.1 接口说明 
`def ubs_mem_return(is_async: bool = False) -> Tuple[int, str]`

归还内存

### 6.4.2 参数与返回值说明
```
参数：
is_async # 是否异步执行归还

返回值：
Tuple[int, str] # int: 0 成功，其他值失败；str: 异步任务的task_id
```
### 6.4.3 调用示例
详见[test_mem_return.py](../../../sdk/python/sample/test_mem_return.py)
## 6.5 ubs_task_result_query
### 6.5.1 接口说明
`def ubs_task_result_query(task_id: str) -> Tuple[int, TaskInfoT]`

根据task_id查询任务结果
### 6.5.2 参数与返回值说明
```
参数：
task_id # 任务ID

返回值：
Tuple[int, TaskInfoT] # int: 0 成功，其他值失败；TaskInfoT: 任务执行结果

class BorrowExecuteResT:
 	     borrow_ids: List[str] # 借用ID列表
 	     present_numa_ids: List[int] # 当前NUMA ID列表
 	     
class TaskInfoT:
 	     task_id: str # 任务ID
 	     status: int # 任务状态
 	     result_code: int # 结果码
 	     mem_borrow_result: BorrowExecuteResT # 内存借用结果
```
### 6.5.3 调用示例
详见[test_task_query.py](../../../sdk/python/sample/test_task_query.py)
