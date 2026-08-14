#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 
"""
ubse_ssu_test_python - SSU Python SDK 交互式命令行测试工具
 
命令格式: 扁平下划线形式，所有参数为有序位置参数，不接受可选参数或省略。
枚举值填写 SDK 底层十进制数值；空字符串输入 ""，数值零输入 0。
ns_size 只接受可解析为 uint64 的十进制数值，不接受 1G 等单位形式。
 
用法:
  python3 ubse_ssu_test_python                   # 交互模式
  python3 ubse_ssu_test_python ssu_list_alloc_info  # 单次执行
 
环境变量:
  UBSE_SDK_DIR  - SDK 路径 (默认此文件所在目录)
  UBSE_IPC_SOCKET_PATH - daemon socket 路径 (默认 /var/run/ubse/ubse.sock)
"""
 
import cmd
import logging
import shlex
import sys
import os
 
logging.basicConfig(level=logging.INFO, format='%(levelname)s: %(message)s')
logger = logging.getLogger('UBSE_SSU_CLI')
 
SDK_DIR = os.environ.get('UBSE_SDK_DIR', os.path.dirname(os.path.abspath(__file__)))
 
SOCKET_PATH = os.environ.get('UBSE_IPC_SOCKET_PATH', '/var/run/ubse/ubse.sock')
 
if not os.path.isdir(os.path.join(SDK_DIR, 'ubse', 'ipc')):
    import tempfile
    import atexit
    import shutil
    _tmp = tempfile.mkdtemp(prefix='ubse_sdk_')
    _ubse_dir = os.path.join(_tmp, 'ubse')
    for _sub in ['', 'ffi', 'ipc', 'models']:
        os.makedirs(os.path.join(_ubse_dir, _sub), exist_ok=True)
    for _src in os.listdir(SDK_DIR):
        _src_path = os.path.join(SDK_DIR, _src)
        if _src == 'ubse' or _src == '__pycache__' or _src == 'requirements.txt':
            continue
        if os.path.isfile(_src_path) and _src.endswith('.py'):
            os.symlink(_src_path, os.path.join(_ubse_dir, _src))
        elif os.path.isdir(_src_path) and not _src.startswith('__'):
            for _f in os.listdir(_src_path):
                _fp = os.path.join(_src_path, _f)
                if _f.endswith('.py'):
                    os.symlink(_fp, os.path.join(_ubse_dir, _src, _f))

    def _cleanup_tmp_sdk():
        if _tmp in sys.path:
            sys.path.remove(_tmp)
        shutil.rmtree(_tmp, ignore_errors=True)

    atexit.register(_cleanup_tmp_sdk)
    sys.path.insert(0, _tmp)
else:
    sys.path.insert(0, SDK_DIR)
 
from ubse.ffi.ubs_engine_exceptions import (
    UbsEngineConnectionError,
    UbsEngineTimeoutError, UbsEngineInternalError, UbsErrInvalidArg,
    UbsEngineNotExistError, UbsEngineAllocateError,
    UbsEngineAlreadyAttachedError,
)
from ubse.ipc.ubs_engine_ipc import UBSE_IPC_SOCKET_PATH
from ubse.models.ubs_engine_model_ssu import (
    UbsSsuAllocSpaceReq, UbsSsuSpaceReq, UbsSsuLinearSpaceReq, UbsSsuStripedSpaceReq,
    UbsSsuLbaFormat, UbsSsuRaidLevel, UbsSsuChunkSize, UbsSsuAllocStrategy,
    UbsUbVfe, UbsUbFe, UbsSsuAllocResult, UbsSsuConnectInfo, UbsSsuNsStats,
    UBS_SSU_MAX_NAME_LENGTH, UBS_SSU_GUID_LENGTH,
)
from ubse import ubs_engine_ssu
 
 
def print_alloc_result(result: UbsSsuAllocResult):
    logger.info(f"  name={result.name}, strategy={result.strategy.value}, namespace_cnt={len(result.namespaces)}")
    for ns in result.namespaces:
        logger.info(f"    ns_id={ns.namespace_id}, path={ns.ns_dev_path}, "
                     f"size={ns.ns_size}, lba={ns.lba_format.value}, uuid={ns.ns_uuid}")
 
 
def print_connect_info(info: UbsSsuConnectInfo):
    logger.info(f"  src_eid={info.src_eid}, tgt_eid={info.tgt_eid}, "
                 f"tgt_nqn={info.tgt_nqn}, host_nqn={info.host_nqn}, "
                 f"ns_uuid={info.ns_uuid}, ns_id={info.ns_id}")
 
 
def print_ns_stats(stats: UbsSsuNsStats):
    pct = round(stats.used_size / stats.total_size * 100, 2) if stats.total_size > 0 else 0.0
    logger.info(f"  ns_uuid={stats.ns_uuid}, ns_id={stats.ns_id}, "
                 f"total={stats.total_size}, used={stats.used_size}, usage={pct}%")
 
 
def print_fe(fe: UbsUbFe):
    logger.info(f"  FE: slot={fe.slot_id}, chip={fe.chip_id}, die={fe.die_id}, "
                 f"pfe_id={fe.pfe_id}, pfe_guid={fe.pfe_guid}, vfe_cnt={len(fe.vfe_list)}")
    for vfe in fe.vfe_list:
        logger.info(f"    VFE: slot={vfe.slot_id}, chip={vfe.chip_id}, die={vfe.die_id}, "
                     f"pfe_id={vfe.pfe_id}, vfe_id={vfe.vfe_id}, vfe_guid={vfe.vfe_guid}, "
                     f"bind_bus_instance_guid={vfe.bind_bus_instance_guid}")
 
 
class UbseSsuTestPython(cmd.Cmd):
    """SSU Python SDK 交互式命令行测试工具"""
 
    def __init__(self):
        super().__init__()
        self.prompt = "ubse_ssu_test_python> "
        self.intro = (
            'Welcome to ubse_ssu_test_python. Type help or ? to list commands.\n'
            '\n'
            'Commands:\n'
            '  ssu_list_alloc_info                            - 列出所有已分配存储空间\n'
            '  ssu_alloc_space <name> <ns_size> <ns_num>     - 分配存储空间\n'
            '    <lba_format> <strategy> <tenant>\n'
            '  ssu_free_space <name>                          - 释放存储空间\n'
            '  ssu_add_access_permission <name> <nqn>        - 添加访问权限\n'
            '  ssu_remove_access_permission <name> <nqn>     - 移除访问权限\n'
            '  ssu_attach_space <name> <nqn> <src_eid>       - 挂载存储空间\n'
            '  ssu_detach_space <name> <nqn> <src_eid>       - 卸载存储空间\n'
            '  ssu_attach_linear_space <name> <nqn>          - 挂载线性编址存储空间\n'
            '    <src_eid> <dev_name>\n'
            '  ssu_detach_linear_space <name> <nqn>          - 卸载线性编址存储空间\n'
            '    <src_eid> <dev_name>\n'
            '  ssu_attach_striped_space <name> <nqn>         - 挂载条带化编址存储空间\n'
            '    <src_eid> <dev_name> <level> <chunk_size>\n'
            '  ssu_detach_striped_space <name> <nqn>         - 卸载条带化编址存储空间\n'
            '    <src_eid> <dev_name> <level> <chunk_size>\n'
            '  ssu_get_ns_stats <name>                       - 查询命名空间统计信息\n'
            '  ssu_get_connect_info <name> <vfe_present>     - 查询连接信息\n'
            '    <slot_id> <chip_id> <die_id> <pfe_id> <vfe_id>\n'
            '    <vfe_guid> <bind_bus_instance_guid>\n'
            '  ssu_get_fe_device_list                        - 查询FE设备列表\n'
            '  ssu_fe_device_alloc <upi> <slot_id> <chip_id> - 分配VFE设备\n'
            '    <die_id> <pfe_id> <vfe_id> <vfe_guid>\n'
            '    <bind_bus_instance_guid> <bus_instance_guid>\n'
            '  ssu_fe_device_free <upi> <slot_id> <chip_id>  - 释放VFE设备\n'
            '    <die_id> <pfe_id> <vfe_id> <vfe_guid>\n'
            '    <bind_bus_instance_guid>\n'
            '  quit                                           - 退出\n'
            '\n'
            'Notes:\n'
            '  所有参数为有序位置参数，不接受可选参数或省略。\n'
            '  枚举值填写SDK底层十进制数值(lba_format=4096/512, strategy=0/1, level=0/5, chunk_size=4/16/32/64/128/256/512)。\n'
            '  空字符串输入 ""，数值零输入 0。\n'
            '  ns_size只接受uint64十进制数值，不接受1G等单位形式。\n'
            '  vfe_present=0时向SDK传nil，但仍须写全后续占位参数。\n'
        )
 
    def _handle_error(self, ex):
        if isinstance(ex, UbsErrInvalidArg):
            logger.error(f"Invalid argument: {ex}")
        elif isinstance(ex, UbsEngineConnectionError):
            logger.error(f"Connection error: {ex}")
        elif isinstance(ex, UbsEngineNotExistError):
            logger.error(f"Not exist: {ex}")
        elif isinstance(ex, UbsEngineAllocateError):
            logger.error(f"Allocate error: {ex}")
        elif isinstance(ex, UbsEngineAlreadyAttachedError):
            # 重复挂载场景，服务端依然返回已挂载的设备路径，输出异常携带的data
            logger.warning(f"Already attached: {ex}")
            logger.info(f"Attached device data: {ex.data}")
        elif isinstance(ex, UbsEngineTimeoutError):
            logger.error(f"Timeout: {ex}")
        elif isinstance(ex, UbsEngineInternalError):
            logger.error(f"Internal error: {ex}")
        else:
            logger.error(f"Error: {ex}")
 
    def do_ssu_list_alloc_info(self, arg):
        """ssu_list_alloc_info
        列出所有已分配存储空间信息。
        Example: ssu_list_alloc_info
        """
        try:
            results = ubs_engine_ssu.ubs_ssu_alloc_info_list()
            logger.info(f"Total alloc info: {len(results)}")
            for r in results:
                print_alloc_result(r)
        except Exception as ex:
            self._handle_error(ex)
 
    def do_ssu_alloc_space(self, arg):
        """ssu_alloc_space <name> <ns_size> <ns_num> <lba_format> <strategy> <tenant>
        分配SSU存储空间。ns_size为uint64十进制数值(如1073741824)，lba_format=4096/512，strategy=0(striped)/1(linear)，tenant为字节串(字符串形式传入)。
        Example: ssu_alloc_space test-space 1073741824 2 4096 0 tenant-a
        """
        try:
            parts = shlex.split(arg)
            if len(parts) != 6:
                logger.error("Usage: ssu_alloc_space <name> <ns_size> <ns_num> <lba_format> <strategy> <tenant>")
                logger.error("  lba_format: 4096 or 512")
                logger.error("  strategy: 0(striped) or 1(linear)")
                logger.error("  Example: ssu_alloc_space test-space 1073741824 2 4096 0 tenant-a")
                return
            name = parts[0]
            ns_size = int(parts[1])
            ns_num = int(parts[2])
            lba_format = UbsSsuLbaFormat(int(parts[3]))
            strategy = UbsSsuAllocStrategy(int(parts[4]))
            tenant = parts[5] if parts[5] else ''
            req = UbsSsuAllocSpaceReq(name=name, ns_size=ns_size, ns_num=ns_num,
                                       lba_format=lba_format, strategy=strategy, tenant=tenant)
            logger.info(f"Allocating: name={name}, ns_size={ns_size}, ns_num={ns_num}, "
                         f"lba_format={lba_format.value}, strategy={strategy.value}")
            result = ubs_engine_ssu.ubs_ssu_space_alloc(req)
            logger.info("Alloc succeeded")
            print_alloc_result(result)
        except Exception as ex:
            self._handle_error(ex)
 
    def do_ssu_free_space(self, arg):
        """ssu_free_space <name>
        释放已分配的存储空间。释放操作具有幂等性。
        Example: ssu_free_space test-space
        """
        try:
            name = arg.strip()
            if not name:
                logger.error("Usage: ssu_free_space <name>")
                logger.error("  Example: ssu_free_space test-space")
                return
            logger.info(f"Freeing: name={name}")
            ubs_engine_ssu.ubs_ssu_space_free(name)
            logger.info("Free succeeded")
        except Exception as ex:
            self._handle_error(ex)
 
    def do_ssu_add_access_permission(self, arg):
        """ssu_add_access_permission <name> <nqn>
        为指定Host添加对已分配存储空间的访问权限。重复添加同一Host的访问权限应返回成功(幂等性保证)。
        Example: ssu_add_access_permission test-space nqn.2024-01.example:host-a
        """
        try:
            parts = shlex.split(arg)
            if len(parts) != 2:
                logger.error("Usage: ssu_add_access_permission <name> <nqn>")
                logger.error("  Example: ssu_add_access_permission test-space nqn.2024-01.example:host-a")
                return
            name, nqn = parts[0], parts[1]
            logger.info(f"Adding access: name={name}, nqn={nqn}")
            ubs_engine_ssu.ubs_ssu_access_permission_add(name, nqn)
            logger.info("Add access succeeded")
        except Exception as ex:
            self._handle_error(ex)
 
    def do_ssu_remove_access_permission(self, arg):
        """ssu_remove_access_permission <name> <nqn>
        移除指定Host对已分配存储空间的访问权限。移除不存在的访问权限应返回成功(幂等性保证)。
        Example: ssu_remove_access_permission test-space nqn.2024-01.example:host-a
        """
        try:
            parts = shlex.split(arg)
            if len(parts) != 2:
                logger.error("Usage: ssu_remove_access_permission <name> <nqn>")
                logger.error("  Example: ssu_remove_access_permission test-space nqn.2024-01.example:host-a")
                return
            name, nqn = parts[0], parts[1]
            logger.info(f"Removing access: name={name}, nqn={nqn}")
            ubs_engine_ssu.ubs_ssu_access_permission_remove(name, nqn)
            logger.info("Remove access succeeded")
        except Exception as ex:
            self._handle_error(ex)
 
    def do_ssu_attach_space(self, arg):
        """ssu_attach_space <name> <nqn> <src_eid>
        挂载已分配的存储空间，使其可被主机访问。返回命名空间设备路径列表。
        Example: ssu_attach_space test-space nqn.2024-01.example:host-a 0000000000000001
        """
        try:
            parts = shlex.split(arg)
            if len(parts) != 3:
                logger.error("Usage: ssu_attach_space <name> <nqn> <src_eid>")
                logger.error("  Example: ssu_attach_space test-space nqn.2024-01.example:host-a 0000000000000001")
                return
            name, nqn, src_eid = parts[0], parts[1], parts[2]
            req = UbsSsuSpaceReq(name=name, nqn=nqn, src_eid=src_eid)
            logger.info(f"Attaching: name={name}, nqn={nqn}, src_eid={src_eid}")
            dev_paths = ubs_engine_ssu.ubs_ssu_space_attach(req)
            logger.info(f"Attach succeeded, device paths: {dev_paths}")
        except Exception as ex:
            self._handle_error(ex)
 
    def do_ssu_detach_space(self, arg):
        """ssu_detach_space <name> <nqn> <src_eid>
        卸载已分配的存储空间，释放设备占用。卸载前需确保没有进程正在使用该存储空间。
        Example: ssu_detach_space test-space nqn.2024-01.example:host-a 0000000000000001
        """
        try:
            parts = shlex.split(arg)
            if len(parts) != 3:
                logger.error("Usage: ssu_detach_space <name> <nqn> <src_eid>")
                logger.error("  Example: ssu_detach_space test-space nqn.2024-01.example:host-a 0000000000000001")
                return
            name, nqn, src_eid = parts[0], parts[1], parts[2]
            req = UbsSsuSpaceReq(name=name, nqn=nqn, src_eid=src_eid)
            logger.info(f"Detaching: name={name}, nqn={nqn}, src_eid={src_eid}")
            ubs_engine_ssu.ubs_ssu_space_detach(req)
            logger.info("Detach succeeded")
        except Exception as ex:
            self._handle_error(ex)
 
    def do_ssu_attach_linear_space(self, arg):
        """ssu_attach_linear_space <name> <nqn> <src_eid> <dev_name>
        挂载线性编址存储空间，将多个命名空间设备以线性拼接方式聚合为一个逻辑块设备。
        返回(ns_dev_paths, dev_path)。
        Example: ssu_attach_linear_space test-space nqn.2024-01.example:host-a 0000000000000001 test-linear
        """
        try:
            parts = shlex.split(arg)
            if len(parts) != 4:
                logger.error("Usage: ssu_attach_linear_space <name> <nqn> <src_eid> <dev_name>")
                logger.error("  Example: ssu_attach_linear_space test-space nqn.2024-01.example:host-a 0000000000000001 test-linear")
                return
            name, nqn, src_eid, dev_name = parts[0], parts[1], parts[2], parts[3]
            req = UbsSsuLinearSpaceReq(name=name, nqn=nqn, src_eid=src_eid, dev_name=dev_name)
            logger.info(f"Attaching linear: name={name}, nqn={nqn}, src_eid={src_eid}, dev_name={dev_name}")
            ns_paths, dev_path = ubs_engine_ssu.ubs_ssu_linear_space_attach(req)
            logger.info(f"Attach succeeded, ns_paths={ns_paths}, dev_path={dev_path}")
        except Exception as ex:
            self._handle_error(ex)
 
    def do_ssu_detach_linear_space(self, arg):
        """ssu_detach_linear_space <name> <nqn> <src_eid> <dev_name>
        卸载线性编址存储空间，释放线性聚合的块设备。
        Example: ssu_detach_linear_space test-space nqn.2024-01.example:host-a 0000000000000001 test-linear
        """
        try:
            parts = shlex.split(arg)
            if len(parts) != 4:
                logger.error("Usage: ssu_detach_linear_space <name> <nqn> <src_eid> <dev_name>")
                logger.error("  Example: ssu_detach_linear_space test-space nqn.2024-01.example:host-a 0000000000000001 test-linear")
                return
            name, nqn, src_eid, dev_name = parts[0], parts[1], parts[2], parts[3]
            req = UbsSsuLinearSpaceReq(name=name, nqn=nqn, src_eid=src_eid, dev_name=dev_name)
            logger.info(f"Detaching linear: name={name}, nqn={nqn}, src_eid={src_eid}, dev_name={dev_name}")
            ubs_engine_ssu.ubs_ssu_linear_space_detach(req)
            logger.info("Detach succeeded")
        except Exception as ex:
            self._handle_error(ex)
 
    def do_ssu_attach_striped_space(self, arg):
        """ssu_attach_striped_space <name> <nqn> <src_eid> <dev_name> <level> <chunk_size>
        挂载条带化编址存储空间，支持RAID0(level=0)和RAID5(level=5)。
        chunk_size单位KB，需为LBA格式的整数倍(4/16/32/64/128/256/512)。RAID5至少需要3个成员设备。
        返回(ns_dev_paths, dev_path)。
        Example: ssu_attach_striped_space test-space nqn.2024-01.example:host-a 0000000000000001 test-striped 0 64
        """
        try:
            parts = shlex.split(arg)
            if len(parts) != 6:
                logger.error("Usage: ssu_attach_striped_space <name> <nqn> <src_eid> <dev_name> <level> <chunk_size>")
                logger.error("  level: 0(RAID0) or 5(RAID5)")
                logger.error("  chunk_size(KB): 4/16/32/64/128/256/512")
                logger.error("  Example: ssu_attach_striped_space test-space nqn.xxx host-a 0000000000000001 test-striped 0 64")
                return
            name, nqn, src_eid, dev_name = parts[0], parts[1], parts[2], parts[3]
            level = UbsSsuRaidLevel(int(parts[4]))
            chunk_size = UbsSsuChunkSize(int(parts[5]))
            req = UbsSsuStripedSpaceReq(name=name, nqn=nqn, src_eid=src_eid,
                                          dev_name=dev_name, level=level, chunk_size=chunk_size)
            logger.info(f"Attaching striped: name={name}, nqn={nqn}, src_eid={src_eid}, "
                         f"dev_name={dev_name}, level={level.value}, chunk_size={chunk_size.value}")
            ns_paths, dev_path = ubs_engine_ssu.ubs_ssu_striped_space_attach(req)
            logger.info(f"Attach succeeded, ns_paths={ns_paths}, dev_path={dev_path}")
        except Exception as ex:
            self._handle_error(ex)
 
    def do_ssu_detach_striped_space(self, arg):
        """ssu_detach_striped_space <name> <nqn> <src_eid> <dev_name> <level> <chunk_size>
        卸载条带化编址存储空间。
        Example: ssu_detach_striped_space test-space nqn.2024-01.example:host-a 0000000000000001 test-striped 0 64
        """
        try:
            parts = shlex.split(arg)
            if len(parts) != 6:
                logger.error("Usage: ssu_detach_striped_space <name> <nqn> <src_eid> <dev_name> <level> <chunk_size>")
                logger.error("  level: 0(RAID0) or 5(RAID5)")
                logger.error("  chunk_size(KB): 4/16/32/64/128/256/512")
                logger.error("  Example: ssu_detach_striped_space test-space nqn.xxx host-a 0000000000000001 test-striped 0 64")
                return
            name, nqn, src_eid, dev_name = parts[0], parts[1], parts[2], parts[3]
            level = UbsSsuRaidLevel(int(parts[4]))
            chunk_size = UbsSsuChunkSize(int(parts[5]))
            req = UbsSsuStripedSpaceReq(name=name, nqn=nqn, src_eid=src_eid,
                                          dev_name=dev_name, level=level, chunk_size=chunk_size)
            logger.info(f"Detaching striped: name={name}, nqn={nqn}, src_eid={src_eid}, "
                         f"dev_name={dev_name}, level={level.value}, chunk_size={chunk_size.value}")
            ubs_engine_ssu.ubs_ssu_striped_space_detach(req)
            logger.info("Detach succeeded")
        except Exception as ex:
            self._handle_error(ex)
 
    def do_ssu_get_ns_stats(self, arg):
        """ssu_get_ns_stats <name>
        查询指定存储空间下各命名空间的容量使用情况，包括总容量和已用容量。
        Example: ssu_get_ns_stats test-space
        """
        try:
            name = arg.strip()
            if not name:
                logger.error("Usage: ssu_get_ns_stats <name>")
                logger.error("  Example: ssu_get_ns_stats test-space")
                return
            logger.info(f"Querying ns stats: name={name}")
            stats = ubs_engine_ssu.ubs_ssu_ns_stats_get(name)
            logger.info(f"Total ns: {len(stats)}")
            for s in stats:
                print_ns_stats(s)
        except Exception as ex:
            self._handle_error(ex)
 
    def do_ssu_get_connect_info(self, arg):
        """ssu_get_connect_info <name> <vfe_present> <slot_id> <chip_id> <die_id> <pfe_id> <vfe_id> <vfe_guid> <bind_bus_instance_guid>
        查询指定存储空间在指定VFE上的NVMe连接信息。
        vfe_present=0时向SDK传nil(vfe=None)，但仍须写全后续占位参数；vfe_present=1时使用后续全部字段组装非空VFE。
        Example: ssu_get_connect_info test-space 0 0 0 0 0 0 "" ""
        Example: ssu_get_connect_info test-space 1 1 0 0 0 2 00112233445566778899aabbccddeeff 11112222333344445555666677778888
        """
        try:
            parts = shlex.split(arg)
            if len(parts) != 9:
                logger.error("Usage: ssu_get_connect_info <name> <vfe_present> <slot_id> <chip_id> <die_id> "
                             "<pfe_id> <vfe_id> <vfe_guid> <bind_bus_instance_guid>")
                logger.error("  vfe_present: 0(nil) or 1(use VFE fields)")
                logger.error("  Example: ssu_get_connect_info test-space 0 0 0 0 0 0 \"\" \"\"")
                logger.error("  Example: ssu_get_connect_info test-space 1 1 0 0 0 2 "
                             "00112233445566778899aabbccddeeff 11112222333344445555666677778888")
                return
            name = parts[0]
            vfe_present = int(parts[1])
            slot_id = int(parts[2])
            chip_id = int(parts[3])
            die_id = int(parts[4])
            pfe_id = int(parts[5])
            vfe_id = int(parts[6])
            vfe_guid = parts[7]
            bind_busi = parts[8]
            vfe = None
            if vfe_present == 1:
                vfe = UbsUbVfe(slot_id=slot_id, chip_id=chip_id, die_id=die_id,
                               pfe_id=pfe_id, vfe_id=vfe_id, vfe_guid=vfe_guid,
                               bind_bus_instance_guid=bind_busi)
            logger.info(f"Querying connect info: name={name}, vfe_present={vfe_present}, vfe={vfe}")
            infos = ubs_engine_ssu.ubs_ssu_connect_info_get(name, vfe)
            logger.info(f"Total connect info: {len(infos)}")
            for info in infos:
                print_connect_info(info)
        except Exception as ex:
            self._handle_error(ex)
 
    def do_ssu_get_fe_device_list(self, arg):
        """ssu_get_fe_device_list
        查询系统中所有FE设备信息，包括每个PFE下的VFE列表。
        Example: ssu_get_fe_device_list
        """
        try:
            logger.info("Querying FE device list")
            fe_list = ubs_engine_ssu.ubs_ssu_fe_device_list()
            logger.info(f"Total FE devices: {len(fe_list)}")
            for fe in fe_list:
                print_fe(fe)
        except Exception as ex:
            self._handle_error(ex)
 
    def do_ssu_fe_device_alloc(self, arg):
        """ssu_fe_device_alloc <upi> <slot_id> <chip_id> <die_id> <pfe_id> <vfe_id> <vfe_guid> <bind_bus_instance_guid> <bus_instance_guid>
        将指定VFE绑定到目标虚拟机。bus_instance_guid为32字符字符串或空字符串("")。
        Example: ssu_fe_device_alloc 1 1 0 0 10 20 00112233445566778899aabbccddeeff 11112222333344445555666677778888 9999aaaabbbbccccddddeeeeffff0000
        Example: ssu_fe_device_alloc 1 1 0 0 10 20 00112233445566778899aabbccddeeff 11112222333344445555666677778888 ""
        """
        try:
            parts = shlex.split(arg)
            if len(parts) != 9:
                logger.error("Usage: ssu_fe_device_alloc <upi> <slot_id> <chip_id> <die_id> <pfe_id> "
                             "<vfe_id> <vfe_guid> <bind_bus_instance_guid> <bus_instance_guid>")
                logger.error("  bus_instance_guid: 32字符字符串或空字符串\"\"")
                logger.error("  Example: ssu_fe_device_alloc 1 1 0 0 10 20 "
                             "00112233445566778899aabbccddeeff 11112222333344445555666677778888 "
                             "9999aaaabbbbccccddddeeeeffff0000")
                return
            upi = int(parts[0])
            slot_id = int(parts[1])
            chip_id = int(parts[2])
            die_id = int(parts[3])
            pfe_id = int(parts[4])
            vfe_id = int(parts[5])
            vfe_guid = parts[6]
            bind_busi = parts[7]
            bus_instance_guid = parts[8]
            vfe = UbsUbVfe(slot_id=slot_id, chip_id=chip_id, die_id=die_id,
                           pfe_id=pfe_id, vfe_id=vfe_id, vfe_guid=vfe_guid,
                           bind_bus_instance_guid=bind_busi)
            logger.info(f"Allocating VFE: upi={upi}, vfe=slot={slot_id} chip={chip_id} die={die_id} "
                         f"pfe={pfe_id} vfe={vfe_id} vfe_guid={vfe_guid} bind_busi={bind_busi}")
            result_guid = ubs_engine_ssu.ubs_ssu_fe_device_alloc(upi, vfe, bus_instance_guid)
            logger.info(f"Alloc succeeded, guid={result_guid}")
        except Exception as ex:
            self._handle_error(ex)
 
    def do_ssu_fe_device_free(self, arg):
        """ssu_fe_device_free <upi> <slot_id> <chip_id> <die_id> <pfe_id> <vfe_id> <vfe_guid> <bind_bus_instance_guid>
        将已分配的VFE从目标虚拟机释放，回收VFE设备资源。
        Example: ssu_fe_device_free 1 1 0 0 10 20 00112233445566778899aabbccddeeff 11112222333344445555666677778888
        """
        try:
            parts = shlex.split(arg)
            if len(parts) != 8:
                logger.error("Usage: ssu_fe_device_free <upi> <slot_id> <chip_id> <die_id> <pfe_id> "
                             "<vfe_id> <vfe_guid> <bind_bus_instance_guid>")
                logger.error("  Example: ssu_fe_device_free 1 1 0 0 10 20 "
                             "00112233445566778899aabbccddeeff 11112222333344445555666677778888")
                return
            upi = int(parts[0])
            slot_id = int(parts[1])
            chip_id = int(parts[2])
            die_id = int(parts[3])
            pfe_id = int(parts[4])
            vfe_id = int(parts[5])
            vfe_guid = parts[6]
            bind_busi = parts[7]
            vfe = UbsUbVfe(slot_id=slot_id, chip_id=chip_id, die_id=die_id,
                           pfe_id=pfe_id, vfe_id=vfe_id, vfe_guid=vfe_guid,
                           bind_bus_instance_guid=bind_busi)
            logger.info(f"Freeing VFE: upi={upi}, vfe=slot={slot_id} chip={chip_id} die={die_id} "
                         f"pfe={pfe_id} vfe={vfe_id} vfe_guid={vfe_guid} bind_busi={bind_busi}")
            ubs_engine_ssu.ubs_ssu_fe_device_free(upi, vfe)
            logger.info("Free succeeded")
        except Exception as ex:
            self._handle_error(ex)
 
    def do_quit(self, arg):
        """quit - 退出"""
        return True
 
    do_EOF = do_quit
 
    def postloop(self):
        print("Goodbye!")
 
 
def main():
    if SOCKET_PATH != UBSE_IPC_SOCKET_PATH:
        from ubse.ipc.ubs_engine_ipc import set_socket_path
        set_socket_path(SOCKET_PATH)
    app = UbseSsuTestPython()
    if len(sys.argv) > 1:
        app.onecmd(shlex.join(sys.argv[1:]))
    else:
        app.cmdloop()
 
 
if __name__ == '__main__':
    main()
