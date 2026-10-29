# PNT VPN

Windows VPN client for [PNT](https://github.com/spark108/pnt), based on AmneziaVPN.

Import one PNT access profile issued by your administrator. Each connection retrieves the permitted exit servers and opens the server picker. VPN profiles are provisioned by PNT on the selected server; the client prefers a confirmed direct connection and uses a PNT route when necessary.

- Separate, restricted client identity with no cluster administration or transit role.
- AmneziaWG servers installed natively on Linux or in Docker.
- One PNT profile import; no Premium/Free, SSH provisioning or ordinary VPN configuration imports.
- No official Amnezia gateway backend or website/update integrations in the PNT build.
- Separate Windows service, local IPC endpoint and tunnel interface.

See [PNT.md](PNT.md) for build, installation and usage instructions. The default build enables `PNT_CLIENT_ONLY`; the server-side commands and access profile format are in the PNT repository.

The relayed datagram transport currently uses encrypted TCP streams. A broken relay disconnects the VPN; reconnecting obtains a new route. Native QUIC and seamless relay replacement are not implemented.

Upstream source: [amnezia-vpn/amnezia-client](https://github.com/amnezia-vpn/amnezia-client). License and third-party notices remain in [LICENSE](LICENSE) and [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).
