package org.amnezia.vpn

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Test
import java.io.File

class PntTransportTest {
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
