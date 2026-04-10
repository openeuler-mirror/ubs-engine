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

// package topo 实现了topo的go sdk.
package topo

/*
#cgo CFLAGS: -I/usr/include/ubse -I${SRCDIR}/../../c/include
#include <stdlib.h>
#include <netinet/in.h>
#include "ubs_engine_topo.h"

// 定义函数指针类型
typedef int32_t (*ubs_topo_node_list_func)(ubs_topo_node_t**, uint32_t*);
typedef int32_t (*ubs_topo_node_local_get_func)(ubs_topo_node_t*);
typedef int32_t (*ubs_topo_link_list_func)(ubs_topo_link_t**, uint32_t*);

// C桥接函数
int call_ubs_topo_node_list(ubs_topo_node_list_func fn, ubs_topo_node_t** nodes, uint32_t* count) {
    if (fn == NULL) return -1;
    return fn(nodes, count);
}

int call_ubs_topo_node_local_get(ubs_topo_node_local_get_func fn, ubs_topo_node_t* node) {
    if (fn == NULL) return -1;
    return fn(node);
}

int call_ubs_topo_link_list(ubs_topo_link_list_func fn, ubs_topo_link_t** links, uint32_t* count) {
    if (fn == NULL) return -1;
    return fn(links, count);
}
*/
import "C"
import (
	"fmt"
	"net"
	"unsafe"

	"atomgit.com/openeuler/ubs-engine.git/src/sdk/go/dlopen"
)

// 常量定义，与C头文件保持一致
const (
	TopoSocketNum  = 2   // UBS_TOPO_SOCKET_NUM
	TopoNumaNum    = 4   // UBS_TOPO_NUMA_NUM
	TopoIpAddrNum  = 50  // UBS_TOPO_IPADDR_NUM
	TopoMaxNodeNum = 512 // UBS_TOPO_MAX_NODE_NUM
	MaxCount       = 4096
	Ipv4Length     = 4
	Ipv6Length     = 16
)

// UbsTopoNode 定义节点信息结构体
type UbsTopoNode struct {
	SlotId   uint32
	SocketId [TopoSocketNum]uint32
	NumaIds  [TopoSocketNum][TopoNumaNum]uint32
	IPs      []net.IP
	HostName string
}

// UbsTopoLink 定义拓扑连接信息结构体
type UbsTopoLink struct {
	SlotId       uint32
	SocketId     uint32
	PortId       uint32
	PeerSlotId   uint32
	PeerSocketId uint32
	PeerPortId   uint32
}

// UbsTopoNodeList 查询全量节点信息
func UbsTopoNodeList() ([]UbsTopoNode, error) {
	// 加载函数符号
	nodeListPtr, err := dlopen.LoadFunc("ubs_topo_node_list")
	if err != nil {
		return nil, fmt.Errorf("failed to load ubs_topo_node_list: %v", err)
	}

	// 转换为函数指针类型
	nodeListFunc := (C.ubs_topo_node_list_func)(nodeListPtr)

	var cNodes *C.ubs_topo_node_t
	var cCount C.uint32_t

	ret := C.call_ubs_topo_node_list(nodeListFunc, &cNodes, &cCount)
	if ret != 0 {
		return nil, fmt.Errorf("ubs_topo_node_list failed, error code=%d", ret)
	}
	defer C.free(unsafe.Pointer(cNodes))

	count := int(cCount)
	if count <= 0 || count > MaxCount {
		return []UbsTopoNode{}, nil
	}

	return convertTopoNodeArray(cNodes, count), nil
}

// UbsTopoNodeLocalGet 查询本节点信息
func UbsTopoNodeLocalGet() (*UbsTopoNode, error) {
	// 加载函数符号
	nodeLocalGetPtr, err := dlopen.LoadFunc("ubs_topo_node_local_get")
	if err != nil {
		return nil, fmt.Errorf("failed to load ubs_topo_node_local_get: %v", err)
	}

	// 转换为函数指针类型
	nodeLocalGetFunc := (C.ubs_topo_node_local_get_func)(nodeLocalGetPtr)

	var cNode C.ubs_topo_node_t

	ret := C.call_ubs_topo_node_local_get(nodeLocalGetFunc, &cNode)
	if ret != 0 {
		return nil, fmt.Errorf("ubs_topo_node_local_get failed, error code=%d", ret)
	}

	return convertTopoNode(&cNode), nil
}

// UbsTopoLinkList 查询全量拓扑连接信息
func UbsTopoLinkList() ([]UbsTopoLink, error) {
	// 加载函数符号
	linkListPtr, err := dlopen.LoadFunc("ubs_topo_link_list")
	if err != nil {
		return nil, fmt.Errorf("failed to load ubs_topo_link_list: %v", err)
	}

	// 转换为函数指针类型
	linkListFunc := (C.ubs_topo_link_list_func)(linkListPtr)

	var cLinks *C.ubs_topo_link_t
	var cCount C.uint32_t

	ret := C.call_ubs_topo_link_list(linkListFunc, &cLinks, &cCount)
	if ret != 0 {
		return nil, fmt.Errorf("ubs_topo_link_list failed, code=%d", ret)
	}
	defer C.free(unsafe.Pointer(cLinks))

	count := int(cCount)
	result := make([]UbsTopoLink, 0, count)
	size := unsafe.Sizeof(C.ubs_topo_link_t{})

	for i := 0; i < count; i++ {
		link := (*C.ubs_topo_link_t)(unsafe.Pointer(uintptr(unsafe.Pointer(cLinks)) + uintptr(i)*size))
		goLink := UbsTopoLink{
			SlotId:       uint32(link.slot_id),
			SocketId:     uint32(link.socket_id),
			PortId:       uint32(link.port_id),
			PeerSlotId:   uint32(link.peer_slot_id),
			PeerSocketId: uint32(link.peer_socket_id),
			PeerPortId:   uint32(link.peer_port_id),
		}
		result = append(result, goLink)
	}
	return result, nil
}

// convertTopoNodeArray 转换C节点数组为Go节点切片
func convertTopoNodeArray(cNodes *C.ubs_topo_node_t, count int) []UbsTopoNode {
	if count <= 0 || count > MaxCount {
		return []UbsTopoNode{}
	}
	nodes := make([]UbsTopoNode, 0, count)
	size := unsafe.Sizeof(C.ubs_topo_node_t{})

	for i := 0; i < count; i++ {
		cNode := (*C.ubs_topo_node_t)(unsafe.Pointer(uintptr(unsafe.Pointer(cNodes)) + uintptr(i)*size))
		goNode := convertTopoNode(cNode)
		nodes = append(nodes, *goNode)
	}

	return nodes
}

// convertTopoNode 将C的ubs_topo_node_t转换为Go的UbsTopoNode
func convertTopoNode(cNode *C.ubs_topo_node_t) *UbsTopoNode {
	if cNode == nil {
		return nil
	}

	goNode := &UbsTopoNode{
		SlotId:   uint32(cNode.slot_id),
		IPs:      make([]net.IP, 0),
		HostName: C.GoString(&cNode.host_name[0]),
	}

	// 复制SocketId数组
	for j := 0; j < TopoSocketNum; j++ {
		goNode.SocketId[j] = uint32(cNode.socket_id[j])
	}

	// 复制NumaIds二维数组
	for j := 0; j < TopoSocketNum; j++ {
		for k := 0; k < TopoNumaNum; k++ {
			goNode.NumaIds[j][k] = uint32(cNode.numa_ids[j][k])
		}
	}

	// 处理IP地址
	goNode.IPs = extractIPsFromCNode(cNode)

	return goNode
}

// extractIPsFromCNode 从C节点中提取IP地址
func extractIPsFromCNode(cNode *C.ubs_topo_node_t) []net.IP {
	ips := make([]net.IP, 0)

	for j := 0; j < TopoIpAddrNum; j++ {
		cIp := cNode.ips[j]

		var ip net.IP
		if cIp.af == C.AF_INET {
			ipv4Bytes := C.GoBytes(unsafe.Pointer(&cIp.ipv4), Ipv4Length)
			ip = net.IP(ipv4Bytes)
		} else if cIp.af == C.AF_INET6 {
			ipv6Bytes := C.GoBytes(unsafe.Pointer(&cIp.ipv6), Ipv6Length)
			ip = net.IP(ipv6Bytes)
		}

		// 检查是否为有效IP（非零IP）
		if ip != nil && !ip.IsUnspecified() {
			ips = append(ips, ip)
		}
	}

	return ips
}
