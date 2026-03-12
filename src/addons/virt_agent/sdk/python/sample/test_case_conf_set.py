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

from ubse.ffi.ubs_virt_agent_case_conf_set import UbsVirtAgentCaseConfSet


def set_case_conf():
    case_conf_set = UbsVirtAgentCaseConfSet()
    try:
        case_conf_set.ubs_virt_agent_initialize()
        param = {
            "caseType": "overCommitment",
            "overCommitment": 1.25
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