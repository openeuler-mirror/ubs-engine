#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
# ubs-engine is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
"""通用 Python 子包构建器。

通过清单文件(subpackages/<name>.txt)驱动,支持任意数量的子包拆分,
无需为每个子包编写独立的 setup_xxx.py。

清单文件格式:
    每行一个相对 src/sdk/python/ 的路径(.py 文件,可带子目录)
    空行或以 # 开头的行被忽略
    示例:
        ubs_engine_ssu.py
        ffi/ubs_engine_binding_ssu.py
        models/ubs_engine_model_ssu.py

用法:
    python3 build_subpackage.py <name> <setuptools_cmd> [args...]
    例: python3 build_subpackage.py ssu build
    例: python3 build_subpackage.py ssu install --root=/tmp/buildroot --prefix=/usr
"""
import os
import sys
import glob
from setuptools import setup

SDK_DIR = "src/sdk/python"
SUBPKG_DIR = f"{SDK_DIR}/subpackages"
# ubse 顶层包支持的子目录(对应 ubse.<sub> 命名空间)
SUBDIRS = ("ffi", "models", "ipc")
# 统一版本号,主 setup.py 与子包共用,避免多处手动同步
UBSE_VERSION = os.environ.get("UBSE_VERSION", "1.0.1")


def parse_manifest(name):
    """读取 subpackages/<name>.txt 清单,返回 (modules, package_dir)

    Returns:
        modules: 模块全名列表(如 ['ubse.ubs_engine_ssu', 'ubse.ffi.ubs_engine_binding_ssu'])
        package_dir: setuptools package_dir 映射
    """
    manifest = f"{SUBPKG_DIR}/{name}.txt"
    if not os.path.exists(manifest):
        raise FileNotFoundError(
            f"Subpackage manifest not found: {manifest}\n"
            f"Please create it with one .py path per line (relative to {SDK_DIR}/)."
        )

    modules = []
    package_dir = {"ubse": SDK_DIR}
    with open(manifest) as f:
        for line in f:
            rel = line.strip()
            if not rel or rel.startswith("#"):
                continue
            # 去掉可能的 .py 后缀,按 / 拆分
            rel = rel[:-3] if rel.endswith(".py") else rel
            # 防御性校验: 拒绝目录遍历和绝对路径
            if ".." in rel.split("/") or rel.startswith("/"):
                raise ValueError(f"Invalid path in manifest: {rel}")
            parts = rel.split("/")
            if parts[0] in SUBDIRS:
                # ffi/ubs_engine_binding_ssu -> ubse.ffi.ubs_engine_binding_ssu
                module = "ubse." + ".".join(parts)
                package_dir.setdefault("ubse." + parts[0], f"{SDK_DIR}/{parts[0]}")
            else:
                # ubs_engine_ssu -> ubse.ubs_engine_ssu
                module = "ubse." + ".".join(parts)
            modules.append(module)
    return modules, package_dir


def collect_all_subpackage_files():
    """扫描所有 manifest,返回主包需要排除的文件相对路径集合(相对 SDK_DIR)

    主 setup.py 调用此函数,自动把所有已声明子包的文件从主包剔除,
    确保单一数据源(manifest),不会出现两处文件列表不同步的问题。
    """
    excluded = set()
    if not os.path.exists(SUBPKG_DIR):
        return excluded
    for manifest in glob.glob(f"{SUBPKG_DIR}/*.txt"):
        with open(manifest) as f:
            for line in f:
                rel = line.strip()
                if rel and not rel.startswith("#"):
                    excluded.add(rel)
    return excluded


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    name = sys.argv[1]
    # 重构 sys.argv,让 setuptools 看到正确的命令行
    sys.argv = [sys.argv[0]] + sys.argv[2:]

    modules, package_dir = parse_manifest(name)
    setup(
        name=f"ubse-{name}",
        version=UBSE_VERSION,
        package_dir=package_dir,
        py_modules=modules,                # 精确安装清单中列出的模块
        install_requires=[f"ubse=={UBSE_VERSION}"],
    )


if __name__ == "__main__":
    main()
