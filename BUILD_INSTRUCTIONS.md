# Build Instructions for Updated TabDisplay

## Android APK Build

Since I can't directly execute the Android build in this environment, here are the exact steps to build the updated APK with the DiscoveryService fix:

### **Option 1: Command Line (Recommended)**

Open **Command Prompt** or **PowerShell** as Administrator and run:

```cmd
cd C:\Users\Nav\Desktop\display\android
gradlew.bat assembleDebug
```

### **Option 2: Using Android Studio**

1. Open Android Studio
2. File → Open → Select `C:\Users\Nav\Desktop\display\android`
3. Wait for project sync to complete
4. Build → Generate Signed Bundle/APK → APK → Debug
5. Click Build

### **Output Location**

The built APK will be located at:
```
C:\Users\Nav\Desktop\display\android\app\build\outputs\apk\debug\app-debug.apk
```

### **Install on Device**

Once built, install on your Galaxy Tab S10+ using:

```cmd
adb install "C:\Users\Nav\Desktop\display\android\app\build\outputs\apk\debug\app-debug.apk"
```

Or simply copy the APK file to your tablet and install it manually.

## **What's New in This Build**

The updated APK includes:

✅ **DiscoveryService.kt** - Listens on UDP port 45678 for host discovery
✅ **Enhanced MainActivity.kt** - Starts both video and discovery services  
✅ **Updated AndroidManifest.xml** - Registers the new discovery service

### **Expected Behavior After Install**

1. **Two Notifications**: You'll see both "TabDisplay Service" and "TabDisplay Discovery" notifications
2. **Discovery Ready**: App will respond to host "HELLO" broadcasts
3. **Automatic Connection**: Host should now detect "Galaxy_Tab_S10+" in device list

## **Testing the Complete Fix**

### **Step 1: Install Updated APK**
Follow build instructions above

### **Step 2: Connect USB Tethering**
- Connect Galaxy Tab S10+ to PC via USB
- Enable USB tethering on tablet

### **Step 3: Run Enhanced Host**
- Run the new `TabDisplay.exe` from `host\build\vs2022-debug\Debug\`
- Should automatically start discovery

### **Step 4: Check Connection**
- Right-click tray icon
- Should see "Connect to: Galaxy_Tab_S10+" option
- Click to connect and start video streaming

## **Troubleshooting Build Issues**

### **Java Not Found**
If you get "JAVA_HOME not set" error:
1. Install Android Studio (includes JDK)
2. Or install JDK 17+ separately
3. Set JAVA_HOME environment variable

### **Gradle Build Failed**
- Open project in Android Studio first
- Let it download dependencies and sync
- Try build again

### **ADB Not Found**
- Install Android SDK Platform Tools
- Add to PATH: `C:\Users\Nav\AppData\Local\Android\Sdk\platform-tools`

## **Alternative: Use Android Studio**

If command line build fails:
1. Open Android Studio
2. Open the android project folder
3. Build → Make Project
4. APK will be generated automatically

The updated APK should resolve the discovery issue and enable proper host-to-tablet connection!