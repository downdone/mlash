package wireguard

import (
	"encoding/base64"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/http/httptest"
	"net/netip"
	"net/url"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"
	"time"
)

// startMihomoProbe optionally runs the pinned real proxy next to the two
// WireGuard peers. Both the proxy and its destination use only loopback.
func startMihomoProbe(t *testing.T, executable string) func() {
	t.Helper()
	origin := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		_, _ = io.WriteString(w, "mihomo-probe")
	}))
	t.Cleanup(origin.Close)
	fetch := startMihomo(t, executable, "rules: [\"MATCH,DIRECT\"]\n")
	waitForMihomo(t, fetch, origin.URL, "mihomo-probe")
	return func() {
		if err := fetch(origin.URL, "mihomo-probe"); err != nil {
			t.Fatal("Mihomo local proxy stopped responding during the WireGuard lifecycle:", err)
		}
	}
}

func startMihomo(t *testing.T, executable, profile string) func(string, string) error {
	t.Helper()
	portReservation, err := net.Listen("tcp4", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	address := portReservation.Addr().String()
	port := portReservation.Addr().(*net.TCPAddr).Port
	defer portReservation.Close()
	directory := t.TempDir()
	config := fmt.Sprintf("mixed-port: %d\nallow-lan: false\nbind-address: 127.0.0.1\nmode: rule\ngeo-auto-update: false\n%s", port, profile)
	configPath := filepath.Join(directory, "config.yaml")
	if err := os.WriteFile(configPath, []byte(config), 0600); err != nil {
		t.Fatal(err)
	}
	logPath := filepath.Join(directory, "mihomo.log")
	log, err := os.Create(logPath)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { _ = log.Close() })
	command := exec.Command(executable, "-d", directory, "-f", configPath)
	command.Stdout, command.Stderr = log, log
	if err := portReservation.Close(); err != nil {
		t.Fatal(err)
	}
	if err := command.Start(); err != nil {
		t.Fatal(err)
	}
	exited := make(chan struct{})
	go func() {
		_ = command.Wait()
		close(exited)
	}()
	t.Cleanup(func() {
		_ = command.Process.Kill()
		<-exited
	})

	transport := &http.Transport{Proxy: http.ProxyURL(&url.URL{Scheme: "http", Host: address})}
	t.Cleanup(transport.CloseIdleConnections)
	client := &http.Client{Transport: transport, Timeout: time.Second}
	return func(target, expected string) error {
		select {
		case <-exited:
			file, err := os.Open(logPath)
			if err != nil {
				return fmt.Errorf("Mihomo exited before the probe: %w", err)
			}
			defer file.Close()
			preview, _ := io.ReadAll(io.LimitReader(file, 4096))
			message := string(preview)
			for _, line := range strings.Split(profile, "\n") {
				field, value, _ := strings.Cut(strings.TrimSpace(line), ":")
				if strings.HasSuffix(field, "-key") && strings.TrimSpace(value) != "" {
					message = strings.ReplaceAll(message, strings.TrimSpace(value), "[redacted]")
				}
			}
			return fmt.Errorf("Mihomo exited before the probe: %s", strings.TrimSpace(message))
		default:
		}
		transport.CloseIdleConnections()
		response, err := client.Get(target)
		if err != nil {
			return err
		}
		defer response.Body.Close()
		body, err := io.ReadAll(io.LimitReader(response.Body, 64))
		if err != nil {
			return err
		}
		if response.StatusCode != http.StatusOK || string(body) != expected {
			return fmt.Errorf("proxy did not return loopback fixture: status %d", response.StatusCode)
		}
		return nil
	}
}

func waitForMihomo(t *testing.T, fetch func(string, string) error, target, expected string) {
	t.Helper()
	deadline := time.Now().Add(6 * time.Second)
	for {
		err := fetch(target, expected)
		if err == nil {
			break
		}
		if time.Now().After(deadline) {
			t.Fatal("Mihomo local proxy did not become ready:", err)
		}
		time.Sleep(40 * time.Millisecond)
	}
}

func TestLockedMihomoDirectAndWireGuardOutbound(t *testing.T) {
	executable := os.Getenv("MLASH_WG_SDK_MIHOMO")
	if executable == "" {
		t.Skip("requires the locked Mihomo executable")
	}
	clientKey, serverKey := testKey(t), testKey(t)
	const clientIP, serverIP = "192.0.2.3", "192.0.2.4"
	client := netip.MustParseAddr(clientIP)
	server := netip.MustParseAddr(serverIP)
	session, stack := startTestSession(t, server, Config{
		PrivateKey: [32]byte(serverKey.Bytes()),
		Peers: []PeerConfig{{PublicKey: [32]byte(clientKey.PublicKey().Bytes()),
			AllowedIPs: []netip.Prefix{netip.PrefixFrom(client, 32)}}},
	})
	wgPort := sample(t, session).ListenPort
	target := netip.AddrPortFrom(server, 18080)
	listener, err := stack.ListenTCPAddrPort(target)
	if err != nil {
		t.Fatal(err)
	}
	// The response identifies the destination that exists only behind the peer.
	origin := &http.Server{Handler: http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		_, _ = io.WriteString(w, "wireguard-destination")
	})}
	t.Cleanup(func() { _ = origin.Close() })
	go func() { _ = origin.Serve(listener) }()
	direct := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		_, _ = io.WriteString(w, "direct-destination")
	}))
	t.Cleanup(direct.Close)
	profile := fmt.Sprintf(`proxies:
  - name: sdk-tunnel
    type: wireguard
    server: 127.0.0.1
    port: %d
    ip: %s
    private-key: %s
    public-key: %s
    allowed-ips: ['%s/32']
    udp: true
rules:
  - IP-CIDR,%s/32,sdk-tunnel,no-resolve
  - MATCH,DIRECT
`, wgPort, clientIP, base64.StdEncoding.EncodeToString(clientKey.Bytes()),
		base64.StdEncoding.EncodeToString(serverKey.PublicKey().Bytes()), serverIP, serverIP)
	fetch := startMihomo(t, executable, profile)
	waitForMihomo(t, fetch, direct.URL, "direct-destination")
	waitForMihomo(t, fetch, "http://"+target.String(), "wireguard-destination")
	if observed := sample(t, session).Peers[0]; observed.LastHandshake.IsZero() || observed.RxBytes == 0 || observed.TxBytes == 0 {
		t.Fatal("the Mihomo WireGuard outbound did not reach the expected peer")
	}
	if err := fetch(direct.URL, "direct-destination"); err != nil {
		t.Fatal("the same Mihomo instance failed to route direct traffic:", err)
	}
}
