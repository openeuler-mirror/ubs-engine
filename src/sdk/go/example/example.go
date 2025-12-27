// Package main
package main

import (
	"fmt"
	
	"ubs_engine_go_sdk/engine"
	"ubs_engine_go_sdk/urma"
)

func main() {
	_, connectError := engine.UbsEngineClientInitialize("")
	if connectError != nil {
		fmt.Printf("connectError: %s\n", connectError.Error())
	}
	defer engine.UbsEngineClientFinalize()
	devices1, err := urma.UbsGetVfeDevice()
	if err != nil {
		fmt.Printf("UbsGetVfeDevice Error: %s\n", err.Error())
	}
	fmt.Printf("devices1: %v\n", devices1)

	devices2, err := urma.UbsGetSharedDevice()
	if err != nil {
		fmt.Printf("Error getting shared device: %s", err)
	}
	fmt.Printf("devices2: %v\n", devices2)

	if len(devices1) == 0 || len(devices2) == 0 {
		fmt.Println("No devices found")
	}
	device1Paths, err := urma.UbsAllocateDevice(devices1[0].Name)
	if err != nil {
		fmt.Printf("Error allocating device1: %s, err: %s", devices1[0].Name, err)
	}
	fmt.Printf("devicePaths: %v\n", device1Paths)

	device2Paths, err := urma.UbsAllocateDevice(devices2[0].Name)
	if err != nil {
		fmt.Printf("Error allocating device2: %s, err: %s", devices2[0].Name, err)
	}
	fmt.Printf("device2Paths: %v\n", device2Paths)
}
