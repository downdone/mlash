// Package wireguard adapts the WireGuard device API to a single owned session.
package wireguard

import (
	"bytes"
	"encoding/hex"
	"errors"
	"fmt"
	"net/netip"
	"strconv"
	"sync"
	"time"

	"golang.zx2c4.com/wireguard/conn"
	"golang.zx2c4.com/wireguard/device"
	"golang.zx2c4.com/wireguard/tun"
)

const maxPeers = 32
const maxPeerPrefixes = 64
const maxPrefixes = 256

type Config struct {
	PrivateKey [32]byte
	ListenPort uint16
	Peers      []PeerConfig
}

type PeerConfig struct {
	PublicKey    [32]byte
	PresharedKey [32]byte
	Endpoint     netip.AddrPort
	AllowedIPs   []netip.Prefix
	Keepalive    uint16
}

// PeerID is the one-based position in the configuration for this session.
type PeerStats struct {
	PeerID        int
	RxBytes       uint64
	TxBytes       uint64
	LastHandshake time.Time
}

type Observation struct {
	ListenPort uint16
	Peers      []PeerStats
}

type Session struct {
	mu          sync.Mutex
	device      *device.Device
	peerIDs     map[[32]byte]int
	adapterLUID uint64
}

var errClosed = errors.New("wireguard session is closed")

// Start takes ownership of the unused TUN and bind. The caller owns Windows
// route and DNS state, and selects an appropriate TUN and UDP bind.
func Start(tunnel tun.Device, bind conn.Bind, config Config) (*Session, error) {
	if tunnel == nil || bind == nil {
		if tunnel != nil {
			_ = tunnel.Close()
		}
		return nil, errors.New("wireguard requires a TUN device and UDP bind")
	}
	peerIDs, err := validate(config)
	if err != nil {
		_ = tunnel.Close()
		return nil, err
	}
	dev := device.NewDevice(tunnel, bind, device.NewLogger(device.LogLevelSilent, ""))
	// IpcSet errors can contain configuration values. Expose only the failed stage.
	if err := apply(dev, config); err != nil {
		dev.Close()
		return nil, errors.New("wireguard could not apply device configuration")
	}
	if err := dev.Up(); err != nil {
		dev.Close()
		return nil, errors.New("wireguard could not start device")
	}
	session := &Session{device: dev, peerIDs: peerIDs}
	if native, ok := tunnel.(interface{ LUID() uint64 }); ok {
		session.adapterLUID = native.LUID()
	}
	return session, nil
}

func validate(config Config) (map[[32]byte]int, error) {
	var zero [32]byte
	if config.PrivateKey == zero || len(config.Peers) == 0 || len(config.Peers) > maxPeers {
		return nil, errors.New("wireguard requires a private key and 1–32 peers")
	}
	peerIDs := make(map[[32]byte]int, len(config.Peers))
	var previousPrefixes []netip.Prefix
	for index, peer := range config.Peers {
		if peer.PublicKey == zero || peerIDs[peer.PublicKey] != 0 {
			return nil, fmt.Errorf("wireguard peer %d has a missing or duplicate public key", index+1)
		}
		if peer.Endpoint.IsValid() && (peer.Endpoint.Port() == 0 || peer.Endpoint.Addr().Zone() != "") {
			return nil, fmt.Errorf("wireguard peer %d has an invalid endpoint", index+1)
		}
		if len(peer.AllowedIPs) == 0 || len(peer.AllowedIPs) > maxPeerPrefixes ||
			len(previousPrefixes)+len(peer.AllowedIPs) > maxPrefixes {
			return nil, fmt.Errorf("wireguard peer %d has an invalid prefix count", index+1)
		}
		for _, prefix := range peer.AllowedIPs {
			if !prefix.IsValid() || prefix.Addr().Is4In6() || prefix != prefix.Masked() {
				return nil, fmt.Errorf("wireguard peer %d has an invalid prefix", index+1)
			}
			for _, earlier := range previousPrefixes {
				if earlier.Overlaps(prefix) {
					return nil, fmt.Errorf("wireguard peer %d overlaps another peer", index+1)
				}
			}
		}
		previousPrefixes = append(previousPrefixes, peer.AllowedIPs...)
		peerIDs[peer.PublicKey] = index + 1
	}
	return peerIDs, nil
}

func apply(dev *device.Device, config Config) error {
	var request bytes.Buffer
	defer func() { clear(request.Bytes()) }()
	writeKey := func(name string, key [32]byte) {
		var encoded [64]byte
		hex.Encode(encoded[:], key[:])
		request.WriteString(name)
		request.Write(encoded[:])
		request.WriteByte('\n')
		clear(encoded[:])
	}
	writeKey("private_key=", config.PrivateKey)
	fmt.Fprintf(&request, "listen_port=%d\nreplace_peers=true\n", config.ListenPort)
	for _, peer := range config.Peers {
		writeKey("public_key=", peer.PublicKey)
		writeKey("preshared_key=", peer.PresharedKey)
		if peer.Endpoint.IsValid() {
			fmt.Fprintf(&request, "endpoint=%s\n", peer.Endpoint)
		}
		fmt.Fprintf(&request, "persistent_keepalive_interval=%d\nreplace_allowed_ips=true\n", peer.Keepalive)
		for _, prefix := range peer.AllowedIPs {
			fmt.Fprintf(&request, "allowed_ip=%s\n", prefix)
		}
	}
	request.WriteByte('\n')
	return dev.IpcSetOperation(bytes.NewReader(request.Bytes()))
}

func (s *Session) Observe() (Observation, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.device == nil {
		return Observation{}, errClosed
	}
	observation := observationReader{
		peerIDs:     s.peerIDs,
		observation: Observation{Peers: make([]PeerStats, len(s.peerIDs))},
	}
	if err := s.device.IpcGetOperation(&observation); err != nil {
		return Observation{}, errors.New("wireguard could not read device status")
	}
	for _, peer := range observation.observation.Peers {
		if peer.PeerID == 0 {
			return Observation{}, errors.New("wireguard peer missing from device status")
		}
	}
	return observation.observation, nil
}

func (s *Session) Close() {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.device != nil {
		s.device.Close()
		s.device = nil
	}
}

type observationReader struct {
	peerIDs     map[[32]byte]int
	observation Observation
}

// The pinned WireGuard library supplies one complete status buffer to Write.
// Inspect it in place and retain only the allowlisted data fields.
func (r *observationReader) Write(payload []byte) (int, error) {
	remaining := payload
	var peer *PeerStats
	var seconds int64
	for len(remaining) != 0 {
		var line []byte
		line, remaining, _ = bytes.Cut(remaining, []byte{'\n'})
		key, value, _ := bytes.Cut(line, []byte{'='})
		switch string(key) {
		case "listen_port":
			port, err := strconv.ParseUint(string(value), 10, 16)
			if err != nil {
				return 0, err
			}
			r.observation.ListenPort = uint16(port)
		case "public_key":
			var publicKey [32]byte
			id := 0
			if n, err := hex.Decode(publicKey[:], value); err == nil && n == len(publicKey) {
				id = r.peerIDs[publicKey]
			}
			if id == 0 {
				return 0, errors.New("unrecognized wireguard peer")
			}
			peer = &r.observation.Peers[id-1]
			peer.PeerID = id
			seconds = 0
		case "last_handshake_time_sec", "last_handshake_time_nsec", "rx_bytes", "tx_bytes":
			if peer == nil {
				return 0, errors.New("wireguard stats before peer")
			}
			number, err := strconv.ParseUint(string(value), 10, 64)
			if err != nil {
				return 0, err
			}
			switch string(key) {
			case "last_handshake_time_sec":
				if number > 1<<63-1 {
					return 0, errors.New("invalid wireguard handshake time")
				}
				seconds = int64(number)
			case "last_handshake_time_nsec":
				if number >= uint64(time.Second) {
					return 0, errors.New("invalid wireguard handshake time")
				}
				if seconds != 0 || number != 0 {
					peer.LastHandshake = time.Unix(seconds, int64(number)).UTC()
				}
			case "rx_bytes":
				peer.RxBytes = number
			case "tx_bytes":
				peer.TxBytes = number
			}
		}
	}
	return len(payload), nil
}
