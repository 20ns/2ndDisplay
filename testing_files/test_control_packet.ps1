#!/usr/bin/env pwsh
# Test sending a properly formatted control packet to Android

$androidIP = "192.168.238.161"
$androidPort = 5004

Write-Host "Creating control packet for Android device at ${androidIP}:${androidPort}"

# Control packet JSON
$controlJson = @{
    type = "keepalive"
    width = 1920
    height = 1080
    fps = 60
    bitrate = 30
    touchPort = 12345
} | ConvertTo-Json -Compress

Write-Host "Control JSON: $controlJson"

# Create 12-byte header for control packet
$header = @(
    # sequenceId (2 bytes) = 1
    0x01, 0x00,
    # frameId (2 bytes) = 0
    0x00, 0x00,
    # totalChunks (2 bytes) = 1
    0x01, 0x00,
    # chunkIndex (2 bytes) = 0
    0x00, 0x00,
    # flags (4 bytes) = FLAG_CONTROL_PACKET (0x04)
    0x04, 0x00, 0x00, 0x00
)

# Convert JSON to bytes
$jsonBytes = [System.Text.Encoding]::UTF8.GetBytes($controlJson)

# Combine header + JSON payload
$packet = $header + $jsonBytes

Write-Host "Packet size: $($packet.Length) bytes (header: 12, payload: $($jsonBytes.Length))"

# Send UDP packet
try {
    $udpClient = New-Object System.Net.Sockets.UdpClient
    $endpoint = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Parse($androidIP), $androidPort)
    
    $sent = $udpClient.Send($packet, $packet.Length, $endpoint)
    Write-Host "Control packet sent successfully! ($sent bytes)"
    
    $udpClient.Close()
    Write-Host "✅ Check your Android tablet - the status should change to show connection info"
} catch {
    Write-Host "❌ Error sending packet: $_"
}
