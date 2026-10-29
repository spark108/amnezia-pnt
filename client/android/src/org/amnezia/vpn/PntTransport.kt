package org.amnezia.vpn

import org.json.JSONArray
import org.json.JSONObject
import java.io.ByteArrayInputStream
import java.io.Closeable
import java.io.DataInputStream
import java.io.DataOutputStream
import java.math.BigInteger
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.InetSocketAddress
import java.net.Socket
import java.net.SocketException
import java.net.SocketTimeoutException
import java.security.KeyStore
import java.security.MessageDigest
import java.security.Signature
import java.security.cert.CertificateFactory
import java.security.cert.X509Certificate
import java.security.interfaces.ECPublicKey
import java.util.Base64
import java.util.concurrent.atomic.AtomicBoolean
import javax.net.ssl.KeyManagerFactory
import javax.net.ssl.SSLContext
import javax.net.ssl.SSLSocket
import javax.net.ssl.TrustManager
import javax.net.ssl.X509TrustManager
import kotlin.concurrent.thread

@androidx.annotation.Keep
object PntTransport {
    @Volatile private var activeMode = ""
    fun setConnectionMode(mode: String) { activeMode = mode }
    @JvmStatic fun connectionMode(): String = activeMode
    private fun bytes(value: String) = Base64.getDecoder().decode(value)
    private fun hash(value: ByteArray) = MessageDigest.getInstance("SHA-256").digest(value)
        .joinToString("") { "%02X".format(it.toInt() and 255) }

    private fun quoted(value: String) = buildString {
        append('"')
        for (char in value) when (char) {
            '\\' -> append("\\\\")
            '\b' -> append("\\b")
            '\t' -> append("\\t")
            '\n' -> append("\\n")
            '\u000c' -> append("\\f")
            '\r' -> append("\\r")
            else -> if (char.code !in 32..126 || char in "\"&'+<>`") append("\\u%04X".format(char.code)) else append(char)
        }
        append('"')
    }

    private fun canonical(value: Any, fields: List<String>? = null): String = when (value) {
        is JSONObject -> {
            val order = fields ?: when {
                value.has("format") -> listOf("format", "cluster", "clientId", "identity", "entrypoints", "servers", "issuer", "signature")
                value.has("ip") -> listOf("id", "name", "ip", "cert", "version", "removed", "updating", "updatingSince", "behindNat", "platform")
                else -> listOf("id", "name", "version", "removed")
            }
            order.joinToString(",", "{", "}") { key -> quoted(key) + ":" + if (key == "signature") "\"\"" else canonical(value.get(key)) }
        }
        is JSONArray -> (0 until value.length()).joinToString(",", "[", "]") { canonical(value.get(it)) }
        is String -> quoted(value)
        is Number -> value.toLong().toString()
        is Boolean -> value.toString()
        else -> throw IllegalArgumentException("Invalid PNT profile field")
    }

    private fun identity(profile: JSONObject): KeyStore = KeyStore.getInstance("PKCS12").apply {
        load(ByteArrayInputStream(bytes(profile.getString("identity"))), CharArray(0))
    }

    fun validate(profile: JSONObject) {
        require(profile.getInt("format") == 1) { "Unsupported PNT profile version" }
        require(profile.getJSONArray("entrypoints").length() in 1..1024) { "Invalid bootstrap nodes" }
        val issuer = CertificateFactory.getInstance("X.509").generateCertificate(ByteArrayInputStream(bytes(profile.getString("issuer")))) as X509Certificate
        val nodes = profile.getJSONArray("entrypoints")
        require((0 until nodes.length()).any { nodes.getJSONObject(it).getString("cert") == hash(issuer.encoded) }) { "Unknown profile issuer" }
        val signature = bytes(profile.getString("signature"))
        require(signature.size == 64) { "Invalid PNT profile signature" }
        fun integer(part: ByteArray) = BigInteger(1, part).toByteArray().let { byteArrayOf(2, it.size.toByte()) + it }
        val integers = integer(signature.copyOfRange(0, 32)) + integer(signature.copyOfRange(32, 64))
        val verifier = Signature.getInstance("SHA256withECDSA")
        verifier.initVerify(issuer.publicKey)
        verifier.update(canonical(profile).toByteArray(Charsets.UTF_8))
        require(verifier.verify(byteArrayOf(0x30, integers.size.toByte()) + integers)) { "PNT profile signature verification failed" }
        val store = identity(profile)
        val alias = store.aliases().asSequence().first { store.isKeyEntry(it) }
        val certificate = store.getCertificate(alias) as X509Certificate
        certificate.checkValidity()
        val public = certificate.publicKey as ECPublicKey
        fun coordinate(number: BigInteger) = number.toByteArray().takeLast(32).toByteArray().let { ByteArray(32 - it.size) + it }
        val point = byteArrayOf(4) + coordinate(public.w.affineX) + coordinate(public.w.affineY)
        require(hash(point).take(16).lowercase() == profile.getString("clientId")) { "PNT client identity mismatch" }
    }

    class Channel(val socket: SSLSocket) : Closeable {
        val input = DataInputStream(socket.inputStream)
        val output = DataOutputStream(socket.outputStream)
        override fun close() = socket.close()
        fun message(value: JSONObject) {
            val data = value.toString().toByteArray(Charsets.UTF_8)
            output.writeInt(data.size)
            output.write(data)
            output.flush()
        }
        fun reply(): JSONObject {
            val size = input.readInt()
            require(size in 1..1_048_576) { "Invalid PNT response length" }
            val data = ByteArray(size)
            input.readFully(data)
            val result = JSONObject(String(data, Charsets.UTF_8))
            require(result.optString("type") == "vpn-result") { result.optString("error", "PNT request failed") }
            return result
        }
        fun sendPacket(data: ByteArray, size: Int) {
            require(size in 1..65507)
            output.writeShort(size)
            output.write(data, 0, size)
            output.flush()
        }
        fun receivePacket(): ByteArray {
            val size = input.readUnsignedShort()
            require(size in 1..65507)
            return ByteArray(size).also { input.readFully(it) }
        }
    }

    fun open(profile: JSONObject, action: String, server: String?, protect: ((Socket) -> Boolean)? = null): Pair<Channel, JSONObject> {
        val store = identity(profile)
        val managers = KeyManagerFactory.getInstance(KeyManagerFactory.getDefaultAlgorithm()).apply { init(store, CharArray(0)) }
        val nodes = profile.getJSONArray("entrypoints")
        var last: Exception? = null
        for (index in 0 until nodes.length()) {
            val node = nodes.getJSONObject(index)
            if (node.optBoolean("removed")) continue
            var raw: Socket? = null
            var secure: SSLSocket? = null
            try {
                val trust = object : X509TrustManager {
                    override fun getAcceptedIssuers() = emptyArray<X509Certificate>()
                    override fun checkClientTrusted(chain: Array<X509Certificate>, authType: String) = Unit
                    override fun checkServerTrusted(chain: Array<X509Certificate>, authType: String) {
                        require(chain.isNotEmpty() && hash(chain[0].encoded) == node.getString("cert")) { "PNT server certificate mismatch" }
                    }
                }
                val context = SSLContext.getInstance("TLSv1.3").apply { init(managers.keyManagers, arrayOf<TrustManager>(trust), null) }
                raw = Socket().apply { bind(InetSocketAddress(0)) }
                require(protect == null || protect(raw)) { "Cannot protect PNT control socket" }
                raw.connect(InetSocketAddress(node.getString("ip"), 442), 5000)
                secure = context.socketFactory.createSocket(raw, node.getString("ip"), 442, true) as SSLSocket
                secure.enabledProtocols = arrayOf("TLSv1.3")
                secure.soTimeout = 50000
                secure.startHandshake()
                val channel = Channel(secure)
                channel.message(JSONObject().put("type", "vpn-client").put("id", profile.getString("clientId")).put("name", action).apply { if (server != null) put("target", server) })
                return channel to channel.reply()
            } catch (error: Exception) {
                last = error
                runCatching { secure?.close() }
                runCatching { raw?.close() }
            }
        }
        throw IllegalStateException("PNT bootstrap nodes unavailable: ${last?.message}")
    }

    fun request(profile: JSONObject, action: String, server: String?, protect: ((Socket) -> Boolean)? = null): JSONObject {
        val (channel, reply) = open(profile, action, server, protect)
        channel.use { return reply }
    }

    @JvmStatic fun request(payload: String): String = try {
        require(payload.length <= 1100000) { "PNT request is too large" }
        val input = JSONObject(payload)
        val profile = input.getJSONObject("profile")
        validate(profile)
        when (input.getString("action")) {
            "import" -> JSONObject().put("ok", true).put("clientId", profile.getString("clientId"))
            "catalog" -> {
                val reply = request(profile, "catalog", null)
                JSONObject().put("ok", true).put("servers", reply.optJSONArray("vpnServers") ?: JSONArray())
                    .put("nodes", reply.optJSONArray("nodes") ?: JSONArray()).put("clientId", profile.getString("clientId"))
            }
            "stats" -> JSONObject(request(profile, "stats", input.getString("server")).getString("vpnPayload"))
            "connect" -> {
                val server = input.getString("server")
                val reply = JSONObject(request(profile, "ensure", server).getString("vpnPayload"))
                reply.put("ok", true).put("server", server).put("entrypoints", JSONArray().apply {
                    val nodes = profile.getJSONArray("entrypoints")
                    for (i in 0 until nodes.length()) put(nodes.getJSONObject(i).getString("ip"))
                })
            }
            else -> error("Unsupported PNT action")
        }.toString()
    } catch (error: Exception) {
        val message = if (error is org.json.JSONException) "Invalid PNT profile or response JSON" else error.message ?: "PNT request failed"
        JSONObject().put("error", message).toString()
    }
}

class PntVpnSession(private val service: android.net.VpnService, private val metadata: JSONObject, private val onFailure: (String) -> Unit) : Closeable {
    val mode: String get() = if (relay == null) "direct" else "relay"
    private val running = AtomicBoolean(true)
    private val profile = metadata.getJSONObject("profile")
    private val server = metadata.getString("server")
    private val local = DatagramSocket(0, InetAddress.getByName("127.0.0.1"))
    private var direct: DatagramSocket? = null
    @Volatile private var relay: PntTransport.Channel? = null
    @Volatile private var consumer: InetSocketAddress? = null
    @Volatile private var lastPacket: ByteArray? = null
    @Volatile private var lastTraffic = 0L
    private val writer = Any()
    private val workers = mutableListOf<Thread>()

    fun start(config: JSONObject) {
        android.util.Log.i("PntTransport", "Preparing protected VPN transport")
        PntTransport.validate(profile)
        val protect: (Socket) -> Boolean = { service.protect(it) }
        val remote = JSONObject(PntTransport.request(profile, "ensure", server, protect).getString("vpnPayload"))
        val baseline = JSONObject(PntTransport.request(profile, "stats", server, protect).getString("vpnPayload")).getLong("handshake")
        val outgoing = DatagramSocket()
        direct = outgoing
        require(service.protect(outgoing)) { "Cannot protect PNT UDP socket" }
        outgoing.connect(InetAddress.getByName(remote.getString("host")), remote.getInt("port"))
        val data = config.optJSONObject("awg_config_data") ?: config.optJSONObject("wireguard_config_data") ?: error("PNT requires AmneziaWG or WireGuard")
        data.put("hostName", "127.0.0.1").put("port", local.localPort)
        config.put("hostName", "127.0.0.1")
        android.util.Log.i("PntTransport", "IPv4 loopback UDP listener ready")
        launch {
            val buffer = ByteArray(65507)
            while (running.get()) {
                val packet = DatagramPacket(buffer, buffer.size)
                local.receive(packet)
                val sender = packet.socketAddress as InetSocketAddress
                if (consumer == null) android.util.Log.i("PntTransport", "Receiving packets from VPN backend")
                consumer = sender
                lastPacket = packet.data.copyOf(packet.length)
                synchronized(writer) {
                    val channel = relay
                    if (channel != null) channel.sendPacket(packet.data, packet.length)
                    else try { outgoing.send(DatagramPacket(packet.data, packet.length)) } catch (_: SocketException) { }
                }
            }
        }
        launch {
            val buffer = ByteArray(65507)
            outgoing.soTimeout = 2000
            while (running.get()) {
                try {
                    val packet = DatagramPacket(buffer, buffer.size)
                    outgoing.receive(packet)
                    if (relay == null && consumer != null) {
                        lastTraffic = android.os.SystemClock.elapsedRealtime()
                        local.send(DatagramPacket(packet.data, packet.length, consumer))
                    }
                } catch (_: SocketTimeoutException) { } catch (_: SocketException) { if (running.get()) Thread.sleep(500) }
            }
        }
        launch {
            Thread.sleep(10000)
            while (running.get()) {
                val stats = JSONObject(PntTransport.request(profile, "stats", server, protect).getString("vpnPayload"))
                if (!running.get()) break
                if (relay == null && !(stats.getLong("handshake") > baseline && android.os.SystemClock.elapsedRealtime() - lastTraffic < 60000)) {
                    val (channel, _) = PntTransport.open(profile, "udp", server, protect)
                    android.util.Log.i("PntTransport", "PNT UDP relay connected")
                    synchronized(writer) {
                        relay = channel
                        if (!running.get()) {
                            channel.close()
                            return@synchronized
                        }
                        channel.socket.soTimeout = 0
                        lastPacket?.let { channel.sendPacket(it, it.size) }
                    }
                    launch {
                        while (running.get()) {
                            val packet = channel.receivePacket()
                            lastTraffic = android.os.SystemClock.elapsedRealtime()
                            consumer?.let { local.send(DatagramPacket(packet, packet.size, it)) }
                        }
                    }
                }
                Thread.sleep(15000)
            }
        }
    }

    private fun launch(block: () -> Unit) {
        val worker = thread(start = false, isDaemon = true, name = "PNT VPN") {
            try { block() } catch (_: InterruptedException) { } catch (error: Exception) { if (running.get()) { close(); onFailure(error.message ?: "PNT transport disconnected") } }
        }
        synchronized(workers) { workers.add(worker) }
        worker.start()
    }

    override fun close() {
        if (!running.getAndSet(false)) return
        local.close()
        direct?.close()
        runCatching { relay?.close() }
        synchronized(workers) { workers.forEach { if (it !== Thread.currentThread()) it.interrupt() } }
    }
}
