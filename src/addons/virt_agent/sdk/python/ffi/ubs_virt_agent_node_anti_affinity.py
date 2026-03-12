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
from typing import Dict, List, Union

from ubse.ffi.ubs_virt_agent_base import UbsVirtAgentBase
from ubse.ffi.ubs_virt_agent_types import KeyValuePair, NodeAntiDictionary, VIRT_MAX_NODE_ID_LENGTH, MAX_NODE_NUM


class UbsVirtAgentNodeAntiAffinity(UbsVirtAgentBase):

    def __init__(self):
        super().__init__()
        self._setup_anti_affinity_functions()

    def ubs_node_anti_affinity(self, node_anti_dict: Union[NodeAntiDictionary, Dict[str, List[str]]]) -> int:
        """
        Calls the C++ function ubs_virt_agent_mem_fragmentation_node_anti_affinity

        Args:
            node_anti_dict (Union[NodeAntiDictionary, Dict[str, List[str]]]):
                Anti-affinity configuration parameter, can be a NodeAntiDictionary object or a dictionary
                Example: {"1": ["2"], "2": ["3"], "3": ["4"], "4": ["1"]}

        Returns:
            int: Call result, 0 indicates success, non-zero indicates failure
        """
        if not self.lib_ubse:
            raise ConnectionError("Native library not loaded")
        if isinstance(node_anti_dict, dict):
            node_anti_dict = self._dict_to_node_anti_dict(node_anti_dict)

        if not isinstance(node_anti_dict, NodeAntiDictionary):
            raise TypeError("node_anti_dict must be a NodeAntiDictionary object or a dictionary")

        result = self.lib_ubse.ubs_virt_agent_mem_fragmentation_node_anti_affinity(ctypes.byref(node_anti_dict))
        self.ubs_virt_agent_handle_result(result, "ubs_node_anti_affinity")
        return result

    def _dict_to_node_anti_dict(self, node_anti_affinity_map: Dict[str, List[str]]) -> NodeAntiDictionary:
        entries = []
        for key, values in node_anti_affinity_map.items():
            if len(key.encode('utf-8')) >= VIRT_MAX_NODE_ID_LENGTH:
                raise ValueError(f"key '{key}' exceeds or equals maximum length of {VIRT_MAX_NODE_ID_LENGTH}")
            c_key = key.encode('utf-8') + b'\0'

            value_count = len(values)
            if value_count > MAX_NODE_NUM:
                raise ValueError(f"values for key '{key}' exceed maximum count of {MAX_NODE_NUM}")
            c_values = ((ctypes.c_char * VIRT_MAX_NODE_ID_LENGTH) * MAX_NODE_NUM)()
            for i, value in enumerate(values):
                if len(value.encode('utf-8')) >= VIRT_MAX_NODE_ID_LENGTH:
                    raise ValueError(f"value '{value}' exceeds or equals maximum length of {VIRT_MAX_NODE_ID_LENGTH}")
                c_values[i] = ctypes.create_string_buffer(value.encode('utf-8'), VIRT_MAX_NODE_ID_LENGTH)

            entry = KeyValuePair()
            entry.key = c_key
            entry.value = c_values
            entry.value_count = value_count

            entries.append(entry)

        node_anti_dict = NodeAntiDictionary()
        node_anti_dict.entries = (KeyValuePair * MAX_NODE_NUM)(*entries)
        node_anti_dict.entry_count = len(entries)

        return node_anti_dict

    def _setup_anti_affinity_functions(self):
        self.lib_ubse.ubs_virt_agent_mem_fragmentation_node_anti_affinity.argtypes = [
            ctypes.POINTER(NodeAntiDictionary)
        ]
        self.lib_ubse.ubs_virt_agent_mem_fragmentation_node_anti_affinity.restype = ctypes.c_int32

        if hasattr(self.lib_ubse, 'free'):
            self.lib_ubse.free.argtypes = [ctypes.c_void_p]
            self.lib_ubse.free.restype = None
