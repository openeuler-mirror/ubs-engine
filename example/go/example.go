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

// package main 实现了go sdk的使用例子.
package main

import (
	"fmt"

	"atomgit.com/openeuler/ubs-engine.git/src/sdk/go/engine"
	"atomgit.com/openeuler/ubs-engine.git/src/sdk/go/log"
	"atomgit.com/openeuler/ubs-engine.git/src/sdk/go/mem"
	"atomgit.com/openeuler/ubs-engine.git/src/sdk/go/topo"
)

// 打印单个节点信息
func printNode(node *topo.UbsTopoNode, index int) {
	fmt.Printf("=== 节点信息 [%d] ===\n", index)
	fmt.Printf("槽位ID (SlotId): %d\n", node.SlotId)
	fmt.Printf("主机名 (HostName): %s\n", node.HostName)

	// 打印Socket ID
	fmt.Printf("Socket ID: ")
	for i, socketId := range node.SocketId {
		fmt.Printf("Socket[%d]=%d ", i, socketId)
	}
	fmt.Println()

	// 打印NUMA ID
	fmt.Printf("NUMA IDs:\n")
	for i, numaArray := range node.NumaIds {
		fmt.Printf("  Socket[%d]: ", i)
		for j, numaId := range numaArray {
			fmt.Printf("NUMA[%d]=%d ", j, numaId)
		}
		fmt.Println()
	}

	// 打印IP地址
	fmt.Printf("IP地址列表 (%d个):\n", len(node.IPs))
	for i, ip := range node.IPs {
		fmt.Printf("  IP[%d]: %s\n", i, ip.String())
	}
	fmt.Println()
}

// 打印NUMA内存描述信息
func printNumaDesc(desc mem.UbsMemNumaDesc, index int) {
	fmt.Printf("=== NUMA内存描述信息 [%d] ===\n", index)
	fmt.Printf("借用标识 (Name): %s\n", desc.Name)
	fmt.Printf("NUMA ID: %d\n", desc.NumaId)
	fmt.Printf("借用大小 (Size): %d bytes (%.2f GB)\n",
		desc.Size, float64(desc.Size)/(1024*1024*1024))

	fmt.Printf("用户信息 (UsrInfo): ")
	for _, b := range desc.UsrInfo {
		fmt.Printf("%02x ", b)
	}
	fmt.Println()

	// 打印借出节点信息
	fmt.Printf("\n借出节点 (Export Node):\n")
	fmt.Printf("  槽位ID: %d\n", desc.ExportNode.SlotId)
	fmt.Printf("  主机名: %s\n", desc.ExportNode.HostName)
	fmt.Printf("  IP地址: ")
	for i, ip := range desc.ExportNode.IPs {
		if i > 0 {
			fmt.Printf(", ")
		}
		fmt.Printf("%s", ip.String())
	}
	fmt.Printf("\n  Socket IDs: ")
	for i, socketId := range desc.ExportNode.SocketId {
		if i > 0 {
			fmt.Printf(", ")
		}
		fmt.Printf("%d", socketId)
	}
	fmt.Println()

	// 打印借入节点信息
	fmt.Printf("\n借入节点 (Import Node):\n")
	fmt.Printf("  槽位ID: %d\n", desc.ImportNode.SlotId)
	fmt.Printf("  主机名: %s\n", desc.ImportNode.HostName)
	fmt.Printf("  IP地址: ")
	for i, ip := range desc.ImportNode.IPs {
		if i > 0 {
			fmt.Printf(", ")
		}
		fmt.Printf("%s", ip.String())
	}
	fmt.Printf("\n  Socket IDs: ")
	for i, socketId := range desc.ImportNode.SocketId {
		if i > 0 {
			fmt.Printf(", ")
		}
		fmt.Printf("%d", socketId)
	}
	fmt.Println("\n")
}

func initializeClient() bool {
	result, err := engine.UbsEngineClientInitialize("/var/run/ubse/ubse.sock")
	if err != nil {
		fmt.Printf("Failed to initialize client: %v", err)
		return false
	}
	fmt.Printf("Initialize result: %d\n", result)
	return true
}

func finalizeClient() {
	err := engine.UbsEngineClientFinalize()
	if err != nil {
		fmt.Printf("Failed to finalize client: %v", err)
		return
	}
	fmt.Println("Client finalized successfully")
}

func registerLogCallback() {
	log.UbsEngineLogCallbackRegister(func(level log.Level, message string) {
		fmt.Printf("[log_example][%s]%s\n", level, message)
	})
}

func demonstrateTopology() {
	demonstrateAllNodes()
	demonstrateLocalNode()
	demonstrateAllLinks()
}

func demonstrateAllNodes() {
	fmt.Println("====== 开始查询全量节点信息 ======")
	nodes, err := topo.UbsTopoNodeList()
	if err != nil {
		fmt.Printf("查询节点列表失败: %v", err)
		return
	}

	fmt.Printf("成功获取到 %d 个节点信息\n\n", len(nodes))

	for i, node := range nodes {
		printNode(&node, i)
	}
	fmt.Println("====== 全量节点信息查询完成 ======\n")
}

func demonstrateLocalNode() {
	fmt.Println("====== 开始查询本节点信息 ======")
	node, err := topo.UbsTopoNodeLocalGet()
	if err != nil {
		fmt.Printf("查询本节点信息失败: %v", err)
		return
	}

	fmt.Println("成功获取到本节点信息")
	printNode(node, 0)
	fmt.Println("====== 本节点信息查询完成 ======\n")
}

func demonstrateAllLinks() {
	fmt.Println("====== 开始查询全量连接信息 ======")
	links, err := topo.UbsTopoLinkList()
	if err != nil {
		fmt.Printf("查询连接信息失败: %v", err)
		return
	}

	for i, goLink := range links {
		fmt.Printf("Link: %d\n", i)
		fmt.Printf("        SlotId=%d, SocketId=%d, PortId=%d\n", goLink.SlotId, goLink.SocketId, goLink.PortId)
		fmt.Printf("        PeerSlotId=%d, PeerSocketId=%d, PeerPortId=%d\n", goLink.PeerSlotId, goLink.PeerSocketId,
			goLink.PeerPortId)
	}
	fmt.Println("====== 全量连接信息查询完成 ======\n")
}

func demonstrateMemory() {
	fmt.Println("====== 开始查询全量NUMA内存信息 ======")
	descs, err := mem.UbsMemNumaList()
	if err != nil {
		fmt.Printf("查询NUMA内存信息失败: %v", err)
		return
	}

	fmt.Printf("成功获取到 %d 个NUMA内存描述信息\n", len(descs))

	for i, desc := range descs {
		printNumaDesc(desc, i)
	}
	fmt.Println("====== 全量NUMA内存信息查询完成 ======")
}

func main() {
	// 初始化客户端
	if !initializeClient() {
		return
	}
	defer finalizeClient() // 确保程序退出时清理资源

	// 注册日志回调
	registerLogCallback()

	// 执行拓扑查询示例
	demonstrateTopology()

	// 执行内存查询示例
	demonstrateMemory()
}
