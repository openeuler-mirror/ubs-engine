#!/usr/bin/python3
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# virtagent is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
from dataclasses import dataclass
from typing import List, Dict, Callable, TypeVar
import ctypes
from ubse.ffi.ubs_virt_agent_types import NumaInfoHugePageData, NumaInfo, VmNumaInfo, VmMetaData, VmInfo, \
    CaseConfInfo, TaskInfo, NumaMetaInfoC, NodeInfoC, MemBorrowParamC, NumaQuotaC, PageSwapPairC, MemBorrowResult

T = TypeVar('T')


def iterate_c_array(base_ptr: ctypes.pointer, count: int, struct_type: type,
                    converter: Callable[[any], T]) -> List[T]:
    """
    Generic C structure array iterator
    
    Used to traverse C structure pointer arrays and convert each element to a Python object.
    
    Args:
        base_ptr: Array base address pointer
        count: Number of array elements
        struct_type: C structure type (used to calculate sizeof)
        converter: Conversion function that receives C structure instance and returns Python object
        
    Returns:
        List[T]: List of converted Python objects
    """
    result = []
    for i in range(count):
        item_ptr = ctypes.cast(
            ctypes.addressof(base_ptr.contents) + i * ctypes.sizeof(struct_type),
            ctypes.POINTER(struct_type)
        )
        result.append(converter(item_ptr.contents))
    return result


@dataclass
class NumaInfoHugePageDataT:
    page_size: int
    huge_page_total: int
    huge_page_free: int

    @staticmethod
    def from_c_struct(c_link: NumaInfoHugePageData) -> "NumaInfoHugePageDataT":
        return NumaInfoHugePageDataT(
            page_size=c_link.pageSize,
            huge_page_total=c_link.hugePageTotal,
            huge_page_free=c_link.hugePageFree,
        )


@dataclass
class NumaInfoT:
    host_id: str
    hostname: str
    numa_id: str
    socket_id: int
    is_local: int
    mem_total: int
    mem_free: int
    numa_huge_page_info: Dict[int, NumaInfoHugePageDataT]

    @staticmethod
    def from_c_struct(c_link: NumaInfo) -> "NumaInfoT":
        numa_huge_page_info_tmp = {}
        for i in range(c_link.numaPageInfoCount):
            huge_page_data = c_link.numaPageInfo[i]
            numa_huge_page_info = NumaInfoHugePageDataT.from_c_struct(huge_page_data)
            numa_huge_page_info_tmp[numa_huge_page_info.page_size] = numa_huge_page_info
        return NumaInfoT(
            host_id=c_link.nodeId.decode('utf-8', errors='ignore'),
            hostname=c_link.hostName.decode('utf-8', errors='ignore'),
            numa_id=str(c_link.numaId),
            socket_id=c_link.socketId,
            is_local=c_link.isLocal,
            mem_total=c_link.memTotal,
            mem_free=c_link.memFree,
            numa_huge_page_info=numa_huge_page_info_tmp
        )


@dataclass
class NodeNumaInfoT:
    numa_infos: List[NumaInfoT]
    host_id: str
    hostname: str


@dataclass
class VmNumaInfoT:
    numa_id: int
    socket_id: int
    is_local: bool
    page_size: int
    used_mem: int

    @staticmethod
    def from_c_struct(c_link: VmNumaInfo) -> "VmNumaInfoT":
        return VmNumaInfoT(
            numa_id=c_link.numaId,
            socket_id=c_link.socketId,
            is_local=c_link.isLocal,
            page_size=c_link.pageSize,
            used_mem=c_link.usedMem,
        )


@dataclass
class VmMetaDataT:
    host_id: str
    hostname: str
    uuid: str
    name: str
    vm_create_time: str
    state: str
    max_mem: str
    pid: int

    @staticmethod
    def from_c_struct(c_link: VmMetaData) -> "VmMetaDataT":
        return VmMetaDataT(
            host_id=c_link.nodeId.decode('utf-8', errors='ignore'),
            hostname=c_link.hostName.decode('utf-8', errors='ignore'),
            uuid=c_link.uuid.decode('utf-8', errors='ignore'),
            name=c_link.name.decode('utf-8', errors='ignore'),
            state=c_link.state.decode('utf-8', errors='ignore'),
            vm_create_time=str(c_link.vmCreateTime),
            max_mem=str(c_link.maxMem),
            pid=c_link.pid,
        )


@dataclass
class VmInfoT:
    timestamp: int
    metadata: VmMetaDataT
    numa_datas: Dict[int, VmNumaInfoT]

    @staticmethod
    def from_c_struct(c_link: VmInfo) -> "VmInfoT":
        vm_numa_data_tmp = {}
        for i in range(c_link.numaInfoCount):
            link_ptr = ctypes.cast(
                ctypes.addressof(c_link.numaInfo.contents) + i * ctypes.sizeof(VmNumaInfo),
                ctypes.POINTER(VmNumaInfo)
            )
            numa_info_t = VmNumaInfoT.from_c_struct(link_ptr.contents)
            vm_numa_data_tmp[numa_info_t.numa_id] = numa_info_t
        return VmInfoT(
            timestamp=c_link.timestamp,
            metadata=VmMetaDataT.from_c_struct(c_link.metadata),
            numa_datas=vm_numa_data_tmp)


@dataclass
class NodeVmInfoT:
    vm_infos: List[VmInfoT]


@dataclass
class CaseConfInfoT:
    case_type: str
    over_commitment: float
    migrate_water_line: int
    index: int
    host_id: str

    @classmethod
    def from_c_struct(cls, c_struct: CaseConfInfo) -> "CaseConfInfoT":
        # parse
        cur_case_str = c_struct.cur_case.decode('utf-8', errors='ignore')
        over_commitment_str = c_struct.over_commitment_ratio.decode('utf-8', errors='ignore')
        migrate_water_line_str = c_struct.migrate_water_line.decode('utf-8', errors='ignore')
        host_id_str = c_struct.host_id.decode('utf-8', errors='ignore')

        return cls(
            case_type=cur_case_str,
            over_commitment=float(over_commitment_str),
            migrate_water_line=int(migrate_water_line_str),
            index=c_struct.index,
            host_id=host_id_str
        )


@dataclass
class DstNumaInfoT:
    host_id: str
    socket_id: int
    numa_nums: int
    numa_ids: List[int]
    mem_sizes: List[int]


@dataclass
class BorrowStrategyT:
    src_host_id: str
    src_socket_id: int
    src_numa_id: int
    borrow_size: int
    dest_numa_infos: List[DstNumaInfoT]


@dataclass
class BorrowExecuteResT:
    borrow_ids: List[str]
    present_numa_ids: List[int]


@dataclass
class VmMigrateStrategyT:
    dest_numa_id: int
    mem_size: int
    pid: int


@dataclass
class MemMigrateStrategyT:
    vm_info_list: List[VmMigrateStrategyT]
    waiting_time: int


@dataclass
class TaskInfoT:
    task_id: str
    status: int
    result_code: int
    mem_borrow_result: BorrowExecuteResT

    @classmethod
    def from_c_struct(cls, c_struct: TaskInfo) -> "TaskInfoT":
        # parse
        mem_borrow_res = c_struct.mem_borrow_result

        borrow_ids = []
        for i in range(mem_borrow_res.borrow_ids_num):
            borrow_id = mem_borrow_res.borrow_ids[i]
            id_str = borrow_id.value.decode('utf-8').rstrip('\x00')
            borrow_ids.append(id_str)

        present_numa_ids = []
        for i in range(mem_borrow_res.present_numa_ids_num):
            numa_id = int(mem_borrow_res.present_numa_ids[i])
            present_numa_ids.append(numa_id)

        mem_borrow_result = BorrowExecuteResT(
            borrow_ids=borrow_ids,
            present_numa_ids=present_numa_ids
        )

        return cls(
            task_id=c_struct.task_id.decode('utf-8'),
            status=int(c_struct.status),
            result_code=int(c_struct.result_code),
            mem_borrow_result=mem_borrow_result
        )


@dataclass
class NumaMetaInfoT:
    """
    NUMA meta information data class

    Corresponds to C structure NumaMetaInfo, containing socket ID and NUMA ID information.
    Used in memory borrow parameter structures.

    Attributes:
        socket_id: Socket ID
        numa_id: NUMA ID
    """
    socket_id: int
    numa_id: int

    @staticmethod
    def from_c_struct(c_struct: NumaMetaInfoC) -> "NumaMetaInfoT":
        """
        Convert from C structure to Python data class

        Args:
            c_struct: C structure NumaMetaInfoC

        Returns:
            NumaMetaInfoT: Python data class instance
        """
        return NumaMetaInfoT(
            socket_id=c_struct.socket_id,
            numa_id=c_struct.numa_id
        )

    def __str__(self) -> str:
        return f"socketId={self.socket_id}, numaId={self.numa_id}"


@dataclass
class NodeInfoT:
    """
    Node information data class

    Corresponds to C++ structure NodeInfo, containing node ID, NUMA information array, and flag indicating whether it's the current node.
    Used in the node information list for memory fragmentation queries.

    Attributes:
        node_id: Node ID
        numa_infos: NUMA information list
        is_current: Whether it is the current node
    """
    node_id: str
    numa_infos: List[NumaInfoT]
    is_current: bool

    @staticmethod
    def from_c_struct(c_struct: NodeInfoC) -> "NodeInfoT":
        """
        Convert from C structure to Python data class

        Args:
            c_struct: C structure NodeInfoC

        Returns:
            NodeInfoT: Python data class instance
        """
        numa_infos_list = iterate_c_array(
            c_struct.numa_infos,
            c_struct.numa_len,
            NumaInfo,
            NumaInfoT.from_c_struct
        )

        return NodeInfoT(
            node_id=c_struct.node_id.decode('utf-8', errors='ignore'),
            numa_infos=numa_infos_list,
            is_current=c_struct.is_current
        )

    def __str__(self) -> str:
        numa_info_strs = [str(info) for info in self.numa_infos]
        return f"nodeId={self.node_id}, isCurrent={self.is_current}, numaInfos=[{', '.join(numa_info_strs)}]"


@dataclass
class BorrowParamT:
    """
    Memory borrow parameter data class

    Corresponds to C++ structure BorrowParam, containing source node ID, NUMA meta information array, and borrow size.
    Used for memory borrow strategy calculation.

    Attributes:
        node_id: Source node ID
        numa_meta_infos: NUMA meta information list
        borrow_size: Borrow size (MB)
    """
    node_id: str
    numa_meta_infos: List[NumaMetaInfoT]
    borrow_size: int

    @staticmethod
    def from_c_struct(c_struct: MemBorrowParamC) -> "BorrowParamT":
        """
        Convert from C structure to Python data class

        Args:
            c_struct: C structure MemBorrowParamC

        Returns:
            BorrowParamT: Python data class instance
        """
        numa_meta_infos_list = iterate_c_array(
            c_struct.numa_meta_infos,
            c_struct.numa_len,
            NumaMetaInfoC,
            NumaMetaInfoT.from_c_struct
        )

        return BorrowParamT(
            node_id=c_struct.src_nid.decode('utf-8', errors='ignore'),
            numa_meta_infos=numa_meta_infos_list,
            borrow_size=c_struct.borrow_size
        )

    def __str__(self) -> str:
        numa_meta_strs = [f"{info.numa_id}:{info.socket_id}" for info in self.numa_meta_infos]
        return f"nodeId={self.node_id}, numaMetaInfos={','.join(numa_meta_strs)}, borrowSize={self.borrow_size}"


@dataclass
class MemBorrowResultT:
    """
    Memory borrow result data class

    Corresponds to C structure mem_borrow_result_c, containing borrowed ID list, current NUMA ID list, and task ID.
    Used for returning memory borrow execution results.

    Attributes:
        borrow_ids: Borrowed ID list
        present_numa_ids: Current NUMA ID list
        task_id: Task ID
    """
    borrow_ids: List[str]
    present_numa_ids: List[int]
    task_id: str

    @staticmethod
    def from_c_struct(c_struct: MemBorrowResult) -> "MemBorrowResultT":
        """
        Convert from C structure to Python data class

        Args:
            c_struct: C structure MemBorrowResult

        Returns:
            MemBorrowResultT: Python data class instance
        """
        borrow_ids = []
        for i in range(c_struct.borrow_ids_num):
            borrow_id_bytes = c_struct.borrow_ids[i]
            id_str = borrow_id_bytes.decode('utf-8', errors='ignore').rstrip('\x00')
            if id_str:
                borrow_ids.append(id_str)

        present_numa_ids = []
        for i in range(c_struct.present_numa_ids_num):
            numa_id = int(c_struct.present_numa_ids[i])
            present_numa_ids.append(numa_id)

        task_id = c_struct.task_id.decode('utf-8', errors='ignore').rstrip('\x00')

        return MemBorrowResultT(
            borrow_ids=borrow_ids,
            present_numa_ids=present_numa_ids,
            task_id=task_id
        )

    def __str__(self) -> str:
        return f"taskId={self.task_id}, borrowIdsCount={len(self.borrow_ids)}, " \
               f"presentNumaIdsCount={len(self.present_numa_ids)}"


@dataclass
class NumaQuotaT:
    """
    NUMA quota data class

    Corresponds to C++ structure NumaQuota, containing NUMA ID and quota value.
    Used for page swap configuration.

    Attributes:
        numa_id: NUMA ID
        quota: Quota value
    """
    numa_id: int
    quota: int

    @staticmethod
    def from_c_struct(c_struct: NumaQuotaC) -> "NumaQuotaT":
        """
        Convert from C structure to Python data class

        Args:
            c_struct: C structure NumaQuotaC

        Returns:
            NumaQuotaT: Python data class instance
        """
        return NumaQuotaT(
            numa_id=c_struct.numa_id,
            quota=c_struct.quota
        )

    def __str__(self) -> str:
        return f"numaId={self.numa_id}, quota={self.quota}"


@dataclass
class PageSwapPairT:
    """
    Page swap pair data class

    Corresponds to C++ structure PageSwapPair, containing local and remote NUMA quota arrays.
    Used for page swap enable configuration.

    Attributes:
        local_numa_quotas: Local NUMA quota list
        remote_numa_quotas: Remote NUMA quota list
    """
    local_numa_quotas: List[NumaQuotaT]
    remote_numa_quotas: List[NumaQuotaT]

    @staticmethod
    def from_c_struct(c_struct: PageSwapPairC) -> "PageSwapPairT":
        """
        Convert from C structure to Python data class

        Args:
            c_struct: C structure PageSwapPairC

        Returns:
            PageSwapPairT: Python data class instance
        """
        local_quotas = iterate_c_array(
            c_struct.local_numas,
            c_struct.local_numa_len,
            NumaQuotaC,
            NumaQuotaT.from_c_struct
        )

        remote_quotas = iterate_c_array(
            c_struct.remote_numas,
            c_struct.remote_numa_len,
            NumaQuotaC,
            NumaQuotaT.from_c_struct
        )

        return PageSwapPairT(
            local_numa_quotas=local_quotas,
            remote_numa_quotas=remote_quotas
        )

    def __str__(self) -> str:
        local_strs = [str(quota) for quota in self.local_numa_quotas]
        remote_strs = [str(quota) for quota in self.remote_numa_quotas]
        return f"localNumaQuotas={'; '.join(local_strs)}; remoteNumaQuotas={'; '.join(remote_strs)}"
