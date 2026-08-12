package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"io"
	"os"
	"strconv"
	"strings"
	"unicode"

	"atomgit.com/openeuler/ubs-engine.git/src/sdk/go/ssu"
)

// sdkFuncs 集中保存可注入的 SDK 函数，生产运行和离线测试使用不同绑定。
type sdkFuncs struct {
	listAllocInfo          func() ([]ssu.UbsSsuAllocResult, error)
	getNsStats             func(string) ([]ssu.UbsSsuNsStats, error)
	getConnectInfo         func(string, *ssu.UbsUbVfe) ([]ssu.UbsSsuConnectInfo, error)
	getFeDeviceList        func() ([]ssu.UbsSsuFe, error)
	allocSpace             func(ssu.UbsSsuAllocSpaceReq) (ssu.UbsSsuAllocResult, error)
	freeSpace              func(string) error
	addAccessPermission    func(string, string) error
	removeAccessPermission func(string, string) error
	attachSpace            func(ssu.UbsSsuSpaceReq) ([]string, error)
	detachSpace            func(ssu.UbsSsuSpaceReq) error
	attachLinearSpace      func(ssu.UbsSsuLinearSpaceReq) ([]string, string, error)
	detachLinearSpace      func(ssu.UbsSsuLinearSpaceReq) error
	attachStripedSpace     func(ssu.UbsSsuStripedSpaceReq) ([]string, string, error)
	detachStripedSpace     func(ssu.UbsSsuStripedSpaceReq) error
	feDeviceAlloc          func(uint32, *ssu.UbsUbVfe, string) (string, error)
	feDeviceFree           func(uint32, *ssu.UbsUbVfe) error
}

// command 描述一条 CLI 命令的用法、固定参数数量和执行入口。
type command struct {
	usage string
	argc  int
	run   func([]string, io.Writer) error
}

// commandUsages 是帮助输出和命令注册共用的完整命令清单。
var commandUsages = []string{
	"ssu_list_alloc_info",
	"ssu_get_ns_stats <name>",
	"ssu_get_connect_info <name> <vfe_present> <slot_id> <chip_id> <die_id> <pfe_id> <vfe_id> <vfe_guid> <bind_bus_instance_guid>",
	"ssu_get_fe_device_list",
	"ssu_alloc_space <name> <ns_size> <ns_num> <lba_format> <strategy> <tenant>",
	"ssu_free_space <name>",
	"ssu_add_access_permission <name> <nqn>",
	"ssu_remove_access_permission <name> <nqn>",
	"ssu_attach_space <name> <nqn> <src_eid>",
	"ssu_detach_space <name> <nqn> <src_eid>",
	"ssu_attach_linear_space <name> <nqn> <src_eid> <dev_name>",
	"ssu_detach_linear_space <name> <nqn> <src_eid> <dev_name>",
	"ssu_attach_striped_space <name> <nqn> <src_eid> <dev_name> <level> <chunk_size>",
	"ssu_detach_striped_space <name> <nqn> <src_eid> <dev_name> <level> <chunk_size>",
	"ssu_fe_device_alloc <upi> <slot_id> <chip_id> <die_id> <pfe_id> <vfe_id> <vfe_guid> <bind_bus_instance_guid> <bus_instance_guid>",
	"ssu_fe_device_free <upi> <slot_id> <chip_id> <die_id> <pfe_id> <vfe_id> <vfe_guid> <bind_bus_instance_guid>",
}

// runOnce 校验并执行一条完整命令，供单次模式和交互模式复用。
func runOnce(args []string, output io.Writer, deps sdkFuncs) error {
	if len(args) == 0 {
		return fmt.Errorf("missing command")
	}
	if args[0] == "help" || args[0] == "?" {
		if len(args) == 1 {
			for _, usage := range commandUsages {
				fmt.Fprintln(output, usage)
			}
			return nil
		}
		if len(args) != 2 {
			return fmt.Errorf("help accepts exactly one command name")
		}
		for _, usage := range commandUsages {
			if strings.Fields(usage)[0] == args[1] {
				fmt.Fprintln(output, usage)
				return nil
			}
		}
		return fmt.Errorf("unknown command: %s", args[1])
	}
	cmd, ok := commands(deps)[args[0]]
	if !ok {
		return fmt.Errorf("unknown command: %s", args[0])
	}
	if len(args)-1 != cmd.argc {
		return fmt.Errorf("invalid argument count, usage: %s", cmd.usage)
	}
	return cmd.run(args[1:], output)
}

// commands 将 16 条业务命令绑定到对应的 SDK 函数。
func commands(deps sdkFuncs) map[string]command {
	result := make(map[string]command, len(commandUsages))
	add := func(usage string, argc int, run func([]string, io.Writer) error) {
		result[strings.Fields(usage)[0]] = command{usage, argc, run}
	}
	add(commandUsages[0], 0, func(_ []string, out io.Writer) error {
		value, err := deps.listAllocInfo()
		return printOne(out, value, err)
	})
	add(commandUsages[1], 1, func(a []string, out io.Writer) error {
		value, err := deps.getNsStats(a[0])
		return printOne(out, value, err)
	})
	add(commandUsages[2], 9, func(a []string, out io.Writer) error {
		present, err := parseUint(a[1], 8)
		if err != nil || present > 1 {
			return fmt.Errorf("vfe_present must be 0 or 1")
		}
		vfe, err := parseVfe(a, 2)
		if err != nil {
			return err
		}
		if present == 0 {
			vfe = nil
		}
		value, callErr := deps.getConnectInfo(a[0], vfe)
		return printOne(out, value, callErr)
	})
	add(commandUsages[3], 0, func(_ []string, out io.Writer) error {
		value, err := deps.getFeDeviceList()
		return printOne(out, value, err)
	})
	add(commandUsages[4], 6, func(a []string, out io.Writer) error {
		nsSize, err := parseUint(a[1], 64)
		if err != nil {
			return err
		}
		nsNum, err := parseUint(a[2], 32)
		if err != nil {
			return err
		}
		lba, err := parseUint(a[3], 32)
		if err != nil {
			return err
		}
		strategy, err := parseUint(a[4], 8)
		if err != nil {
			return err
		}
		req := ssu.UbsSsuAllocSpaceReq{
			Name: a[0], NsSize: nsSize, NsNum: uint32(nsNum),
			LbaFormat: ssu.UbsSsuLbaFormat(lba), Strategy: ssu.UbsSsuAllocStrategy(strategy), Tenant: a[5],
		}
		value, callErr := deps.allocSpace(req)
		return printOne(out, value, callErr)
	})
	add(commandUsages[5], 1, func(a []string, out io.Writer) error {
		return printSuccess(out, deps.freeSpace(a[0]))
	})
	add(commandUsages[6], 2, func(a []string, out io.Writer) error {
		return printSuccess(out, deps.addAccessPermission(a[0], a[1]))
	})
	add(commandUsages[7], 2, func(a []string, out io.Writer) error {
		return printSuccess(out, deps.removeAccessPermission(a[0], a[1]))
	})
	add(commandUsages[8], 3, func(a []string, out io.Writer) error {
		value, err := deps.attachSpace(spaceReq(a))
		return printOne(out, value, err)
	})
	add(commandUsages[9], 3, func(a []string, out io.Writer) error {
		return printSuccess(out, deps.detachSpace(spaceReq(a)))
	})
	add(commandUsages[10], 4, func(a []string, out io.Writer) error {
		first, second, err := deps.attachLinearSpace(linearReq(a))
		return printTwo(out, first, second, err)
	})
	add(commandUsages[11], 4, func(a []string, out io.Writer) error {
		return printSuccess(out, deps.detachLinearSpace(linearReq(a)))
	})
	add(commandUsages[12], 6, func(a []string, out io.Writer) error {
		req, err := stripedReq(a)
		if err != nil {
			return err
		}
		first, second, callErr := deps.attachStripedSpace(req)
		return printTwo(out, first, second, callErr)
	})
	add(commandUsages[13], 6, func(a []string, out io.Writer) error {
		req, err := stripedReq(a)
		if err != nil {
			return err
		}
		return printSuccess(out, deps.detachStripedSpace(req))
	})
	add(commandUsages[14], 9, func(a []string, out io.Writer) error {
		upi, err := parseUint(a[0], 32)
		if err != nil {
			return err
		}
		vfe, err := parseVfe(a, 1)
		if err != nil {
			return err
		}
		value, callErr := deps.feDeviceAlloc(uint32(upi), vfe, a[8])
		return printOne(out, value, callErr)
	})
	add(commandUsages[15], 8, func(a []string, out io.Writer) error {
		upi, err := parseUint(a[0], 32)
		if err != nil {
			return err
		}
		vfe, err := parseVfe(a, 1)
		if err != nil {
			return err
		}
		return printSuccess(out, deps.feDeviceFree(uint32(upi), vfe))
	})
	return result
}

// parseUint 按 SDK 目标位宽解析十进制无符号整数。
func parseUint(value string, bits int) (uint64, error) {
	parsed, err := strconv.ParseUint(value, 10, bits)
	if err != nil {
		return 0, fmt.Errorf("%q is not a valid base-10 uint%d: %w", value, bits, err)
	}
	return parsed, nil
}

// parseVfe 从固定位置参数构造 VFE，不执行额外业务校验。
func parseVfe(args []string, start int) (*ssu.UbsUbVfe, error) {
	slot, err := parseUint(args[start], 8)
	if err != nil {
		return nil, err
	}
	chip, err := parseUint(args[start+1], 8)
	if err != nil {
		return nil, err
	}
	die, err := parseUint(args[start+2], 8)
	if err != nil {
		return nil, err
	}
	pfe, err := parseUint(args[start+3], 16)
	if err != nil {
		return nil, err
	}
	vfe, err := parseUint(args[start+4], 16)
	if err != nil {
		return nil, err
	}
	return &ssu.UbsUbVfe{
		SlotId: uint8(slot), ChipId: uint8(chip), DieId: uint8(die),
		PfeId: uint16(pfe), VfeId: uint16(vfe),
		VfeGuid: args[start+5], BindBusInstanceGuid: args[start+6],
	}, nil
}

func spaceReq(args []string) ssu.UbsSsuSpaceReq {
	return ssu.UbsSsuSpaceReq{Name: args[0], Nqn: args[1], SrcEid: args[2]}
}

func linearReq(args []string) ssu.UbsSsuLinearSpaceReq {
	return ssu.UbsSsuLinearSpaceReq{Name: args[0], Nqn: args[1], SrcEid: args[2], DevName: args[3]}
}

// stripedReq 解析条带参数并构造 SDK 请求。
func stripedReq(args []string) (ssu.UbsSsuStripedSpaceReq, error) {
	level, err := parseUint(args[4], 8)
	if err != nil {
		return ssu.UbsSsuStripedSpaceReq{}, err
	}
	chunk, err := parseUint(args[5], 32)
	if err != nil {
		return ssu.UbsSsuStripedSpaceReq{}, err
	}
	return ssu.UbsSsuStripedSpaceReq{
		Name: args[0], Nqn: args[1], SrcEid: args[2], DevName: args[3],
		Level: ssu.UbsSsuAggregationRaidLevel(level), ChunkSize: ssu.UbsSsuChunkSize(chunk),
	}, nil
}

// printOne 以 JSON 对象输出 SDK 的单个返回值，并保留原始错误。
// ErrAlreadyAttached 时 SDK 依然返回已挂载的设备路径, 输出数据并附带 warning。
func printOne(out io.Writer, value any, err error) error {
	if err != nil {
		if errors.Is(err, errcode.ErrAlreadyAttached) {
			_ = printResponse(out, value)
			return printWarning(out, err)
		}
		return err
	}
	return printResponse(out, value)
}

// printTwo 以 JSON 数组按 SDK 返回顺序输出两个返回值。
func printTwo(out io.Writer, first any, second string, err error) error {
	if err != nil {
		if errors.Is(err, errcode.ErrAlreadyAttached) {
			_ = printResponse(out, []any{first, second})
			return printWarning(out, err)
		}
		return err
	}
	return printResponse(out, []any{first, second})
}

// printResponse 输出统一的 JSON 应答对象。
func printResponse(out io.Writer, value any) error {
	return json.NewEncoder(out).Encode(struct {
		Response any `json:"response"`
	}{Response: value})
}

// printError 输出统一的 JSON 错误应答对象。
func printError(out io.Writer, err error) error {
	return json.NewEncoder(out).Encode(struct {
		Error string `json:"error"`
	}{Error: err.Error()})
}

// printSuccess 为只有 error 返回值的 SDK 函数输出成功应答。
func printSuccess(out io.Writer, err error) error {
	if err != nil {
		return err
	}
	return printResponse(out, "success")
}

// printWarning 输出统一的 JSON 告警应答对象。
// 用于 ErrAlreadyAttached 等场景: 数据已通过 printResponse 输出, 错误以 warning 形式附带。
func printWarning(out io.Writer, err error) error {
	return json.NewEncoder(out).Encode(struct {
		Warning string `json:"warning"`
	}{Warning: err.Error()})
}

// splitLine 按 shell 引用与反斜线规则拆分交互输入，但不执行任何展开。
func splitLine(line string) ([]string, error) {
	var fields []string
	var current strings.Builder
	var quote rune
	escaped, active := false, false
	flush := func() {
		if active {
			fields = append(fields, current.String())
			current.Reset()
			active = false
		}
	}
	for _, char := range line {
		if escaped {
			escaped = false
			if quote == '"' && !strings.ContainsRune("\"\\$`", char) {
				current.WriteRune('\\')
			}
			current.WriteRune(char)
			active = true
		} else if quote != 0 {
			if char == quote {
				quote = 0
			} else if quote == '"' && char == '\\' {
				escaped = true
			} else {
				current.WriteRune(char)
			}
		} else if char == '\\' {
			escaped = true
		} else if char == '\'' || char == '"' {
			quote, active = char, true
		} else if unicode.IsSpace(char) {
			flush()
		} else {
			current.WriteRune(char)
			active = true
		}
	}
	if escaped || quote != 0 {
		return nil, fmt.Errorf("unterminated quote or backslash")
	}
	flush()
	return fields, nil
}

// runInteractive 循环读取命令，并分离正常输出与错误输出。
func runInteractive(input io.Reader, output, errorOutput io.Writer, deps sdkFuncs) error {
	scanner := bufio.NewScanner(input)
	for {
		fmt.Fprint(output, "ubse_ssu_test_go> ")
		if !scanner.Scan() {
			return scanner.Err()
		}
		args, err := splitLine(scanner.Text())
		if err != nil {
			_ = printError(errorOutput, err)
			continue
		}
		if len(args) == 0 {
			continue
		}
		if len(args) == 1 && (args[0] == "quit" || args[0] == "exit") {
			return nil
		}
		if err := runOnce(args, output, deps); err != nil {
			_ = printError(errorOutput, err)
		}
	}
}

// defaultSDK 建立生产运行时使用的 16 个 SDK 函数绑定。
func defaultSDK() sdkFuncs {
	return sdkFuncs{
		listAllocInfo: ssu.UbsSsuListAllocInfo, getNsStats: ssu.UbsSsuGetNsStats,
		getConnectInfo: ssu.UbsSsuGetConnectInfo, getFeDeviceList: ssu.UbsSsuGetFeDeviceList,
		allocSpace: ssu.UbsSsuAllocSpace, freeSpace: ssu.UbsSsuFreeSpace,
		addAccessPermission: ssu.UbsSsuAddAccessPermission, removeAccessPermission: ssu.UbsSsuRemoveAccessPermission,
		attachSpace: ssu.UbsSsuAttachSpace, detachSpace: ssu.UbsSsuDetachSpace,
		attachLinearSpace: ssu.UbsSsuAttachLinearSpace, detachLinearSpace: ssu.UbsSsuDetachLinearSpace,
		attachStripedSpace: ssu.UbsSsuAttachStripedSpace, detachStripedSpace: ssu.UbsSsuDetachStripedSpace,
		feDeviceAlloc: ssu.UbsSsuFeDeviceAlloc, feDeviceFree: ssu.UbsSsuFeDeviceFree,
	}
}

func main() {
	deps := defaultSDK()
	if len(os.Args) == 1 {
		if err := runInteractive(os.Stdin, os.Stdout, os.Stderr, deps); err != nil {
			_ = printError(os.Stderr, err)
			os.Exit(1)
		}
		return
	}
	if err := runOnce(os.Args[1:], os.Stdout, deps); err != nil {
		_ = printError(os.Stderr, err)
		os.Exit(1)
	}
}
