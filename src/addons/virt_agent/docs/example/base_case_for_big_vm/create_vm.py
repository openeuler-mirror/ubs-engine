# -*- coding: utf-8 -*-
import asyncio
import logging
import os
import subprocess
import uuid
import math

from ubse.ubs_virt_agent_fragmentation import (
    ubs_task_result_query,
    ubs_mem_fragmentation_node_info_list,
    ubs_mem_borrow,
    ubs_page_swap_enable
)
from ubse.models.ubs_virt_agent_model import BorrowParamT, NumaMetaInfoT, PageSwapPairT, NumaQuotaT

from ubse.ubs_engine_log import ubs_engine_log_callback_register
from ubse.ubs_virt_agent_log import ubs_virt_agent_log_callback_register

ubs_engine_log_callback_register(lambda level, msg: None)
ubs_virt_agent_log_callback_register(lambda level, msg: None)

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
LOG = logging.getLogger(__name__)

MEM_SIZE = 5120
IMAGE_NAME = "openEuler-22.03-SP2-aarch64-everything-redis-Performance.qcow2"
VM_NAME = "vm1"
IMAGE_BASE_PATH = "/opt/install/tmp/images"
XML_FILE_PATH = "/etc/libvirt/qemu/vm1.xml"
VIRSH_TIMEOUT = 3600
MONITOR_INTERVAL = 10
MAX_MONITOR_ATTEMPTS = 720
ASYNC_MAX_RETRY = 3600
ASYNC_RETRY_INTERVAL = 10
GB_TO_MB = 1024
MB_TO_KB = 1024
HUGE_PAGE_TO_KB = 2 * 1024
HUGE_PAGE_TO_MB = 2
MEM_BORROW_UNIT_GB = 1024
MEM_BORROW_UNIT_MB = 128
COLLECT_HUGE_PAGE_GB = 1024
REMOTE_MEMORY_USAGE_PERCENTAGE = 50
ONE_HUNDRED_PERCENT = 100
MEM_FLOW_RATIO = 1

VM_XML_TEMPLATE = """<domain type="kvm">
    <uuid>{uuid}</uuid>
    <name>{vm_name}</name>
    <memory>{memory}</memory>
    <memoryBacking>
        <hugepages>
            <page size="2048" nodeset="0" unit="KiB" />
        </hugepages>
        <allocation mode="hugepage-ondemand" />
    </memoryBacking>
    <numatune>
        {numa_tune}
    </numatune>
    <vcpu placement="static">192</vcpu>
    <cputune>
        {cputune}
        <emulatorpin cpuset="0-47,96-143,192-239,288-335" />
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
        <topology sockets="2" cores="48" threads="2" />
        <numa>
            <cell id="0" cpus="0-191" memory="{memory}" memAccess="shared" />
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


class AsyncTaskStatus:
    RUNNING = 1
    SUCCESS = 2
    FAILED = 3


def generate_numa_tune(numa_infos):
    proportions = []
    for info in numa_infos:
        proportions.append(f"{info['size']}-node{info['numa_id']}")
    return f'<memnode cellid="0" mode="preferred" proportion="{":".join(proportions)}" />'


def generate_cputune():
    vcpupin_list = []
    mappings = [
        (0, 47, 0, 47),
        (48, 95, 96, 143),
        (96, 143, 192, 239),
        (144, 191, 288, 335),
    ]
    for vcpu_start, vcpu_end, cpu_start, cpu_end in mappings:
        for vcpu, cpu in zip(range(vcpu_start, vcpu_end + 1), range(cpu_start, cpu_end + 1)):
            vcpupin_list.append(f'        <vcpupin vcpu="{vcpu}" cpuset="{cpu}" />')
    return "\n".join(vcpupin_list)


async def send_cmd(cmd, timeout=VIRSH_TIMEOUT):
    result = await asyncio.to_thread(
        subprocess.run, cmd, shell=False, check=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=timeout, text=True
    )
    return result.stdout.strip()


async def get_vm_status(vm_name):
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
    for _ in range(max_attempts):
        status = await get_vm_status(vm_name)
        if status == "running":
            LOG.info(f"VM {vm_name} is running")
            return
        await asyncio.sleep(interval)
    raise TimeoutError(f"VM {vm_name} failed to reach running status")


def write_xml_file(xml_content, file_path):
    os.makedirs(os.path.dirname(file_path), exist_ok=True)
    with open(file_path, "w") as f:
        f.write(xml_content)


async def create_vm_xml(numa_infos):
    memory = str(MEM_SIZE * 1024 * 1024)
    image_path = os.path.join(IMAGE_BASE_PATH, IMAGE_NAME)
    numa_tune = generate_numa_tune(numa_infos)
    cputune = generate_cputune()

    xml_content = VM_XML_TEMPLATE.format(
        uuid=str(uuid.uuid4()),
        vm_name=VM_NAME,
        memory=memory,
        numa_tune=numa_tune,
        cputune=cputune,
        image_path=image_path
    )
    write_xml_file(xml_content, XML_FILE_PATH)
    LOG.info(f"VM XML generated at {XML_FILE_PATH}")
    return XML_FILE_PATH


async def define_and_start_vm(xml_file_path):
    await send_cmd(["virsh", "define", xml_file_path])
    await send_cmd(["virsh", "start", VM_NAME])
    LOG.info(f"VM {VM_NAME} defined and started")


async def get_pid(vm_name):
    pid_file = f"/var/run/libvirt/qemu/{vm_name}.pid"
    with open(pid_file) as f:
        return int(f.read().strip())


async def get_local_node(node_info_list):
    for node_info in node_info_list:
        if node_info.is_current:
            return node_info.node_id
    raise ValueError("Couldn't get local_node_id")


async def get_borrow_mem(size, node_info_list):
    vm_size_mb = size * GB_TO_MB

    collect_hugepage_type = COLLECT_HUGE_PAGE_GB
    if collect_hugepage_type == COLLECT_HUGE_PAGE_GB:
        mem_borrow_unit = MEM_BORROW_UNIT_GB
    else:
        mem_borrow_unit = MEM_BORROW_UNIT_MB

    local_free_huge_page = 0
    local_free_huge_page_for_flow = 0
    for node_info in node_info_list:
        if not node_info.is_current:
            continue
        for numa_info in node_info.numa_infos:
            huge_page_free = numa_info.numa_huge_page_info[2 * MB_TO_KB].huge_page_free * HUGE_PAGE_TO_MB
            local_free_huge_page += huge_page_free
            mem_flow = math.ceil((huge_page_free * MEM_FLOW_RATIO) / ONE_HUNDRED_PERCENT)
            local_free_huge_page_for_flow += huge_page_free - mem_flow
    local_free_huge_page = local_free_huge_page // mem_borrow_unit * mem_borrow_unit
    local_free_huge_page_for_flow = local_free_huge_page_for_flow // mem_borrow_unit * mem_borrow_unit
    borrow_mem = 0
    if vm_size_mb > local_free_huge_page:
        borrow_mem = vm_size_mb - local_free_huge_page_for_flow

    remote_memory_usage_percentage = REMOTE_MEMORY_USAGE_PERCENTAGE
    if int(vm_size_mb * remote_memory_usage_percentage / ONE_HUNDRED_PERCENT) < borrow_mem:
        msg = "The amount of borrowed memory cannot exceed the amount of local memory."
        LOG.error(msg)
        raise Exception(msg)

    return borrow_mem


async def get_numa_len_and_numa_info(node_info_list):
    new_numa_infos = []
    new_numa_len = 0
    for node_info in node_info_list:
        if not node_info.is_current:
            continue
        new_numa_len = len(node_info.numa_infos)
        for numa_info in node_info.numa_infos:
            if numa_info.numa_huge_page_info[2 * MB_TO_KB].huge_page_total == 0:
                continue
            new_numa_info = NumaMetaInfoT(
                socket_id=numa_info.socket_id,
                numa_id=int(numa_info.numa_id)
            )
            new_numa_infos.append(new_numa_info)
    return new_numa_len, new_numa_infos


async def get_page_swap_param(node_info_list, numa_mem_dict):
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


async def create():
    try:
        LOG.info("Start creating vm.")

        LOG.info("Getting node info...")
        node_info_list = ubs_mem_fragmentation_node_info_list()
        if not node_info_list:
            raise RuntimeError("Failed to call ubs_mem_fragmentation_node_info_list")

        LOG.info("Getting local_node and borrow_mem...")
        local_node = await get_local_node(node_info_list)
        borrow_mem = await get_borrow_mem(MEM_SIZE, node_info_list)
        new_numa_len, new_numa_infos = await get_numa_len_and_numa_info(node_info_list)

        param = BorrowParamT(
            node_id=local_node,
            numa_meta_infos=new_numa_infos,
            borrow_size=borrow_mem
        )

        borrow_result = []
        if borrow_mem > 0:
            LOG.info("Borrowing memory...")
            borrow_result = ubs_mem_borrow(param, True)
            for i in range(len(borrow_result)):
                task_id = borrow_result[i].task_id
                flag = False
                for _ in range(ASYNC_MAX_RETRY):
                    ret, task_info = ubs_task_result_query(task_id)
                    if ret != 0 or task_info is AsyncTaskStatus.FAILED:
                        raise RuntimeError(f"Failed to get task result or task is failed, task_id: {task_id}")
                    if task_info.status == AsyncTaskStatus.SUCCESS:
                        flag = True
                        borrow_result[i].borrow_ids = task_info.mem_borrow_result.borrow_ids
                        borrow_result[i].present_numa_ids = task_info.mem_borrow_result.present_numa_ids
                        break
                    await asyncio.sleep(ASYNC_RETRY_INTERVAL)
                if not flag:
                    raise RuntimeError(f"Failed to get task result, task_id: {task_id}")

        node_info_list_after_borrow = ubs_mem_fragmentation_node_info_list()
        LOG.info("Generating xml...")

        numa_infos = []
        numa_mem_dict = {}
        vm_size_mb = MEM_SIZE * GB_TO_MB
        for node_info in node_info_list_after_borrow:
            if not node_info.is_current:
                continue
            for numa_info in node_info.numa_infos:
                size = numa_info.numa_huge_page_info[HUGE_PAGE_TO_KB].huge_page_free * HUGE_PAGE_TO_MB
                if numa_info.is_local and borrow_mem > 0:
                    flow_mem = math.ceil((size * MEM_FLOW_RATIO) / ONE_HUNDRED_PERCENT)
                    size -= flow_mem
                size = size // MEM_BORROW_UNIT_GB * MEM_BORROW_UNIT_GB
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
                    break

        LOG.info(f"Creating VM: {VM_NAME}, mem_size: {MEM_SIZE}GB, image: {IMAGE_NAME}")
        xml_file_path = await create_vm_xml(numa_infos)

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