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
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import java.security.KeyPairGenerator
import java.security.spec.ECGenParameterSpec
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

private const val TCP_KEEPIDLE = 4
private const val TCP_KEEPINTVL = 5
private const val TCP_KEEPCNT = 6

private class PntDeviceAuthorizationException(message: String) : SecurityException(message)

@androidx.annotation.Keep
object PntTransport {
    private val relayCursor = java.util.concurrent.atomic.AtomicInteger()
    @Volatile private var activeMode = ""
    fun setConnectionMode(mode: String) { activeMode = mode }
    @JvmStatic fun connectionMode(): String = activeMode
    private fun bytes(value: String) = Base64.getDecoder().decode(value)
    private fun hash(value: ByteArray) = MessageDigest.getInstance("SHA-256").digest(value)
        .joinToString("") { "%02X".format(it.toInt() and 255) }

    internal fun devicePayload(nonce: String, clientId: String, action: String, server: String?, epoch: String?) =
        "PNT device binding v1\n$nonce\n$clientId\n$action\n${server ?: ""}\n${epoch ?: ""}".toByteArray(Charsets.UTF_8)

    @Synchronized private fun deviceProof(nonce: String, profile: JSONObject, action: String, server: String?, epoch: String?): JSONObject {
        val alias = "PNT.VPN.Device.v1"
        val store = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }
        if (!store.containsAlias(alias)) {
            KeyPairGenerator.getInstance(KeyProperties.KEY_ALGORITHM_EC, "AndroidKeyStore").apply {
                initialize(KeyGenParameterSpec.Builder(alias, KeyProperties.PURPOSE_SIGN or KeyProperties.PURPOSE_VERIFY)
                    .setAlgorithmParameterSpec(ECGenParameterSpec("secp256r1"))
                    .setDigests(KeyProperties.DIGEST_SHA256).build())
                generateKeyPair()
            }
        }
        val payload = devicePayload(nonce, profile.getString("clientId"), action, server, epoch)
        val signature = Signature.getInstance("SHA256withECDSA").apply {
            initSign(store.getKey(alias, null) as java.security.PrivateKey)
            update(payload)
        }.sign()
        return JSONObject().put("key", Base64.getEncoder().encodeToString(store.getCertificate(alias).publicKey.encoded))
            .put("signature", Base64.getEncoder().encodeToString(signature))
    }

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

    class Channel(val socket: SSLSocket, override val nodeId: String = "") : PntRelayLink {
        val input = DataInputStream(socket.inputStream)
        val output = DataOutputStream(socket.outputStream)
        val frames = PntRelayFrames(input, output)
        override fun close() = socket.close()
        fun message(value: JSONObject) {
            val data = value.toString().toByteArray(Charsets.UTF_8)
            output.writeInt(data.size)
            output.write(data)
            output.flush()
        }
        fun reply(expectedType: String = "vpn-result"): JSONObject {
            val size = input.readInt()
            require(size in 1..1_048_576) { "Invalid PNT response length" }
            val data = ByteArray(size)
            input.readFully(data)
            val result = JSONObject(String(data, Charsets.UTF_8))
            if (result.optString("name") == "vpn-device-denied") throw PntDeviceAuthorizationException(result.optString("error", "Device authorization revoked"))
            require(result.optString("type") == expectedType) { result.optString("error", "PNT request failed") }
            return result
        }
        override fun sendPacket(data: ByteArray, size: Int) = frames.sendPacket(data, size)
        override fun receivePacket(): ByteArray = frames.receivePacket()
        override fun checkHealth() = frames.checkHealth()
    }

    fun open(profile: JSONObject, action: String, server: String?, protect: ((Socket) -> Boolean)? = null, exclude: String? = null, timeoutMillis: Int = 50000, epoch: String? = null): Pair<Channel, JSONObject> {
        val deadline = System.nanoTime() + java.util.concurrent.TimeUnit.MILLISECONDS.toNanos(timeoutMillis.toLong())
        val store = identity(profile)
        val managers = KeyManagerFactory.getInstance(KeyManagerFactory.getDefaultAlgorithm()).apply { init(store, CharArray(0)) }
        val nodes = profile.getJSONArray("entrypoints")
        var last: Exception? = null
        val offset = if (action == "udp") Math.floorMod(relayCursor.incrementAndGet(), nodes.length()) else 0
        for (position in 0 until nodes.length()) {
            val index = (position + offset) % nodes.length()
            val node = nodes.getJSONObject(index)
            if (node.optBoolean("removed") || node.optString("id") == exclude) continue
            fun remaining(): Int = ((deadline - System.nanoTime()) / 1000000).coerceIn(1, timeoutMillis.toLong()).toInt()
            if (System.nanoTime() >= deadline) break
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
                raw = Socket().apply {
                    bind(InetSocketAddress(0))
                    keepAlive = true
                    android.os.ParcelFileDescriptor.fromSocket(this).use {
                        runCatching {
                            android.system.Os.setsockoptInt(it.fileDescriptor, android.system.OsConstants.IPPROTO_TCP, TCP_KEEPIDLE, 3)
                            android.system.Os.setsockoptInt(it.fileDescriptor, android.system.OsConstants.IPPROTO_TCP, TCP_KEEPINTVL, 2)
                            android.system.Os.setsockoptInt(it.fileDescriptor, android.system.OsConstants.IPPROTO_TCP, TCP_KEEPCNT, 3)
                        }
                    }
                }
                require(protect == null || protect(raw)) { "Cannot protect PNT control socket" }
                raw.connect(InetSocketAddress(node.getString("ip"), 442), minOf(5000, remaining()))
                secure = context.socketFactory.createSocket(raw, node.getString("ip"), 442, true) as SSLSocket
                secure.enabledProtocols = arrayOf("TLSv1.3")
                secure.soTimeout = remaining()
                secure.startHandshake()
                secure.soTimeout = remaining()
                val channel = Channel(secure, node.getString("id"))
                channel.message(JSONObject().put("type", "vpn-client").put("id", profile.getString("clientId")).put("name", action).apply {
                    if (server != null) put("target", server)
                    if (epoch != null) put("deviceEpoch", epoch)
                    if (action == "udp") put("command", "vpn-keepalive-v1")
                })
                val challenge = channel.reply("vpn-device-challenge")
                channel.message(JSONObject().put("type", "vpn-device-proof")
                    .put("vpnPayload", deviceProof(challenge.getString("vpnPayload"), profile, action, server, epoch).toString()))
                val reply = channel.reply()
                if (action == "udp") {
                    val keepAlive = reply.optString("name") == "vpn-keepalive-v1"
                    channel.frames.negotiate(keepAlive)
                    secure.soTimeout = if (keepAlive) 3000 else 0
                }
                return channel to reply
            } catch (error: Exception) {
                last = error
                runCatching { secure?.close() }
                runCatching { raw?.close() }
                if (error is PntDeviceAuthorizationException) throw error
            }
        }
        throw IllegalStateException("PNT bootstrap nodes unavailable: ${last?.message}")
    }

    fun request(profile: JSONObject, action: String, server: String?, protect: ((Socket) -> Boolean)? = null, epoch: String? = null): JSONObject {
        val (channel, reply) = open(profile, action, server, protect, epoch = epoch)
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
            "stats" -> JSONObject(request(profile, "stats", input.getString("server"), epoch = input.optString("deviceEpoch").ifEmpty { null }).getString("vpnPayload"))
            "connect" -> {
                val server = input.getString("server")
                val response = request(profile, "ensure", server)
                val reply = JSONObject(response.getString("vpnPayload")).put("deviceEpoch", response.getString("deviceEpoch"))
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
    val mode: String get() = if (relay.enabled) "relay" else "direct"
    private val running = AtomicBoolean(true)
    private val profile = metadata.getJSONObject("profile")
    private val server = metadata.getString("server")
    private val deviceEpoch = metadata.getString("deviceEpoch")
    private val local = DatagramSocket(0, InetAddress.getByName("127.0.0.1"))
    private var direct: DatagramSocket? = null
    private val relay = PntRelay({ avoid ->
        val nodes = profile.getJSONArray("entrypoints")
        if ((0 until nodes.length()).none { !nodes.getJSONObject(it).optBoolean("removed") && nodes.getJSONObject(it).optString("id") != avoid }) null
        else PntTransport.open(profile, "udp", server, { service.protect(it) }, avoid, 8000, deviceEpoch).first
    }, { packet ->
        lastTraffic = android.os.SystemClock.elapsedRealtime()
        consumer?.let { local.send(DatagramPacket(packet, packet.size, it)) }
    }, { android.util.Log.i("PntTransport", "PNT relay route ready") })
    @Volatile private var consumer: InetSocketAddress? = null
    @Volatile private var lastPacket: ByteArray? = null
    @Volatile private var lastTraffic = 0L
    private val workers = mutableListOf<Thread>()

    fun start(config: JSONObject) {
        android.util.Log.i("PntTransport", "Preparing protected VPN transport")
        PntTransport.validate(profile)
        val protect: (Socket) -> Boolean = { service.protect(it) }
        val remote = JSONObject(PntTransport.request(profile, "ensure", server, protect, deviceEpoch).getString("vpnPayload"))
        val baseline = JSONObject(PntTransport.request(profile, "stats", server, protect, deviceEpoch).getString("vpnPayload")).getLong("handshake")
        val outgoing = DatagramSocket()
        direct = outgoing
        require(service.protect(outgoing)) { "Cannot protect PNT UDP socket" }
        outgoing.connect(InetAddress.getByName(remote.getString("host")), remote.getInt("port"))
        val data = config.optJSONObject("awg_config_data") ?: config.optJSONObject("wireguard_config_data") ?: error("PNT requires AmneziaWG or WireGuard")
        data.put("hostName", "127.0.0.1").put("port", local.localPort)
        config.put("hostName", "127.0.0.1")
        relay.start()
        android.util.Log.i("PntTransport", "IPv4 loopback UDP listener ready")
        launch {
            val buffer = ByteArray(65507)
            while (running.get()) {
                val packet = DatagramPacket(buffer, buffer.size)
                local.receive(packet)
                val sender = packet.socketAddress as InetSocketAddress
                if (consumer == null) android.util.Log.i("PntTransport", "Receiving packets from VPN backend")
                consumer = sender
                val bytes = packet.data.copyOf(packet.length)
                lastPacket = bytes
                if (relay.enabled) relay.send(bytes)
                else try { outgoing.send(DatagramPacket(packet.data, packet.length)) } catch (_: SocketException) { }
            }
        }
        launch {
            val buffer = ByteArray(65507)
            outgoing.soTimeout = 2000
            while (running.get()) {
                try {
                    val packet = DatagramPacket(buffer, buffer.size)
                    outgoing.receive(packet)
                    if (!relay.enabled && consumer != null) {
                        lastTraffic = android.os.SystemClock.elapsedRealtime()
                        local.send(DatagramPacket(packet.data, packet.length, consumer))
                    }
                } catch (_: SocketTimeoutException) { } catch (_: SocketException) { if (running.get()) Thread.sleep(500) }
            }
        }
        launch {
            Thread.sleep(10000)
            while (running.get()) {
                try {
                    val stats = JSONObject(PntTransport.request(profile, "stats", server, protect, deviceEpoch).getString("vpnPayload"))
                    if (!running.get()) break
                    if (!relay.enabled && !(stats.getLong("handshake") > baseline && android.os.SystemClock.elapsedRealtime() - lastTraffic < 60000)) relay.enable(lastPacket)
                } catch (error: PntDeviceAuthorizationException) {
                    throw error
                } catch (_: Exception) {
                    if (running.get() && android.os.SystemClock.elapsedRealtime() - lastTraffic > 60000) relay.enable(lastPacket)
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
        relay.close()
        synchronized(workers) { workers.forEach { if (it !== Thread.currentThread()) it.interrupt() } }
    }
}
