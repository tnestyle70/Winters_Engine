package main

import (
	"encoding/binary"
	"flag"
	"fmt"
	"net"
	"os"
	"time"

	"github.com/google/uuid"
	"winters-backend/pkg/matchticket"
)

const (
	packetMagic       = uint16(0x5742)
	packetVersion     = byte(3)
	packetHeaderSize  = 40
	controlLane       = byte(1)
	handshakeFlag     = byte(1 << 2)
	ackOnlyFlag       = byte(1 << 1)
	typeClientHello   = uint16(100)
	typeServerRetry   = uint16(101)
	typeClientConnect = uint16(102)
	typeServerAccept  = uint16(103)
	typeClientConfirm = uint16(104)
)

type packetHeader struct {
	connectionID uint64
	generation   uint32
	typeID       uint16
	lane         byte
	flags        byte
	packetSeq    uint32
	messageSeq   uint32
	ackSeq       uint32
	payloadBytes uint16
}

func main() {
	address := flag.String("address", "127.0.0.1:9000", "UDP game server address")
	secret := flag.String("secret", "", "match ticket HMAC secret")
	ticketText := flag.String("ticket", "", "pre-issued match ticket (overrides -secret signing)")
	gameSessionID := flag.String("game-session-id", "local-game-1", "expected game session id")
	matchIDText := flag.String("match-id", uuid.NewString(), "match UUID")
	userIDText := flag.String("user-id", uuid.NewString(), "user UUID")
	timeout := flag.Duration("timeout", 3*time.Second, "handshake timeout")
	flag.Parse()
	if *ticketText == "" && len(*secret) < 32 {
		fmt.Fprintln(os.Stderr, "-secret must be at least 32 bytes")
		os.Exit(2)
	}
	matchID, matchErr := uuid.Parse(*matchIDText)
	userID, userErr := uuid.Parse(*userIDText)
	if matchErr != nil || userErr != nil {
		fmt.Fprintln(os.Stderr, "match-id and user-id must be UUIDs")
		os.Exit(2)
	}

	ticket := *ticketText
	if ticket == "" {
		signer, err := matchticket.NewSigner(*secret, 5*time.Minute)
		if err != nil {
			fatal(err)
		}
		ticket, err = signer.Issue(matchID, userID, *gameSessionID)
		if err != nil {
			fatal(err)
		}
	}
	remote, err := net.ResolveUDPAddr("udp", *address)
	if err != nil {
		fatal(err)
	}
	connection, err := net.DialUDP("udp", nil, remote)
	if err != nil {
		fatal(err)
	}
	defer connection.Close()
	if err := connection.SetDeadline(time.Now().Add(*timeout)); err != nil {
		fatal(err)
	}

	nonce := uint64(time.Now().UnixNano())
	hello := make([]byte, 16)
	binary.BigEndian.PutUint64(hello[0:8], nonce)
	binary.BigEndian.PutUint16(hello[8:10], 1200)
	if _, err := connection.Write(encodePacket(packetHeader{
		typeID: typeClientHello, lane: controlLane, flags: handshakeFlag,
		packetSeq: 1, messageSeq: 1,
	}, hello)); err != nil {
		fatal(err)
	}

	retryHeader, retryPayload, err := readPacket(connection)
	if err != nil {
		fatal(fmt.Errorf("read server retry: %w", err))
	}
	if retryHeader.typeID != typeServerRetry || len(retryPayload) != 32 ||
		binary.BigEndian.Uint64(retryPayload[0:8]) != nonce {
		fatal(fmt.Errorf("invalid server retry"))
	}
	connectPayload := make([]byte, 34+len(ticket))
	binary.BigEndian.PutUint64(connectPayload[0:8], nonce)
	copy(connectPayload[8:32], retryPayload[8:32])
	binary.BigEndian.PutUint16(connectPayload[32:34], uint16(len(ticket)))
	copy(connectPayload[34:], ticket)
	if _, err := connection.Write(encodePacket(packetHeader{
		typeID: typeClientConnect, lane: controlLane, flags: handshakeFlag,
		packetSeq: 2, messageSeq: 2,
	}, connectPayload)); err != nil {
		fatal(err)
	}

	acceptHeader, acceptPayload, err := readPacket(connection)
	if err != nil {
		fatal(fmt.Errorf("read server accept: %w", err))
	}
	if acceptHeader.typeID != typeServerAccept || len(acceptPayload) != 24 ||
		acceptHeader.connectionID == 0 || acceptHeader.generation == 0 ||
		binary.BigEndian.Uint64(acceptPayload[16:24]) != nonce {
		fatal(fmt.Errorf("invalid server accept"))
	}
	confirm := encodePacket(packetHeader{
		connectionID: acceptHeader.connectionID,
		generation:   acceptHeader.generation,
		typeID:       typeClientConfirm, lane: controlLane, flags: handshakeFlag,
		packetSeq: 1, messageSeq: 1, ackSeq: acceptHeader.packetSeq,
	}, nil)
	if _, err := connection.Write(confirm); err != nil {
		fatal(err)
	}

	for {
		header, _, err := readPacket(connection)
		if err != nil {
			fatal(fmt.Errorf("wait for confirm acknowledgement: %w", err))
		}
		if header.connectionID == acceptHeader.connectionID &&
			header.generation == acceptHeader.generation &&
			header.flags&ackOnlyFlag != 0 {
			break
		}
	}

	fmt.Printf("udp_ticket_smoke=pass match_id=%s user_id=%s game_session_id=%s ticket_bytes=%d\n",
		matchID, userID, *gameSessionID, len(ticket))
}

func encodePacket(header packetHeader, payload []byte) []byte {
	packet := make([]byte, packetHeaderSize+len(payload))
	binary.BigEndian.PutUint16(packet[0:2], packetMagic)
	packet[2] = packetVersion
	packet[3] = packetHeaderSize
	binary.BigEndian.PutUint64(packet[4:12], header.connectionID)
	binary.BigEndian.PutUint32(packet[12:16], header.generation)
	binary.BigEndian.PutUint16(packet[16:18], header.typeID)
	packet[18] = header.lane
	packet[19] = header.flags
	binary.BigEndian.PutUint32(packet[20:24], header.packetSeq)
	binary.BigEndian.PutUint32(packet[24:28], header.messageSeq)
	binary.BigEndian.PutUint32(packet[28:32], header.ackSeq)
	binary.BigEndian.PutUint16(packet[36:38], uint16(len(payload)))
	copy(packet[packetHeaderSize:], payload)
	return packet
}

func readPacket(connection *net.UDPConn) (packetHeader, []byte, error) {
	buffer := make([]byte, 1200)
	read, err := connection.Read(buffer)
	if err != nil {
		return packetHeader{}, nil, err
	}
	if read < packetHeaderSize || binary.BigEndian.Uint16(buffer[0:2]) != packetMagic ||
		buffer[2] != packetVersion || buffer[3] != packetHeaderSize {
		return packetHeader{}, nil, fmt.Errorf("invalid UDP packet header")
	}
	header := packetHeader{
		connectionID: binary.BigEndian.Uint64(buffer[4:12]),
		generation:   binary.BigEndian.Uint32(buffer[12:16]),
		typeID:       binary.BigEndian.Uint16(buffer[16:18]),
		lane:         buffer[18], flags: buffer[19],
		packetSeq:    binary.BigEndian.Uint32(buffer[20:24]),
		messageSeq:   binary.BigEndian.Uint32(buffer[24:28]),
		ackSeq:       binary.BigEndian.Uint32(buffer[28:32]),
		payloadBytes: binary.BigEndian.Uint16(buffer[36:38]),
	}
	if int(header.payloadBytes) != read-packetHeaderSize {
		return packetHeader{}, nil, fmt.Errorf("invalid UDP payload length")
	}
	return header, buffer[packetHeaderSize:read], nil
}

func fatal(err error) {
	fmt.Fprintln(os.Stderr, err)
	os.Exit(1)
}
