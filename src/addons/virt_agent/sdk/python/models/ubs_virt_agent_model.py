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
from typing import List, Dict
import ctypes
from ubse.ffi.ubs_virt_agent_types import NumaInfoHugePageData, NumaInfo, VmNumaInfo, VmMetaData, VmInfo, \
    CaseConfInfo, TaskInfo


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
