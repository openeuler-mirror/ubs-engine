package main

import (
	"fmt"

	urmav1 "atomgit.com/openeuler/ubs-engine.git/src/sdk/go/urmav1"
)

func main() {
	fmt.Println("Testing URMA functions...")

	// Test UbsGetVfeDevice
	fmt.Println("\n=== Testing UbsGetVfeDevice ===")
	devices, err := urmav1.UbsGetVfeDevice()
	if err != nil {
		fmt.Printf("UbsGetVfeDevice failed: %v\n", err)
	} else {
		fmt.Printf("Found %d VFE devices\n", len(devices))
		for _, dev := range devices {
			fmt.Printf("Device: %s, Healthy: %v, HwResId: %d\n", dev.Name, dev.Healthy, dev.HwResId)
		}
	}

	// Test UbsGetSharedDevice
	fmt.Println("\n=== Testing UbsGetSharedDevice ===")
	sharedDevices, err := urmav1.UbsGetSharedDevice()
	if err != nil {
		fmt.Printf("UbsGetSharedDevice failed: %v\n", err)
	} else {
		fmt.Printf("Found %d shared devices\n", len(sharedDevices))
		for _, dev := range sharedDevices {
			fmt.Printf("Device: %s, Healthy: %v, HwResId: %d\n", dev.Name, dev.Healthy, dev.HwResId)
		}
	}

	// Test UbsAllocateDevice
	fmt.Println("\n=== Testing UbsAllocateDevice ===")
	info, err := urmav1.UbsAllocateDevice("urma_96")
	if err != nil {
		fmt.Printf("UbsAllocateDevice failed: %v\n", err)
	} else {
		fmt.Printf("Allocated device:\n")
		fmt.Printf("BondingPath: %s\n", info.BondingPath)
		fmt.Printf("BondingEid: %s\n", info.BondingEid)
		fmt.Printf("VfePaths: %v\n", info.VfePaths)
	}

	// // Test UbsSetBandwidth
	// fmt.Println("\n=== Testing UbsSetBandwidth ===")
	// err = urmav1.UbsSetBandwidth("urma_96", 100, 1000)
	// if err != nil {
	// 	fmt.Printf("UbsSetBandwidth failed: %v\n", err)
	// } else {
	// 	fmt.Println("UbsSetBandwidth succeeded")
	// }

	// // Test UbsGetBandwidth
	// fmt.Println("\n=== Testing UbsGetBandwidth ===")
	// minBandwidth, maxBandwidth, err := urmav1.UbsGetBandwidth("urma_96")
	// if err != nil {
	// 	fmt.Printf("UbsGetBandwidth failed: %v\n", err)
	// } else {
	// 	fmt.Printf("Bandwidth: min=%d, max=%d\n", minBandwidth, maxBandwidth)
	// }

	// // Test UbsResetBandwidth
	// fmt.Println("\n=== Testing UbsResetBandwidth ===")
	// err = urmav1.UbsResetBandwidth("urma_96")
	// if err != nil {
	// 	fmt.Printf("UbsResetBandwidth failed: %v\n", err)
	// } else {
	// 	fmt.Println("UbsResetBandwidth succeeded")
	// }

	// Test UbsFreeDevice
	fmt.Println("\n=== Testing UbsFreeDevice ===")
	err = urmav1.UbsFreeDevice("urma_96")
	if err != nil {
		fmt.Printf("UbsFreeDevice failed: %v\n", err)
	} else {
		fmt.Println("UbsFreeDevice succeeded")
	}
}
