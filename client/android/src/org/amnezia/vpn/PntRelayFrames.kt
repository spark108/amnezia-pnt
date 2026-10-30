package org.amnezia.vpn

import java.io.DataInputStream
import java.io.DataOutputStream
import java.io.EOFException
import java.io.IOException
import java.net.SocketTimeoutException
import java.util.concurrent.TimeUnit
import java.util.concurrent.locks.ReentrantLock
import kotlin.concurrent.withLock

class PntRelayFrames(
    private val input: DataInputStream,
    private val output: DataOutputStream,
    private val clock: () -> Long = System::nanoTime
) {
    private val writer = ReentrantLock()
    @Volatile private var enabled = false
    @Volatile private var lastRead = clock()
    private var lastPing = clock()

    fun negotiate(keepAlive: Boolean) {
        lastRead = clock()
        lastPing = lastRead
        enabled = keepAlive
    }

    fun sendPacket(data: ByteArray, size: Int) = writer.withLock {
        require(size in 1..65507 && size <= data.size)
        output.writeShort(size)
        output.write(data, 0, size)
        output.flush()
    }

    fun checkHealth() {
        if (!enabled) return
        val now = clock()
        if (now - lastRead >= TimeUnit.SECONDS.toNanos(15)) throw SocketTimeoutException("PNT exit heartbeat timed out")
        if (writer.tryLock()) {
            try {
                if (now - lastPing >= TimeUnit.SECONDS.toNanos(3)) {
                    output.writeShort(0)
                    output.flush()
                    lastPing = now
                }
            } finally { writer.unlock() }
        }
    }

    fun receivePacket(): ByteArray {
        while (true) {
            val header = ByteArray(2)
            readFully(header)
            val size = ((header[0].toInt() and 255) shl 8) or (header[1].toInt() and 255)
            if (enabled && size == 65535) {
                lastRead = clock()
                continue
            }
            if (size !in 1..65507) throw IOException("Invalid PNT datagram length")
            return ByteArray(size).also { readFully(it); lastRead = clock() }
        }
    }

    private fun readFully(bytes: ByteArray) {
        var offset = 0
        while (offset < bytes.size) {
            val count = try { input.read(bytes, offset, bytes.size - offset) } catch (error: SocketTimeoutException) {
                if (!enabled) throw error
                checkHealth()
                continue
            }
            if (count < 0) throw EOFException("PNT relay disconnected")
            offset += count
        }
    }
}
