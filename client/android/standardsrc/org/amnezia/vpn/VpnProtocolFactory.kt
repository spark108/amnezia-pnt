package org.amnezia.vpn

import org.amnezia.vpn.protocol.Protocol
import org.amnezia.vpn.protocol.openvpn.OpenVpn
import org.amnezia.vpn.protocol.xray.Xray

object VpnProtocolFactory {
    fun openVpn(): Protocol = OpenVpn()
    fun xray(): Protocol = Xray.instance
}
