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
"""ubse 主 Python SDK 构建脚本。

主包仅包含未在 subpackages/*.txt 中声明的模块。子包(SSU/MEM 等)由
build_subpackage.py 通过清单驱动单独构建,扩展 ubse namespace。

新增子包步骤:
    1. 在 src/sdk/python/subpackages/ 下新建 <name>.txt 清单文件
    2. 在 ubs-engine.spec 中加 python3 build_subpackage.py <name> build/install
    3. 在 ubs-engine.spec 中加对应的 %files -n python3-%{name}-<name> 段
    本 setup.py 无需修改,会自动收集所有 manifest 并排除对应文件。
"""
import shutil
import os
from setuptools import setup
from build_subpackage import collect_all_subpackage_files, UBSE_VERSION

# 自动收集所有 subpackages/*.txt 中声明的文件,从主包剔除
SUBPKG_FILES = collect_all_subpackage_files()


def remove_subpackage_files(base_dir, layer):
    """根据 manifest 从 temp 目录删除子包文件

    Args:
        base_dir: 临时目录路径
        layer: 该临时目录对应的 ubse 子层('root' | 'ffi' | 'models' | 'ipc')
    """
    for rel in SUBPKG_FILES:
        parts = rel.split("/")
        if len(parts) == 1 and layer == "root":
            p = os.path.join(base_dir, parts[0])
        elif len(parts) == 2 and parts[0] == layer:
            p = os.path.join(base_dir, parts[1])
        else:
            continue
        if os.path.exists(p):
            os.remove(p)


def reset_dir(path):
    """清理临时目录:若存在则删除,确保后续 copytree 不残留旧文件"""
    if os.path.exists(path):
        shutil.rmtree(path)


TEMP_DIR = "temp_ubse"
reset_dir(TEMP_DIR)
shutil.copytree("src/sdk/python", TEMP_DIR, dirs_exist_ok=True)
shutil.copytree("src/addons/virt_agent/sdk/python", TEMP_DIR, dirs_exist_ok=True)
remove_subpackage_files(TEMP_DIR, "root")

TEMP_FFI_DIR = "temp_ubse_ffi"
reset_dir(TEMP_FFI_DIR)
shutil.copytree("src/sdk/python/ffi", TEMP_FFI_DIR, dirs_exist_ok=True)
shutil.copytree("src/addons/virt_agent/sdk/python/ffi", TEMP_FFI_DIR, dirs_exist_ok=True)
remove_subpackage_files(TEMP_FFI_DIR, "ffi")

TEMP_MODELS_DIR = "temp_ubse_models"
reset_dir(TEMP_MODELS_DIR)
shutil.copytree("src/sdk/python/models", TEMP_MODELS_DIR, dirs_exist_ok=True)
shutil.copytree("src/addons/virt_agent/sdk/python/models", TEMP_MODELS_DIR, dirs_exist_ok=True)
remove_subpackage_files(TEMP_MODELS_DIR, "models")

TEMP_IPC_DIR = "temp_ubse_ipc"
reset_dir(TEMP_IPC_DIR)
shutil.copytree("src/sdk/python/ipc", TEMP_IPC_DIR, dirs_exist_ok=True)
remove_subpackage_files(TEMP_IPC_DIR, "ipc")

setup(
    name="ubse",
    version=UBSE_VERSION,
    package_dir={"ubse": TEMP_DIR,
                 "ubse.ffi": TEMP_FFI_DIR,
                 "ubse.models": TEMP_MODELS_DIR,
                 "ubse.ipc": TEMP_IPC_DIR},
    packages=["ubse", "ubse.ffi", "ubse.models", "ubse.ipc"],
)
