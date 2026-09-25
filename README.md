# PSVitaman — Spotify Remote for PlayStation Vita

[![Build PSVitaman VPK](https://github.com/fabio/psvitaman/actions/workflows/build.yml/badge.svg)](https://github.com/fabio/psvitaman/actions/workflows/build.yml)
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
- **Threaded Network Engine**: Non-blocking Spotify Web API synchronization via `libcurl`, maintaining 60 FPS rendering on the Vita display.
- **Resilient TLS/SSL**: Bundled with modern Mozilla CA bundle (`cacert.pem`) for HTTPS compatibility on all Vita firmware versions.

---

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

## Setup & Configuration

PSVitaman supports **zero-config builds** via `.env` as well as runtime configuration via `ux0:data/psvitaman/config.ini`.

### Option A: Zero-Config VPK Build (Recommended)
You can embed your Client ID (and optionally your Refresh Token) directly into the VPK so that no manual configuration is needed on the Vita:

1. Create a `.env` file in the project root (this file is ignored by git):
   ```env
   SPOTIFY_CLIENT_ID=4274a722ea7d42c1b9cc3f3247a7be4a
   # Optional: To skip any configuration on Vita entirely:
   # SPOTIFY_REFRESH_TOKEN=your_refresh_token
   ```
2. Build `PSVitaman.vpk` (or build in GitHub Actions with Secrets).
3. Install `PSVitaman.vpk` on your Vita — the on-screen QR code will automatically link directly to your app's Spotify authorization without editing any files!

### Option B: Obtain Refresh Token via Companion Script
Run the companion script on your PC:
```bash
python tools/get_token.py
```
This script will:
- Automatically load `SPOTIFY_CLIENT_ID` from `.env`.
- Support PKCE (no client secret required).
- Open your browser to approve Spotify.
- Automatically save `SPOTIFY_REFRESH_TOKEN` to `.env` (for zero-config builds), save local `config.ini`, or upload directly to your PS Vita via VitaShell FTP.

### Option C: Manual `config.ini` Setup
If using a generic VPK without baked-in credentials:
Create `ux0:data/psvitaman/config.ini`:
```ini
[spotify]
client_id = 4274a722ea7d42c1b9cc3f3247a7be4a
refresh_token = YOUR_REFRESH_TOKEN
```

> [!NOTE]
> When `SPOTIFY_CLIENT_ID` is embedded at build time, PSVitaman's on-screen QR code and `tools/get_token.py` immediately use it out-of-the-box. If `config.ini` does not exist on the Vita, it will auto-create a template pre-populated with your app's Client ID.

---

## Installation (.vpk)

1. Download `PSVitaman.vpk` from the [Releases](https://github.com/fabio/psvitaman/releases) page or install via **VitaDB Downloader**.
2. Open **VitaShell** on your PS Vita, navigate to the downloaded `.vpk`, and press $\times$ to install.
3. Launch PSVitaman from the LiveArea.

---

## Building from Source

### Automated CI
The repository includes a GitHub Actions workflow that automatically compiles and packages `PSVitaman.vpk` using the official `vitasdk/vitasdk:latest` Docker image on every push and release.

### Local Build (with VitaSDK installed)
```bash
git clone https://github.com/fabio/psvitaman.git
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
