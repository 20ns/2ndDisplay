package com.example.secondscreen

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.graphics.Color
import android.os.Build
import android.os.IBinder
import android.util.Log
import android.view.Surface
import androidx.core.app.NotificationCompat
import kotlinx.coroutines.*
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.flow.*
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.NetworkInterface
import com.google.gson.Gson

class VideoReceiverService : Service() {
    private val serviceScope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
    private var udpSocket: DatagramSocket? = null

    private var decoder: Decoder? = null
    private var packetAssembler: PacketAssembler? = null
    private var touchSender: TouchSender? = null

    // Add display mode parameters
    private var currentMode: String = "1080p_60hz"
    private var surface: Surface? = null
    private var currentWidth = 1920
    private var currentHeight = 1080
    private var currentFps = 60
    private var currentBitrate = 30 // Mbps

    // Store current remote address
    private var currentRemoteAddress: InetAddress? = null

    private val packetChannel = Channel<ByteArray>(Channel.UNLIMITED)

    companion object {
        private const val TAG = "TabDisplay_VideoService"
        const val ACTION_START = "com.example.secondscreen.ACTION_START"
        const val ACTION_STOP = "com.example.secondscreen.ACTION_STOP"
        const val ACTION_SURFACE_CREATED = "com.example.secondscreen.ACTION_SURFACE_CREATED"
        const val ACTION_SURFACE_DESTROYED = "com.example.secondscreen.ACTION_SURFACE_DESTROYED"
        const val ACTION_TOUCH_EVENT = "com.example.secondscreen.ACTION_TOUCH_EVENT"
        const val ACTION_CHANGE_MODE = "com.example.secondscreen.ACTION_CHANGE_MODE"
        const val EXTRA_SURFACE = "surface"
        private const val NOTIFICATION_ID = 1001
        private const val CHANNEL_ID = "video_receiver_channel"
        private const val UDP_PORT = 5004
        private const val MAX_PACKET_SIZE = 1500
        private const val HEADER_SIZE = 12
        const val FLAG_KEYFRAME = 0x01
        private const val FLAG_INPUT_PACKET = 0x02
        private const val FLAG_CONTROL_PACKET = 0x04

        data class PacketHeader(
            val sequenceId: UShort,
            val frameId: UShort,
            val totalChunks: UShort,
            val chunkIndex: UShort,
            val flags: Int
        )
    }

    data class ControlPacket(
        val type: String? = null,
        val width: Int? = null,
        val height: Int? = null,
        val fps: Int? = null,
        val bitrate: Int? = null,
        val touchPort: Int? = null
    )

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        Log.d(TAG, "VideoReceiverService onCreate")

        // Create and start foreground notification
        createNotificationChannel()
        startForeground(NOTIFICATION_ID, createNotification("Service starting..."))
        Log.d(TAG, "Foreground service started")

        // Initialize the components
        packetAssembler = PacketAssembler()
        decoder = Decoder(this)
        touchSender = TouchSender()

        // Start network receiver and processing pipeline
        logNetworkInterfaces()
        startNetworkReceiver()
        startProcessingPipeline()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        Log.d(TAG, "onStartCommand called with action: ${intent?.action}")

        if (intent == null) {
            Log.w(TAG, "Intent is null, returning START_STICKY")
            return START_STICKY
        }

        when (intent.action) {
            ACTION_START -> {
                Log.d(TAG, "ACTION_START received - service already started in onCreate")
                updateNotification("Waiting for connection...")
            }
            ACTION_STOP -> {
                Log.d(TAG, "ACTION_STOP received - stopping service")
                stopSelf()
            }
            ACTION_CHANGE_MODE -> {
                // Handle mode change from settings
                val newMode = intent.getStringExtra("mode") ?: return START_STICKY
                Log.d(TAG, "ACTION_CHANGE_MODE received: $newMode")
                handleModeChange(newMode)
            }
            ACTION_SURFACE_CREATED -> {
                Log.d(TAG, "ACTION_SURFACE_CREATED received")
                surface = intent.getParcelableExtra(EXTRA_SURFACE)
                surface?.let {
                    Log.d(TAG, "Configuring decoder surface: ${currentWidth}x${currentHeight}@${currentFps}fps")
                    decoder?.configureSurface(it, currentWidth, currentHeight, currentFps)
                    updateNotification("Surface ready, waiting for video...")
                } ?: Log.w(TAG, "Surface is null")
            }
            ACTION_SURFACE_DESTROYED -> {
                Log.d(TAG, "ACTION_SURFACE_DESTROYED received")
                surface = null
                decoder?.releaseSurface()
                updateNotification("Surface destroyed")
            }
            ACTION_TOUCH_EVENT -> {
                val pointerId = intent.getIntExtra("pointerId", 0)
                val normalizedX = intent.getFloatExtra("normalizedX", 0f)
                val normalizedY = intent.getFloatExtra("normalizedY", 0f)
                val action = intent.getStringExtra("action") ?: "move"

                // Convert normalized coordinates to actual frame coordinates
                val x = (normalizedX * currentWidth).toInt()
                val y = (normalizedY * currentHeight).toInt()

                Log.v(TAG, "Touch event: $action at ($x, $y) [normalized: ($normalizedX, $normalizedY)]")
                touchSender?.sendTouchEvent(pointerId, x, y, action)
            }
            else -> {
                Log.w(TAG, "Unknown action received: ${intent.action}")
            }
        }

        return START_STICKY
    }

    // Handle manual mode changes from settings
    private fun handleModeChange(newMode: String) {
        if (newMode == currentMode) return

        currentMode = newMode

        // Update dimensions and fps based on selected mode
        when (newMode) {
            "1080p_60hz" -> {
                currentWidth = 1920
                currentHeight = 1080
                currentFps = 60
                currentBitrate = 30
            }
            "tablet_60hz" -> {
                currentWidth = 1752
                currentHeight = 2800
                currentFps = 60
                currentBitrate = 40
            }
            "tablet_120hz" -> {
                currentWidth = 1752
                currentHeight = 2800
                currentFps = 120
                currentBitrate = 40
            }
        }

        // Apply the new mode to the decoder if surface is available
        surface?.let {
            decoder?.configureSurface(it, currentWidth, currentHeight, currentFps)
        }

        // Update notification with new mode info
        updateNotification("Mode: ${currentWidth}×${currentHeight} @ ${currentFps}Hz")
    }

    override fun onDestroy() {
        serviceScope.cancel()

        udpSocket?.close()
        decoder?.release()
        touchSender?.release()

        super.onDestroy()
    }

    private fun startNetworkReceiver() {
        serviceScope.launch {
            try {
                Log.d(TAG, "Starting UDP network receiver on port $UDP_PORT (all interfaces)")
                udpSocket = DatagramSocket(null).apply {
                    reuseAddress = true
                    // Bind to all interfaces (0.0.0.0) instead of specific interface
                    bind(InetSocketAddress("0.0.0.0", UDP_PORT))
                }
                Log.d(TAG, "UDP socket successfully bound to port $UDP_PORT on all interfaces")
                Log.d(TAG, "Socket bound to: ${udpSocket?.localSocketAddress}")
                Log.d(TAG, "Socket info: localPort=${udpSocket?.localPort}, connected=${udpSocket?.isConnected}, closed=${udpSocket?.isClosed}")

                // Add socket timeout to prevent indefinite blocking
                udpSocket?.soTimeout = 5000 // 5 second timeout

                val buffer = ByteArray(MAX_PACKET_SIZE)
                val packet = DatagramPacket(buffer, buffer.size)

                Log.d(TAG, "Starting UDP receive loop with 5 second timeout...")
                var receiveAttempts = 0
                while (isActive) {
                    try {
                        receiveAttempts++
                        Log.d(TAG, "UDP receive attempt #$receiveAttempts - waiting for packet...")
                        udpSocket?.receive(packet)

                        Log.d(TAG, "*** RECEIVED UDP PACKET ***")
                        Log.d(TAG, "From: ${packet.address}:${packet.port}")
                        Log.d(TAG, "Size: ${packet.length} bytes")
                        // sendDebugBroadcast("UDP packet received: ${packet.length} bytes from ${packet.address}", "NET") // Too frequent

                        // Copy the data to avoid buffer reuse issues
                        val data = packet.data.copyOfRange(0, packet.length)
                        Log.d(TAG, "First 20 bytes: ${data.take(20).joinToString(" ") { "%02x".format(it) }}")

                        // Store remote address for the touch sender
                        currentRemoteAddress = packet.address
                        touchSender?.setRemoteAddress(packet.address, packet.port)

                        // Send to processing channel
                        packetChannel.send(data)
                        Log.d(TAG, "Packet sent to processing channel")
                    } catch (e: java.net.SocketTimeoutException) {
                        // Timeout is expected, just log periodically
                        if (receiveAttempts % 12 == 0) { // Every minute (5s * 12)
                            Log.d(TAG, "UDP receive timeout after ${receiveAttempts} attempts - still listening...")
                        }
                    } catch (e: Exception) {
                        Log.e(TAG, "Error receiving packet", e)
                        delay(100) // Brief delay before retrying
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error starting network receiver", e)
            }
        }
    }

    private fun startProcessingPipeline() {
        serviceScope.launch {
            try {
                Log.d(TAG, "Starting packet processing pipeline...")
                // sendDebugBroadcast("Packet processing pipeline started")
                for (packetData in packetChannel) {
                    Log.d(TAG, "Processing packet: ${packetData.size} bytes")
                    // sendDebugBroadcast("Processing packet: ${packetData.size} bytes") // Too frequent
                    
                    if (packetData.size < HEADER_SIZE) {
                        Log.w(TAG, "Packet too small: ${packetData.size} < $HEADER_SIZE")
                        continue // Ignore too small packets
                    }

                    // Parse header
                    val header = parseHeader(packetData)
                    Log.d(TAG, "Parsed header: seqId=${header.sequenceId}, frameId=${header.frameId}, chunk=${header.chunkIndex}/${header.totalChunks}, flags=${header.flags}")

                    // Check for control packet
                    if ((header.flags and FLAG_CONTROL_PACKET) != 0) {
                        Log.d(TAG, "*** CONTROL PACKET DETECTED ***")
                        // Handle control packet (JSON)
                        val jsonStr = packetData.slice(HEADER_SIZE until packetData.size)
                            .toByteArray().toString(Charsets.UTF_8)
                        Log.d(TAG, "Control packet JSON: $jsonStr")
                        handleControlPacket(jsonStr)
                        continue
                    }

                    // Extract payload
                    val payload = packetData.slice(HEADER_SIZE until packetData.size).toByteArray()

                    // Send to packet assembler
                    packetAssembler?.addPacket(header, payload)

                    // Try to get a complete frame
                    packetAssembler?.getNextCompleteFrame()?.let { frameData ->
                        Log.d(TAG, "Complete frame assembled: ${frameData.size} bytes")
                        // sendDebugBroadcast("Frame assembled: ${frameData.size} bytes", "FRAME")
                        
                        // Send to decoder
                        decoder?.decodeFrame(frameData, (header.flags and FLAG_KEYFRAME) != 0)

                        // Update notification periodically
                        if (header.frameId.toInt() % 60 == 0) {
                            updateNotification("Receiving: Frame ${header.frameId}")
                        }
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error in processing pipeline", e)
            }
        }
    }

    private fun parseHeader(data: ByteArray): PacketHeader {
        return PacketHeader(
            sequenceId = ((data[0].toInt() and 0xFF) or
                    ((data[1].toInt() and 0xFF) shl 8)).toUShort(),
            frameId = ((data[2].toInt() and 0xFF) or
                    ((data[3].toInt() and 0xFF) shl 8)).toUShort(),
            totalChunks = ((data[4].toInt() and 0xFF) or
                    ((data[5].toInt() and 0xFF) shl 8)).toUShort(),
            chunkIndex = ((data[6].toInt() and 0xFF) or
                    ((data[7].toInt() and 0xFF) shl 8)).toUShort(),
            flags = ((data[8].toInt() and 0xFF) or
                    ((data[9].toInt() and 0xFF) shl 8) or
                    ((data[10].toInt() and 0xFF) shl 16) or
                    ((data[11].toInt() and 0xFF) shl 24))
        )
    }

    private fun handleControlPacket(jsonStr: String) {
        try {
            Log.d(TAG, "Handling control packet: $jsonStr")
            // Use gson to parse the JSON control packet
            val controlPacket = Gson().fromJson(jsonStr, ControlPacket::class.java)
            Log.d(TAG, "Parsed control packet: $controlPacket")

            // Update parameters based on control packet
            controlPacket?.let {
                Log.d(TAG, "Control packet type: ${it.type}")
                if (it.type == "keepalive" &&
                    (it.width != null && it.height != null && it.fps != null)) {

                    Log.d(TAG, "Keepalive packet with resolution: ${it.width}x${it.height}@${it.fps}fps")

                    // Check if resolution or fps changed
                    val resolutionChanged = it.width != currentWidth || it.height != currentHeight
                    val fpsChanged = it.fps != currentFps

                    // Update stored values
                    currentWidth = it.width
                    currentHeight = it.height
                    currentFps = it.fps
                    it.bitrate?.let { newBitrate -> currentBitrate = newBitrate }

                    // Update touch sender with correct port if provided
                    it.touchPort?.let { touchPort ->
                        Log.d(TAG, "Setting touch port to: $touchPort")
                        currentRemoteAddress?.let { address ->
                            touchSender?.setRemoteAddress(address, touchPort)
                        }
                    }

                    // Configure decoder with new parameters if needed
                    if (resolutionChanged || fpsChanged) {
                        Log.d(TAG, "Resolution/FPS changed, reconfiguring decoder")
                        surface?.let { surface ->
                            decoder?.configureSurface(surface, currentWidth, currentHeight, currentFps)
                        }
                    }

                    // Update notification
                    val connectionMsg = "Connected to ${currentRemoteAddress}"
                    Log.d(TAG, "Updating notification: $connectionMsg")
                    updateNotification(connectionMsg)
                } else {
                    Log.w(TAG, "Invalid control packet - missing required fields")
                }
            } ?: Log.w(TAG, "Failed to parse control packet JSON")
        } catch (e: Exception) {
            Log.e(TAG, "Error handling control packet", e)
        }
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                "Display Service Channel",
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "Second Screen Display Service"
                lightColor = Color.BLUE
                lockscreenVisibility = Notification.VISIBILITY_PUBLIC
            }

            val notificationManager = getSystemService(NotificationManager::class.java)
            notificationManager.createNotificationChannel(channel)
        }
    }

    private fun createNotification(statusMessage: String = "Receiving display data"): Notification {
        val notificationIntent = Intent(this, MainActivity::class.java)
        val pendingIntent = PendingIntent.getActivity(
            this, 0, notificationIntent,
            PendingIntent.FLAG_IMMUTABLE
        )

        // Open settings on long-press
        val settingsIntent = Intent(this, SettingsActivity::class.java)
        val settingsPendingIntent = PendingIntent.getActivity(
            this, 1, settingsIntent,
            PendingIntent.FLAG_IMMUTABLE
        )

        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("TabDisplay Service")
            .setContentText(statusMessage)
            .setSmallIcon(android.R.drawable.ic_dialog_info)
            .setContentIntent(pendingIntent)
            .setOngoing(true)
            .addAction(android.R.drawable.ic_menu_preferences, "Settings", settingsPendingIntent)
            .build()
    }

    private fun updateNotification(statusMessage: String = "Receiving display data") {
        val notificationManager = getSystemService(NotificationManager::class.java)
        notificationManager?.notify(NOTIFICATION_ID, createNotification(statusMessage))
        Log.d(TAG, "Notification updated: $statusMessage")
    }
    
    private fun logNetworkInterfaces() {
        try {
            Log.d(TAG, "=== NETWORK INTERFACES DEBUG ===")
            val interfaces = NetworkInterface.getNetworkInterfaces()
            var interfaceCount = 0
            
            for (networkInterface in interfaces) {
                interfaceCount++
                Log.d(TAG, "Interface #$interfaceCount: ${networkInterface.name}")
                Log.d(TAG, "  Display Name: ${networkInterface.displayName}")
                Log.d(TAG, "  Is Up: ${networkInterface.isUp}")
                Log.d(TAG, "  Is Loopback: ${networkInterface.isLoopback}")
                
                val addresses = networkInterface.inetAddresses
                var addressCount = 0
                for (address in addresses) {
                    addressCount++
                    Log.d(TAG, "    IP #$addressCount: ${address.hostAddress} (${address.javaClass.simpleName})")
                }
                
                if (addressCount == 0) {
                    Log.d(TAG, "    No IP addresses")
                }
            }
            Log.d(TAG, "=== END NETWORK INTERFACES ===")
        } catch (e: Exception) {
            Log.e(TAG, "Error logging network interfaces", e)
        }
    }
    
    private fun sendDebugBroadcast(message: String, level: String = "INFO") {
        try {
            val intent = Intent("com.example.secondscreen.DEBUG_UPDATE").apply {
                putExtra("debug_message", message)
                putExtra("debug_level", level)
            }
            sendBroadcast(intent)
        } catch (e: Exception) {
            Log.e(TAG, "Failed to send debug broadcast: ${e.message}")
        }
    }
}
