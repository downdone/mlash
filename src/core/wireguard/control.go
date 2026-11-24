package wireguard

// CONTRACT owner=core layer=core
// invariant: 控制帧为 4 字节小端长度加 UTF-8 JSON，版本 1；单帧上限 128 KiB
// forbidden: 不得改为无长度前缀的流；不得回传原始密钥或 IpcGet 全量输出
// lifecycle: 输入 EOF 时关闭设备；宿主关闭句柄后不得再写
// tests: wireguard_profile_tests
// spec: docs/design/mihomo-wireguard-dual-engine.md

import (
	"bytes"
	"encoding/binary"
	"encoding/json"
	"errors"
	"io"
	"net/netip"
	"strconv"
	"time"

	"golang.zx2c4.com/wireguard/conn"
	"golang.zx2c4.com/wireguard/tun"
)

const controlVersion = 1
const maxControlFrame = 128 * 1024

// DeviceFactory supplies an unused TUN and bind. Its caller retains ownership
// of platform route/DNS changes and the authenticated transport to this process.
type DeviceFactory func(mtu int) (tun.Device, conn.Bind, error)

type controlRequest struct {
	Version   int          `json:"version"`
	Sequence  uint64       `json:"sequence"`
	Operation string       `json:"operation"`
	Config    *startConfig `json:"config,omitempty"`
}

type startConfig struct {
	PrivateKey []byte       `json:"private_key"`
	ListenPort uint16       `json:"listen_port"`
	MTU        int          `json:"mtu"`
	Peers      []peerConfig `json:"peers"`
}

type peerConfig struct {
	PublicKey    []byte         `json:"public_key"`
	PresharedKey []byte         `json:"preshared_key,omitempty"`
	Endpoint     netip.AddrPort `json:"endpoint"`
	AllowedIPs   []netip.Prefix `json:"allowed_ips"`
	Keepalive    uint16         `json:"keepalive"`
}

type controlReply struct {
	Version     int           `json:"version"`
	Sequence    uint64        `json:"sequence"`
	OK          bool          `json:"ok"`
	Error       string        `json:"error,omitempty"`
	Observation *deviceStatus `json:"observation,omitempty"`
}

type deviceStatus struct {
	AdapterLUID string       `json:"adapter_luid,omitempty"`
	ListenPort  uint16       `json:"listen_port"`
	Peers       []peerStatus `json:"peers"`
}

type peerStatus struct {
	PeerID    int        `json:"peer_id"`
	RxBytes   uint64     `json:"rx_bytes,string"`
	TxBytes   uint64     `json:"tx_bytes,string"`
	Handshake *time.Time `json:"last_handshake,omitempty"`
}

// ServeControl runs one device control loop on pre-established private pipes.
// It never listens on a control socket. A dropped pipe closes the device.
func ServeControl(input io.Reader, output io.Writer, open DeviceFactory) error {
	if open == nil {
		return errors.New("wireguard device factory is required")
	}
	var session *Session
	defer func() {
		if session != nil {
			session.Close()
		}
	}()
	var previous uint64
	for {
		var size uint32
		switch err := binary.Read(input, binary.LittleEndian, &size); {
		case err == io.EOF:
			return nil
		case err != nil || size == 0 || size > maxControlFrame:
			return errors.New("invalid wireguard control frame")
		}
		data := make([]byte, size)
		if _, err := io.ReadFull(input, data); err != nil {
			clear(data)
			return errors.New("incomplete wireguard control frame")
		}
		request, valid := decodeRequest(data)
		clear(data)
		reply := controlReply{Version: controlVersion, Sequence: request.Sequence}
		switch {
		case !valid:
			reply.Error = "invalid_request"
		case request.Version != controlVersion:
			reply.Error = "unsupported_version"
		case request.Sequence <= previous:
			reply.Error = "stale_sequence"
		default:
			previous = request.Sequence
			reply.Observation, reply.Error = perform(request, &session, open)
		}
		if request.Config != nil {
			clear(request.Config.PrivateKey)
			for _, peer := range request.Config.Peers {
				clear(peer.PresharedKey)
			}
		}
		reply.OK = reply.Error == ""
		if err := writeReply(output, reply); err != nil {
			return err
		}
	}
}

func decodeRequest(data []byte) (controlRequest, bool) {
	var request controlRequest
	decoder := json.NewDecoder(bytes.NewReader(data))
	decoder.DisallowUnknownFields()
	if decoder.Decode(&request) != nil {
		return request, false
	}
	return request, decoder.Decode(&struct{}{}) == io.EOF
}

func perform(request controlRequest, session **Session, open DeviceFactory) (*deviceStatus, string) {
	if request.Operation != "start" && request.Config != nil {
		return nil, "unexpected_config"
	}
	switch request.Operation {
	case "start":
		if *session != nil {
			return nil, "device_already_started"
		}
		cfg, ok := request.Config.deviceConfig()
		if !ok {
			return nil, "invalid_config"
		}
		if _, err := validate(cfg); err != nil {
			return nil, "invalid_config"
		}
		tunnel, bind, err := open(request.Config.MTU)
		if err != nil {
			return nil, "device_unavailable"
		}
		started, err := Start(tunnel, bind, cfg)
		if err != nil {
			return nil, "device_start_failed"
		}
		*session = started
	case "status":
		if *session == nil {
			return nil, "device_stopped"
		}
		observed, err := (*session).Observe()
		if err != nil {
			return nil, "observation_failed"
		}
		status := &deviceStatus{ListenPort: observed.ListenPort, Peers: make([]peerStatus, len(observed.Peers))}
		if (*session).adapterLUID != 0 {
			status.AdapterLUID = strconv.FormatUint((*session).adapterLUID, 10)
		}
		for index, peer := range observed.Peers {
			status.Peers[index] = peerStatus{PeerID: peer.PeerID, RxBytes: peer.RxBytes, TxBytes: peer.TxBytes}
			if !peer.LastHandshake.IsZero() {
				status.Peers[index].Handshake = &peer.LastHandshake
			}
		}
		return status, ""
	case "stop":
		if *session != nil {
			(*session).Close()
			*session = nil
		}
	default:
		return nil, "unsupported_operation"
	}
	return nil, ""
}

func (config *startConfig) deviceConfig() (Config, bool) {
	if config == nil || len(config.PrivateKey) != 32 || len(config.Peers) > maxPeers ||
		(config.MTU != 0 && (config.MTU < 576 || config.MTU > 65535)) {
		return Config{}, false
	}
	value := Config{PrivateKey: [32]byte(config.PrivateKey), ListenPort: config.ListenPort}
	for _, peer := range config.Peers {
		if len(peer.PublicKey) != 32 || (len(peer.PresharedKey) != 0 && len(peer.PresharedKey) != 32) ||
			!peer.Endpoint.IsValid() || peer.Endpoint.Addr().IsUnspecified() {
			return Config{}, false
		}
		entry := PeerConfig{PublicKey: [32]byte(peer.PublicKey), Endpoint: peer.Endpoint,
			AllowedIPs: peer.AllowedIPs, Keepalive: peer.Keepalive}
		if len(peer.PresharedKey) == 32 {
			entry.PresharedKey = [32]byte(peer.PresharedKey)
		}
		value.Peers = append(value.Peers, entry)
	}
	return value, true
}

func writeReply(output io.Writer, reply controlReply) error {
	body, err := json.Marshal(reply)
	if err != nil {
		return err
	}
	frame := binary.LittleEndian.AppendUint32(make([]byte, 0, 4+len(body)), uint32(len(body)))
	frame = append(frame, body...)
	n, err := output.Write(frame)
	if err == nil && n != len(frame) {
		return io.ErrShortWrite
	}
	return err
}
