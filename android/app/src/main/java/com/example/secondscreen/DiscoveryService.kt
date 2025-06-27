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
import androidx.core.app.NotificationCompat
import kotlinx.coroutines.*
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.InetSocketAddress

class DiscoveryService : Service() {
    private val serviceScope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
    private var discoverySocket: DatagramSocket? = null
    
    companion object {
        private const val TAG = "TabDisplay_Discovery"
        const val ACTION_START = "com.example.secondscreen.DISCOVERY_START"
        const val ACTION_STOP = "com.example.secondscreen.DISCOVERY_STOP"
        private const val NOTIFICATION_ID = 1002
        private const val CHANNEL_ID = "discovery_service_channel"
        private const val DISCOVERY_PORT = 45678
        private const val MAX_PACKET_SIZE = 1024
        private const val HELLO_MESSAGE = "HELLO"
        
        // Device identification
        private const val DEVICE_NAME = "Galaxy_Tab_S10+"
        private const val RESPONSE_MESSAGE = "HELLO:$DEVICE_NAME"
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        Log.d(TAG, "DiscoveryService onCreate")

        createNotificationChannel()
        startForeground(NOTIFICATION_ID, createNotification("Discovery service starting..."))
        Log.d(TAG, "Discovery foreground service started")

        startDiscoveryListener()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        Log.d(TAG, "onStartCommand called with action: ${intent?.action}")

        when (intent?.action) {
            ACTION_START -> {
                Log.d(TAG, "ACTION_START received - service already started in onCreate")
                updateNotification("Listening for host discovery...")
            }
            ACTION_STOP -> {
                Log.d(TAG, "ACTION_STOP received - stopping service")
                stopSelf()
            }
        }

        return START_STICKY
    }

    override fun onDestroy() {
        Log.d(TAG, "DiscoveryService onDestroy")
        serviceScope.cancel()
        discoverySocket?.close()
        super.onDestroy()
    }

    private fun startDiscoveryListener() {
        serviceScope.launch {
            try {
                Log.d(TAG, "Starting discovery listener on port $DISCOVERY_PORT")
                discoverySocket = DatagramSocket(null).apply {
                    reuseAddress = true
                    bind(InetSocketAddress(DISCOVERY_PORT))
                }
                Log.d(TAG, "Discovery socket successfully bound to port $DISCOVERY_PORT")
                updateNotification("Listening on port $DISCOVERY_PORT")

                val buffer = ByteArray(MAX_PACKET_SIZE)
                val packet = DatagramPacket(buffer, buffer.size)

                Log.d(TAG, "Starting discovery receive loop...")
                while (isActive) {
                    try {
                        Log.v(TAG, "Waiting for discovery packet...")
                        discoverySocket?.receive(packet)

                        Log.d(TAG, "*** RECEIVED DISCOVERY PACKET ***")
                        Log.d(TAG, "From: ${packet.address}:${packet.port}")
                        Log.d(TAG, "Size: ${packet.length} bytes")

                        // Extract message
                        val message = String(packet.data, 0, packet.length, Charsets.UTF_8).trim()
                        Log.d(TAG, "Discovery message: '$message'")

                        // Check if it's a HELLO message from the host
                        if (message == HELLO_MESSAGE) {
                            Log.d(TAG, "Valid HELLO message received from host: ${packet.address}")
                            sendDiscoveryResponse(packet.address, packet.port)
                            updateNotification("Responded to host: ${packet.address}")
                        } else {
                            Log.w(TAG, "Unknown discovery message: '$message'")
                        }

                    } catch (e: Exception) {
                        Log.e(TAG, "Error receiving discovery packet", e)
                        delay(100) // Brief delay before retrying
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error starting discovery listener", e)
                updateNotification("Discovery error: ${e.message}")
            }
        }
    }

    private fun sendDiscoveryResponse(hostAddress: InetAddress, hostPort: Int) {
        serviceScope.launch {
            try {
                Log.d(TAG, "Sending discovery response to $hostAddress:$hostPort")
                
                // Create response packet
                val responseData = RESPONSE_MESSAGE.toByteArray(Charsets.UTF_8)
                val responsePacket = DatagramPacket(
                    responseData,
                    responseData.size,
                    hostAddress,
                    hostPort
                )

                // Send response using the same socket
                discoverySocket?.send(responsePacket)
                
                Log.d(TAG, "Discovery response sent: '$RESPONSE_MESSAGE'")
                Log.d(TAG, "Response sent to: $hostAddress:$hostPort")
                
            } catch (e: Exception) {
                Log.e(TAG, "Error sending discovery response", e)
            }
        }
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                "Discovery Service Channel",
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "Device Discovery Service"
                lightColor = Color.GREEN
                lockscreenVisibility = Notification.VISIBILITY_PUBLIC
            }

            val notificationManager = getSystemService(NotificationManager::class.java)
            notificationManager.createNotificationChannel(channel)
        }
    }

    private fun createNotification(statusMessage: String = "Listening for host discovery"): Notification {
        val notificationIntent = Intent(this, MainActivity::class.java)
        val pendingIntent = PendingIntent.getActivity(
            this, 0, notificationIntent,
            PendingIntent.FLAG_IMMUTABLE
        )

        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("TabDisplay Discovery")
            .setContentText(statusMessage)
            .setSmallIcon(android.R.drawable.ic_dialog_info)
            .setContentIntent(pendingIntent)
            .setOngoing(true)
            .build()
    }

    private fun updateNotification(statusMessage: String = "Listening for host discovery") {
        val notificationManager = getSystemService(NotificationManager::class.java)
        notificationManager?.notify(NOTIFICATION_ID, createNotification(statusMessage))
        Log.d(TAG, "Discovery notification updated: $statusMessage")
    }
}