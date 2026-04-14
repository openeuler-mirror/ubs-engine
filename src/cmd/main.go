package main

import (
	"fmt"
	"time"

	urmav1 "atomgit.com/openeuler/ubs-engine.git/src/sdk/go/urmav1"
)

func main() {

	// Start a timer to call UbsGetVfeDevice and UbsGetSharedDevice every 10 seconds
	go func() {
		ticker := time.NewTicker(10 * time.Second)
		defer ticker.Stop()

		for range ticker.C {
			fmt.Println("\n=== Periodic Test UbsGetVfeDevice ===")
			start := time.Now()
			devices, err := urmav1.UbsGetVfeDevice()
			duration := time.Since(start)
			if err != nil {
				fmt.Printf("UbsGetVfeDevice failed: %v\n", err)
			} else {
				fmt.Printf("Found %d VFE devices\n", len(devices))
				// for _, dev := range devices {
				// 	fmt.Printf("Device: %s, Healthy: %v, HwResId: %d\n", dev.Name, dev.Healthy, dev.HwResId)
				// }
			}
			fmt.Printf("UbsGetVfeDevice took %v\n", duration)

			fmt.Println("\n=== Periodic Test UbsGetSharedDevice ===")
			start = time.Now()
			sharedDevices, err := urmav1.UbsGetSharedDevice()
			duration = time.Since(start)
			if err != nil {
				fmt.Printf("UbsGetSharedDevice failed: %v\n", err)
			} else {
				fmt.Printf("Found %d shared devices\n", len(sharedDevices))
				// for _, dev := range sharedDevices {
				// 	fmt.Printf("Device: %s, Healthy: %v, HwResId: %d\n", dev.Name, dev.Healthy, dev.HwResId)
				// }
			}
			fmt.Printf("UbsGetSharedDevice took %v\n", duration)
		}
	}()

	// Start a timer to call UbsAllocateDevice and UbsFreeDevice every minute
	go func() {
		ticker := time.NewTicker(1 * time.Minute)
		defer ticker.Stop()

		for range ticker.C {
			fmt.Println("\n=== Periodic Test UbsAllocateDevice ===")
			start := time.Now()
			info, err := urmav1.UbsAllocateDevice("urma_96")
			duration := time.Since(start)
			if err != nil {
				fmt.Printf("UbsAllocateDevice failed: %v\n", err)
			} else {
				fmt.Printf("Allocated device:\n")
				fmt.Printf("BondingPath: %s\n", info.BondingPath)
				fmt.Printf("BondingEid: %s\n", info.BondingEid)
				fmt.Printf("VfePaths: %v\n", info.VfePaths)

				// Test UbsFreeDevice
				fmt.Println("\n=== Periodic Test UbsFreeDevice ===")
				start = time.Now()
				err = urmav1.UbsFreeDevice("urma_96")
				duration = time.Since(start)
				if err != nil {
					fmt.Printf("UbsFreeDevice failed: %v\n", err)
				} else {
					fmt.Println("UbsFreeDevice succeeded")
				}
				fmt.Printf("UbsFreeDevice took %v\n", duration)
			}
			fmt.Printf("UbsAllocateDevice took %v\n", duration)
		}
	}()

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
	// Keep main goroutine alive
	select {}
}
