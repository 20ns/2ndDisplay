package com.example.secondscreen

import android.media.MediaCodec
import android.media.MediaFormat
import android.os.Handler
import android.os.HandlerThread
import android.view.Surface

class Decoder {
    private var mediaCodec: MediaCodec? = null
    private var handlerThread: HandlerThread? = null
    private var handler: Handler? = null

    private var width: Int = 1920
    private var height: Int = 1080
    private var frameRate: Int = 60

    fun configureSurface(surface: Surface, width: Int, height: Int, frameRate: Int) {
        // Release previous codec if exists
        release()

        this@Decoder.width = width
        this@Decoder.height = height
        this@Decoder.frameRate = frameRate

        try {
            // Create a new codec
            mediaCodec = MediaCodec.createDecoderByType(MediaFormat.MIMETYPE_VIDEO_AVC)

            // Configure the media format
            val format = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, width, height).apply {
                // For surface decoding we don't need to set a specific color format, but some devices require it.
                // MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface is the correct constant.
                setInteger(MediaFormat.KEY_COLOR_FORMAT, android.media.MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface)
                setInteger(MediaFormat.KEY_FRAME_RATE, frameRate)
                setInteger(MediaFormat.KEY_LOW_LATENCY, 1)
                setInteger(MediaFormat.KEY_PRIORITY, 0) // Real-time priority

                // Set operating rate to high value for low-latency decoding
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
        } catch (e: Exception) {
            // Log error and clean up
            release()
        }
    }

    suspend fun decodeFrame(frameData: ByteArray, isKeyFrame: Boolean) {
        try {
            val codec = mediaCodec ?: return

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
            // Handle decode errors
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

    companion object {
        private const val DEQUEUE_TIMEOUT = 0L // Non-blocking
    }
}
