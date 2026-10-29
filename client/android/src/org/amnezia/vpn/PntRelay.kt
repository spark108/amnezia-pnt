package org.amnezia.vpn

import java.io.Closeable
import java.util.concurrent.LinkedBlockingDeque
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import kotlin.concurrent.thread

interface PntRelayLink : Closeable {
    val nodeId: String
    fun sendPacket(data: ByteArray, size: Int)
    fun receivePacket(): ByteArray
}

class PntRelay(
    private val connect: (String?) -> PntRelayLink?,
    private val receive: (ByteArray) -> Unit,
    private val changed: () -> Unit
) : Closeable {
    private data class Packet(val bytes: ByteArray, val created: Long = System.nanoTime())
    private val running = AtomicBoolean(true)
    private val queue = LinkedBlockingDeque<Packet>(128)
    private val gate = Any()
    private val workers = mutableListOf<Thread>()
    private var active: PntRelayLink? = null
    private var standby: PntRelayLink? = null
    private var failed: String? = null
    @Volatile var enabled: Boolean = false
        private set

    fun enable(first: ByteArray?) {
        if (enabled) return
        enabled = true
        first?.let { send(it) }
    }

    fun send(bytes: ByteArray) {
        val packet = Packet(bytes)
        while (!queue.offerLast(packet)) queue.pollFirst()
    }

    private fun remove(link: PntRelayLink) {
        val promoted = synchronized(gate) {
            if (active === link) {
                failed = link.nodeId
                active = standby
                standby = null
                active != null
            } else {
                if (standby === link) standby = null
                false
            }
        }
        runCatching { link.close() }
        if (promoted && running.get()) changed()
    }

    fun start() {
        launch {
            while (running.get()) {
                val state = synchronized(gate) { active to standby }
                if (enabled && (state.first == null || state.second == null)) {
                    var link = runCatching { connect(state.first?.nodeId ?: failed) }.getOrNull()
                    if (link == null && state.first == null && running.get()) link = runCatching { connect(null) }.getOrNull()
                    if (link != null) {
                        val candidate = link
                        var selected = false
                        val accepted = synchronized(gate) {
                            when {
                                !running.get() -> false
                                active == null -> { active = candidate; selected = true; true }
                                standby == null && active?.nodeId != candidate.nodeId -> { standby = candidate; true }
                                else -> false
                            }
                        }
                        if (accepted) {
                            launch {
                                try {
                                    while (running.get()) {
                                        val bytes = candidate.receivePacket()
                                        if (synchronized(gate) { active === candidate }) receive(bytes)
                                    }
                                } catch (_: Exception) { if (running.get()) remove(candidate) }
                            }
                            if (selected) changed()
                        } else runCatching { candidate.close() }
                    }
                }
                Thread.sleep(500)
            }
        }
        launch {
            while (running.get()) {
                val packet = queue.poll(500, TimeUnit.MILLISECONDS) ?: continue
                while (running.get() && System.nanoTime() - packet.created < TimeUnit.SECONDS.toNanos(5)) {
                    val link = synchronized(gate) { active }
                    if (link == null) { Thread.sleep(50); continue }
                    try { link.sendPacket(packet.bytes, packet.bytes.size); break }
                    catch (_: Exception) { remove(link) }
                }
            }
        }
    }

    private fun launch(block: () -> Unit) {
        val worker = thread(start = false, isDaemon = true, name = "PNT relay") {
            try { block() } catch (_: InterruptedException) { }
            finally { synchronized(workers) { workers.remove(Thread.currentThread()) } }
        }
        synchronized(workers) { workers.add(worker) }
        worker.start()
    }

    override fun close() {
        if (!running.getAndSet(false)) return
        synchronized(gate) {
            runCatching { active?.close() }
            runCatching { standby?.close() }
            active = null
            standby = null
        }
        synchronized(workers) { workers.forEach { if (it !== Thread.currentThread()) it.interrupt() } }
        queue.clear()
    }
}
