#!/bin/bash

#
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# virtagent is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
#


# 通过修改.lcovrc来配置只扫描vm的ut覆盖率
lcovrc_file=".lcovrc"
sed -i '/include =/d' ${lcovrc_file}
sed -i '/exclude =/d' ${lcovrc_file}
echo "include = src/addons/virt_agent/common" >> ${lcovrc_file}
echo "include = src/addons/virt_agent/common/libvirt" >> ${lcovrc_file}
echo "include = src/addons/virt_agent/common/message" >> ${lcovrc_file}
echo "include = src/addons/virt_agent/common/util" >> ${lcovrc_file}
echo "include = src/addons/virt_agent/default_strategy" >> ${lcovrc_file}
echo "include = src/addons/virt_agent/dto" >> ${lcovrc_file}
echo "include = src/addons/virt_agent/election" >> ${lcovrc_file}
echo "include = src/addons/virt_agent/export/libvirt_helper" >> ${lcovrc_file}
echo "include = src/addons/virt_agent/export/vm_event" >> ${lcovrc_file}
echo "include = src/addons/virt_agent/over_commit/mem_manager" >> ${lcovrc_file}
echo "include = src/addons/virt_agent/status_manager" >> ${lcovrc_file}
echo "exclude = .h" >> ${lcovrc_file}