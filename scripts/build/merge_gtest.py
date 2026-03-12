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

import xml.etree.ElementTree as ET
import os
import sys
from datetime import datetime, timezone
from pathlib import Path

def merge_xml_files(output_file, xml_files):
    # 创建一个根元素
    merged_root = ET.Element("testsuites")

    # 初始化合并后的属性值
    total_tests = 0
    total_failures = 0
    total_disabled = 0
    total_errors = 0
    total_time = 0.0

    for xml_file in xml_files:
        if not os.path.isfile(xml_file):
            continue

        tree = ET.parse(xml_file)
        root = tree.getroot()

        # 处理每个 testsuite 元素
        for testsuite in root.findall("testsuite"):
            # 累加属性值
            total_tests += int(testsuite.get("tests", 0))
            total_failures += int(testsuite.get("failures", 0))
            total_disabled += int(testsuite.get("disabled", 0))
            total_errors += int(testsuite.get("errors", 0))
            total_time += float(testsuite.get("time", 0))

            # 直接将 testsuite 元素及其内容原样复制到合并的根元素中
            merged_root.append(testsuite)

    # 设置合并后的属性值
    merged_root.set("tests", str(total_tests))
    merged_root.set("failures", str(total_failures))
    merged_root.set("disabled", str(total_disabled))
    merged_root.set("errors", str(total_errors))
    merged_root.set("time", str(total_time))

    # 获取当前时间并格式化为 ISO 8601 格式
    current_timestamp = datetime.now(tz=timezone.utc).isoformat()
    merged_root.set("timestamp", current_timestamp)
    merged_root.set("name", "AllTests")

    # 创建一个新的 XML 树
    merged_tree = ET.ElementTree(merged_root)

    # 确保输出目录存在
    output_dir = os.path.dirname(output_file)
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    # 写入文件，添加异常处理
    try:
        with open(output_file, "wb") as f:
            # 写入 XML 声明
            f.write(b'<?xml version="1.0" encoding="UTF-8"?>\n')
            f.write(f'<testsuites tests="{total_tests}" failures="{total_failures}" disabled="{total_disabled}" '
                    f'errors="{total_errors}" time="{total_time:.3f}" timestamp="{current_timestamp}" '
                    f'name="AllTests">\n  '.encode('utf-8'))
            # 写入每个 testsuite 元素
            for testsuite in merged_root.findall("testsuite"):
                f.write(ET.tostring(testsuite, encoding="utf-8"))
            f.write(b'</testsuites>\n')
    except IOError as e:
        print(f"Error writing to file {output_file}: {e}")


if __name__ == "__main__":
    # 从命令行参数获取 base_dir
    if len(sys.argv) != 2:
        sys.exit(1)

    base_dir = sys.argv[1]

    ut_xml_directory = Path(os.path.join(base_dir, "ut"))

    xml_files = [str(f) for f in ut_xml_directory.glob("*.xml")]
    # 拼接 XML 文件路径
    xml_files.append(os.path.join(base_dir, "it_detail.xml"))

    # 输出文件路径
    output_file = os.path.join(base_dir, "test_detail.xml")

    merge_xml_files(output_file, xml_files)
