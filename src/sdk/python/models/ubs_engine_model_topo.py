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
from dataclasses import dataclass

import ipaddress
import socket
import struct
from ubse.ffi.ubs_engine_types import UbsTopoLinkT, UbsTopoNodeT, UbsTopoIpAddressT, UBS_TOPO_SOCKET_NUM, \
    UBS_TOPO_NUMA_NUM, UBS_TOPO_IPADDR_NUM



@dataclass
class UbsTopoNode:
    slot_id: int
    socket_ids: list  # List[int]
    numa_ids: list  # List[List[int]] 二维数组
    ips: list  # List[Union[ipaddress.IPv4Address, ipaddress.IPv6Address]]
    host_name: str

    def __str__(self):
        # 构建IP地址的字符串表示
        ip_strings = []
        for ip in self.ips:
            ip_strings.append(f"IPv{ip.version}:{ip}")
        ips_str = ", ".join(ip_strings)

        return (
            f"Node {self.slot_id} ('{self.host_name}'): "
            f"{len(self.socket_ids)} sockets, "
            f"IPs: [{ips_str}]"
        )

    @staticmethod
    def from_c_struct(c_node) -> "UbsTopoNode":
        # 转换socket_id一维数组
        socket_ids = [c_node.socket_id[i] for i in range(UBS_TOPO_SOCKET_NUM)]

        # 转换numa_ids二维数组
        numa_ids = []
        for i in range(UBS_TOPO_SOCKET_NUM):
            numa_row = [c_node.numa_ids[i][j] for j in range(UBS_TOPO_NUMA_NUM)]
            numa_ids.append(numa_row)

        # 转换IP地址数组 - 使用标准库的ipaddress
        ips = []
        for i in range(UBS_TOPO_IPADDR_NUM):
            if c_node.ips[i].af != 0:
                ip_obj = UbsTopoNode._convert_ip_from_c_struct(c_node.ips[i])
                if ip_obj is not None:
                    ips.append(ip_obj)

        # 转换主机名
        hostname = c_node.host_name.decode('utf-8', errors='ignore')

        return UbsTopoNode(
            slot_id=c_node.slot_id,
            socket_ids=socket_ids,
            numa_ids=numa_ids,
            ips=ips,
            host_name=hostname
        )

    @staticmethod
    def _convert_ip_from_c_struct(c_ip):
        """将C结构体的IP地址转换为Python标准库的IP地址对象"""
        try:
            if c_ip.af == socket.AF_INET:
                # IPv4地址处理
                ipv4_int = c_ip.ipv4
                # 将32位整数转换为点分十进制
                ipv4_str = socket.inet_ntoa(struct.pack('<I', ipv4_int))
                return ipaddress.IPv4Address(ipv4_str)
            elif c_ip.af == socket.AF_INET6:
                # IPv6地址处理
                ipv6_bytes = bytes(c_ip.ipv6)
                # 检查是否为有效IPv6地址（非全零）
                if any(ipv6_bytes):  # 如果任何字节不为0
                    ipv6_str = socket.inet_ntop(socket.AF_INET6, ipv6_bytes)
                    return ipaddress.IPv6Address(ipv6_str)
            return None
        except (ValueError, OSError, struct.error):
            return None


@dataclass
class UbsTopoLink:
    slot_id: int
    socket_id: int
    port_id: int
    peer_slot_id: int
    peer_socket_id: int
    peer_port_id: int

    def __str__(self):
        # 用户友好输出，可简化
        return (f"Link : {self.slot_id}-{self.socket_id}-{self.port_id} <->"
                f" {self.peer_slot_id}-{self.peer_socket_id}-{self.peer_port_id}")

    @staticmethod
    def from_c_struct(c_link: UbsTopoLinkT) -> "UbsTopoLink":
        return UbsTopoLink(
            slot_id=c_link.slot_id,
            socket_id=c_link.socket_id,
            port_id=c_link.port_id,
            peer_slot_id=c_link.peer_slot_id,
            peer_socket_id=c_link.peer_socket_id,
            peer_port_id=c_link.peer_port_id
        )
