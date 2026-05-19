#  Copyright (c) Huawei Technologies Co., Ltd. $tody.year. All rights reserved.
#
#  virtagent is licensed under Mulan PSL v2.
#  You can use this software according to the terms and conditions of the Mulan PSL v2.
#  You may obtain a copy of Mulan PSL v2 at:
#           http://license.coscl.org.cn/MulanPSL2
#  THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
#  EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
#  MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
#  See the Mulan PSL v2 for more details.
#
#  virtagent is licensed under Mulan PSL v2.
#  You can use this software according to the terms and conditions of the Mulan PSL v2.
#  You may obtain a copy of Mulan PSL v2 at:
#           http://license.coscl.org.cn/MulanPSL2
#  THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
#  EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
#  MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
#  See the Mulan PSL v2 for more details.

#!/usr/bin/python3
import ctypes
from typing import List
from ubse.models.ubs_virt_agent_model import PageSwapPairT
from ubse.ffi.ubs_virt_agent_base import UbsVirtAgentBase
from ubse.ffi.ubs_virt_agent_types import PageSwapEnableC, PageSwapPairC, NumaQuotaC


class UbsVirtAgentPageSwapEnable(UbsVirtAgentBase):
    """Page swap enable configuration interface"""

    def __init__(self):
        super().__init__()
        self._setup_page_swap_enable_functions()

    def ubs_page_swap_enable(self, pid: int, page_swap_enable: List[PageSwapPairT]) -> int:
        """
        Enable page swap functionality

        Configure NUMA page swap policy for specified process, setting quotas for local and remote NUMA nodes,
        optimizing cross-NUMA node memory access performance.

        Args:
            pid (int): Process ID
            page_swap_enable (List[PageSwapPairT]): Page swap pair list, each element contains:
                - local_numa_quotas: Local NUMA quota list (numa_id, quota)
                - remote_numa_quotas: Remote NUMA quota list (numa_id, quota)

        Returns:
            int: Call result, 0 indicates success, non-zero indicates failure

        Raises:
            ConnectionError: Raised when native library is not loaded
            TypeError: Raised when parameter type is incorrect
            Exception: Raised when configuration fails
        """
        if not self.lib_ubse:
            raise ConnectionError("Native library not loaded")

        if not isinstance(pid, int):
            raise TypeError(f"pid must be int, got {type(pid).__name__}")

        if not isinstance(page_swap_enable, list):
            raise TypeError(f"page_swap_enable must be list, got {type(page_swap_enable).__name__}")

        page_swap_enable_c = self._convert_page_swap_enable_to_c(page_swap_enable)

        result = self.lib_ubse.ubs_virt_agent_page_swap_enable(
            ctypes.c_int32(pid),  # Use c_int32 to match pid_t
            ctypes.byref(page_swap_enable_c)
        )

        self.ubs_virt_agent_handle_result(result, "ubs_page_swap_enable")

        return result

    @staticmethod
    def _convert_page_swap_enable_to_c(page_swap_enable: List[PageSwapPairT]) -> PageSwapEnableC:
        """
        Convert Python page swap configuration to C structure

        Args:
            page_swap_enable (List[PageSwapPairT]): Python page swap configuration list

        Returns:
            PageSwapEnableC: C structure page swap configuration
        """
        page_swap_enable_c = PageSwapEnableC()
        page_swap_enable_c.page_swap_pairs_len = len(page_swap_enable)

        if page_swap_enable:
            page_swap_pair_array = (PageSwapPairC * len(page_swap_enable))()
            
            for i, swap_pair in enumerate(page_swap_enable):
                page_swap_pair_array[i].local_numa_len = len(swap_pair.local_numa_quotas)
                page_swap_pair_array[i].remote_numa_len = len(swap_pair.remote_numa_quotas)

                if swap_pair.local_numa_quotas:
                    local_quota_array = (NumaQuotaC * len(swap_pair.local_numa_quotas))()
                    for j, quota in enumerate(swap_pair.local_numa_quotas):
                        local_quota_array[j].numa_id = quota.numa_id
                        local_quota_array[j].quota = quota.quota
                    page_swap_pair_array[i].local_numas = ctypes.cast(
                        local_quota_array,
                        ctypes.POINTER(NumaQuotaC)
                    )

                if swap_pair.remote_numa_quotas:
                    remote_quota_array = (NumaQuotaC * len(swap_pair.remote_numa_quotas))()
                    for j, quota in enumerate(swap_pair.remote_numa_quotas):
                        remote_quota_array[j].numa_id = quota.numa_id
                        remote_quota_array[j].quota = quota.quota
                    page_swap_pair_array[i].remote_numas = ctypes.cast(
                        remote_quota_array,
                        ctypes.POINTER(NumaQuotaC)
                    )

            page_swap_enable_c.page_swap_pairs = ctypes.cast(
                page_swap_pair_array,
                ctypes.POINTER(PageSwapPairC)
            )
        else:
            page_swap_enable_c.page_swap_pairs = None

        return page_swap_enable_c

    def _free_page_swap_enable_c(self, page_swap_enable_c: PageSwapEnableC):
        """
        Free memory occupied by page swap C structure

        Args:
            page_swap_enable_c (PageSwapEnableC): C structure page swap configuration
        """
        if hasattr(self.lib_ubse, 'free') and page_swap_enable_c.page_swap_pairs:
            pairs_len = page_swap_enable_c.page_swap_pairs_len
            for i in range(pairs_len):
                pair_ptr = ctypes.cast(
                    ctypes.addressof(page_swap_enable_c.page_swap_pairs.contents) + 
                    i * ctypes.sizeof(PageSwapPairC),
                    ctypes.POINTER(PageSwapPairC)
                )
                if pair_ptr.contents.local_numas:
                    self.lib_ubse.free(pair_ptr.contents.local_numas)
                if pair_ptr.contents.remote_numas:
                    self.lib_ubse.free(pair_ptr.contents.remote_numas)
            self.lib_ubse.free(page_swap_enable_c.page_swap_pairs)

    def _setup_page_swap_enable_functions(self):
        """Setup function prototypes for page swap enable"""
        self.lib_ubse.ubs_virt_agent_page_swap_enable.argtypes = [
            ctypes.c_int32,  # pid_t is typically int32_t on Linux
            ctypes.POINTER(PageSwapEnableC)
        ]
        self.lib_ubse.ubs_virt_agent_page_swap_enable.restype = ctypes.c_int32

        if hasattr(self.lib_ubse, 'free'):
            self.lib_ubse.free.argtypes = [ctypes.c_void_p]
            self.lib_ubse.free.restype = None
