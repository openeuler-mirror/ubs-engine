#!/usr/bin/python3
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MatrixEngine is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
import ctypes

UBSE_MAX_LINK_ID_LENGTH = 65


class UbsTopoLinkT(ctypes.Structure):
    _fields_ = [
        ("slot_id", ctypes.c_uint32),
        ("socket_id", ctypes.c_uint32),
        ("port_id", ctypes.c_uint32),
        ("peer_slot_id", ctypes.c_uint32),
        ("peer_socket_id", ctypes.c_uint32),
        ("peer_port_id", ctypes.c_uint32),
    ]

    def __str__(self):
        return (
            f"ubse_cpu_topo_info(\n"
            f"  slot id={self.slot_id}\n"
            f"  socket ids={self.socket_id}\n"
            f"  port id={self.port_id}\n"
            f"  peer slot id={self.peer_slot_id}\n"
            f"  peer socket ids={self.peer_socket_id}\n"
            f"  peer port id={self.peer_port_id}\n"
            f")"
        )


UBS_TOPO_SOCKET_NUM = 2
UBS_TOPO_NUMA_NUM = 4
UBS_TOPO_IPADDR_NUM = 50
HOST_NAME_MAX = 64


# 定义IP地址结构体
class UbsTopoIpAddressT(ctypes.Structure):
    _fields_ = [
        ("af", ctypes.c_int32),                    # 地址族
        ("ipv4", ctypes.c_uint32),                 # IPv4地址（网络字节序）
        ("ipv6", ctypes.c_uint8 * 16),             # IPv6地址（16字节）
    ]

    def __str__(self):
        import socket
        import struct

        if self.af == socket.AF_INET:  # IPv4
            ip_str = socket.inet_ntop(socket.AF_INET, struct.pack("!I", self.ipv4))
        elif self.af == socket.AF_INET6:  # IPv6
            ip_str = socket.inet_ntop(socket.AF_INET6, bytes(self.ipv6))
        else:
            ip_str = "Unknown AF"

        return f"ubs_topo_ip_address_t(af={self.af}, ip={ip_str})"


# 定义节点拓扑结构体
class UbsTopoNodeT(ctypes.Structure):
    _fields_ = [
        ("slot_id", ctypes.c_uint32),
        ("socket_id", ctypes.c_uint32 * UBS_TOPO_SOCKET_NUM),
        ("numa_ids", (ctypes.c_uint32 * UBS_TOPO_NUMA_NUM) * UBS_TOPO_SOCKET_NUM),
        ("ips", UbsTopoIpAddressT * UBS_TOPO_IPADDR_NUM),
        ("host_name", ctypes.c_char * HOST_NAME_MAX),
    ]

    def __str__(self):
        # 转换socket_id数组
        socket_ids = [self.socket_id[i] for i in range(UBS_TOPO_SOCKET_NUM)]

        # 转换numa_ids二维数组
        numa_list = []
        for i in range(UBS_TOPO_SOCKET_NUM):
            numa_row = [self.numa_ids[i][j] for j in range(UBS_TOPO_NUMA_NUM)]
            numa_list.append(numa_row)

        # 转换IP地址数组
        ip_list = [str(self.ips[i]) for i in range(UBS_TOPO_IPADDR_NUM)]

        # 转换主机名
        hostname = self.host_name.decode('utf-8', errors='ignore')

        return (
            f"ubs_topo_node_t(\n"
            f"  slot_id={self.slot_id}\n"
            f"  socket_id={socket_ids}\n"
            f"  numa_ids={numa_list}\n"
            f"  ips={ip_list}\n"
            f"  host_name='{hostname}'\n"
            f")"
        )

UBSE_MAX_MEM_RESOURCE_NAME_LENGTH = 48
UBSE_MAX_USR_INFO_LENGTH = 32


# C结构体定义
class UbsMemNumaDescT(ctypes.Structure):
    _fields_ = [
        ("name", ctypes.c_char * UBSE_MAX_MEM_RESOURCE_NAME_LENGTH),
        ("numaid", ctypes.c_int64),
        ("export_node", UbsTopoNodeT),
        ("import_node", UbsTopoNodeT),
        ("size", ctypes.c_uint64),
        ("usr_info", ctypes.c_uint8 * UBSE_MAX_USR_INFO_LENGTH)
    ]

    def __str__(self):
        usr_info_hex = ''.join(f'{b:02x}' for b in self.usr_info)
        return (
            f"ubs_mem_numa_desc_t(\n"
            f"  name={self.name.decode('utf-8', errors='ignore')}\n"
            f"  numaid={self.numaid}\n"
            f"  export_node=...\n"
            f"  import_node=...\n"
            f"  size={self.size}\n"
            f"  usr_info={usr_info_hex}\n"
            f")"
        )