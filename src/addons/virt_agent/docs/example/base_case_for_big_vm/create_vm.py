# -*- coding: utf-8 -*-
"""超大规格虚机创建示例.

本地大页内存不足时, 先通过内存借用接口从远端节点借入内存, 再基于借用后的大页信息
生成虚机XML并启动虚机, 最后开启冷热页流动以提升远端内存的访问性能.

使用方式: 按需修改下方配置项后执行 `python3 create_vm.py`.
"""
import asyncio
import logging
import math
import os
import subprocess
import uuid

from ubse.ubs_virt_agent_fragmentation import (
    ubs_task_result_query,
    ubs_mem_fragmentation_node_info_list,
    ubs_mem_borrow,
    ubs_page_swap_enable
)
from ubse.models.ubs_virt_agent_model import BorrowParamT, NumaMetaInfoT, PageSwapPairT, NumaQuotaT

from ubse.ubs_engine_log import ubs_engine_log_callback_register
from ubse.ubs_virt_agent_log import ubs_virt_agent_log_callback_register

# 屏蔽SDK内部日志, 示例中只输出脚本自身日志
ubs_engine_log_callback_register(lambda level, msg: None)
ubs_virt_agent_log_callback_register(lambda level, msg: None)

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
LOG = logging.getLogger(__name__)

# ==============================================================================
# 虚机基础配置
# ==============================================================================
VM_NAME = "vm1"                                # 虚机名称
MEM_SIZE = 5120                                # 虚机内存规格, 单位: GB
IMAGE_NAME = "openEuler-22.03-SP2-aarch64-everything-redis-Performance.qcow2"  # 系统盘镜像文件名
IMAGE_BASE_PATH = "/opt/install/tmp/images"    # 系统盘镜像所在目录
XML_FILE_PATH = "/etc/libvirt/qemu/vm1.xml"    # 生成的虚机XML文件路径

# ==============================================================================
# vCPU与guest NUMA配置
# ==============================================================================
VCPU_COUNT = 384        # 虚机vCPU数量, 固定384, 与物理节点CPU数保持一致
CPU_SOCKETS = 2         # 虚机CPU socket数
CPU_CORES = 96          # 每个socket的核数
CPU_THREADS = 2         # 每核线程数; sockets * cores * threads 须等于 VCPU_COUNT
GUEST_NUMA_COUNT = 1    # 虚机NUMA节点数, 支持 1/2/4

# ==============================================================================
# 单位换算
# ==============================================================================
GB_TO_MB = 1024     # 1 GB = 1024 MB
MB_TO_KB = 1024     # 1 MB = 1024 KiB

# ==============================================================================
# 大页配置
# ==============================================================================
HUGE_PAGE_TYPE = "2M"                                                          # 虚机大页类型, 支持 "1G" 或 "2M"
HUGE_PAGE_TO_KB = {"1G": 1024 * MB_TO_KB, "2M": 2 * MB_TO_KB}[HUGE_PAGE_TYPE]   # 大页大小(KiB), 用于大页信息字典取值和XML的page size
HUGE_PAGE_TO_MB = {"1G": 1024, "2M": 2}[HUGE_PAGE_TYPE]                         # 大页大小(MB), 用于大页数量与内存容量的换算
MEM_BORROW_UNIT_GB = 1024                                                       # 1G大页场景的内存借用粒度, 单位: MB
MEM_BORROW_UNIT_MB = 128                                                        # 2M大页场景的内存借用粒度, 单位: MB
MEM_BORROW_UNIT = MEM_BORROW_UNIT_GB if HUGE_PAGE_TYPE == "1G" else MEM_BORROW_UNIT_MB  # 实际内存借用粒度, 单位: MB

# ==============================================================================
# 内存借用与冷热页流动参数
# ==============================================================================
REMOTE_MEMORY_USAGE_PERCENTAGE = 50  # 借用内存占虚机内存规格的最大比例, 单位: %
ONE_HUNDRED_PERCENT = 100            # 百分比基数
MEM_FLOW_RATIO = 1                   # 冷热页流动预留比例, 单位: %, 从本地可用内存中预留

# ==============================================================================
# 超时与轮询参数
# ==============================================================================
VIRSH_TIMEOUT = 3600        # virsh命令执行超时时间, 单位: 秒
MONITOR_INTERVAL = 10       # 虚机状态轮询间隔, 单位: 秒
MAX_MONITOR_ATTEMPTS = 720  # 虚机状态最大轮询次数
ASYNC_MAX_RETRY = 3600      # 异步任务最大轮询次数
ASYNC_RETRY_INTERVAL = 10   # 异步任务轮询间隔, 单位: 秒


class AsyncTaskStatus:
    """异步任务状态码, 与接口返回的status字段对应."""

    RUNNING = 1     # 任务执行中
    SUCCESS = 2     # 任务执行成功
    FAILED = 3      # 任务执行失败


# 虚机XML模板, 花括号占位符在create_vm_xml中填充
VM_XML_TEMPLATE = """<domain type="kvm">
    <uuid>{uuid}</uuid>
    <name>{vm_name}</name>
    <memory>{memory}</memory>
    <memoryBacking>
        <hugepages>
            <page size="{huge_page_size}" nodeset="0" unit="KiB" />
        </hugepages>
        <allocation mode="hugepage-ondemand" />
    </memoryBacking>
    <numatune>
{numa_tune}
    </numatune>
    <vcpu placement="static">{vcpu_count}</vcpu>
    <cputune>
        {cputune}
        <emulatorpin cpuset="0-383" />
    </cputune>
    <os>
        <type arch="aarch64" machine="virt">hvm</type>
        <loader readonly="yes" type="pflash">/usr/share/edk2/aarch64/QEMU_EFI-pflash.raw</loader>
        <boot dev="hd" />
    </os>
    <features>
        <acpi />
        <gic version="3" />
    </features>
    <cpu mode="host-passthrough" match="exact">
        <topology sockets="{cpu_sockets}" cores="{cpu_cores}" threads="{cpu_threads}" />
        <numa>
{numa_cells}
        </numa>
    </cpu>
    <clock offset="utc">
    </clock>
    <on_poweroff>destroy</on_poweroff>
    <on_reboot>restart</on_reboot>
    <on_crash>restart</on_crash>
    <devices>
        <controller type="scsi" index="0" model="virtio-scsi">
            <alias name="scsi0" />
            <address type="pci" domain="0x0000" bus="0x00" slot="0x04" function="0x0" />
        </controller>
        <emulator>/usr/bin/qemu-kvm</emulator>
        <disk type="file" device="disk">
            <driver name="qemu" type="qcow2" cache="none" io="native" />
            <source file="{image_path}" />
            <target dev="sda" bus="scsi" />
            <address type="drive" controller="0" bus="0" target="0" unit="0" />
        </disk>
        <interface type = "bridge">
            <source bridge="br0" />
            <model type="virtio" />
            <driver name='vhost' queues='1' mtu='9000' />
        </interface>
        <console type="pty" />
        <channel type="unix">
            <source mode="bind" />
            <target type="virtio" name="org.qemu.guest_agent.0" />
        </channel>
        <memballoon model="virtio" free_page_reporting="off">
            <stats period="10" />
            <alias name="balloon0" />
            <address type="pci" domain="0x0000" bus="0x04" slot="0x00" function="0x0" />
        </memballoon>
    </devices>
</domain>"""


# ==============================================================================
# 大页与NUMA规划
# ==============================================================================
def get_huge_page_info(numa_info):
    """按配置的大页类型获取NUMA的大页信息.

    Args:
        numa_info: NUMA信息对象, 其numa_huge_page_info以KiB为单位的大页大小为key.

    Returns:
        NumaInfoHugePageDataT | None: 对应大页类型的大页信息; 该NUMA未配置
        此大页类型时返回None.
    """
    return numa_info.numa_huge_page_info.get(HUGE_PAGE_TO_KB)


def validate_guest_numa_config(node_info_list):
    """在借用内存前校验guest NUMA配置与本地NUMA资源.

    Args:
        node_info_list: ubs_mem_fragmentation_node_info_list的返回结果.

    Raises:
        ValueError: guest NUMA数量非法、vCPU无法按guest NUMA均分或本地NUMA数量不足.
    """
    if GUEST_NUMA_COUNT not in (1, 2, 4):
        raise ValueError(f"Unsupported GUEST_NUMA_COUNT: {GUEST_NUMA_COUNT}, only 1/2/4 supported")
    if VCPU_COUNT % GUEST_NUMA_COUNT != 0:
        raise ValueError(
            f"VCPU_COUNT {VCPU_COUNT} cannot be evenly divided into {GUEST_NUMA_COUNT} guest numa"
        )
    local_numa_ids = []
    for node_info in node_info_list:
        if not node_info.is_current:
            continue
        for numa_info in node_info.numa_infos:
            if not numa_info.is_local:
                continue
            huge_page_info = get_huge_page_info(numa_info)
            if huge_page_info is None or huge_page_info.huge_page_total == 0:
                continue
            local_numa_ids.append(int(numa_info.numa_id))
    if len(local_numa_ids) < GUEST_NUMA_COUNT:
        raise ValueError(
            f"Local numa count {len(local_numa_ids)} {sorted(local_numa_ids)} is less than "
            f"GUEST_NUMA_COUNT {GUEST_NUMA_COUNT}"
        )


def assign_numa_to_guest(numa_infos, guest_numa_count):
    """将宿主NUMA按顺序轮流划分到各个guest NUMA.

    划分规则:
    1. 本地NUMA按numa_id升序排在前面, 借用NUMA按接口返回顺序排在后面;
    2. 第i个宿主NUMA分配给guest NUMA i % guest_numa_count, 即按顺序轮转分配;
    3. 每个宿主NUMA整体分配给一个guest NUMA, 不做拆分.

    Args:
        numa_infos: 宿主NUMA内存信息列表, 元素包含numa_id/size/is_local.
        guest_numa_count: guest NUMA个数.

    Returns:
        List[List[dict]]: 每个guest NUMA对应的宿主NUMA信息列表.

    Raises:
        ValueError: 宿主NUMA数量少于guest NUMA个数.
    """
    ordered_infos = sorted(
        (info for info in numa_infos if info["is_local"]), key=lambda info: info["numa_id"]
    )
    ordered_infos += [info for info in numa_infos if not info["is_local"]]
    if len(ordered_infos) < guest_numa_count:
        raise ValueError(
            f"Host numa count {len(ordered_infos)} is less than GUEST_NUMA_COUNT {guest_numa_count}"
        )
    groups = [[] for _ in range(guest_numa_count)]
    for index, info in enumerate(ordered_infos):
        groups[index % guest_numa_count].append(info)
    return groups


def generate_guest_numa_xml(guest_numa_groups):
    """生成guest NUMA相关的XML片段.

    每个guest NUMA生成一条numatune的memnode(内存来自哪些宿主NUMA及配额),
    以及一个CPU numa cell(vCPU区间按guest NUMA数量顺序均分, 内存为该guest NUMA
    对应宿主NUMA内存之和).

    Args:
        guest_numa_groups: assign_numa_to_guest返回的guest NUMA分组.

    Returns:
        Tuple[str, str]: numatune中的memnode片段, CPU numa中的cell片段.

    Raises:
        ValueError: 宿主NUMA内存总和不等于虚机内存规格, 或某个guest NUMA未分配到宿主NUMA.
    """
    guest_numa_count = len(guest_numa_groups)
    total_memory_mb = sum(info["size"] for group in guest_numa_groups for info in group)
    expected_memory_mb = MEM_SIZE * GB_TO_MB
    if total_memory_mb != expected_memory_mb:
        raise ValueError(
            f"Host numa memory {total_memory_mb}MB does not match VM memory {expected_memory_mb}MB"
        )
    vcpu_per_guest = VCPU_COUNT // guest_numa_count
    numa_tune_lines = []
    numa_cell_lines = []
    for guest_id, group in enumerate(guest_numa_groups):
        if not group:
            raise ValueError(f"No host numa assigned to guest numa {guest_id}")
        proportions = ":".join(f"{info['size']}-node{info['numa_id']}" for info in group)
        numa_tune_lines.append(
            f'        <memnode cellid="{guest_id}" mode="preferred" proportion="{proportions}" />'
        )
        vcpu_start = guest_id * vcpu_per_guest
        vcpu_end = vcpu_start + vcpu_per_guest - 1
        memory = sum(info["size"] for info in group) * MB_TO_KB
        numa_cell_lines.append(
            f'            <cell id="{guest_id}" cpus="{vcpu_start}-{vcpu_end}" '
            f'memory="{memory}" memAccess="shared" />'
        )
        LOG.info(
            f"Guest numa {guest_id}: cpus {vcpu_start}-{vcpu_end}, "
            f"host numas {[info['numa_id'] for info in group]}, memory {memory // MB_TO_KB}MB"
        )
    return "\n".join(numa_tune_lines), "\n".join(numa_cell_lines)


def generate_cputune():
    """生成vCPU与物理CPU的绑定关系.

    vCPU数量固定与物理节点CPU数一致, 因此按vCPU编号与物理CPU编号1:1绑定.

    Returns:
        str: cputune中的vcpupin片段.
    """
    return "\n".join(
        f'        <vcpupin vcpu="{vcpu}" cpuset="{vcpu}" />' for vcpu in range(VCPU_COUNT)
    )


# ==============================================================================
# 虚机XML生成与libvirt操作
# ==============================================================================
def write_xml_file(xml_content, file_path):
    """将XML内容写入文件, 目录不存在时自动创建.

    Args:
        xml_content: XML文本内容.
        file_path: XML文件路径.
    """
    os.makedirs(os.path.dirname(file_path), exist_ok=True)
    with open(file_path, "w") as f:
        f.write(xml_content)


async def create_vm_xml(guest_numa_groups):
    """按虚机规格与NUMA分组生成虚机XML文件.

    Args:
        guest_numa_groups: assign_numa_to_guest返回的guest NUMA分组.

    Returns:
        str: 生成的XML文件路径.
    """
    memory = str(MEM_SIZE * GB_TO_MB * MB_TO_KB)
    image_path = os.path.join(IMAGE_BASE_PATH, IMAGE_NAME)
    numa_tune, numa_cells = generate_guest_numa_xml(guest_numa_groups)
    cputune = generate_cputune()

    xml_content = VM_XML_TEMPLATE.format(
        uuid=str(uuid.uuid4()),
        vm_name=VM_NAME,
        memory=memory,
        huge_page_size=HUGE_PAGE_TO_KB,
        vcpu_count=VCPU_COUNT,
        cpu_sockets=CPU_SOCKETS,
        cpu_cores=CPU_CORES,
        cpu_threads=CPU_THREADS,
        numa_tune=numa_tune,
        numa_cells=numa_cells,
        cputune=cputune,
        image_path=image_path
    )
    write_xml_file(xml_content, XML_FILE_PATH)
    LOG.info(f"VM XML generated at {XML_FILE_PATH}")
    return XML_FILE_PATH


async def send_cmd(cmd, timeout=VIRSH_TIMEOUT):
    """异步执行命令并返回标准输出.

    Args:
        cmd: 命令及参数列表.
        timeout: 命令执行超时时间, 单位: 秒.

    Returns:
        str: 命令标准输出(已去除首尾空白).
    """
    result = await asyncio.to_thread(
        subprocess.run, cmd, shell=False, check=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=timeout, text=True
    )
    return result.stdout.strip()


async def get_vm_status(vm_name):
    """查询虚机状态.

    Args:
        vm_name: 虚机名称.

    Returns:
        str | None: 虚机状态(如running); 虚机不存在时返回None.
    """
    try:
        output = await send_cmd(["virsh", "list", "--all"])
        for line in output.splitlines():
            if vm_name in line:
                parts = line.split()
                return parts[2] if len(parts) >= 3 else None
        return None
    except Exception as e:
        LOG.error(f"Get vm status failed: {e}")
        raise


async def wait_vm_running(vm_name, max_attempts=MAX_MONITOR_ATTEMPTS, interval=MONITOR_INTERVAL):
    """等待虚机进入running状态.

    Args:
        vm_name: 虚机名称.
        max_attempts: 最大轮询次数.
        interval: 轮询间隔, 单位: 秒.

    Raises:
        TimeoutError: 超过最大轮询次数虚机仍未running.
    """
    for _ in range(max_attempts):
        status = await get_vm_status(vm_name)
        if status == "running":
            LOG.info(f"VM {vm_name} is running")
            return
        await asyncio.sleep(interval)
    raise TimeoutError(f"VM {vm_name} failed to reach running status")


async def define_and_start_vm(xml_file_path):
    """定义并启动虚机.

    Args:
        xml_file_path: 虚机XML文件路径.
    """
    await send_cmd(["virsh", "define", xml_file_path])
    await send_cmd(["virsh", "start", VM_NAME])
    LOG.info(f"VM {VM_NAME} defined and started")


async def get_pid(vm_name):
    """获取虚机qemu进程PID.

    Args:
        vm_name: 虚机名称.

    Returns:
        int: qemu进程PID.
    """
    pid_file = f"/var/run/libvirt/qemu/{vm_name}.pid"
    with open(pid_file) as f:
        return int(f.read().strip())


# ==============================================================================
# 内存借用与冷热页流动
# ==============================================================================
async def get_local_node(node_info_list):
    """从节点信息列表中获取当前节点ID.

    Args:
        node_info_list: ubs_mem_fragmentation_node_info_list的返回结果.

    Returns:
        str: 当前节点ID.

    Raises:
        ValueError: 未找到当前节点.
    """
    for node_info in node_info_list:
        if node_info.is_current:
            return node_info.node_id
    raise ValueError("Couldn't get local_node_id")


async def get_borrow_mem(size, node_info_list):
    """计算虚机需要从远端借用的内存大小.

    本地可用大页内存不足以覆盖虚机规格时, 按借入内存扣除流动预留后的差额借用,
    并按借用粒度向下对齐; 同时校验借用内存不超过虚机规格的指定比例.

    Args:
        size: 虚机内存规格, 单位: GB.
        node_info_list: ubs_mem_fragmentation_node_info_list的返回结果.

    Returns:
        int: 需要借用的内存大小, 单位: MB; 无需借用时为0.

    Raises:
        Exception: 借用内存超过虚机规格的REMOTE_MEMORY_USAGE_PERCENTAGE比例.
    """
    vm_size_mb = size * GB_TO_MB

    local_free_huge_page = 0
    local_free_huge_page_for_flow = 0
    for node_info in node_info_list:
        if not node_info.is_current:
            continue
        for numa_info in node_info.numa_infos:
            huge_page_info = get_huge_page_info(numa_info)
            if huge_page_info is None:
                continue
            huge_page_free = huge_page_info.huge_page_free * HUGE_PAGE_TO_MB
            local_free_huge_page += huge_page_free
            mem_flow = math.ceil((huge_page_free * MEM_FLOW_RATIO) / ONE_HUNDRED_PERCENT)
            local_free_huge_page_for_flow += huge_page_free - mem_flow
    local_free_huge_page = local_free_huge_page // MEM_BORROW_UNIT * MEM_BORROW_UNIT
    local_free_huge_page_for_flow = local_free_huge_page_for_flow // MEM_BORROW_UNIT * MEM_BORROW_UNIT
    borrow_mem = 0
    if vm_size_mb > local_free_huge_page:
        borrow_mem = vm_size_mb - local_free_huge_page_for_flow

    if int(vm_size_mb * REMOTE_MEMORY_USAGE_PERCENTAGE / ONE_HUNDRED_PERCENT) < borrow_mem:
        msg = "The amount of borrowed memory cannot exceed the amount of local memory."
        LOG.error(msg)
        raise Exception(msg)

    return borrow_mem


async def get_numa_meta_infos(node_info_list):
    """获取参与内存借用的NUMA元信息.

    仅保留配置了对应大页类型且大页总数不为0的NUMA.

    Args:
        node_info_list: ubs_mem_fragmentation_node_info_list的返回结果.

    Returns:
        List[NumaMetaInfoT]: 参与借用的NUMA元信息列表.
    """
    new_numa_infos = []
    for node_info in node_info_list:
        if not node_info.is_current:
            continue
        for numa_info in node_info.numa_infos:
            huge_page_info = get_huge_page_info(numa_info)
            if huge_page_info is None or huge_page_info.huge_page_total == 0:
                continue
            new_numa_infos.append(NumaMetaInfoT(numa_id=int(numa_info.numa_id)))
    return new_numa_infos


async def wait_borrow_tasks(borrow_result):
    """等待全部内存借用异步任务完成并回填借用结果.

    Args:
        borrow_result: ubs_mem_borrow异步调用返回的列表, 每个元素包含task_id,
            任务完成后回填borrow_ids与present_numa_ids.

    Raises:
        RuntimeError: 任务查询失败、任务执行失败或轮询超时.
    """
    for result in borrow_result:
        task_id = result.task_id
        for _ in range(ASYNC_MAX_RETRY):
            ret, task_info = ubs_task_result_query(task_id)
            if ret != 0 or task_info.status == AsyncTaskStatus.FAILED:
                raise RuntimeError(f"Failed to get task result or task is failed, task_id: {task_id}")
            if task_info.status == AsyncTaskStatus.SUCCESS:
                result.borrow_ids = task_info.mem_borrow_result.borrow_ids
                result.present_numa_ids = task_info.mem_borrow_result.present_numa_ids
                break
            await asyncio.sleep(ASYNC_RETRY_INTERVAL)
        else:
            raise RuntimeError(f"Failed to get task result, task_id: {task_id}")


def collect_numa_infos(node_info_list, borrow_mem):
    """按虚机规格收集各NUMA可分配给虚机的内存大小.

    借用场景下本地NUMA需先扣除冷热页流动预留; 单个NUMA的容量按1GB向下对齐,
    无可用大页的NUMA直接跳过, 且单个NUMA的容量不超过剩余的待分配内存;
    待分配内存分配完后立即停止收集.

    Args:
        node_info_list: 借用完成后的节点信息列表.
        borrow_mem: 借用内存大小, 单位: MB; 大于0时本地NUMA需扣除流动预留.

    Returns:
        Tuple[List[dict], Dict[int, int]]: 宿主NUMA内存信息列表(供XML生成与
        guest NUMA分组使用); 宿主NUMA编号到内存大小(MB)的映射(供页流动使用).
    """
    numa_infos = []
    numa_mem_dict = {}
    vm_size_mb = MEM_SIZE * GB_TO_MB
    for node_info in node_info_list:
        if not node_info.is_current:
            continue
        for numa_info in node_info.numa_infos:
            huge_page_info = get_huge_page_info(numa_info)
            if huge_page_info is None:
                continue
            size = huge_page_info.huge_page_free * HUGE_PAGE_TO_MB
            if numa_info.is_local and borrow_mem > 0:
                flow_mem = math.ceil((size * MEM_FLOW_RATIO) / ONE_HUNDRED_PERCENT)
                size -= flow_mem
            size = size // MEM_BORROW_UNIT_GB * MEM_BORROW_UNIT_GB
            if size <= 0:
                continue
            if size > vm_size_mb:
                size = vm_size_mb
            numa_infos.append({
                "numa_id": int(numa_info.numa_id),
                "size": size,
                "is_local": numa_info.is_local
            })
            numa_mem_dict[int(numa_info.numa_id)] = size
            vm_size_mb -= size
            if vm_size_mb <= 0:
                return numa_infos, numa_mem_dict
    return numa_infos, numa_mem_dict


async def get_page_swap_param(node_info_list, numa_mem_dict):
    """构造冷热页流动的NUMA配额参数.

    Args:
        node_info_list: 借用完成后的节点信息列表.
        numa_mem_dict: 宿主NUMA编号到该NUMA提供给虚机的内存大小(MB)的映射.

    Returns:
        List[PageSwapPairT]: 仅含一组本地/远端NUMA配额的页流动参数.
    """
    local_numas = []
    remote_numas = []
    for node_info in node_info_list:
        if not node_info.is_current:
            continue
        for numa_info in node_info.numa_infos:
            numa_quota = NumaQuotaT(
                numa_id=int(numa_info.numa_id),
                quota=numa_mem_dict.get(int(numa_info.numa_id), 0)
            )
            if numa_quota.quota == 0:
                continue
            if numa_info.is_local:
                local_numas.append(numa_quota)
            else:
                remote_numas.append(numa_quota)

    page_swap_pair = PageSwapPairT(
        local_numa_quotas=local_numas,
        remote_numa_quotas=remote_numas,
    )
    return [page_swap_pair]


# ==============================================================================
# 主流程
# ==============================================================================
async def create():
    """超大规格虚机创建主流程.

    步骤: 校验配置 -> 查询节点信息 -> 计算借用内存 -> 异步借用并等待完成 ->
    收集NUMA内存并划分guest NUMA -> 生成XML并启动虚机 -> 开启冷热页流动.
    """
    try:
        LOG.info("Start creating vm.")

        LOG.info("Getting node info...")
        node_info_list = ubs_mem_fragmentation_node_info_list()
        if not node_info_list:
            raise RuntimeError("Failed to call  ubs_mem_fragmentation_node_info_list")

        validate_guest_numa_config(node_info_list)

        LOG.info("Getting local_node and borrow_mem...")
        local_node = await get_local_node(node_info_list)
        borrow_mem = await get_borrow_mem(MEM_SIZE, node_info_list)
        numa_meta_infos = await get_numa_meta_infos(node_info_list)

        param = BorrowParamT(
            node_id=local_node,
            numa_meta_infos=numa_meta_infos,
            borrow_size=borrow_mem,
            page_size=HUGE_PAGE_TO_KB,
        )

        if borrow_mem > 0:
            LOG.info("Borrowing memory...")
            borrow_result = ubs_mem_borrow(param, True)
            await wait_borrow_tasks(borrow_result)

        node_info_list_after_borrow = ubs_mem_fragmentation_node_info_list()
        LOG.info("Generating xml...")
        numa_infos, numa_mem_dict = collect_numa_infos(node_info_list_after_borrow, borrow_mem)

        LOG.info(f"Creating VM: {VM_NAME}, mem_size: {MEM_SIZE}GB, image: {IMAGE_NAME}")
        guest_numa_groups = assign_numa_to_guest(numa_infos, GUEST_NUMA_COUNT)
        xml_file_path = await create_vm_xml(guest_numa_groups)

        await define_and_start_vm(xml_file_path)
        await wait_vm_running(VM_NAME)

        if borrow_mem > 0:
            LOG.info("Enabling page swap...")
            pid = await get_pid(VM_NAME)
            page_swap_enable = await get_page_swap_param(node_info_list_after_borrow, numa_mem_dict)
            res = ubs_page_swap_enable(pid, page_swap_enable)
            if res != 0:
                raise RuntimeError("Page swap failed")

        LOG.info(f"VM {VM_NAME} created successfully")
    except Exception as e:
        LOG.error(f"Create VM failed: {e}")
        raise SystemExit(1)


if __name__ == "__main__":
    asyncio.run(create())