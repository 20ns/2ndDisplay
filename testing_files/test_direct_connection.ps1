# Test direct UDP connection to Android device
$AndroidIP = "192.168.238.161"
$AndroidPort = 5004

Write-Host "Testing direct UDP connection to Android device at ${AndroidIP}:${AndroidPort}"

# Create UDP client
$client = New-Object System.Net.Sockets.UdpClient

try {
    # Create a simple test packet (control packet)
    $message = [System.Text.Encoding]::UTF8.GetBytes("CONTROL_TEST")
    $bytes = $client.Send($message, $message.Length, $AndroidIP, $AndroidPort)
    Write-Host "Sent $bytes bytes to $AndroidIP`:$AndroidPort"
    Write-Host "If the Android app shows any status change, the connection is working!"
    
    # Wait a moment to see if anything happens
    Start-Sleep -Seconds 2
    
} catch {
    Write-Host "Error sending UDP packet: $($_.Exception.Message)"
} finally {
    $client.Close()
}

Write-Host "Test completed. Check your Android device for any status changes."
