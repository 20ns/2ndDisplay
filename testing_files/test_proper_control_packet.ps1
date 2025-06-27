# Test proper JSON control packet to Android device
$AndroidIP = "192.168.238.161"
$AndroidPort = 5004

Write-Host "Testing proper JSON control packet to Android device at ${AndroidIP}:${AndroidPort}"

# Create UDP client
$client = New-Object System.Net.Sockets.UdpClient

try {
    # Create a proper JSON control packet
    $controlPacket = @{
        type = "keepalive"
        width = 1920
        height = 1080
        fps = 30
        bitrate = 5000000
        touchPort = 5005
    } | ConvertTo-Json -Compress
    
    Write-Host "Sending control packet: $controlPacket"
    
    # Convert to bytes and send
    $message = [System.Text.Encoding]::UTF8.GetBytes($controlPacket)
    
    # Packet needs FLAG_CONTROL_PACKET header (12 bytes)
    # Format: [4 bytes packet ID][4 bytes payload size][4 bytes flags][payload]
    $header = New-Object byte[] 12
    # Packet ID (0)
    [System.BitConverter]::GetBytes([int]0).CopyTo($header, 0)
    # Payload size
    [System.BitConverter]::GetBytes([int]$message.Length).CopyTo($header, 4)
    # Flags: FLAG_CONTROL_PACKET = 0x04
    [System.BitConverter]::GetBytes([int]4).CopyTo($header, 8)
    
    # Combine header and payload
    $fullPacket = New-Object byte[] ($header.Length + $message.Length)
    $header.CopyTo($fullPacket, 0)
    $message.CopyTo($fullPacket, $header.Length)
    
    $bytes = $client.Send($fullPacket, $fullPacket.Length, $AndroidIP, $AndroidPort)
    Write-Host "Sent $bytes bytes to $AndroidIP`:$AndroidPort"
    Write-Host "If the Android app shows 'Connected to...' status, the connection is working!"
    
    # Wait a moment to see if anything happens
    Start-Sleep -Seconds 3
    
} catch {
    Write-Host "Error sending UDP packet: $($_.Exception.Message)"
} finally {
    $client.Close()
}

Write-Host "Test completed. Check your Android device for status change to 'Connected to...'."
