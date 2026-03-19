#!/usr/bin/env python
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
import shutil
import os
from setuptools import setup

TEMP_DIR = "temp_ubse"
if os.path.exists(TEMP_DIR):
    shutil.rmtree(TEMP_DIR)
shutil.copytree("src/sdk/python", TEMP_DIR, dirs_exist_ok=True)
shutil.copytree("src/addons/virt_agent/sdk/python", TEMP_DIR, dirs_exist_ok=True)

TEMP_FFI_DIR = "temp_ubse_ffi"
shutil.copytree("src/sdk/python/ffi", TEMP_FFI_DIR, dirs_exist_ok=True)
shutil.copytree("src/addons/virt_agent/sdk/python/ffi", TEMP_FFI_DIR, dirs_exist_ok=True)

TEMP_MODELS_DIR = "temp_ubse_models"
shutil.copytree("src/sdk/python/models", TEMP_MODELS_DIR, dirs_exist_ok=True)
shutil.copytree("src/addons/virt_agent/sdk/python/models", TEMP_MODELS_DIR, dirs_exist_ok=True)

setup(
    name="ubse",
    version="1.0.0",
    package_dir={"ubse": TEMP_DIR,
                 "ubse.ffi": TEMP_FFI_DIR,
                 "ubse.models": TEMP_MODELS_DIR},
    packages=["ubse", "ubse.ffi", "ubse.models"],
)