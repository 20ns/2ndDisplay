import subprocess
import sys
import time

print("TabDisplay System Test")
print("=" * 50)

print("\n1. Checking host application status...")
try:
    # Check if TabDisplay.exe is running
    result = subprocess.run(
        ["tasklist", "/FI", "IMAGENAME eq TabDisplay.exe"],
        capture_output=True,
        text=True,
        shell=True
    )
    
    if "TabDisplay.exe" in result.stdout:
        print("✅ Host application (TabDisplay.exe) is running")
    else:
        print("❌ Host application is not running")
        print("Please start the host application first:")
        print("   cd host\\build\\vs2022-release")
        print("   .\\TabDisplay.exe")
        sys.exit(1)
        
except Exception as e:
    print(f"❌ Error checking host status: {e}")

print("\n2. Checking network interfaces...")
try:
    # Check for potential USB tethering interfaces
    result = subprocess.run(
        ["powershell", "-Command", "Get-NetIPConfiguration | Where-Object { $_.InterfaceAlias -like '*USB*' -or $_.InterfaceAlias -like '*RNDIS*' -or $_.InterfaceAlias -like '*Android*' -or $_.IPv4Address -like '192.168.42.*' -or $_.IPv4Address -like '192.168.43.*' } | Format-Table InterfaceAlias, IPv4Address -AutoSize"],
        capture_output=True,
        text=True
    )
    
    if result.stdout.strip():
        print("✅ Potential Android USB interfaces found:")
        print(result.stdout)
    else:
        print("⚠️ No obvious Android USB tethering interfaces detected")
        print("Please ensure:")
        print("   - Android device is connected via USB")
        print("   - USB tethering is enabled on Android device")
        print("   - Android device appears in network interfaces")
        
except Exception as e:
    print(f"❌ Error checking network interfaces: {e}")

print("\n3. Checking all network interfaces...")
try:
    result = subprocess.run(
        ["powershell", "-Command", "Get-NetIPConfiguration | Format-Table InterfaceAlias, IPv4Address -AutoSize"],
        capture_output=True,
        text=True
    )
    print("All network interfaces:")
    print(result.stdout)
except Exception as e:
    print(f"❌ Error listing network interfaces: {e}")

print("\n4. Instructions for manual testing:")
print("─" * 50)
print("To test the connection:")
print("1. Look for the TabDisplay icon in your system tray (bottom-right)")
print("2. Right-click the TabDisplay tray icon")
print("3. Select 'Connect to Android'")
print("4. Check the host\\TabDisplay.log file for detailed logging")
print("5. If successful, you should see discovery and connection messages")

print("\n5. To check logs in real-time:")
print("   tail -f host\\TabDisplay.log")
print("   (or open the file and refresh it)")

print("\nTest complete!")
