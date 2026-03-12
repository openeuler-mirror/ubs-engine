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

// package log 实现了log的go sdk.
package log

/*
#include <stdlib.h>
#include <stdint.h>

// 定义函数指针类型
typedef void (*ubs_engine_log_handler)(uint32_t level, char* message);
typedef void (*ubs_engine_log_callback_register_func)(ubs_engine_log_handler);

// C桥接函数
static void call_ubs_engine_log_callback_register(
    ubs_engine_log_callback_register_func fn,
    ubs_engine_log_handler handler) {
    if (fn != NULL) {
        fn(handler);
    }
}

// 声明Go回调函数
extern void goLogCallback(uint32_t level, char* message);
*/
import "C"
import (
	"ubs_engine_go_sdk/dlopen"
)

// Level 定义日志级别类型
type Level uint32

// 日志级别常量定义
const (
	Debug Level = 0
	Info  Level = 1
	Warn  Level = 2
	Error Level = 3
	Crit  Level = 4
)

// String 返回日志级别的字符串表示
func (l Level) String() string {
	switch l {
	case Debug:
		return "DEBUG"
	case Info:
		return "INFO"
	case Warn:
		return "WARN"
	case Error:
		return "ERROR"
	case Crit:
		return "CRIT"
	default:
		return "UNKNOWN"
	}
}

// Handler 日志处理函数类型
type Handler func(level Level, message string)

var logHandler Handler

//export goLogCallback
func goLogCallback(cLevel C.uint32_t, cMessage *C.char) {
	level := Level(cLevel)
	var message string
	if cMessage != nil {
		message = C.GoString(cMessage)
	} else {
		message = "<nil log message>"
	}

	if logHandler != nil {
		logHandler(level, message)
	}
}

// UbsEngineLogCallbackRegister 注册日志回调函数
func UbsEngineLogCallbackRegister(handler Handler) error {
	logHandler = handler

	// 加载函数符号
	registerPtr, err := dlopen.LoadFunc("ubs_engine_log_callback_register")
	if err != nil {
		return err
	}

	// 转换为函数指针类型
	registerFunc := (C.ubs_engine_log_callback_register_func)(registerPtr)

	// 调用函数注册回调
	C.call_ubs_engine_log_callback_register(registerFunc, (C.ubs_engine_log_handler)(C.goLogCallback))

	return nil
}
