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

// package dlopen Loading dynamic library packages at runtime
package dlopen

/*
#include <dlfcn.h>
#include <stdlib.h>
*/
import "C"
import (
	"fmt"
	"unsafe"
)

var soPath = "/usr/lib64/libubs-virt-agent.so.1"
var libHandle unsafe.Pointer

// GetLibHandle Obtain dynamic library handle
func GetLibHandle() error {
	cpath := C.CString(soPath)
	defer C.free(unsafe.Pointer(cpath))

	libHandle = C.dlopen(cpath, C.RTLD_LAZY)
	if libHandle == nil {
		errStr := C.dlerror()
		return fmt.Errorf("dlopen failed: %v", C.GoString(errStr))
	}
	return nil
}

// LoadFunc Loading functions from a dynamic library
func LoadFunc(funcName string) (unsafe.Pointer, error) {
	if libHandle == nil {
		return nil, fmt.Errorf("dlopen handle is null")
	}

	symbol := C.CString(funcName)
	defer C.free(unsafe.Pointer(symbol))

	fn := C.dlsym(libHandle, symbol)

	if fn == nil {
		return nil, fmt.Errorf("failed to load func %s", funcName)
	}
	return fn, nil
}

// CloseLibHandle Releasing a Function Handle
func CloseLibHandle() {
	cLibPath := C.CString(soPath)
	defer C.free(unsafe.Pointer(cLibPath))
	C.dlclose(libHandle)
}
