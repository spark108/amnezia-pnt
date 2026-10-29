# PNT VPN client

The default `PNT_CLIENT_ONLY=ON` build uses a single imported PNT access profile. Standard Amnezia server setup, Premium/Free and ordinary VPN config imports are unavailable. The official gateway backend is excluded from this build. Disabling `PNT_CLIENT_ONLY` retains the original application build.

## Build

Requirements: Qt 6.10+ for MSVC (including RemoteObjects, Core5Compat, HttpServer and WebSockets), Visual Studio C++ tools, CMake, Ninja, Conan 2 and .NET 10 SDK.

Initialize Git submodules and configure from an x64 Visual Studio developer terminal:

```powershell
git submodule update --init --recursive
$env:VSLANG='1033'
cmake -S . -B deploy/pnt-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<Qt-msvc-directory> -DPNT_CLIENT_ONLY=ON -DPNT_SOURCE_DIR=<PNT-source-directory>
cmake --build deploy/pnt-build --parallel 8
cmake --install deploy/pnt-build --prefix deploy/pnt-dist --component AmneziaVPN
```

The build publishes the .NET PNT transport and bundles it as `pnt-transport.exe`. The GUI, Windows service, IPC endpoint, tunnel interface and tunnel service have separate PNT names. The PNT installer does not invoke the original Amnezia uninstall/post-install scripts.

## Install and use

Extract the complete release archive to a temporary folder. In an elevated PowerShell, run `./install.ps1` from that folder. It installs into Program Files, starts the automatic `PntVPN-service` service and creates a desktop shortcut.

Open PNT VPN as an ordinary user. Import the `.pnt` configuration issued with `pnt vpn client create`. It includes the device's private PNT identity; store the original file securely and use a distinct profile per device. The application saves its imported profile in encrypted settings.

Click Connect and choose an exit server. PNT requests a VPN profile on the selected server, checks a direct VPN handshake and uses a PNT route if direct connectivity fails. Every new Connect action opens the current server picker; no repeated profile import is needed. Reset Access Profile removes the local identity and permits importing another profile.

The server picker displays each exit node's external address. Once connected, the main window shows direct/relayed transport, the assigned VPN address, the external address, the last handshake, total transferred bytes for the current connection and separate receive/send speeds. Traffic counters refresh every second and do not reset when minimizing the application; handshake information refreshes every 15 seconds. External address detection uses an HTTPS request to `api.ipify.org` after VPN connection, following the configured application routing; an unavailable check is shown explicitly.

Split tunneling settings are available from the main window while disconnected. Existing site/IP and application screens support the original inclusion/exclusion modes (application inclusion is Android-only). User rules persist across connections. PNT control addresses are excluded internally without overwriting either list.

The first server adapter supports AmneziaWG on Debian/Linux, both native interfaces and Docker installations. An administrator must explicitly register each exit server using `pnt vpn server install <node>` and grant client access. Update transit nodes before using client VPN transport.

Relayed UDP uses encrypted PNT TCP streams. The client prepares a second relay through a different entry node when available. A broken active stream promotes the connected standby and replenishes the reserve in the background. If every relay is unavailable, the local VPN interface, keys and address stay in place while the transport reconnects. The short packet queue is bounded and discards packets older than five seconds. TCP keepalive detects a lost relay connection, including an idle one. No new VPN configuration is issued during relay recovery. Existing inner TCP sessions can retransmit missing packets while the same exit server remains available; UDP packets can be lost during the switch. Failure or replacement of the exit server itself cannot preserve its existing NAT connections. QUIC is not included.

## Android

The Android PNT build supports Android 10 or later and AmneziaWG/WireGuard. It imports the same signed `.pnt` access profile as the Windows client, lists permitted exit servers and runs its PNT transport inside the Android VPN service. Control, direct UDP and relay sockets are individually protected from capture by the VPN. The application itself uses the VPN, including external-address detection. Direct UDP is tried first; a PNT relay is used when a direct handshake cannot be confirmed.

Install the APK, import a profile once, select Connect and choose an exit server. Android asks for permission to create a VPN. Closing the application screen leaves the VPN service running; use Disconnect to stop it.

Build requirements: JDK 17, Android SDK, NDK r27c, Qt 6.10+ for Android ARM64 and a matching desktop Qt host installation, CMake, Ninja and Conan 2. Initialize submodules first. Configure with the Qt Android toolchain:

```sh
cmake -S . -B deploy/pnt-android-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DPNT_CLIENT_ONLY=ON \
  -DCMAKE_TOOLCHAIN_FILE=<Qt-Android>/lib/cmake/Qt6/qt.toolchain.cmake \
  -DQT_HOST_PATH=<Qt-host> -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-29 -DAPP_ANDROID_MIN_SDK=29
cmake --build deploy/pnt-android-build --parallel 8
```

Use `androiddeployqt` with the generated deployment settings to prepare the Gradle project, then run `assembleOssDebug` there for an installable development APK. Production releases require a persistent signing key. Set `PNT_TEST_PROFILE` to a temporary profile generated by the server's `tests/VpnChecks` and run `testOssDebugUnitTest` to verify compatibility with .NET profile signatures; keep this profile outside the repository.

## Client updates

Windows x64 and Android ARM64 check the latest stable release of `spark108/amnezia-pnt` silently on startup and every six hours. The current version stays at the bottom center of the main window; click it for a manual check. If an update is available it shows an arrow and the new version in green. The update source is GitHub Releases, independent of the imported cluster profile. No Amnezia update service is used.

Each stable release must provide `PntVPN-windows-x64.zip` and `PntVPN-android-arm64.apk`. The updater selects the package for the current platform, compares numeric versions and verifies the size and GitHub asset SHA-256 digest before allowing installation. Drafts, prereleases, older versions and assets without a digest are rejected. Increase `PNT_VERSION` and the PNT Android version code for each release.

Click the version with an available update to download it and start installation after verification. VPN is disconnected before installation. Windows requests administrator access, waits for the GUI to close, stops the service, swaps the installation and restarts it. An unsuccessful installation restores the previous directory. Profile and split-tunneling settings remain in the user's encrypted settings. Android opens the system package installer; allow installation from PNT VPN if requested, return to the application and click the version again. The APK must keep the same application ID and compatible signing identity. An installed package cannot gain this updater without first installing a release containing it.

Connection statistics include `Ping: <server>ms/<internet>ms`. ICMP echo probes run every five seconds against the exit node's external IPv4 address and `1.1.1.1`; timeout or filtered ICMP is shown as `—`. Probes follow the system's actual routes, including split tunneling and the exclusions needed for PNT control connections. Consequently the exit node's public address uses its control-route exclusion, while the internet probe normally traverses VPN unless a split rule excludes it.

Run the standalone release metadata/version and ICMP checks with CMake in `client/tests/pnt-update` and a Qt Core/Network/Test installation.
