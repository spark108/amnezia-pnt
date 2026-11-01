# PNT VPN

[Русский](Readme_RU.md) · [PNT server](https://github.com/spark108/pnt) · [Releases](https://github.com/spark108/amnezia-pnt/releases/latest)

PNT VPN is a Windows and Android VPN client based on AmneziaVPN. Import one administrator-issued `.pnt` access profile, press Connect and choose an allowed exit server. PNT provisions AmneziaWG on the selected server and connects directly when possible, or carries VPN traffic over a PNT route.

## Features

- Windows x64 GUI with a separate background VPN service; Android ARM64 with the Android VPN service.
- One PNT profile import. The server picker refreshes permitted exits; permission changes do not require reimporting.
- Restricted client identities: clients are not cluster members, do not appear in node topology, cannot relay traffic and cannot administer PNT.
- AmneziaWG exits installed natively on Debian or in Docker. Ordinary WireGuard server management is not supported by PNT.
- Server list with external IPv4 and latency in `ms`.
- Connection statistics: device external/internal IPs, handshake, total received/sent traffic, rates, direct/transport mode and exit/internet ping.
- Split tunneling, session-preserving relay replacement and GitHub release updates.

The PNT build removes Premium/Free, SSH server provisioning, ordinary VPN-key/file/QR imports, official Amnezia backend/website integration and official update checks. Only PNT access profiles are accepted.

## Installation and use

Download a package from [GitHub Releases](https://github.com/spark108/amnezia-pnt/releases/latest).

**Windows:** extract the complete `PntVPN-windows-x64.zip`, run `install.ps1` in an elevated PowerShell, then launch the installed `PntVPN.exe`. The service and transport are required; do not copy only the GUI executable.

```powershell
powershell -ExecutionPolicy Bypass -File .\install.ps1
```

**Android:** install `PntVPN-android-arm64.apk` on Android 10 or later. Allow APK installation when requested. Grant Android's VPN permission on the first connection. Closing the screen leaves the VPN service running; Disconnect stops it.

1. Obtain a `.pnt` file from the cluster administrator and import it once.
2. Press Connect and choose an exit server. Only permitted servers appear.
3. View statistics in the main window after connecting. Split tunneling and profile reset are available while disconnected.
4. Reset Access Profile removes the local profile; it does not revoke the server identity or reset its device binding. Reset requires confirmation.

Profiles include a private identity and trusted PNT entry certificates. Store them as credentials. Device binding is additional protection, not a replacement for keeping profiles private.

## Device binding

A new profile is unbound. Its first connection binds it to a separate key in Android Keystore or Windows CNG. The device key is not part of `.pnt`; importing a copied profile on another device does not grant access once it is bound. Before binding, whoever connects first can claim the profile.

Administrators manage this on a PNT member:

```sh
pnt vpn client rename <client-id> NewName
pnt vpn client unbind <client-id>
pnt vpn client revoke <client-id>
```

`unbind` resets the binding, deauthorizes the current session and removes issued AmneziaWG keys on reachable VPN servers. Offline removals wait for connectivity. The next device can bind, including the previous device. Use `revoke` for a permanent denial. Reinstallation/OS changes that lose the device key require `unbind`.

The client creator node serializes binding decisions and must be reachable for authorization; there is no automatic authority failover. Update all PNT members and clients for this protocol. Clients created before binding support need a one-time `unbind` to assign an authority and revoke old sessions. Changes to a profile's signed contents invalidate its signature, including changes to node geolocation.

## Direct and relayed connections

VPN traffic initially uses direct UDP to the exit server. PNT confirms the handshake and falls back to a route when direct access cannot be established. Client-to-entry control/transport uses certificate-pinned TLS over TCP 442; current PNT members prefer QUIC over UDP 442 between nodes, with TCP fallback.

Datagrams retain their boundaries inside reliable streams; this is not QUIC DATAGRAM. Active and standby relay channels use heartbeat checks. When a relay fails, the transport can replace it without rebuilding the VPN interface or keys. Some packets may be lost; replacing the exit server cannot preserve that server's NAT sessions.

Ping is displayed as exit/internet latency, for example `100ms/103ms`; `—` means unavailable. Direct VPN connection measurements are not the cluster's internal PNT latency. Split-tunneling exclusions can affect the actual probe path.

## Updates and diagnostics

The version is shown at the bottom center. Updates are checked in the background on startup and periodically; click the version to check manually. A newer version appears in green with an arrow. Click it to download and install after size/SHA-256 verification. Windows requests elevation; Android uses the system package installer. VPN is disconnected for installation. Keep the same Android application ID and compatible signing identity to preserve data.

Release assets must be named `PntVPN-windows-x64.zip` and `PntVPN-android-arm64.apk`; checksums are provided in `SHA256SUMS.txt`. Replaced assets with the same version do not trigger a version-based update: download and reinstall the APK manually in that case.

If profile import fails, use the latest Android APK: the 5.1.0.9 replacement fixes verification of profiles containing geolocation, fractional coordinates and nulls while retaining older profiles. Do not edit signed profile contents. If the server rejects a device binding, contact the administrator instead of altering the profile.

Android diagnostics are available through `adb logcat`; filter for `PntTransport`, `PntVPN` and `AndroidRuntime`. Do not publish full profiles or private keys with logs. Server-side logs and administration are documented in the [PNT repository](https://github.com/spark108/pnt).

## Build from source

Replace `<...>` placeholders with your local paths. Initialize submodules:

```sh
git submodule update --init --recursive
```

Requirements: CMake, Ninja, Conan 2, Qt 6.10+ and the platform toolchain. Windows needs MSVC, the .NET 10 SDK and the PNT source checkout for its transport helper. Android needs JDK 17, Android SDK, NDK r27c, Qt for Android ARM64 and matching desktop Qt host tools.

```sh
# Windows, in the MSVC environment:
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DPNT_CLIENT_ONLY=ON -DPNT_SOURCE_DIR=<pnt-source> -DCMAKE_PREFIX_PATH=<Qt-desktop>
cmake --build build --parallel
```

```sh
# Android:
cmake -S . -B build-android -G Ninja -DCMAKE_BUILD_TYPE=Release -DPNT_CLIENT_ONLY=ON -DCMAKE_TOOLCHAIN_FILE=<Qt-Android>/lib/cmake/Qt6/qt.toolchain.cmake -DQT_HOST_PATH=<Qt-host> -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29
cmake --build build-android --target PntVPN_prepare_apk_dir --parallel
```

Use the generated deployment settings with `androiddeployqt` to prepare the full Android manifest/resources and native libraries, then build the OSS Gradle variant. Ensure the newly built `libPntVPN_arm64-v8a.so` is included. Signing production APKs requires the persistent release key and its signing lineage. Never put keys/passwords in the repository.

Android unit checks use `testOssDebugUnitTest`. `PNT_TEST_PROFILE` can point to a temporary test profile outside the repository for .NET signature/identity interoperability tests; geolocation signature fixtures contain public test certificates only. Additional focused checks are under `client/tests/`. Increase `PNT_VERSION` and the PNT Android version code for a new release.

## Upstream and license

Based on [AmneziaVPN](https://github.com/amnezia-vpn/amnezia-client). The source retains upstream components; `PNT_CLIENT_ONLY` is enabled by default for this fork. See [LICENSE](LICENSE) (GPLv3) and [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).
