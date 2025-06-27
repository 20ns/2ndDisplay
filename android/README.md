# Second Screen Android Client

This Android application implements a second-screen display client that receives H.264 video from a Windows host and returns touch events.

## Setup Instructions

### Enable USB Tethering

1. Connect your Android device to the Windows host via USB
2. On your Android device, go to **Settings > Network & Internet > Hotspot & tethering**
3. Enable **USB tethering**
4. Windows should recognize a new network connection
5. Set the Windows network interface IP address to `192.168.42.1` with subnet mask `255.255.255.0`

### Enable USB Debugging (for development)

1. Go to **Settings > About phone**
2. Tap **Build number** 7 times to enable developer options
3. Go to **Settings > System > Developer options**
4. Enable **USB debugging**
5. Connect your device to the computer and accept the debugging prompt

## Using the App

1. Start the app on your Android device
2. Accept the prompt to allow the device to act as a wired display
3. Run the host application on your Windows computer
4. The tablet should automatically connect and begin displaying the Windows screen
5. Touch interactions on the tablet will be sent back to Windows

## Display Modes

The app supports three display modes:
- 1920×1080 @ 60Hz (Full HD) - scales to fit screen
- 1752×2800 @ 60Hz (Native resolution)
- 1752×2800 @ 120Hz (requires compatible display)

You can change the display mode by long-pressing on the notification and selecting "Settings".

## Troubleshooting

- **No image appears**: Make sure USB tethering is enabled and Windows host is running
- **Image freezes**: Network congestion may be causing packet loss, try lower resolution mode
- **High latency**: Check that your device supports hardware acceleration for H.264 decoding
