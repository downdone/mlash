package main

import (
	"os"

	"golang.org/x/sys/windows"
	"golang.zx2c4.com/wireguard/conn"
	"golang.zx2c4.com/wireguard/tun"
	"mlash.local/wireguard"
)

func main() {
	if len(os.Args) != 2 || os.Args[1] != "--service" {
		os.Exit(2)
	}
	guid, err := windows.GUIDFromString("{206B571A-6242-4D19-BE25-90788B778933}")
	if err != nil {
		os.Exit(2)
	}
	if err := wireguard.ServeControl(os.Stdin, os.Stdout, func(mtu int) (tun.Device, conn.Bind, error) {
		device, err := tun.CreateTUNWithRequestedGUID("Mlash WireGuard", &guid, mtu)
		return device, conn.NewDefaultBind(), err
	}); err != nil {
		os.Exit(1)
	}
}
