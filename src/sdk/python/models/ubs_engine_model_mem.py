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

from ubse.ffi.ubs_engine_types import UbsMemNumaDescT
from ubse.models.ubs_engine_model_topo import UbsTopoNode


@dataclass
class UbsMemNumaDesc:
    name: str
    numaid: int
    export_node: 'UbsTopoNode'
    import_node: 'UbsTopoNode'
    size: int
    usr_info: bytes = b''

    def __str__(self):
        usr_info_hex = self.usr_info.hex() if self.usr_info else "空"
        return (
            f"内存借用 '{self.name}': "
            f"NUMA {self.numaid}, 大小 {self.size} bytes, "
            f"用户信息: {usr_info_hex}"
        )

    @staticmethod
    def from_c_struct(c_desc: UbsMemNumaDescT) -> "UbsMemNumaDesc":
        # 将c_uint8数组转换为Python bytes对象
        usr_info_bytes = bytes(c_desc.usr_info)

        return UbsMemNumaDesc(
            name=c_desc.name.decode('utf-8', errors='ignore'),
            numaid=c_desc.numaid,
            export_node=UbsTopoNode.from_c_struct(c_desc.export_node),
            import_node=UbsTopoNode.from_c_struct(c_desc.import_node),
            size=c_desc.size,
            usr_info=usr_info_bytes
        )

    def to_dict(self) -> dict:
        """转换为字典格式"""
        usr_info_hex = self.usr_info.hex() if self.usr_info else ""

        return {
            'name': self.name,
            'numaid': self.numaid,
            'export_node': self.export_node.to_dict() if hasattr(self.export_node, 'to_dict') else str(
                self.export_node),
            'import_node': self.import_node.to_dict() if hasattr(self.import_node, 'to_dict') else str(
                self.import_node),
            'size': self.size,
            'usr_info': usr_info_hex
        }