package wireguard

import (
	"bytes"
	"encoding/binary"
	"encoding/json"
	"io"
	"net"
	"net/netip"
	"testing"

	"golang.zx2c4.com/wireguard/conn"
	"golang.zx2c4.com/wireguard/tun"
	"golang.zx2c4.com/wireguard/tun/netstack"
)

func appendFrame(t *testing.T, input *bytes.Buffer, request controlRequest) {
	t.Helper()
	body, err := json.Marshal(request)
	if err != nil {
		t.Fatal(err)
	}
	if err := binary.Write(input, binary.LittleEndian, uint32(len(body))); err != nil {
		t.Fatal(err)
	}
	input.Write(body)
}

func nextReply(t *testing.T, output *bytes.Buffer) controlReply {
	t.Helper()
	var size uint32
	if err := binary.Read(output, binary.LittleEndian, &size); err != nil {
		t.Fatal(err)
	}
	if size == 0 || size > maxControlFrame {
		t.Fatal("invalid response frame size")
	}
	body := make([]byte, size)
	if _, err := io.ReadFull(output, body); err != nil {
		t.Fatal(err)
	}
	var reply controlReply
	decoder := json.NewDecoder(bytes.NewReader(body))
	decoder.DisallowUnknownFields()
	if err := decoder.Decode(&reply); err != nil {
		t.Fatal(err)
	}
	return reply
}

func TestControlFrameLifecycle(t *testing.T) {
	key := testKey(t)
	peer := testKey(t)
	config := &startConfig{
		PrivateKey: key.Bytes(),
		MTU:        1400,
		Peers: []peerConfig{{
			PublicKey:    peer.PublicKey().Bytes(),
			PresharedKey: testKey(t).Bytes(),
			Endpoint:     netip.MustParseAddrPort("127.0.0.1:51820"),
			AllowedIPs:   []netip.Prefix{netip.MustParsePrefix("192.0.2.2/32")},
		}},
	}
	var input, output bytes.Buffer
	for _, request := range []controlRequest{
		{controlVersion, 1, "status", nil},
		{controlVersion, 2, "start", config},
		{controlVersion, 2, "stop", nil},
		{controlVersion, 3, "status", nil},
		{controlVersion, 4, "stop", nil},
		{controlVersion, 5, "stop", nil},
		{controlVersion, 6, "start", config},
		{controlVersion, 7, "status", nil},
	} {
		appendFrame(t, &input, request)
	}
	if err := ServeControl(&input, &output, func(mtu int) (tun.Device, conn.Bind, error) {
		if mtu != 1400 {
			t.Fatal("configured MTU was not forwarded")
		}
		tunnel, _, err := netstack.CreateNetTUN([]netip.Addr{netip.MustParseAddr("192.0.2.1")}, nil, mtu)
		return tunnel, conn.NewStdNetBind(), err
	}); err != nil {
		t.Fatal(err)
	}
	for _, marker := range [][]byte{[]byte(`"private_key"`), []byte(`"public_key"`), []byte(`"preshared_key"`)} {
		if bytes.Contains(output.Bytes(), marker) {
			t.Fatal("credential was serialized by the control interface")
		}
	}
	codes := []string{"device_stopped", "", "stale_sequence", "", "", "", "", ""}
	sequences := []uint64{1, 2, 2, 3, 4, 5, 6, 7}
	var finalPort uint16
	for index, code := range codes {
		reply := nextReply(t, &output)
		if reply.Version != controlVersion || reply.Sequence != sequences[index] || reply.OK != (code == "") || reply.Error != code {
			t.Fatalf("unexpected response at frame %d", index+1)
		}
		if reply.Sequence == 3 || reply.Sequence == 7 {
			if reply.Observation == nil || reply.Observation.ListenPort == 0 ||
				len(reply.Observation.Peers) != 1 || reply.Observation.Peers[0].PeerID != 1 {
				t.Fatal("no live device observation returned")
			}
			finalPort = reply.Observation.ListenPort
		}
	}
	if output.Len() != 0 {
		t.Fatal("unexpected extra control frames")
	}
	reused, err := net.ListenUDP("udp4", &net.UDPAddr{IP: net.IPv4(127, 0, 0, 1), Port: int(finalPort)})
	if err != nil {
		t.Fatal("EOF did not close the device:", err)
	}
	reused.Close()
}

func TestControlRejectsOversizeBeforeOpeningDevice(t *testing.T) {
	var header [4]byte
	binary.LittleEndian.PutUint32(header[:], maxControlFrame+1)
	err := ServeControl(bytes.NewReader(header[:]), io.Discard, func(int) (tun.Device, conn.Bind, error) {
		t.Fatal("oversized request opened a device")
		return nil, nil, nil
	})
	if err == nil {
		t.Fatal("oversized request was accepted")
	}
}
