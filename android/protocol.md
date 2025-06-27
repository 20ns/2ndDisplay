# TabDisplay Protocol Specification

This document describes the protocol used between the Windows host and Android client.

## Transport Layer

- **Protocol**: UDP over USB Tethering (RNDIS)
- **Default Port**: 5004
- **Maximum Packet Size**: 1350 bytes payload + 12 bytes header

## Packet Format

### Header Structure (12 bytes)

| Field       | Type    | Bytes | Description                             |
|-------------|---------|-------|-----------------------------------------|
| sequenceId  | uint16  | 2     | Unique sequence identifier              |
| frameId     | uint16  | 2     | Frame identifier                        |
| totalChunks | uint16  | 2     | Total chunks in this frame              |
| chunkIndex  | uint16  | 2     | Chunk index (0-based)                   |
| flags       | uint32  | 4     | Bit flags (see below)                   |

### Flags

- **0x01**: Key frame (IDR)
- **0x02**: Input packet (contains touch event)
- **0x04**: Control packet (contains JSON)

## Video Streaming

1. Each H.264/AVC frame is split into chunks of max 1350 bytes
2. Chunks are transmitted with sequential headers
3. Key frames (IDR) are marked with the key frame flag
4. For each frame, 2 additional parity chunks are sent using XOR-based FEC

### Error Correction

Parity chunks are generated as follows:
- Parity[0] = XOR of even-indexed chunks
- Parity[1] = XOR of odd-indexed chunks

Parity chunks have a special chunkIndex starting with 0xF000.

## Control Messages

Control messages are JSON packets sent as keepalive messages:

```json
{
  "type": "keepalive",
  "width": 1920,
  "height": 1080,
  "fps": 60,
  "bitrate": 30
}
```

## Input Events from Android

Android sends touch events as JSON packets:

```json
{
  "type": "touch",
  "id": 0,
  "x": 1234,
  "y": 567,
  "state": "down|move|up"
}
```

## Supported Profiles

| Profile         | Resolution  | Frame Rate | Bitrate      |
|-----------------|-------------|------------|--------------|
| FullHD_60Hz     | 1920×1080   | 60 Hz      | 20-30 Mbps   |
| Tablet_60Hz     | 1752×2800   | 60 Hz      | 30-40 Mbps   |
| Tablet_120Hz    | 1752×2800   | 120 Hz     | 40 Mbps      |
