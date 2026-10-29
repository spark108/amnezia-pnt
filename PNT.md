# PNT VPN Windows client

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

The first server adapter supports AmneziaWG on Debian/Linux, both native interfaces and Docker installations. An administrator must explicitly register each exit server using `pnt vpn add server <node> <display-name>` and grant client access. Update transit nodes before using client VPN transport.

The first relayed UDP transport uses encrypted PNT TCP streams. It preserves packet boundaries, but packet loss can increase latency. If the relay session breaks, the VPN disconnects and a new Connect can select another verified route. QUIC and seamless stream replacement are not included in this version.
