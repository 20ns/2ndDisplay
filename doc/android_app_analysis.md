# Android App Structure Analysis

## Overview
The Android app is a second-screen display client called "Second Screen Display" that receives H.264 video streams from Windows host via UDP and displays them fullscreen while sending touch events back.

## Key Files Structure
```
android/app/src/main/java/com/tabdisplay/secondscreen/
├── MainActivity.kt - Main UI and service coordinator
├── VideoReceiverService.kt - Network communication and video processing
├── Decoder.kt - Hardware H.264 decoder (MediaCodec)
├── PacketAssembler.kt - Packet reassembly with FEC
├── TouchSender.kt - Touch event transmission
└── SettingsActivity.kt - Settings interface
```

## **CRITICAL DISCOVERY PROBLEM IDENTIFIED**

### Expected vs Actual Discovery Protocol

**Host expects (from host/brief-protocol.md):**
- Host broadcasts "HELLO" to UDP port 45678
- Android responds with "HELLO:[device_name]"
- Host adds device to available connections

**Android app reality:**
- **NO discovery listener implemented**
- Only listens on UDP port 5004 for video data
- Missing discovery service on port 45678
- App assumes direct connection without discovery

## Network Architecture
- **Protocol:** UDP over USB Tethering (RNDIS)
- **Video Port:** 5004 (VideoReceiverService)
- **Discovery Port:** 45678 (MISSING SERVICE)
- **Network:** USB tethering creates 192.168.42.x network
- **Expected IPs:** Host at 192.168.42.1, Android at 192.168.42.129

## Video Processing Pipeline
1. VideoReceiverService receives UDP packets on port 5004
2. PacketAssembler reassembles frame chunks with FEC recovery
3. Decoder uses MediaCodec hardware acceleration for H.264
4. Video rendered to SurfaceView in fullscreen landscape

## Packet Structure (12-byte header)
```
| sequenceId (2) | frameId (2) | totalChunks (2) | chunkIndex (2) | flags (4) |
```

### Flag Types
- `0x01`: Key frame (IDR)
- `0x02`: Input packet (touch event) 
- `0x04`: Control packet (JSON)

## Touch Event Format
```json
{
  "type": "touch",
  "id": 0,
  "x": 1234,
  "y": 567,
  "state": "down|move|up"
}
```

## Supported Display Modes
1. **1080p_60hz:** 1920×1080 @ 60Hz (30 Mbps)
2. **tablet_60hz:** 1752×2800 @ 60Hz (40 Mbps)
3. **tablet_120hz:** 1752×2800 @ 120Hz (40 Mbps)

## Notification System
Shows connection status, IP address, resolution, and frame rate:
- "Service starting..."
- "Waiting for connection..."
- "Connected to [IP_ADDRESS]"
- "Receiving: Frame [frame_number]"

## **FIX REQUIRED**
Android app needs a discovery service that:
- Binds to UDP port 45678
- Listens for "HELLO" broadcasts from Windows host
- Responds with "HELLO:[device_name]"
- Announces device availability

This missing component explains why host .exe cannot detect the Android device.