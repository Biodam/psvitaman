# PSVitaman — Spotify Remote for PlayStation Vita

[![Build PSVitaman VPK](https://github.com/Biodam/psvitaman/actions/workflows/build.yml/badge.svg)](https://github.com/Biodam/psvitaman/actions/workflows/build.yml)
[![VitaDB](https://img.shields.io/badge/VitaDB-Available-blue)](https://vitadb.rinnegatamante.it/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

> [!NOTE]
> **Disclaimer**: This project is entirely vibe coded. Expect retro vibes, cassette tape nostalgia, and community-driven experimentation.

**PSVitaman** is a standalone PlayStation Vita homebrew application (`.vpk`) functioning as a remote controller for Spotify playback. Borrowing the retro aesthetic of classic Sony Walkman portable cassette tape players, it features mechanical-style transport buttons, cassette spools that rotate during active playback, text marquee scrolling for long titles, and dual physical/touchscreen controls.

---

## Features

- **Retro Walkman Aesthetic**: Authentic cassette shell design, tape window, tape ribbon, and dual rotating cassette reels.
- **Dynamic Cassette Spools**: Real-time rotational physics when tracks are playing, with progressive tape radius (left supply reel shrinks, right take-up reel expands).
- **On-Screen QR Code Pairing**: Real-time high-contrast QR code generated directly on the PS Vita screen. Point your phone camera at the screen to immediately open the Spotify authorization page or re-pair devices.
- **Physical & Touch Controls**: Seamless control with PS Vita face buttons/triggers or by tapping the on-screen mechanical transport buttons.
- **Live HUD Display**: Track title marquee scrolling, Artist & Album names, millisecond-accurate time counter (`02:14 / 04:30`), volume percentage, and active Spotify device name.
- **Threaded Network Engine**: Non-blocking Spotify Web API synchronization via native Sony `SceHttp`/`SceSsl` with structured C exception handling (`setjmp`/`longjmp`), maintaining 60 FPS rendering on the Vita display.
- **Resilient TLS/SSL**: Native hardware HTTPS with custom SSL handshake bypass and full iTLS-Enso support for TLS 1.2/1.3 communication with Spotify's cloud.

---

## Requirements

Just like `SharkF00D` / `libshacccg.suprx` is required for modern 3D ports, modern web services (Spotify, Cloudflare, GitHub) require **TLS 1.2+** encryption:

1. **PlayStation Vita or PlayStation TV** (Firmware 3.60, 3.65 Enso, or 3.68+).
2. **[iTLS-Enso](https://github.com/SKGleba/iTLS-Enso)** (Essential for Firmware 3.60 / 3.65):
   - The PS Vita's original 2011 SSL library lacks modern root certificates and TLS 1.2 cipher suites.
   - Install **`iTLS-Enso.vpk`** (via [VitaDB Downloader](https://vitadb.rinnegatamante.it/#/info/444) or GitHub).
   - Launch iTLS-Enso and select **"Install the full iTLS package"**.
   - Reboot your PS Vita. This upgrades the system SSL engine and root certificates console-wide.
3. **Active Wi-Fi Connection** (PS Vita and smartphone connected to the same local network for one-tap QR pairing).
4. **Spotify Account** (Free or Premium). Play music on any device (phone, PC, smart speaker) and control it directly from your Vita.

## Control Scheme

| Physical Input | Touchscreen Equivalent | Action | Spotify Endpoint |
|---|---|---|---|
| **Cross ($\times$)** | Tap **PLAY / PAUSE** button | Toggle Play / Pause | `PUT /v1/me/player/play` or `pause` |
| **R-Trigger** | Tap **NEXT** button | Skip to Next Track | `POST /v1/me/player/next` |
| **L-Trigger** | Tap **PREV** button | Skip to Previous Track | `POST /v1/me/player/previous` |
| **Square ($\square$)** | Tap **SHUFFLE** button | Toggle Shuffle On/Off | `PUT /v1/me/player/shuffle` |
| **Triangle ($\triangle$)** | Tap **REPEAT** button | Cycle Repeat Mode (Off/Context/Track) | `PUT /v1/me/player/repeat` |
| **D-Pad Up** | — | Volume Up (+5%) | `PUT /v1/me/player/volume` |
| **D-Pad Down** | — | Volume Down (-5%) | `PUT /v1/me/player/volume` |
| **Select** | — | Toggle QR Code Pairing Modal Overlay | Overlay |
| **Circle ($\bigcirc$)** | — | Close QR Modal Overlay | Overlay |
| **Start** | — | Force Refresh State / Reload Config | `GET /v1/me/player` |

---

## Setup & Pairing

PSVitaman supports **pure phone-only QR pairing** without needing a PC, file transfers, or typing!

### Option A: Pure Phone-Only QR Pairing (Recommended)

1. **Install and launch** `PSVitaman.vpk` on your PS Vita.
2. **Make sure** your PS Vita and phone are connected to the same Wi-Fi network.
3. **Scan the on-screen QR code** with your smartphone's camera and tap the link to open Spotify authorization.
4. **Log in and tap "Agree"** on Spotify.
5. The GitHub Pages web bridge automatically exchanges the OAuth PKCE authorization and forwards the refresh token directly to the PS Vita's embedded local HTTP server (`http://<vita_ip>:8888/save`).
6. PSVitaman writes `ux0:data/psvitaman/config.ini`, starts playback polling, and automatically transitions into the retro cassette player!

> [!TIP]
> **Spotify Developer Dashboard Configuration**:
> Under your app settings on [developer.spotify.com/dashboard](https://developer.spotify.com/dashboard), ensure your **Redirect URIs** list includes:
> - `https://biodam.github.io/psvitaman/callback.html`
> - `https://biodam.github.io/psvitaman/`
> - `http://127.0.0.1:8888/callback` (for PC companion script)

---

### Option B: Obtain Refresh Token via PC Companion Script
If you prefer running a script on your PC:
```bash
python tools/get_token.py
```
This script will:
- Automatically load `SPOTIFY_CLIENT_ID` from `.env`.
- Support PKCE (no client secret required).
- Open your browser to approve Spotify.
- Automatically save `SPOTIFY_REFRESH_TOKEN` to `.env`, save local `config.ini`, or upload directly to your PS Vita via VitaShell FTP.

---

### Option C: Manual `config.ini` Setup
If editing manually via VitaShell USB/FTP:
Create `ux0:data/psvitaman/config.ini`:
```ini
[spotify]
client_id = 4274a722ea7d42c1b9cc3f3247a7be4a
refresh_token = YOUR_REFRESH_TOKEN
```

---

## Installation (.vpk)

1. Download `PSVitaman.vpk` from the [Releases](https://github.com/Biodam/psvitaman/releases) page or install via **VitaDB Downloader**.
2. Open **VitaShell** on your PS Vita, navigate to the downloaded `.vpk`, and press $\times$ to install.
3. Launch PSVitaman from the LiveArea.

---

## Building from Source

### Automated CI
The repository includes a GitHub Actions workflow that automatically compiles and packages `PSVitaman.vpk` using the official `vitasdk/vitasdk:latest` Docker image on every push and release.

### Local Build (with VitaSDK installed)
```bash
git clone https://github.com/Biodam/psvitaman.git
cd psvitaman
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake ..
make -j$(nproc)
```

The compiled package `PSVitaman.vpk` will be located in the `build/` directory.

---

## VitaDB Submission Metadata

- **Title**: PSVitaman
- **Title ID**: `SPOTMAN01`
- **Category**: Utilities / Multimedia
- **Author**: PSVitaman Contributors
- **License**: MIT
