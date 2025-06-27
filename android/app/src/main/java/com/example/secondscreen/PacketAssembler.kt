package com.example.secondscreen

import java.nio.ByteBuffer
import java.util.concurrent.ConcurrentHashMap

class PacketAssembler {
    // Store packet fragments for each frame
    private val frameBuffer = ConcurrentHashMap<UShort, FrameData>()

    // Track sequence numbers to avoid processing duplicates
    private val processedSequences = HashSet<UShort>()

    // Maximum frames to keep in buffer at once
    private val maxBufferedFrames = 5

    /**
     * Add a packet to the assembler for processing
     */
    fun addPacket(header: VideoReceiverService.Companion.PacketHeader, payload: ByteArray) {
        // Skip already processed packets
        if (processedSequences.contains(header.sequenceId)) {
            return
        }

        // Mark as processed
        processedSequences.add(header.sequenceId)

        // Keep sequence set from growing too large
        if (processedSequences.size > 1000) {
            processedSequences.clear()
        }

        // Get or create frame data
        val frame = frameBuffer.computeIfAbsent(header.frameId) {
            FrameData(header.totalChunks.toInt())
        }

        // Add chunk to the appropriate position
        val chunkIndex = header.chunkIndex.toInt()

        // Check if this is a parity packet (they have chunkIndex starting at 0xF000)
        if (chunkIndex >= 0xF000) {
            val parityIndex = chunkIndex - 0xF000
            if (parityIndex <= 1) { // We expect only 2 parity packets (0 and 1)
                frame.parityChunks[parityIndex] = payload
            }
        } else {
            // Regular data packet
            if (chunkIndex < frame.chunks.size) {
                frame.chunks[chunkIndex] = payload
                frame.receivedCount++
            }
        }

        // Mark if this is a keyframe
        if ((header.flags and VideoReceiverService.FLAG_KEYFRAME) != 0) {
            frame.isKeyFrame = true
        }

        // Clean up old frames
        cleanupOldFrames(header.frameId)
    }

    /**
     * Get the next complete frame if available
     */
    fun getNextCompleteFrame(): ByteArray? {
        // Find complete frames or frames that can be repaired
        val completeFrame = frameBuffer.entries
            .sortedBy { it.key.toInt() }
            .firstOrNull { (_, frame) ->
                frame.isComplete() || frame.canBeRepaired()
            }

        if (completeFrame != null) {
            val frameId = completeFrame.key
            val frame = completeFrame.value

            // If frame is already complete, just assemble it
            val assembled = if (frame.isComplete()) {
                assembleFrame(frame)
            } else {
                // Otherwise try to repair it first
                repairAndAssembleFrame(frame)
            }

            // Remove from buffer after processing
            frameBuffer.remove(frameId)

            return assembled
        }

        return null
    }

    /**
     * Clean up old frames to prevent buffer from growing too large
     */
    private fun cleanupOldFrames(currentFrameId: UShort) {
        if (frameBuffer.size > maxBufferedFrames) {
            // Get the oldest frames
            val oldestFrames = frameBuffer.keys
                .sortedBy { it.toInt() }
                .take(frameBuffer.size - maxBufferedFrames)

            // Remove them
            oldestFrames.forEach { frameBuffer.remove(it) }
        }
    }

    /**
     * Assemble a complete frame from chunks
     */
    private fun assembleFrame(frame: FrameData): ByteArray {
        // Calculate total size
        val totalSize = frame.chunks.sumOf { it?.size ?: 0 }

        // Create buffer for the complete frame
        val buffer = ByteBuffer.allocate(totalSize)

        // Add all chunks
        for (chunk in frame.chunks) {
            if (chunk != null) {
                buffer.put(chunk)
            }
        }

        return buffer.array()
    }

    /**
     * Try to repair and assemble a frame using FEC
     */
    private fun repairAndAssembleFrame(frame: FrameData): ByteArray? {
        // Check if we have both parity chunks
        if (frame.parityChunks[0] == null || frame.parityChunks[1] == null) {
            return null
        }

        // Check if we can repair (only one missing chunk per parity group)
        val missingEvenIndices = getMissingIndices(frame, isEven = true)
        val missingOddIndices = getMissingIndices(frame, isEven = false)

        if (missingEvenIndices.size == 1 && missingOddIndices.size <= 1) {
            // Repair the single missing even chunk
            repairChunk(frame, missingEvenIndices[0], isEven = true)

            // Repair missing odd chunk if there is exactly one
            if (missingOddIndices.size == 1) {
                repairChunk(frame, missingOddIndices[0], isEven = false)
            }

            // Now assemble the frame with the repaired chunks
            return assembleFrame(frame)
        }

        return null
    }

    /**
     * Get indices of missing chunks in either the even or odd group
     */
    private fun getMissingIndices(frame: FrameData, isEven: Boolean): List<Int> {
        return frame.chunks.indices
            .filter { index -> (index % 2 == 0) == isEven && frame.chunks[index] == null }
    }

    /**
     * Repair a single chunk using its parity data
     */
    private fun repairChunk(frame: FrameData, missingIndex: Int, isEven: Boolean) {
        // Get the appropriate parity chunk
        val parityChunk = if (isEven) frame.parityChunks[0] else frame.parityChunks[1]
        if (parityChunk == null) return

        // Find the maximum chunk size
        val maxChunkSize = frame.chunks
            .filterNotNull()
            .maxOfOrNull { it.size } ?: return

        // Create a buffer for the repaired chunk
        val repairedChunk = ByteArray(maxChunkSize)

        // Initialize with the parity chunk data
        parityChunk.copyInto(
            repairedChunk,
            0,
            0,
            minOf(parityChunk.size, maxChunkSize)
        )

        // XOR with all other chunks in the same group (even or odd)
        for (i in frame.chunks.indices) {
            if (i != missingIndex && (i % 2 == 0) == isEven) {
                val chunk = frame.chunks[i] ?: continue

                // XOR byte by byte
                for (j in 0 until minOf(chunk.size, repairedChunk.size)) {
                    repairedChunk[j] = (repairedChunk[j].toInt() xor chunk[j].toInt()).toByte()
                }
            }
        }

        // Set the repaired chunk
        frame.chunks[missingIndex] = repairedChunk
        frame.receivedCount++
    }

    /**
     * Class to hold chunks for a single frame
     */
    inner class FrameData(totalChunks: Int) {
        val chunks: Array<ByteArray?> = arrayOfNulls(totalChunks)
        val parityChunks: Array<ByteArray?> = arrayOfNulls(2) // Even and odd parity
        var receivedCount: Int = 0
        var isKeyFrame: Boolean = false

        fun isComplete(): Boolean {
            return receivedCount == chunks.size
        }

        fun canBeRepaired(): Boolean {
            // Check if we're only missing one even chunk and at most one odd chunk
            // (or vice versa) and we have both parity chunks
            if (parityChunks[0] == null || parityChunks[1] == null) {
                return false
            }

            val missingEvenCount = getMissingCount(isEven = true)
            val missingOddCount = getMissingCount(isEven = false)

            return (missingEvenCount <= 1 && missingOddCount <= 1) &&
                    (missingEvenCount + missingOddCount <= 2)
        }

        private fun getMissingCount(isEven: Boolean): Int {
            return chunks.indices.count { i ->
                (i % 2 == 0) == isEven && chunks[i] == null
            }
        }
    }
}
