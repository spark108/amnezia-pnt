package org.amnezia.vpn

import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.DataInputStream
import java.io.DataOutputStream
import java.io.IOException
import java.io.InputStream
import java.net.SocketTimeoutException
import java.util.concurrent.TimeUnit
import org.junit.Assert.*
import org.junit.Test

class PntRelayFramesTest {
    @Test fun heartbeatIsNegotiatedAndDoesNotBecomeVpnTraffic() {
        var now = 0L
        val incoming = ByteArrayOutputStream()
        DataOutputStream(incoming).apply { writeShort(65535); writeShort(2); write(byteArrayOf(7, 8)) }
        val outgoing = ByteArrayOutputStream()
        val frames = PntRelayFrames(DataInputStream(ByteArrayInputStream(incoming.toByteArray())), DataOutputStream(outgoing)) { now }
        frames.negotiate(true)
        now = TimeUnit.SECONDS.toNanos(3)
        frames.checkHealth()
        assertArrayEquals(byteArrayOf(0, 0), outgoing.toByteArray())
        assertArrayEquals(byteArrayOf(7, 8), frames.receivePacket())
        now = TimeUnit.SECONDS.toNanos(17)
        frames.checkHealth()
        now = TimeUnit.SECONDS.toNanos(18)
        try { frames.checkHealth(); fail("Silent relay remained healthy") } catch (_: SocketTimeoutException) { }
    }

    @Test fun legacyRelaysDoNotReceiveHeartbeats() {
        val outgoing = ByteArrayOutputStream()
        var now = 0L
        val frames = PntRelayFrames(DataInputStream(ByteArrayInputStream(byteArrayOf(0, 1, 9))), DataOutputStream(outgoing)) { now }
        frames.negotiate(false)
        now = TimeUnit.MINUTES.toNanos(10)
        frames.checkHealth()
        assertEquals(0, outgoing.size())
        assertArrayEquals(byteArrayOf(9), frames.receivePacket())
    }

    @Test fun unnegotiatedHeartbeatIsRejected() {
        val frames = PntRelayFrames(DataInputStream(ByteArrayInputStream(byteArrayOf(-1, -1))), DataOutputStream(ByteArrayOutputStream()))
        try { frames.receivePacket(); fail("Unnegotiated frame accepted") } catch (_: IOException) { }
    }

    @Test fun readTimeoutInsideHeaderAndPayloadDoesNotLoseFraming() {
        val source = ByteArrayInputStream(byteArrayOf(0, 2, 7, 8))
        var timeout = false
        val input = object : InputStream() {
            override fun read(): Int {
                timeout = !timeout
                if (!timeout) throw SocketTimeoutException()
                return source.read()
            }
            override fun read(bytes: ByteArray, offset: Int, size: Int): Int {
                val value = read()
                if (value < 0) return -1
                bytes[offset] = value.toByte()
                return 1
            }
        }
        val frames = PntRelayFrames(DataInputStream(input), DataOutputStream(ByteArrayOutputStream())) { 0L }
        frames.negotiate(true)
        assertArrayEquals(byteArrayOf(7, 8), frames.receivePacket())
    }
}
