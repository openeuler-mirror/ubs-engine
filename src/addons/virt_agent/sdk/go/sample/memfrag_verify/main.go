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

// Package main mem_fragmentation.md 内存碎片化治理接口的集成冒烟验证程序。
// 覆盖 mem_fragmentation.md 的全部接口（按文档顺序）：
//  1. UbsMemBorrowExecute（borrow-exec）
//  2. UbsMemReturn（return）
//  3. UbsMemRollback（rollback）
//  4. UbsSyncTaskQuery（task）
//  5. UbsMemBorrow（borrow）
//  6. UbsMemFragmentationHeartbeat（默认/heartbeat）
//
// query.md 的只读查询接口（NodeInfo/VmInfo/NodeInfoList/VmInfoList）由 query_verify 覆盖。
//
// 前置条件：
//  1. 所有集群节点已安装相同版本的 ubse RPM
//  2. ubse daemon 已启动
//  3. /usr/lib64/libubs-virt-agent.so.1 存在
//
// 用法（在仓库根目录的 src/addons/virt_agent/sdk/go 下执行）：
//
//	go build -o /tmp/memfrag_verify ./sample/memfrag_verify
//	/tmp/memfrag_verify                        # 默认：单次心跳探测（安全）
//	/tmp/memfrag_verify heartbeat [-count 10] [-interval 1] [-json hb.json]
//	/tmp/memfrag_verify task [taskId]          # 不传taskId时用不存在的探针ID验证调用链
//	/tmp/memfrag_verify borrow-exec -src-host 1 -src-socket 1 -src-numa 0 -size 128 -dst-host 1 -dst-socket 1 -dst-numa 1 [-async]  # -size 单位kB
//	/tmp/memfrag_verify borrow -nid node0 -size 1024 -numas 0,1,2,3 [-async]  # -size 单位MB
//	/tmp/memfrag_verify return [-async]        # 归还本节点全部借用内存，慎用
//	/tmp/memfrag_verify rollback -node 1 -ids id1,id2
package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"strconv"
	"strings"
	"time"

	"atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/dlopen"
	"atomgit.com/openeuler/ubs-engine.git/src/addons/virt_agent/sdk/go/memfragmentation"
)

// probeTaskId 任务不存在路径使用的固定探针ID
const probeTaskId = "memfrag-verify-task-000000000000"

// heartbeatRound 单轮心跳探测结果的JSON载体
type heartbeatRound struct {
	Round     int     `json:"round"`
	Alive     bool    `json:"alive"`
	ElapsedMs float64 `json:"elapsedMs"`
	Error     string  `json:"error,omitempty"`
}

func main() {
	// 加载 SDK 动态库，必须在使用任何接口前调用
	if err := dlopen.GetLibHandle(); err != nil {
		fmt.Fprintf(os.Stderr, "[FAIL] 加载动态库失败: %v\n", err)
		os.Exit(1)
	}
	defer dlopen.CloseLibHandle()

	args := os.Args[1:]
	var cmd string
	if len(args) > 0 {
		cmd, args = args[0], args[1:]
	}

	var ok bool
	switch cmd {
	case "", "heartbeat":
		ok = runHeartbeat(args)
	case "task":
		ok = runTask(args)
	case "borrow-exec":
		ok = runBorrowExec(args)
	case "borrow":
		ok = runBorrow(args)
	case "return":
		ok = runReturn(args)
	case "rollback":
		ok = runRollback(args)
	default:
		usage()
		os.Exit(1)
	}
	if !ok {
		os.Exit(1)
	}
}

func usage() {
	fmt.Fprintf(os.Stderr, `用法：memfrag_verify <子命令> [参数]
  (无参数/heartbeat) 心跳探测 ubse 服务存活（默认单次，安全）
                   可选 -count <次数> -interval <间隔秒> -json <file>（默认 heartbeat_result.json，置空则不输出）
  task [taskId]     查询异步任务状态；不传 taskId 时用不存在的探针ID验证调用链（预期业务错误）
  borrow-exec ...   执行内存借用（UbsMemBorrowExecute）
  borrow ...        大规格虚机按节点借用（UbsMemBorrow）
  return [-async]   归还本节点全部借用内存（UbsMemReturn），慎用
  rollback ...      借用回滚（UbsMemRollback）
只读查询（query.md：NodeInfo/VmInfo/NodeInfoList/VmInfoList）请使用 query_verify。
各写操作子命令的参数说明见其 -h 输出。
`)
}

// isSymbolErr 判断错误是否为符号加载失败（说明动态库链路有问题，而非业务错误）
func isSymbolErr(err error) bool {
	return err != nil && strings.Contains(err.Error(), "failed to load symbol")
}

// statusName 异步任务状态的可读名称
func statusName(s memfragmentation.AsyncTaskStatus) string {
	switch s {
	case memfragmentation.AsyncTaskNotExist:
		return "不存在"
	case memfragmentation.AsyncTaskRunning:
		return "运行中"
	case memfragmentation.AsyncTaskSuccess:
		return "成功"
	case memfragmentation.AsyncTaskFailed:
		return "失败"
	default:
		return fmt.Sprintf("未知(%d)", int32(s))
	}
}

// arrayText 将切片格式化为逗号分隔的数组文本，如 [0,1,2]
func arrayText[T any](items []T) string {
	parts := make([]string, 0, len(items))
	for _, it := range items {
		parts = append(parts, fmt.Sprintf("%v", it))
	}
	return "[" + strings.Join(parts, ",") + "]"
}

// borrowResultText 借用结果字段文本，空数据不输出
func borrowResultText(r memfragmentation.MemBorrowResult) string {
	var parts []string
	if len(r.BorrowIds) > 0 {
		parts = append(parts, "borrowIds="+arrayText(r.BorrowIds))
	}
	if len(r.PresentNumaIds) > 0 {
		parts = append(parts, "presentNumaIds="+arrayText(r.PresentNumaIds))
	}
	if r.TaskId != "" {
		parts = append(parts, "taskId="+r.TaskId)
	}
	return strings.Join(parts, " ")
}

// runHeartbeat UbsMemFragmentationHeartbeat 冒烟：连续探测 ubse 服务存活
func runHeartbeat(args []string) bool {
	fs := flag.NewFlagSet("heartbeat", flag.ExitOnError)
	count := fs.Int("count", 1, "连续探测次数")
	interval := fs.Int("interval", 1, "两次探测之间的间隔秒数")
	jsonPath := fs.String("json", "heartbeat_result.json", "探测结果JSON输出文件（置空则不输出）")
	fs.Parse(args)

	if *count < 1 {
		fmt.Fprintf(os.Stderr, "[FAIL] -count 必须大于0\n")
		return false
	}

	ok := true
	rounds := make([]heartbeatRound, 0, *count)
	for i := 1; i <= *count; i++ {
		r := probeHeartbeat(i)
		rounds = append(rounds, r)
		if !r.Alive {
			ok = false
		}
		if i < *count && *interval > 0 {
			time.Sleep(time.Duration(*interval) * time.Second)
		}
	}

	if *jsonPath != "" {
		data, err := json.MarshalIndent(map[string]interface{}{
			"total":  len(rounds),
			"passed": countAlive(rounds),
			"rounds": rounds,
		}, "", "  ")
		if err != nil {
			fmt.Fprintf(os.Stderr, "[FAIL] JSON序列化失败: %v\n", err)
			ok = false
		} else if err := os.WriteFile(*jsonPath, data, 0644); err != nil {
			fmt.Fprintf(os.Stderr, "[FAIL] 写入JSON文件 %s 失败: %v\n", *jsonPath, err)
			ok = false
		} else {
			fmt.Printf("[PASS] jsonFile=%s\n", *jsonPath)
		}
	}

	if ok {
		fmt.Println("[PASS] 心跳探测全部通过")
	}
	return ok
}

// countAlive 统计存活的轮数
func countAlive(rounds []heartbeatRound) int {
	n := 0
	for _, r := range rounds {
		if r.Alive {
			n++
		}
	}
	return n
}

// probeHeartbeat 执行一轮心跳探测，返回本轮结果
func probeHeartbeat(round int) heartbeatRound {
	start := time.Now()
	err := memfragmentation.UbsMemFragmentationHeartbeat()
	elapsed := time.Since(start)
	if err != nil {
		fmt.Fprintf(os.Stderr, "[FAIL] round=%d alive=false elapsedMs=%.3f error=%v\n",
			round, float64(elapsed.Microseconds())/1000.0, err)
		return heartbeatRound{
			Round:     round,
			Alive:     false,
			ElapsedMs: float64(elapsed.Microseconds()) / 1000.0,
			Error:     err.Error(),
		}
	}
	fmt.Printf("round=%d alive=true elapsedMs=%.3f\n", round, float64(elapsed.Microseconds())/1000.0)
	return heartbeatRound{
		Round:     round,
		Alive:     true,
		ElapsedMs: float64(elapsed.Microseconds()) / 1000.0,
	}
}

// runTask UbsSyncTaskQuery 冒烟：查询异步任务状态，无taskId时验证任务不存在路径
func runTask(args []string) bool {
	if len(args) > 1 {
		fmt.Fprintf(os.Stderr, "[FAIL] 用法：memfrag_verify task [taskId]\n")
		return false
	}
	taskId := probeTaskId
	userProvided := len(args) == 1 && args[0] != ""
	if userProvided {
		taskId = args[0]
	}

	start := time.Now()
	info, err := memfragmentation.UbsSyncTaskQuery(taskId)
	switch {
	case err == nil:
		fmt.Printf("[PASS] interface=UbsSyncTaskQuery elapsedMs=%.3f\n",
			float64(time.Since(start).Microseconds())/1000.0)
		var statusParts []string
		if info.TaskId != "" {
			statusParts = append(statusParts, "taskId="+info.TaskId)
		}
		statusParts = append(statusParts, "status="+statusName(info.Status), fmt.Sprintf("resultCode=%d", info.ResultCode))
		fmt.Printf("  %s\n", strings.Join(statusParts, " "))
		if text := borrowResultText(info.MemBorrowResult); text != "" {
			fmt.Printf("  %s\n", text)
		}
		return true
	case isSymbolErr(err):
		fmt.Fprintf(os.Stderr, "[FAIL] UbsSyncTaskQuery: %v\n", err)
		return false
	case userProvided:
		fmt.Fprintf(os.Stderr, "[FAIL] UbsSyncTaskQuery taskId=%s: %v\n", taskId, err)
		return false
	default:
		// 探针ID不存在属预期业务结果，说明调用链已打通
		fmt.Printf("[PASS] interface=UbsSyncTaskQuery expectedBusinessErr=%v\n", err)
		return true
	}
}

// runBorrowExec UbsMemBorrowExecute 冒烟
func runBorrowExec(args []string) bool {
	fs := flag.NewFlagSet("borrow-exec", flag.ExitOnError)
	srcHost := fs.String("src-host", "", "源节点ID（必填）")
	srcSocket := fs.Int("src-socket", 0, "源Socket ID")
	srcNuma := fs.Int("src-numa", 0, "源NUMA ID")
	size := fs.Uint64("size", 0, "借用大小kB（必填）")
	dstHost := fs.String("dst-host", "", "目标节点ID（必填）")
	dstSocket := fs.Uint("dst-socket", 0, "目标Socket ID")
	dstNuma := fs.Int("dst-numa", 0, "目标NUMA ID")
	dstSize := fs.Uint64("dst-size", 0, "目标单NUMA借用大小kB（默认取-size）")
	async := fs.Bool("async", false, "异步执行")
	pageSize := fs.Uint64("page-size", 2048, "大页规格kB（仅支持2048或1048576）")
	fs.Parse(args)

	if *srcHost == "" || *dstHost == "" || *size == 0 {
		fmt.Fprintf(os.Stderr, "[FAIL] -src-host/-dst-host/-size 为必填参数\n")
		fs.Usage()
		return false
	}
	if *pageSize != 2048 && *pageSize != 1048576 {
		fmt.Fprintf(os.Stderr, "[FAIL] -page-size 仅支持2048或1048576\n")
		return false
	}
	if *dstSize == 0 {
		*dstSize = *size
	}

	setting := memfragmentation.BorrowSetting{
		BorrowStrategy: memfragmentation.BorrowStrategy{
			SrcHostId:   *srcHost,
			SrcSocketId: int16(*srcSocket),
			SrcNumaId:   int16(*srcNuma),
			BorrowSize:  *size,
			DestNumaInfos: []memfragmentation.DstNumaInfo{
				{
					HostId:   *dstHost,
					SocketId: uint16(*dstSocket),
					NumaIds:  []int32{int32(*dstNuma)},
					MemSizes: []uint64{*dstSize},
				},
			},
		},
		IsAsync:  *async,
		PageSize: *pageSize,
	}

	start := time.Now()
	result, err := memfragmentation.UbsMemBorrowExecute(setting)
	if err != nil {
		fmt.Fprintf(os.Stderr, "[FAIL] UbsMemBorrowExecute: %v\n", err)
		return false
	}
	fmt.Printf("[PASS] interface=UbsMemBorrowExecute elapsedMs=%.3f\n",
		float64(time.Since(start).Microseconds())/1000.0)
	if text := borrowResultText(result); text != "" {
		fmt.Printf("  %s\n", text)
	}
	if *async && result.TaskId != "" {
		fmt.Printf("  查询任务：memfrag_verify task %s\n", result.TaskId)
	}
	return true
}

// runBorrow UbsMemBorrow 冒烟
func runBorrow(args []string) bool {
	fs := flag.NewFlagSet("borrow", flag.ExitOnError)
	nid := fs.String("nid", "", "源节点ID（必填）")
	size := fs.Uint64("size", 0, "借用大小MB（必填）")
	numasStr := fs.String("numas", "", "借用方NUMA ID列表，逗号分隔（必填，可由 query_verify 结果 numaIds 填写）")
	async := fs.Bool("async", false, "异步执行")
	pageSize := fs.Uint64("page-size", 2048, "大页规格kB（仅支持2048或1048576）")
	fs.Parse(args)

	if *nid == "" || *size == 0 || *numasStr == "" {
		fmt.Fprintf(os.Stderr, "[FAIL] -nid/-size/-numas 为必填参数\n")
		fs.Usage()
		return false
	}
	if *pageSize != 2048 && *pageSize != 1048576 {
		fmt.Fprintf(os.Stderr, "[FAIL] -page-size 仅支持2048或1048576\n")
		return false
	}

	param := memfragmentation.MemBorrowParam{
		SrcNid:     *nid,
		BorrowSize: *size,
		PageSize:   *pageSize,
	}
	if *numasStr != "" {
		for _, s := range strings.Split(*numasStr, ",") {
			id, err := strconv.Atoi(strings.TrimSpace(s))
			if err != nil {
				fmt.Fprintf(os.Stderr, "[FAIL] 非法NUMA ID: %q\n", s)
				return false
			}
			param.NumaMetaInfos = append(param.NumaMetaInfos, memfragmentation.NumaMetaInfo{NumaId: int16(id)})
		}
	}

	start := time.Now()
	results, err := memfragmentation.UbsMemBorrow(param, *async)
	if err != nil {
		fmt.Fprintf(os.Stderr, "[FAIL] UbsMemBorrow: %v\n", err)
		return false
	}
	fmt.Printf("[PASS] interface=UbsMemBorrow resultCount=%d elapsedMs=%.3f\n",
		len(results), float64(time.Since(start).Microseconds())/1000.0)
	for i, r := range results {
		if text := borrowResultText(r); text != "" {
			fmt.Printf("  index=%d %s\n", i, text)
		}
		if *async && r.TaskId != "" {
			fmt.Printf("      查询任务：memfrag_verify task %s\n", r.TaskId)
		}
	}
	return true
}

// runReturn UbsMemReturn 冒烟（归还本节点全部借用内存）
func runReturn(args []string) bool {
	fs := flag.NewFlagSet("return", flag.ExitOnError)
	async := fs.Bool("async", false, "异步执行")
	fs.Parse(args)

	start := time.Now()
	returnedTaskId, err := memfragmentation.UbsMemReturn(*async)
	if err != nil {
		fmt.Fprintf(os.Stderr, "[FAIL] UbsMemReturn: %v\n", err)
		return false
	}
	fmt.Printf("[PASS] interface=UbsMemReturn elapsedMs=%.3f taskId=%s\n",
		float64(time.Since(start).Microseconds())/1000.0, returnedTaskId)
	if *async && returnedTaskId != "" {
		fmt.Printf("  查询任务：memfrag_verify task %s\n", returnedTaskId)
	}
	return true
}

// runRollback UbsMemRollback 冒烟
func runRollback(args []string) bool {
	fs := flag.NewFlagSet("rollback", flag.ExitOnError)
	node := fs.String("node", "", "节点ID（必填）")
	ids := fs.String("ids", "", "借用ID列表，逗号分隔（必填）")
	fs.Parse(args)

	if *node == "" || *ids == "" {
		fmt.Fprintf(os.Stderr, "[FAIL] -node/-ids 为必填参数\n")
		fs.Usage()
		return false
	}
	var borrowIds []string
	for _, s := range strings.Split(*ids, ",") {
		if s = strings.TrimSpace(s); s != "" {
			borrowIds = append(borrowIds, s)
		}
	}

	start := time.Now()
	if err := memfragmentation.UbsMemRollback(memfragmentation.RollbackParam{
		NodeId:       *node,
		BorrowIdList: borrowIds,
	}); err != nil {
		fmt.Fprintf(os.Stderr, "[FAIL] UbsMemRollback: %v\n", err)
		return false
	}
	fmt.Printf("[PASS] interface=UbsMemRollback elapsedMs=%.3f borrowIdCount=%d\n",
		float64(time.Since(start).Microseconds())/1000.0, len(borrowIds))
	return true
}
