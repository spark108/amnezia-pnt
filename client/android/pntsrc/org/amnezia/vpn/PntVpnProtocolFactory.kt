package org.amnezia.vpn

import org.amnezia.vpn.protocol.Protocol

object VpnProtocolFactory {
    fun openVpn(): Protocol = error("PNT Android supports AmneziaWG/WireGuard only")
    fun xray(): Protocol = error("PNT Android supports AmneziaWG/WireGuard only")
}
