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
import android.view.Surface
import androidx.core.app.NotificationCompat
import kotlinx.coroutines.*
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.flow.*
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetSocketAddress

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

    private val packetChannel = Channel<ByteArray>(Channel.UNLIMITED)

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()

        // Create and start foreground notification
        createNotificationChannel()
        startForeground(NOTIFICATION_ID, createNotification())

        // Initialize the components
        packetAssembler = PacketAssembler()
        decoder = Decoder()
        touchSender = TouchSender()

        // Start network receiver and processing pipeline
        startNetworkReceiver()
        startProcessingPipeline()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent == null) {
            return START_STICKY
        }

        when (intent.action) {
            ACTION_START -> {
                // Already started in onCreate, nothing more to do
            }
            ACTION_STOP -> {
                stopSelf()
            }
            ACTION_CHANGE_MODE -> {
                // Handle mode change from settings
                val newMode = intent.getStringExtra("mode") ?: return START_STICKY
                handleModeChange(newMode)
            }
            ACTION_SURFACE_CREATED -> {
                surface = intent.getParcelableExtra(EXTRA_SURFACE)
                surface?.let {
                    decoder?.configureSurface(it, currentWidth, currentHeight, currentFps)
                }
            }
            ACTION_SURFACE_DESTROYED -> {
                surface = null
                decoder?.releaseSurface()
            }
            ACTION_TOUCH_EVENT -> {
                val pointerId = intent.getIntExtra("pointerId", 0)
                val normalizedX = intent.getFloatExtra("normalizedX", 0f)
                val normalizedY = intent.getFloatExtra("normalizedY", 0f)
                val action = intent.getStringExtra("action") ?: "move"

                // Convert normalized coordinates to actual frame coordinates
                val x = (normalizedX * currentWidth).toInt()
                val y = (normalizedY * currentHeight).toInt()

                touchSender?.sendTouchEvent(pointerId, x, y, action)
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
        updateNotification()
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
                udpSocket = DatagramSocket(null).apply {
                    reuseAddress = true
                    bind(InetSocketAddress(UDP_PORT))
                }

                val buffer = ByteArray(MAX_PACKET_SIZE)
                val packet = DatagramPacket(buffer, buffer.size)

                while (isActive) {
                    try {
                        udpSocket?.receive(packet)

                        // Copy the data to avoid buffer reuse issues
                        val data = packet.data.copyOfRange(0, packet.length)

                        // Store remote address for the touch sender
                        touchSender?.setRemoteAddress(packet.address, packet.port)

                        // Send to processing channel
                        packetChannel.send(data)
                    } catch (e: Exception) {
                        // Log and continue
                    }
                }
            } catch (e: Exception) {
                // Handle startup errors
            }
        }
    }

    private fun startProcessingPipeline() {
        serviceScope.launch {
            try {
                for (packetData in packetChannel) {
                    if (packetData.size < HEADER_SIZE) {
                        continue // Ignore too small packets
                    }

                    // Parse header
                    val header = parseHeader(packetData)

                    // Check for control packet
                    if ((header.flags and FLAG_CONTROL_PACKET) != 0) {
                        // Handle control packet (JSON)
                        val jsonStr = packetData.slice(HEADER_SIZE until packetData.size)
                            .toByteArray().toString(Charsets.UTF_8)
                        handleControlPacket(jsonStr)
                        continue
                    }

                    // Extract payload
                    val payload = packetData.slice(HEADER_SIZE until packetData.size).toByteArray()

                    // Send to packet assembler
                    packetAssembler?.addPacket(header, payload)

                    // Try to get a complete frame
                    packetAssembler?.getNextCompleteFrame()?.let { frameData ->
                        // Send to decoder
                        decoder?.decodeFrame(frameData, (header.flags and FLAG_KEYFRAME) != 0)

                        // Update notification periodically
                        if (header.frameId.toInt() % 60 == 0) {
                            updateNotification()
                        }
                    }
                }
            } catch (e: Exception) {
                // Handle processing errors
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
                    ((data[11].toInt() and 0xFF) shl 24)).toInt()
        )
    }

    private fun handleControlPacket(jsonStr: String) {
        try {
            // Use gson to parse the JSON control packet
            val controlPacket = com.google.gson.Gson().fromJson(jsonStr, ControlPacket::class.java)

            // Update parameters based on control packet
            controlPacket?.let {
                if (it.type == "keepalive" &&
                    (it.width != null && it.height != null && it.fps != null)) {

                    // Check if resolution or fps changed
                    val resolutionChanged = it.width != currentWidth || it.height != currentHeight
                    val fpsChanged = it.fps != currentFps

                    // Update stored values
                    currentWidth = it.width
                    currentHeight = it.height
                    currentFps = it.fps
                    it.bitrate?.let { newBitrate -> currentBitrate = newBitrate }

                    // Configure decoder with new parameters if needed
                    if (resolutionChanged || fpsChanged) {
                        surface?.let { surface ->
                            decoder?.configureSurface(surface, currentWidth, currentHeight, currentFps)
                        }
                    }

                    // Update notification
                    updateNotification()
                }
            }
        } catch (e: Exception) {
            // Handle JSON parsing errors
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

    private fun createNotification(): Notification {
        val notificationIntent = Intent(this, MainActivity::class.java)
        val pendingIntent = PendingIntent.getActivity(
            this, 0, notificationIntent,
            PendingIntent.FLAG_IMMUTABLE
        )

        // Open settings on long-press
        val settingsIntent = Intent(this, SettingsFragment::class.java)
        val settingsPendingIntent = PendingIntent.getActivity(
            this, 1, settingsIntent,
            PendingIntent.FLAG_IMMUTABLE
        )

        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle(getString(R.string.notification_title))
            .setContentText("${currentWidth}×${currentHeight} @ ${currentFps}Hz (${currentBitrate} Mbps)")
            .setSmallIcon(R.drawable.ic_launcher)
            .setContentIntent(pendingIntent)
            .setOngoing(true)
            .addAction(R.drawable.ic_launcher, getString(R.string.settings), settingsPendingIntent)
            .build()
    }

    private fun updateNotification() {
        val notificationManager = getSystemService(NotificationManager::class.java)
        notificationManager?.notify(NOTIFICATION_ID, createNotification())
    }

    data class ControlPacket(
        val type: String? = null,
        val width: Int? = null,
        val height: Int? = null,
        val fps: Int? = null,
        val bitrate: Int? = null
    )

    data class PacketHeader(
        val sequenceId: UShort,
        val frameId: UShort,
        val totalChunks: UShort,
        val chunkIndex: UShort,
        val flags: Int
    )

    companion object {
        const val ACTION_START = "com.example.secondscreen.ACTION_START"
        const val ACTION_STOP = "com.example.secondscreen.ACTION_STOP"
        const val ACTION_SURFACE_CREATED = "com.example.secondscreen.ACTION_SURFACE_CREATED"
        const val ACTION_SURFACE_DESTROYED = "com.example.secondscreen.ACTION_SURFACE_DESTROYED"
        const val ACTION_TOUCH_EVENT = "com.example.secondscreen.ACTION_TOUCH_EVENT"
        const val ACTION_CHANGE_MODE = "com.example.secondscreen.ACTION_CHANGE_MODE"

        const val EXTRA_SURFACE = "surface"

        const val CHANNEL_ID = "SecondScreenDisplayChannel"
        const val NOTIFICATION_ID = 1001

        const val UDP_PORT = 5004
        const val MAX_PACKET_SIZE = 1500 // slightly larger than expected to handle any overhead
        const val HEADER_SIZE = 12

        const val FLAG_KEYFRAME = 0x01
        const val FLAG_INPUT_PACKET = 0x02
        const val FLAG_CONTROL_PACKET = 0x04
    }
}
