package com.example.secondscreen

import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.Canvas
import android.media.MediaCodec
import android.media.MediaFormat
import android.os.Handler
import android.os.HandlerThread
import android.util.Log
import android.view.Surface
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.nio.ByteBuffer

class Decoder(private val context: Context) {
    private var mediaCodec: MediaCodec? = null
    private var handlerThread: HandlerThread? = null
    private var handler: Handler? = null
    private var surface: Surface? = null
    private var canvas: Canvas? = null
    private var isH264Mode: Boolean = true
    private var bitmap: Bitmap? = null

    private var width: Int = 1920
    private var height: Int = 1080
    private var frameRate: Int = 60
    private var frameCount: Int = 0

    companion object {
        private const val TAG = "TabDisplay_Decoder"
        private const val DEQUEUE_TIMEOUT = 0L // Non-blocking
        private const val EXPECTED_RAW_FRAME_SIZE_1080p = 8294400 // 1920*1080*4 bytes (BGRA)
    }

    fun configureSurface(surface: Surface, width: Int, height: Int, frameRate: Int) {
        Log.d(TAG, "Configuring surface: ${width}x${height}@${frameRate}fps")
        
        // Release previous codec if exists
        release()

        this.surface = surface
        this@Decoder.width = width
        this@Decoder.height = height
        this@Decoder.frameRate = frameRate
        this.isH264Mode = true // Start with H.264 mode

        try {
            // Create a new codec for H.264
            mediaCodec = MediaCodec.createDecoderByType(MediaFormat.MIMETYPE_VIDEO_AVC)

            // Configure the media format
            val format = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, width, height).apply {
                setInteger(MediaFormat.KEY_COLOR_FORMAT, android.media.MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface)
                setInteger(MediaFormat.KEY_FRAME_RATE, frameRate)
                setInteger(MediaFormat.KEY_LOW_LATENCY, 1)
                setInteger(MediaFormat.KEY_PRIORITY, 0) // Real-time priority
                setInteger(MediaFormat.KEY_OPERATING_RATE, frameRate * 2)
            }

            // Create handler thread for codec callbacks (needed for API < 23)
            handlerThread = HandlerThread("DecoderCallbackThread").apply { start() }
            handler = Handler(handlerThread!!.looper)

            // Configure the codec
            mediaCodec?.configure(format, surface, null, 0)

            // Configure surface frame rate
            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R) {
                surface.setFrameRate(frameRate.toFloat(), Surface.FRAME_RATE_COMPATIBILITY_FIXED_SOURCE)
            }

            // Start the codec
            mediaCodec?.start()
            Log.d(TAG, "H.264 decoder configured and started successfully")
        } catch (e: Exception) {
            Log.w(TAG, "Failed to configure H.264 decoder: ${e.message}")
            // If H.264 fails, prepare for raw frame mode
            setupRawFrameMode()
        }
    }

    suspend fun decodeFrame(frameData: ByteArray, isKeyFrame: Boolean) {
        try {
            // Check if this looks like raw frame data (large size and no H.264 markers)
            val isLargeFrame = frameData.size >= EXPECTED_RAW_FRAME_SIZE_1080p - 100000 // Allow some tolerance
            val isH264 = isH264Frame(frameData)
            
            Log.d(TAG, "Frame analysis: size=${frameData.size}, isLarge=$isLargeFrame, isH264=$isH264, currentMode=${if(isH264Mode) "H264" else "RAW"}")
            
            if (isLargeFrame && !isH264) {
                Log.d(TAG, "Detected raw frame data (${frameData.size} bytes), switching to raw mode")
                // sendDebugBroadcast("RAW FRAME DETECTED: ${frameData.size} bytes", "DECODE")
                if (isH264Mode) {
                    switchToRawFrameMode()
                }
                handleRawFrame(frameData)
                return
            }

            // Try H.264 decoding
            val codec = mediaCodec
            if (codec == null || !isH264Mode) {
                Log.w(TAG, "No H.264 codec available, trying raw frame handling")
                handleRawFrame(frameData)
                return
            }

            // Get input buffer index
            val inputBufferIndex = codec.dequeueInputBuffer(DEQUEUE_TIMEOUT)
            if (inputBufferIndex >= 0) {
                // Get the input buffer and copy frame data into it
                val inputBuffer = codec.getInputBuffer(inputBufferIndex) ?: return
                inputBuffer.clear()
                inputBuffer.put(frameData)

                // Queue the input buffer
                val flags = if (isKeyFrame) MediaCodec.BUFFER_FLAG_KEY_FRAME else 0
                codec.queueInputBuffer(inputBufferIndex, 0, frameData.size, System.nanoTime() / 1000, flags)
            }

            // Process output buffer
            val bufferInfo = MediaCodec.BufferInfo()
            var outputBufferIndex = codec.dequeueOutputBuffer(bufferInfo, DEQUEUE_TIMEOUT)

            while (outputBufferIndex >= 0) {
                codec.releaseOutputBuffer(outputBufferIndex, true)
                outputBufferIndex = codec.dequeueOutputBuffer(bufferInfo, DEQUEUE_TIMEOUT)
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error in decodeFrame: ${e.message}")
            // If H.264 decoding fails, try raw frame handling as fallback
            if (isH264Mode) {
                Log.w(TAG, "H.264 decoding failed, trying raw frame fallback")
                handleRawFrame(frameData)
            }
        }
    }

    fun releaseSurface() {
        try {
            mediaCodec?.stop()
            mediaCodec?.release()
            mediaCodec = null

            handlerThread?.quitSafely()
            handlerThread = null
            handler = null
        } catch (e: Exception) {
            // Handle release errors
        }
    }

    fun release() {
        releaseSurface()
    }

    private fun isH264Frame(data: ByteArray): Boolean {
        // Check for H.264 NAL unit start codes
        if (data.size < 4) return false
        return (data[0] == 0x00.toByte() && data[1] == 0x00.toByte() && 
                data[2] == 0x00.toByte() && data[3] == 0x01.toByte()) ||
               (data[0] == 0x00.toByte() && data[1] == 0x00.toByte() && data[2] == 0x01.toByte())
    }

    private fun setupRawFrameMode() {
        Log.d(TAG, "Setting up raw frame mode for ${width}x${height}")
        isH264Mode = false
        
        // Create bitmap for raw frame drawing
        bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
        Log.d(TAG, "Created bitmap: ${width}x${height}")
        
        // Don't lock canvas here - do it per frame
        Log.d(TAG, "Raw frame mode setup complete")
    }

    private fun switchToRawFrameMode() {
        Log.d(TAG, "Switching from H.264 to raw frame mode")
        
        // Stop and release H.264 codec
        try {
            mediaCodec?.stop()
            mediaCodec?.release()
            mediaCodec = null
        } catch (e: Exception) {
            Log.w(TAG, "Error releasing H.264 codec: ${e.message}")
        }
        
        setupRawFrameMode()
    }

    private suspend fun handleRawFrame(frameData: ByteArray) = withContext(Dispatchers.Default) {
        try {
            frameCount++
            Log.d(TAG, "Handling raw frame #$frameCount: ${frameData.size} bytes for ${width}x${height}")
            // sendDebugBroadcast("Processing RAW frame #$frameCount", "DECODE")
            
            val currentBitmap = bitmap ?: run {
                Log.e(TAG, "No bitmap available for raw frame, creating new one")
                // sendDebugBroadcast("Creating new bitmap ${width}x${height}", "DECODE")
                bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
                bitmap!!
            }
            
            // Convert BGRA raw data to ARGB bitmap
            val expectedSize = width * height * 4
            if (frameData.size < expectedSize) {
                Log.w(TAG, "Frame data too small: ${frameData.size} < $expectedSize")
                return@withContext
            }
            
            Log.d(TAG, "Converting ${frameData.size} bytes of BGRA data to ARGB bitmap")
            
            // Convert BGRA to ARGB more efficiently
            val argbData = IntArray(width * height)
            var frameIndex = 0
            
            for (i in argbData.indices) {
                val b = frameData[frameIndex++].toInt() and 0xFF
                val g = frameData[frameIndex++].toInt() and 0xFF
                val r = frameData[frameIndex++].toInt() and 0xFF
                val a = frameData[frameIndex++].toInt() and 0xFF
                argbData[i] = (a shl 24) or (r shl 16) or (g shl 8) or b
            }
            
            // Set pixels to bitmap
            currentBitmap.setPixels(argbData, 0, width, 0, 0, width, height)
            Log.d(TAG, "Bitmap pixels set successfully")
            
            // Draw to surface on main thread
            withContext(Dispatchers.Main) {
                surface?.let { surf ->
                    try {
                        val canvas = surf.lockCanvas(null)
                        if (canvas != null) {
                            // Clear the canvas first
                            canvas.drawColor(android.graphics.Color.BLACK)
                            // Draw the bitmap
                            canvas.drawBitmap(currentBitmap, 0f, 0f, null)
                            surf.unlockCanvasAndPost(canvas)
                            Log.d(TAG, "Raw frame #$frameCount drawn to surface successfully")
                            // sendDebugBroadcast("Frame #$frameCount drawn to surface!", "SUCCESS")
                        } else {
                            Log.e(TAG, "Failed to lock canvas - canvas is null")
                            // sendDebugBroadcast("ERROR: Canvas is null", "ERROR")
                        }
                    } catch (e: Exception) {
                        Log.e(TAG, "Error drawing raw frame to surface: ${e.message}", e)
                        // sendDebugBroadcast("ERROR drawing to surface: ${e.message}", "ERROR")
                    }
                } ?: run {
                    Log.e(TAG, "Surface is null, cannot draw frame")
                    // sendDebugBroadcast("ERROR: Surface is null", "ERROR")
                }
            }
            
        } catch (e: Exception) {
            Log.e(TAG, "Error handling raw frame: ${e.message}", e)
            // sendDebugBroadcast("ERROR in handleRawFrame: ${e.message}", "ERROR")
        }
    }
    
    private fun sendDebugBroadcast(message: String, level: String = "INFO") {
        try {
            val intent = Intent("com.example.secondscreen.DEBUG_UPDATE").apply {
                putExtra("debug_message", message)
                putExtra("debug_level", level)
            }
            context.sendBroadcast(intent)
        } catch (e: Exception) {
            Log.e(TAG, "Failed to send debug broadcast: ${e.message}")
        }
    }
}
