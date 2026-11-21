package wireguard

import (
	"context"
	"crypto/ecdh"
	"crypto/rand"
	"errors"
	"io"
	"net"
	"net/netip"
	"os"
	"testing"
	"time"

	"golang.zx2c4.com/wireguard/conn"
	"golang.zx2c4.com/wireguard/tun/netstack"
)

func testKey(t *testing.T) *ecdh.PrivateKey {
	t.Helper()
	key, err := ecdh.X25519().GenerateKey(rand.Reader)
	if err != nil {
		t.Fatal("could not create an ephemeral test key")
	}
	return key
}

func startTestSession(t *testing.T, address netip.Addr, config Config) (*Session, *netstack.Net) {
	t.Helper()
	tunnel, network, err := netstack.CreateNetTUN([]netip.Addr{address}, nil, 1420)
	if err != nil {
		t.Fatal("could not create test TUN:", err)
	}
	session, err := Start(tunnel, conn.NewStdNetBind(), config)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(session.Close)
	return session, network
}

func sample(t *testing.T, session *Session) Observation {
	t.Helper()
	value, err := session.Observe()
	if err != nil || len(value.Peers) != 1 || value.Peers[0].PeerID != 1 || value.ListenPort == 0 {
		t.Fatal("expected a live device with one identified peer:", err)
	}
	return value
}

func TestSDKWireGuardTransfer(t *testing.T) {
	var verifyProxy func()
	if executable := os.Getenv("MLASH_WG_SDK_MIHOMO"); executable != "" {
		verifyProxy = startMihomoProbe(t, executable)
	}
	clientKey := testKey(t)
	serverKey := testKey(t)
	clientAddress := netip.MustParseAddr("192.0.2.1")
	serverAddress := netip.MustParseAddr("192.0.2.2")
	server, serverNetwork := startTestSession(t, serverAddress, Config{
		PrivateKey: [32]byte(serverKey.Bytes()),
		Peers: []PeerConfig{{
			PublicKey:  [32]byte(clientKey.PublicKey().Bytes()),
			AllowedIPs: []netip.Prefix{netip.PrefixFrom(clientAddress, 32)},
		}},
	})
	initial := sample(t, server)
	if peer := initial.Peers[0]; !peer.LastHandshake.IsZero() || peer.RxBytes != 0 || peer.TxBytes != 0 {
		t.Fatal("a listening device without traffic reported a handshake or counters")
	}
	client, clientNetwork := startTestSession(t, clientAddress, Config{
		PrivateKey: [32]byte(clientKey.Bytes()),
		Peers: []PeerConfig{{
			PublicKey:  [32]byte(serverKey.PublicKey().Bytes()),
			Endpoint:   netip.AddrPortFrom(netip.MustParseAddr("127.0.0.1"), initial.ListenPort),
			AllowedIPs: []netip.Prefix{netip.PrefixFrom(serverAddress, 32)},
		}},
	})
	target := netip.AddrPortFrom(serverAddress, 18080)
	listener, err := serverNetwork.ListenTCPAddrPort(target)
	if err != nil {
		t.Fatal(err)
	}
	defer listener.Close()
	result := make(chan error, 1)
	go func() {
		remote, err := listener.Accept()
		if err != nil {
			result <- err
			return
		}
		defer remote.Close()
		_ = remote.SetDeadline(time.Now().Add(8 * time.Second))
		request := make([]byte, len("request"))
		if _, err = io.ReadFull(remote, request); err == nil {
			if string(request) != "request" {
				err = errors.New("unexpected application request")
			} else {
				_, err = io.WriteString(remote, "response")
			}
		}
		result <- err
	}()
	ctx, cancel := context.WithTimeout(context.Background(), 8*time.Second)
	defer cancel()
	local, err := clientNetwork.DialContextTCPAddrPort(ctx, target)
	if err != nil {
		t.Fatal("the TCP connection did not reach the WireGuard peer:", err)
	}
	defer local.Close()
	if err := local.SetDeadline(time.Now().Add(8 * time.Second)); err != nil {
		t.Fatal(err)
	}
	if _, err := io.WriteString(local, "request"); err != nil {
		t.Fatal(err)
	}
	reply := make([]byte, len("response"))
	if _, err := io.ReadFull(local, reply); err != nil || string(reply) != "response" {
		t.Fatal("did not receive the peer's application response:", err)
	}
	if err := <-result; err != nil {
		t.Fatal(err)
	}
	local.Close()
	clientState := sample(t, client)
	for _, state := range []Observation{clientState, sample(t, server)} {
		peer := state.Peers[0]
		if peer.LastHandshake.IsZero() || peer.RxBytes == 0 || peer.TxBytes == 0 {
			t.Fatal("bidirectional transfer did not report a handshake and traffic")
		}
	}
	if verifyProxy != nil {
		verifyProxy()
	}

	client.Close()
	client.Close()
	if _, err := client.Observe(); !errors.Is(err, errClosed) {
		t.Fatal("closed device returned status:", err)
	}
	rebound, err := net.ListenUDP("udp4", &net.UDPAddr{IP: net.IPv4(127, 0, 0, 1), Port: int(clientState.ListenPort)})
	if err != nil {
		t.Fatal("closed device did not release its UDP port:", err)
	}
	rebound.Close()
	if peer := sample(t, server).Peers[0]; peer.LastHandshake.IsZero() {
		t.Fatal("stopping the client changed the other device's handshake status")
	}
	if verifyProxy != nil {
		verifyProxy()
	}
}

func TestStartRejectsCrossPeerPrefixOverlap(t *testing.T) {
	first, second := testKey(t), testKey(t)
	tunnel, _, err := netstack.CreateNetTUN([]netip.Addr{netip.MustParseAddr("192.0.2.1")}, nil, 1420)
	if err != nil {
		t.Fatal(err)
	}
	// Both peers are locally generated. No actual address or key enters logs.
	config := Config{
		PrivateKey: [32]byte(first.Bytes()),
		Peers: []PeerConfig{
			{PublicKey: [32]byte(first.PublicKey().Bytes()), AllowedIPs: []netip.Prefix{netip.MustParsePrefix("192.0.2.0/24")}},
			{PublicKey: [32]byte(second.PublicKey().Bytes()), AllowedIPs: []netip.Prefix{netip.MustParsePrefix("192.0.2.2/32")}},
		},
	}
	if session, err := Start(tunnel, conn.NewStdNetBind(), config); err == nil {
		session.Close()
		t.Fatal("overlapping peers were accepted")
	} else if err.Error() != "wireguard peer 2 overlaps another peer" {
		t.Fatal("unexpected rejection:", err)
	}
	// The test TUN queues one initial up event. The next read must observe close.
	<-tunnel.Events()
	select {
	case _, open := <-tunnel.Events():
		if open {
			t.Fatal("unexpected TUN event after rejected start")
		}
	case <-time.After(time.Second):
		t.Fatal("rejected start did not close its TUN")
	}
}
