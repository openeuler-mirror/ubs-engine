/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

// Package main query.md 只读查询接口的集成冒烟验证程序。
// 覆盖 query.md 的全部接口（按文档顺序）：
//  1. UbsMemFragmentationNodeInfo（本节点NUMA碎片信息）
//  2. UbsMemFragmentationVmInfo（本节点虚机信息）
//  3. UbsMemFragmentationNodeInfoList（集群节点列表）
//  4. UbsMemFragmentationVmInfoList（集群虚机列表）
//
// mem_fragmentation.md 的治理/心跳接口由 memfrag_verify 覆盖。
//
// 前置条件：
//  1. 所有集群节点已安装相同版本的 ubse RPM（混合版本会导致部分节点数据缺失）
//  2. ubse daemon 已启动
//  3. /usr/lib64/libubs-virt-agent.so.1 存在
//
// 用法（在 go.mod 所在仓库根目录的 src/addons/virt_agent/sdk/go 下执行）：
//
//	go run ./sample/query_verify                    # 依次查询4个接口
//	go run ./sample/query_verify -json query.json   # 将查询结果写入JSON文件（默认 query_result.json，置空则不输出）
package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"sort"
	"strings"
	"time"

	"atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/dlopen"
	"atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"
)

// queryResult query.md 四个只读接口结果的JSON载体
type queryResult struct {
	NodeNumaInfo   []memfragmentation.NumaInfo     `json:"nodeNumaInfo,omitempty"`
	NodeVmInfo     []memfragmentation.VmDomainInfo `json:"nodeVmInfo,omitempty"`
	NodeInfoList   []memfragmentation.NodeInfo     `json:"nodeInfoList,omitempty"`
	VmNodeInfoList []memfragmentation.VmNodeInfo   `json:"vmNodeInfoList,omitempty"`
}

func main() {
	jsonPath := flag.String("json", "query_result.json", "查询结果JSON输出文件（置空则不输出）")
	flag.Parse()

	// 加载 SDK 动态库，必须在使用任何接口前调用
	if err := dlopen.GetLibHandle(); err != nil {
		fmt.Fprintf(os.Stderr, "[FAIL] 加载动态库失败: %v\n", err)
		os.Exit(1)
	}
	defer dlopen.CloseLibHandle()

	if !runQuery(*jsonPath) {
		os.Exit(1)
	}
	fmt.Println("[PASS] 只读查询冒烟通过")
}

// arrayText 将切片格式化为逗号分隔的数组文本，如 [0,1,2]
func arrayText[T any](items []T) string {
	parts := make([]string, 0, len(items))
	for _, it := range items {
		parts = append(parts, fmt.Sprintf("%v", it))
	}
	return "[" + strings.Join(parts, ",") + "]"
}

// numaIdsOfNumaInfo 提取NUMA ID数组文本
func numaIdsOfNumaInfo(numas []memfragmentation.NumaInfo) string {
	ids := make([]int16, 0, len(numas))
	for _, n := range numas {
		ids = append(ids, n.NumaId)
	}
	return arrayText(ids)
}

// numaIdsOfVmNumaInfo 提取虚机NUMA ID数组文本
func numaIdsOfVmNumaInfo(numas []memfragmentation.VmNumaInfo) string {
	ids := make([]int16, 0, len(numas))
	for _, n := range numas {
		ids = append(ids, n.NumaId)
	}
	return arrayText(ids)
}

// hugePagesText 大页信息数组文本，如 [pageSize=2048kB,total=10,free=5; pageSize=1024kB,total=2,free=1]
func hugePagesText(pages []memfragmentation.NumaPageData) string {
	parts := make([]string, 0, len(pages))
	for _, p := range pages {
		parts = append(parts, fmt.Sprintf("pageSize=%dkB,total=%d,free=%d",
			p.PageSize, p.HugePageTotal, p.HugePageFree))
	}
	return "[" + strings.Join(parts, "; ") + "]"
}

// countCurrentNode 统计 IsCurrent 节点数
func countCurrentNode(nodes []memfragmentation.NodeInfo) int {
	n := 0
	for _, node := range nodes {
		if node.IsCurrent {
			n++
		}
	}
	return n
}

// countCurrentVmNode 统计 IsCurrent 节点数
func countCurrentVmNode(nodes []memfragmentation.VmNodeInfo) int {
	n := 0
	for _, node := range nodes {
		if node.IsCurrent {
			n++
		}
	}
	return n
}

// runQuery 依次执行 query.md 的4个只读接口，任意一个判定失败则整体失败
func runQuery(jsonPath string) bool {
	ok := true
	result := queryResult{}

	// 1. UbsMemFragmentationNodeInfo：本节点NUMA碎片信息
	start := time.Now()
	numas, err := memfragmentation.UbsMemFragmentationNodeInfo()
	if err != nil {
		fmt.Fprintf(os.Stderr, "[FAIL] UbsMemFragmentationNodeInfo: %v\n", err)
		ok = false
	} else if len(numas) == 0 {
		fmt.Fprintf(os.Stderr, "[FAIL] UbsMemFragmentationNodeInfo 返回0条NUMA\n")
		ok = false
	} else {
		result.NodeNumaInfo = numas
		fmt.Printf("[PASS] interface=UbsMemFragmentationNodeInfo numaCount=%d elapsedMs=%.3f\n",
			len(numas), float64(time.Since(start).Microseconds())/1000.0)
		fmt.Printf("  nodeId=%s hostName=%s timestamp=%d numaIds=%s\n",
			numas[0].NodeId, numas[0].HostName, numas[0].Timestamp, numaIdsOfNumaInfo(numas))
		for _, n := range numas {
			fmt.Printf("  numaId=%d socketId=%d isLocal=%v memTotal=%dkB memFree=%dkB hugePages=%s\n",
				n.NumaId, n.SocketId, n.IsLocal, n.MemTotal, n.MemFree, hugePagesText(n.HugePageData))
		}
	}

	// 2. UbsMemFragmentationVmInfo：本节点虚机信息（允许为空）
	start = time.Now()
	vms, err := memfragmentation.UbsMemFragmentationVmInfo()
	if err != nil {
		fmt.Fprintf(os.Stderr, "[FAIL] UbsMemFragmentationVmInfo: %v\n", err)
		ok = false
	} else {
		result.NodeVmInfo = vms
		fmt.Printf("[PASS] interface=UbsMemFragmentationVmInfo vmCount=%d elapsedMs=%.3f\n",
			len(vms), float64(time.Since(start).Microseconds())/1000.0)
		for _, vm := range vms {
			fmt.Printf("  name=%s uuid=%s state=%s hostName=%s vmCreateTime=%d maxMem=%dkB pid=%d numaIds=%s\n",
				vm.MetaData.Name, vm.MetaData.Uuid, vm.MetaData.State, vm.MetaData.HostName,
				vm.MetaData.VmCreateTime, vm.MetaData.MaxMem, vm.MetaData.Pid,
				numaIdsOfVmNumaInfo(vm.NumaInfo))
			for _, numa := range vm.NumaInfo {
				fmt.Printf("    numaId=%d pageSize=%dkB socketId=%d isLocal=%v usedMem=%dkB\n",
					numa.NumaId, numa.PageSize, numa.SocketId, numa.IsLocal, numa.UsedMem)
			}
		}
	}

	// 3. UbsMemFragmentationNodeInfoList：集群节点列表，要求节点数≥1且IsCurrent恰好1个
	start = time.Now()
	nodes, err := memfragmentation.UbsMemFragmentationNodeInfoList()
	if err != nil {
		fmt.Fprintf(os.Stderr, "[FAIL] UbsMemFragmentationNodeInfoList: %v\n", err)
		ok = false
	} else {
		current := countCurrentNode(nodes)
		if len(nodes) == 0 || current != 1 {
			fmt.Fprintf(os.Stderr, "[FAIL] UbsMemFragmentationNodeInfoList: 节点数=%d IsCurrent数=%d（应为≥1和1）\n",
				len(nodes), current)
			ok = false
		} else {
			result.NodeInfoList = nodes
			fmt.Printf("[PASS] interface=UbsMemFragmentationNodeInfoList nodeCount=%d elapsedMs=%.3f\n",
				len(nodes), float64(time.Since(start).Microseconds())/1000.0)
			for _, n := range nodes {
				fmt.Printf("  nodeId=%s isCurrent=%v isMaster=%v podId=%s numaIds=%s\n",
					n.NodeId, n.IsCurrent, n.IsMaster, n.PodId, numaIdsOfNumaInfo(n.NumaInfos))
				for _, numa := range n.NumaInfos {
					fmt.Printf("    numaId=%d socketId=%d isLocal=%v hostName=%s timestamp=%d memTotal=%dkB memFree=%dkB hugePages=%s\n",
						numa.NumaId, numa.SocketId, numa.IsLocal, numa.HostName, numa.Timestamp,
						numa.MemTotal, numa.MemFree, hugePagesText(numa.HugePageData))
				}
			}
		}
	}

	// 4. UbsMemFragmentationVmInfoList：集群虚机列表，要求节点数≥1且IsCurrent恰好1个
	start = time.Now()
	vmNodes, err := memfragmentation.UbsMemFragmentationVmInfoList()
	if err != nil {
		fmt.Fprintf(os.Stderr, "[FAIL] UbsMemFragmentationVmInfoList: %v\n", err)
		ok = false
	} else {
		current := countCurrentVmNode(vmNodes)
		if len(vmNodes) == 0 || current != 1 {
			fmt.Fprintf(os.Stderr, "[FAIL] UbsMemFragmentationVmInfoList: 节点数=%d IsCurrent数=%d（应为≥1和1）\n",
				len(vmNodes), current)
			ok = false
		} else {
			result.VmNodeInfoList = vmNodes
			totalVm := 0
			for _, n := range vmNodes {
				totalVm += len(n.VmInfos)
			}
			fmt.Printf("[PASS] interface=UbsMemFragmentationVmInfoList nodeCount=%d vmTotalCount=%d elapsedMs=%.3f\n",
				len(vmNodes), totalVm, float64(time.Since(start).Microseconds())/1000.0)
			// 当前节点排最前，其余节点按 nodeId 排序
			sorted := make([]memfragmentation.VmNodeInfo, len(vmNodes))
			copy(sorted, vmNodes)
			sort.SliceStable(sorted, func(i, j int) bool {
				if sorted[i].IsCurrent != sorted[j].IsCurrent {
					return sorted[i].IsCurrent
				}
				return sorted[i].NodeId < sorted[j].NodeId
			})
			for _, n := range sorted {
				fmt.Printf("  nodeId=%s isCurrent=%v isMaster=%v podId=%s vmCount=%d\n",
					n.NodeId, n.IsCurrent, n.IsMaster, n.PodId, len(n.VmInfos))
				for _, vm := range n.VmInfos {
					fmt.Printf("    uuid=%s name=%s state=%s hostName=%s vmCreateTime=%d maxMem=%dkB pid=%d numaIds=%s\n",
						vm.MetaData.Uuid, vm.MetaData.Name, vm.MetaData.State, vm.MetaData.HostName,
						vm.MetaData.VmCreateTime, vm.MetaData.MaxMem, vm.MetaData.Pid,
						numaIdsOfVmNumaInfo(vm.NumaInfo))
					for _, numa := range vm.NumaInfo {
						fmt.Printf("      numaId=%d pageSize=%dkB socketId=%d isLocal=%v usedMem=%dkB\n",
							numa.NumaId, numa.PageSize, numa.SocketId, numa.IsLocal, numa.UsedMem)
					}
				}
			}
		}
	}

	if jsonPath != "" {
		data, err := json.MarshalIndent(result, "", "  ")
		if err != nil {
			fmt.Fprintf(os.Stderr, "[FAIL] JSON序列化失败: %v\n", err)
			ok = false
		} else if err := os.WriteFile(jsonPath, data, 0644); err != nil {
			fmt.Fprintf(os.Stderr, "[FAIL] 写入JSON文件 %s 失败: %v\n", jsonPath, err)
			ok = false
		} else {
			fmt.Printf("[PASS] jsonFile=%s\n", jsonPath)
		}
	}
	return ok
}
