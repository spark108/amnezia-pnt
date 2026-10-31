package org.amnezia.vpn

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Test
import java.io.File

class PntTransportTest {
    @Test fun dotnetSignaturesCoverLocationNullsFractionsAndLegacyProfiles() {
        val profiles = JSONObject(ProfileSignatureFixtures.json)
        for (name in profiles.keys()) PntTransport.verifyProfileSignature(profiles.getJSONObject(name))
        val modified = profiles.getJSONObject("fractional")
        modified.getJSONArray("entrypoints").getJSONObject(0).getJSONObject("location").put("latitude", 1.25)
        var rejected = false
        try { PntTransport.verifyProfileSignature(modified) } catch (_: IllegalArgumentException) { rejected = true }
        assertTrue("Changed location retained a valid signature", rejected)
    }

    @Test fun deviceProofCoversNonceClientTaskAndSession() {
        val payload = PntTransport.devicePayload("nonce", "client", "stats", "exit", "epoch")
        assertEquals("PNT device binding v1\nnonce\nclient\nstats\nexit\nepoch", String(payload, Charsets.UTF_8))
        val pair = java.security.KeyPairGenerator.getInstance("EC").apply {
            initialize(java.security.spec.ECGenParameterSpec("secp256r1"))
        }.generateKeyPair()
        val signature = java.security.Signature.getInstance("SHA256withECDSA").apply {
            initSign(pair.private)
            update(payload)
        }.sign()
        fun verifies(data: ByteArray) = java.security.Signature.getInstance("SHA256withECDSA").apply {
            initVerify(pair.public)
            update(data)
        }.verify(signature)
        assertTrue(verifies(payload))
        assertFalse(verifies(PntTransport.devicePayload("other", "client", "stats", "exit", "epoch")))
        assertFalse(verifies(PntTransport.devicePayload("nonce", "other", "stats", "exit", "epoch")))
        assertFalse(verifies(PntTransport.devicePayload("nonce", "client", "ensure", "exit", "epoch")))
        assertFalse(verifies(PntTransport.devicePayload("nonce", "client", "stats", "other", "epoch")))
        assertFalse(verifies(PntTransport.devicePayload("nonce", "client", "stats", "exit", "other")))
        assertEquals("PNT device binding v1\nnonce\nclient\ncatalog\n\n", String(PntTransport.devicePayload("nonce", "client", "catalog", null, null), Charsets.UTF_8))
    }

    @Test fun malformedInputIsRejected() {
        val response = JSONObject(PntTransport.request("not JSON"))
        assertTrue(response.has("error"))
        assertFalse(response.optBoolean("ok"))
    }

    @Test fun oversizedInputIsRejected() {
        val response = JSONObject(PntTransport.request(" ".repeat(1100001)))
        assertTrue(response.has("error"))
    }

    @Test fun dotnetProfileSignatureAndIdentityAreAccepted() {
        val path = System.getenv("PNT_TEST_PROFILE")
        assumeTrue(path != null)
        val profile = JSONObject(File(path!!).readText())
        val response = JSONObject(PntTransport.request(JSONObject().put("action", "import").put("profile", profile).toString()))
        assertTrue(response.optString("error"), response.optBoolean("ok"))
        assertEquals(profile.getString("clientId"), response.getString("clientId"))
    }

    @Test fun modifiedDotnetProfileIsRejected() {
        val path = System.getenv("PNT_TEST_PROFILE")
        assumeTrue(path != null)
        val profile = JSONObject(File(path!!).readText())
        profile.getJSONArray("servers").getJSONObject(0).put("name", "Modified server")
        val response = JSONObject(PntTransport.request(JSONObject().put("action", "import").put("profile", profile).toString()))
        assertFalse(response.optBoolean("ok"))
        assertTrue(response.optString("error").contains("signature"))
        assertFalse(response.toString().contains(profile.getString("identity")))
    }
}
