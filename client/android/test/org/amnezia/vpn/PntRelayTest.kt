package org.amnezia.vpn

import org.junit.Assert.*
import org.junit.Test
import java.io.IOException
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.CopyOnWriteArrayList
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger

class PntRelayTest {
    private class Link(override val nodeId: String) : PntRelayLink {
        private val packets = LinkedBlockingQueue<ByteArray>()
        private val closed = AtomicBoolean(false)
        override fun sendPacket(data: ByteArray, size: Int) {
            if (closed.get()) throw IOException("Relay disconnected")
            packets.put(data.copyOf(size))
        }
        override fun receivePacket(): ByteArray {
            val packet = packets.take()
            if (packet.isEmpty()) throw IOException("Relay disconnected")
            return packet
        }
        override fun close() { if (!closed.getAndSet(true)) packets.offer(ByteArray(0)) }
    }

    private fun await(condition: () -> Boolean) {
        val deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(5)
        while (!condition()) {
            if (System.nanoTime() > deadline) fail("Relay transition timed out")
            Thread.sleep(10)
        }
    }

    @Test fun standbyAndNetworkRecoveryKeepTheSessionAlive() {
        val online = AtomicBoolean(true)
        val links = CopyOnWriteArrayList<Link>()
        val received = LinkedBlockingQueue<ByteArray>()
        val changes = AtomicInteger()
        val relay = PntRelay({ avoid ->
            if (!online.get()) throw IOException("Network unavailable")
            Link(if (avoid == "primary") "backup" else "primary").also { links.add(it) }
        }, { received.offer(it) }, { changes.incrementAndGet() })
        relay.start()
        try {
            relay.enable(byteArrayOf(1))
            assertArrayEquals(byteArrayOf(1), received.poll(5, TimeUnit.SECONDS))
            await { links.size >= 2 }
            Thread.sleep(100)
            links.first().close()
            await { changes.get() >= 2 }
            relay.send(byteArrayOf(2))
            assertArrayEquals(byteArrayOf(2), received.poll(5, TimeUnit.SECONDS))
            online.set(false)
            links.forEach { it.close() }
            Thread.sleep(700)
            assertTrue(relay.enabled)
            online.set(true)
            relay.send(byteArrayOf(3))
            assertArrayEquals(byteArrayOf(3), received.poll(5, TimeUnit.SECONDS))
        } finally { relay.close() }
    }
}
