// Package engine
package engine

/*
#cgo CFLAGS: -I/usr/include/ubse
#include <stdlib.h>
#include "ubs_engine.h"

typedef int (*ubs_engine_client_initialize_ptr)(const char*);
typedef void (*ubs_engine_client_finalize_ptr)();

int call_ubs_engine_client_initialize(ubs_engine_client_initialize_ptr fn, const char* path) {
	if (fn == NULL) return -1;
	return fn(path);
}

void call_ubs_engine_client_finalize(ubs_engine_client_finalize_ptr fn) {
	if (fn != NULL) {
		fn();
	}
}
*/
import "C"
import (
	"fmt"
	"unsafe"

	"ubs_engine_go_sdk/dlopen"
)

func UbsEngineClientInitialize(ubsEngineUdsPath string) (int32, error) {
	dlopenRet := dlopen.GetLibHandle()
	if dlopenRet != nil {
		return -1, dlopenRet
	}

	initializePtr, err := dlopen.LoadFunc("ubs_engine_client_initialize")
	if err != nil {
		dlopen.CloseLibHandle()
		return -1, err
	}

	initializeFunc := (C.ubs_engine_client_initialize_ptr)(initializePtr)

	var cPath *C.char
	cPath = C.CString(ubsEngineUdsPath)
	defer C.free(unsafe.Pointer(cPath))
	result := C.call_ubs_engine_client_initialize(initializeFunc, cPath)
	if result != 0 {
		dlopen.CloseLibHandle()
		return int32(result), fmt.Errorf("ubs_engine_client_initialize failed with error code %d", result)
	}

	return int32(result), nil
}

func UbsEngineClientFinalize() error {
	finalizePtr, err := dlopen.LoadFunc("ubs_engine_client_finalize")
	if err != nil {
		dlopen.CloseLibHandle()
		return err
	}

	finalizeFunc := (C.ubs_engine_client_finalize_ptr)(finalizePtr)

	C.call_ubs_engine_client_finalize(finalizeFunc)

	dlopen.CloseLibHandle()
	return nil
}
