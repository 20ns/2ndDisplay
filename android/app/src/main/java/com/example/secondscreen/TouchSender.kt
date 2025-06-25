package com.example.secondscreen

import com.google.gson.Gson
import kotlinx.coroutines.*
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress

class TouchSender {
    private var remoteAddress: InetAddress? = null
    private var remotePort: Int = 0
    private var socket: DatagramSocket? = null
    private val gson = Gson()
    private val scope = CoroutineScope(Dispatchers.IO + SupervisorJob())

    init {
        try {
            socket = DatagramSocket()
        } catch (e: Exception) {
            // Handle socket creation error
        }
    }

    /**
     * Set the remote address for sending touch events
     */
    fun setRemoteAddress(address: InetAddress, port: Int) {
        remoteAddress = address
        remotePort = port
    }

    /**
     * Send a touch event to the host
     */
    fun sendTouchEvent(id: Int, x: Int, y: Int, state: String) {
        val address = remoteAddress ?: return

        scope.launch {
            try {
                // Create touch event JSON
                val touchEvent = TouchEvent(
                    type = "touch",
                    id = id,
                    x = x,
                    y = y,
                    state = state
                )

                // Serialize to JSON
                val jsonData = gson.toJson(touchEvent).toByteArray()

                // Create a UDP packet
                val packet = DatagramPacket(
                    jsonData,
                    jsonData.size,
                    address,
                    remotePort
                )

                // Send the packet
                socket?.send(packet)
            } catch (e: Exception) {
                // Handle send error
            }
        }
    }

    /**
     * Release resources
     */
    fun release() {
        scope.cancel()
        socket?.close()
        socket = null
    }

    /**
     * Data class for touch events
     */
    data class TouchEvent(
        val type: String,
        val id: Int,
        val x: Int,
        val y: Int,
        val state: String
    )
}
