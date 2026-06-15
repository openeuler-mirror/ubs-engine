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
import ctypes

HOST_NAME_MAX = 64
NODE_ID_MAX = 48
UUID_MAX = 37
NAME_MAX = 128
STATE_MAX = 128
CUR_CASE_MAX = 128
OVERCOMMITMENT_RATIO_MAX = 128
MIGRATE_WATER_LINE_MAX = 128
MSG_MAX_LENGTH = 128
VIRT_MAX_NODE_ID_LENGTH = 48
MAX_BORROW_ID_COUNT = 2000
MAX_VM_NUM = 300
MAX_BORROW_ID_LENGTH = 128
# Memory Borrow (Big Memory VM) structures from mem_fragmentation_msg.h
MAX_NUMA_META_INFO_NUM = 32
# Node Info List structures from mem_fragmentation_msg.h
MAX_NODE_INFO_NUM = 32
# Page Swap structures
MAX_PAGE_SWAP_PAIR_NUM = 32
MAX_NUMA_QUOTA_NUM = 32


class NumaInfoHugePageData(ctypes.Structure):
    _fields_ = [
        ("pageSize", ctypes.c_uint64),
        ("hugePageTotal", ctypes.c_uint64),
        ("hugePageFree", ctypes.c_uint64),
    ]


class NumaInfo(ctypes.Structure):
    _fields_ = [
        ("timestamp", ctypes.c_int64),
        ("nodeId", ctypes.c_char * NODE_ID_MAX),
        ("hostName", ctypes.c_char * HOST_NAME_MAX),
        ("numaId", ctypes.c_int16),
        ("socketId", ctypes.c_int16),
        ("isLocal", ctypes.c_bool),
        ("memTotal", ctypes.c_int64),
        ("memFree", ctypes.c_int64),
        ("numaPageInfo", ctypes.POINTER(NumaInfoHugePageData)),
        ("numaPageInfoCount", ctypes.c_uint64),
    ]


class VmNumaInfo(ctypes.Structure):
    _fields_ = [
        ("numaId", ctypes.c_int16),
        ("socketId", ctypes.c_int16),
        ("isLocal", ctypes.c_bool),
        ("pageSize", ctypes.c_uint64),
        ("usedMem", ctypes.c_int64),
    ]


class VmMetaData(ctypes.Structure):
    _fields_ = [
        ("nodeId", ctypes.c_char * NODE_ID_MAX),
        ("hostName", ctypes.c_char * HOST_NAME_MAX),
        ("uuid", ctypes.c_char * UUID_MAX),
        ("name", ctypes.c_char * NAME_MAX),
        ("state", ctypes.c_char * STATE_MAX),
        ("vmCreateTime", ctypes.c_int64),
        ("maxMem", ctypes.c_uint64),
        ("pid", ctypes.c_int32),
    ]


class VmInfo(ctypes.Structure):
    _fields_ = [
        ("timestamp", ctypes.c_int64),
        ("metadata", VmMetaData),
        ("numaInfo", ctypes.POINTER(VmNumaInfo)),
        ("numaInfoCount", ctypes.c_uint64),
    ]


class CaseConfInfo(ctypes.Structure):
    _fields_ = [
        ("cur_case", ctypes.c_char * CUR_CASE_MAX),
        ("over_commitment_ratio", ctypes.c_char * OVERCOMMITMENT_RATIO_MAX),
        ("migrate_water_line", ctypes.c_char * MIGRATE_WATER_LINE_MAX),
        ("index", ctypes.c_uint64),
        ("host_id", ctypes.c_char * NODE_ID_MAX),
    ]


class CaseConfSetInfo(ctypes.Structure):
    _fields_ = [
        ("ret", ctypes.c_uint64),
    ]


class VmStrategyInfo(ctypes.Structure):
    _fields_ = [
        ("destNumaId", ctypes.c_uint16),
        ("memSize", ctypes.c_uint64),
        ("pid", ctypes.c_int32)
    ]


class MigrateExecuteSrcParam(ctypes.Structure):
    _fields_ = [
        ("borrowInNode", ctypes.c_char * VIRT_MAX_NODE_ID_LENGTH),
        ("borrowIds", (ctypes.c_char * MAX_BORROW_ID_LENGTH) * MAX_BORROW_ID_COUNT),
        ("borrowIdsCount", ctypes.c_uint32),
        ("vmInfoList", VmStrategyInfo * MAX_VM_NUM),
        ("vmInfoListSize", ctypes.c_uint32),
        ("waiting_time", ctypes.c_uint64)
    ]


class MigrateStrategyVmInfo(ctypes.Structure):
    _fields_ = [
        ("pid", ctypes.c_int32),
        ("ratio", ctypes.c_uint16)
    ]


class MigrateStrategySrcParam(ctypes.Structure):
    _fields_ = [
        ("borrowSize", ctypes.c_uint64),
        ("borrowInNode", ctypes.c_char * VIRT_MAX_NODE_ID_LENGTH),
        ("vmInfoList", MigrateStrategyVmInfo * MAX_VM_NUM),
        ("vmInfoListSize", ctypes.c_uint32)
    ]


class VmMigrateStrategy(ctypes.Structure):
    _fields_ = [
        ("dest_numa_id", ctypes.c_uint16),
        ("mem_size", ctypes.c_uint64),
        ("pid", ctypes.c_int32)
    ]


class MemMigrateStrategy(ctypes.Structure):
    _fields_ = [
        ("vm_info_list_size", ctypes.c_uint32),
        ("vm_info_list", ctypes.POINTER(VmMigrateStrategy)),
        ("waiting_time", ctypes.c_uint64)
    ]


# Borrow Strategy
MAX_DEST_PARAM_SIZE = 2000
MAX_DEST_NUMA_NUM = 1


class DstNumaInfo(ctypes.Structure):
    _fields_ = [
        ("host_id", ctypes.c_char * VIRT_MAX_NODE_ID_LENGTH),
        ("socket_id", ctypes.c_uint16),
        ("numa_nums", ctypes.c_uint16),
        ("numa_ids", ctypes.c_int * MAX_DEST_NUMA_NUM),
        ("mem_sizes", ctypes.c_uint64 * MAX_DEST_NUMA_NUM)
    ]


class BorrowStrategy(ctypes.Structure):
    _fields_ = [
        ("src_host_id", ctypes.c_char * VIRT_MAX_NODE_ID_LENGTH),
        ("src_socket_id", ctypes.c_int16),
        ("src_numa_id", ctypes.c_int16),
        ("borrow_size", ctypes.c_uint64),
        ("dest_numa_infos", DstNumaInfo * MAX_DEST_PARAM_SIZE),
        ("dest_numa_infos_size", ctypes.c_uint32)
    ]


class MemBorrowSetting(ctypes.Structure):
    _fields_ = [
        ("borrow_strategy", BorrowStrategy),
        ("is_async", ctypes.c_bool)
    ]


# Borrow Execute
MEM_TASK_ID_MAX = 256


class MemBorrowResult(ctypes.Structure):
    _fields_ = [
        ("borrow_ids", (ctypes.c_char * MAX_BORROW_ID_LENGTH) * MAX_BORROW_ID_COUNT),
        ("present_numa_ids", ctypes.c_uint16 * MAX_BORROW_ID_COUNT),
        ("borrow_ids_num", ctypes.c_uint32),
        ("present_numa_ids_num", ctypes.c_uint32),
        ("task_id", ctypes.c_char * MEM_TASK_ID_MAX),
    ]


class SrcParam(ctypes.Structure):
    _fields_ = [
        ("src_nid", ctypes.c_char * VIRT_MAX_NODE_ID_LENGTH),
        ("src_socket_id", ctypes.c_int16),
        ("src_numa_id", ctypes.c_int16),
        ("borrow_size", ctypes.c_uint64),
    ]


MAX_NODE_NUM = 32


# Node Anti Affinity
class KeyValuePair(ctypes.Structure):
    _fields_ = [
        ("key", ctypes.c_char * VIRT_MAX_NODE_ID_LENGTH),  # char[] key
        ("value", (ctypes.c_char * VIRT_MAX_NODE_ID_LENGTH) * MAX_NODE_NUM),  # char[][] value
        ("value_count", ctypes.c_size_t)  # size_t value_count
    ]


class NodeAntiDictionary(ctypes.Structure):
    _fields_ = [
        ("entries", KeyValuePair * MAX_NODE_NUM),  # struct KeyValuePair[] entries
        ("entry_count", ctypes.c_size_t)  # size_t entry_count
    ]


class RollbackSrcParam(ctypes.Structure):
    _fields_ = [
        ("node_id", ctypes.c_char * VIRT_MAX_NODE_ID_LENGTH),
        ("borrow_id_list", (ctypes.c_char * MAX_BORROW_ID_LENGTH) * MAX_BORROW_ID_COUNT),
        ("borrow_id_size", ctypes.c_uint32)
    ]


class TaskInfo(ctypes.Structure):
    _fields_ = [
        ("task_id", ctypes.c_char * MEM_TASK_ID_MAX),
        ("status", ctypes.c_int32),
        ("result_code", ctypes.c_uint32),
        ("mem_borrow_result", MemBorrowResult)
    ]


class NodeInfoC(ctypes.Structure):
    """
    C structure for node_info_s.

    Represents node information including node ID, NUMA information array,
    and a flag indicating if this is the current node.
    Used in node info list for memory fragmentation queries.

    """
    _fields_ = [
        ("node_id", ctypes.c_char * VIRT_MAX_NODE_ID_LENGTH),
        ("numa_infos", ctypes.POINTER(NumaInfo)),
        ("numa_len", ctypes.c_uint32),
        ("is_current", ctypes.c_bool)
    ]


class NodeInfoListC(ctypes.Structure):
    """
    C structure for node_info_list_s.

    Contains an array of node_info_s structures and the length.
    Represents the result of node information list query.
    """
    _fields_ = [
        ("node_infos", ctypes.POINTER(NodeInfoC)),
        ("node_len", ctypes.c_uint32)
    ]

    def free_c_memory(self, free_func):
        """
        Free memory occupied by this C structure.

        Args:
            free_func: A callable used to free C-allocated memory (e.g., libc.free).
                       Should accept a ctypes pointer or c_void_p argument.
        """
        if self.node_infos:
            for i in range(self.node_len):
                node_info = self.node_infos[i]
                if node_info.numa_infos:
                    for j in range(node_info.numa_len):
                        numa_info = node_info.numa_infos[j]
                        if numa_info.numaPageInfo:
                            free_func(numa_info.numaPageInfo)
                    free_func(node_info.numa_infos)
            free_func(self.node_infos)
            self.node_infos = None


# Memory Borrow (Big Memory VM) additional structures
class NumaMetaInfoC(ctypes.Structure):
    """
    C structure for numa_meta_info_s.

    Represents NUMA metadata information including socket ID and NUMA ID.
    Used in memory borrow parameter structure.
    """
    _fields_ = [
        ("numa_id", ctypes.c_int16)
    ]


class MemBorrowParamC(ctypes.Structure):
    """
    C structure for mem_borrow_param_s.
    
    Represents memory borrow parameters including source node ID,
    NUMA metadata information array, and borrow size.
    """
    _fields_ = [
        ("src_nid", ctypes.c_char * VIRT_MAX_NODE_ID_LENGTH),
        ("numa_meta_infos", ctypes.POINTER(NumaMetaInfoC)),
        ("numa_len", ctypes.c_uint32),
        ("borrow_size", ctypes.c_uint64)
    ]


class MemBorrowResultC(ctypes.Structure):
    """
    C structure for mem_borrow_result_s.
    
    Contains an array of mem_borrow_result_c structures and the length.
    Represents the result of memory borrow operation.
    """
    _fields_ = [
        ("mem_borrow_result_list", ctypes.POINTER(MemBorrowResult)),
        ("mem_borrow_result_list_len", ctypes.c_uint16)
    ]

    def free_c_memory(self, free_func):
        """
        Free memory occupied by this C structure.

        Args:
            free_func: A callable used to free C-allocated memory (e.g., libc.free).
                       Should accept a ctypes pointer or c_void_p argument.
        """
        if self.mem_borrow_result_list:
            free_func(self.mem_borrow_result_list)
            self.mem_borrow_result_list = None


class NumaQuotaC(ctypes.Structure):
    """
    C structure for numa_quota_s.
    
    Represents NUMA quota information with NUMA ID and quota value.
    """
    _fields_ = [
        ("numa_id", ctypes.c_uint32),
        ("quota", ctypes.c_uint32)
    ]


class PageSwapPairC(ctypes.Structure):
    """
    C structure for page_swap_pair_s.
    
    Contains local and remote NUMA quota arrays for page swap configuration.
    """
    _fields_ = [
        ("local_numas", ctypes.POINTER(NumaQuotaC)),
        ("local_numa_len", ctypes.c_uint8),
        ("remote_numas", ctypes.POINTER(NumaQuotaC)),
        ("remote_numa_len", ctypes.c_uint8)
    ]


class PageSwapEnableC(ctypes.Structure):
    """
    C structure for page_swap_enable_s.
    
    Contains an array of page swap pair configurations.
    """
    _fields_ = [
        ("page_swap_pairs", ctypes.POINTER(PageSwapPairC)),
        ("page_swap_pairs_len", ctypes.c_uint8)
    ]