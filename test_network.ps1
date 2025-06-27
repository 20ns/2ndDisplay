# Test what network interfaces might be detected as Android devices
Write-Host "TabDisplay USB Discovery Network Analysis" -ForegroundColor Green
Write-Host "=" * 50

Write-Host "`nNetwork Interface Analysis:" -ForegroundColor Yellow

# Get all network adapters with details
$adapters = Get-NetAdapter | Where-Object { $_.Status -eq 'Up' }

foreach ($adapter in $adapters) {
    Write-Host "`nAdapter: $($adapter.Name)" -ForegroundColor Cyan
    Write-Host "  Description: $($adapter.InterfaceDescription)"
    Write-Host "  Status: $($adapter.Status)"
    Write-Host "  Link Speed: $($adapter.LinkSpeed)"
    
    # Check for Android-like keywords
    $description = $adapter.InterfaceDescription.ToLower()
    $isAndroidLike = $false
    
    $androidKeywords = @("android", "adb", "rndis", "remote ndis", "usb ethernet", "samsung", "galaxy", "pixel", "nexus")
    foreach ($keyword in $androidKeywords) {
        if ($description.Contains($keyword)) {
            Write-Host "  ✅ Contains Android keyword: $keyword" -ForegroundColor Green
            $isAndroidLike = $true
        }
    }
    
    # Check for secondary ethernet (our fallback logic)
    if ($adapter.Name.ToLower().Contains("ethernet") -and ($adapter.Name.Contains("2") -or $description.Contains("usb"))) {
        Write-Host "  ⚠️  Looks like secondary ethernet interface (possibly USB)" -ForegroundColor Yellow
        $isAndroidLike = $true
    }
    
    if (-not $isAndroidLike) {
        Write-Host "  ❌ Does not match Android patterns" -ForegroundColor Red
        continue
    }
    
    # Get IP configuration for this adapter
    $ipConfig = Get-NetIPConfiguration -InterfaceIndex $adapter.ifIndex -ErrorAction SilentlyContinue
    if ($ipConfig -and $ipConfig.IPv4Address) {
        foreach ($ip in $ipConfig.IPv4Address) {
            Write-Host "  IP Address: $($ip.IPAddress)" -ForegroundColor White
            
            # Check if it's link-local (169.254.x.x)
            if ($ip.IPAddress.StartsWith("169.254")) {
                Write-Host "    → Link-local address detected" -ForegroundColor Magenta
                
                # Show potential gateway IPs we would test
                $testIPs = @(
                    "169.254.1.1",
                    "169.254.208.1", # Based on the 169.254.208.21 we saw
                    "169.254.208.129"
                )
                
                Write-Host "    → Would test these potential Android IPs:" -ForegroundColor Magenta
                foreach ($testIP in $testIPs) {
                    Write-Host "      - $testIP"
                }
            } elseif (-not $ip.IPAddress.StartsWith("127.0.0")) {
                Write-Host "    → Regular IP, would test for gateway" -ForegroundColor White
            }
        }
    } else {
        Write-Host "  No IP configuration found" -ForegroundColor Red
    }
}

Write-Host "`n" + "=" * 50
Write-Host "Recommendation:" -ForegroundColor Green

$ethernet2 = Get-NetAdapter | Where-Object { $_.Name -eq "Ethernet 2" }
if ($ethernet2) {
    Write-Host "✅ 'Ethernet 2' interface detected - this might be a USB connection"
    Write-Host "   Our improved discovery should now handle this interface"
    Write-Host "   Try right-clicking the TabDisplay tray icon and selecting 'Connect to Android'"
} else {
    Write-Host "⚠️  No obvious USB/Android interfaces detected"
    Write-Host "   Make sure Android device is connected and USB tethering is enabled"
}

Write-Host "`nTo test connection:" -ForegroundColor Yellow
Write-Host "1. Look for TabDisplay icon in system tray"
Write-Host "2. Right-click → 'Connect to Android'"  
Write-Host "3. Check the log file: host\TabDisplay.log"
