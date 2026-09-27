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
- **Walkman-Inspired Themes**: 8 authentic colorways inspired by iconic Sony models (1979 TPS-L2 Blue, 1983 WM-F5 Sports Yellow, 1982 WM-DD Graphite, 1981 WM-2 Red, 1984 WM-D6C Pro, 1987 My First Sony, 1989 WM-701C 10th Anniversary Gold, and 0-IRE Stealth AMOLED Black).
- **AMOLED Burn-In Safety**: Periodic 8-point orbital pixel shifting ($\pm 2\text{px}$) designed specifically for PS Vita 1000 OLED displays to prevent static image burn-in.
- **Continuous Desk Playback**: Prevents the PS Vita screen from dimming or going to sleep while active.
- **Authentic Cassette Tape Physics**: Independent spool speeds following linear tape velocity ($v = \omega \cdot r$ — small reels spin $\sim 2.6\times$ faster than full ones), smooth motor spin-up/spin-down inertia, organic belt-drive wow & flutter, track-skip fast whir bursts, tangential tape ribbon routing, and a mechanical 3-digit counter geared to reel revolutions.
- **On-Screen QR Code Pairing**: Real-time high-contrast QR code generated directly on the PS Vita screen. Point your phone camera at the screen to immediately open the Spotify authorization page or re-pair devices.
- **Physical & Touch Controls**: 6 mechanical transport buttons positioned across the bottom of the screen with vector iconography and tactile visual feedback.
- **Live HUD Display**: Battery indicator gauge with charge status, track title marquee scrolling, Artist & Album names, millisecond-accurate time counter (`02:14 / 04:30`), volume percentage meter, and active Spotify device name.
- **Mechanical Sound Effects**: Procedural cassette transport lever "clacks" on playback commands and crisp tactile "clicks" on volume and theme toggles via native `SceAudioOut` with zero external audio assets.
- **Embedded MbedTLS Network Engine**: Full TLS 1.2/1.3 communication with Spotify Web API using built-in MbedTLS 3.6.5. No external SSL modules or `iTLS-Enso` required!

---

## Requirements

1. **PlayStation Vita or PlayStation TV** (Any firmware: 3.60, 3.65 Enso, or 3.68–3.74).
2. **Active Wi-Fi Connection** (PS Vita and smartphone connected to the same local network for one-tap QR pairing).
3. **Spotify Account** (Free or Premium). Play music on any device (phone, PC, smart speaker) and control it directly from your Vita.

> [!NOTE]
> Unlike older homebrew that relied on the Vita's legacy 2011 SSL stack, **PSVitaman embeds MbedTLS 3.6.5 statically**. It communicates securely with Spotify's modern cloud servers out of the box without requiring `iTLS-Enso` or firmware modifications.

---

## Control Scheme

```
+--------------------------------------------------------------------+
| [HUD] SONY [MODEL]  DEV: Speaker  PHONES (o)(o)  VOL: 85%  [BAT 85%] * |
+--------------------------------------------------------------------+
|                                                                    |
|                      CASSETTE TAPE BAY                             |
|          [ (O)           === TAPE ===           (O) ]              |
|                                                                    |
+--------------------------------------------------------------------+
| [THEME]     [PLAY]      [PREV]      [NEXT]    [SHUFFLE]   [REPEAT]  |
|  [< >]       [X]         [L]         [R]        [SQ]       [TRI]    |
+--------------------------------------------------------------------+
```

| Physical Input | Touchscreen Equivalent | Action | Description |
|---|---|---|---|
| **Cross ($\times$)** | Tap **PLAY / PAUSE** button | Toggle Play / Pause | Start or pause Spotify playback |
| **L-Trigger** | Tap **PREV** button | Skip Previous Track | Skip to previous song or track start |
| **R-Trigger** | Tap **NEXT** button | Skip Next Track | Skip to the next song |
| **Square ($\square$)** | Tap **SHUFFLE** button | Toggle Shuffle | Toggle shuffle mode on/off |
| **Triangle ($\triangle$)** | Tap **REPEAT** button | Cycle Repeat Mode | Cycle Repeat Off $\to$ Context $\to$ Track |
| **D-Pad Right** | Tap **THEME** button (`[< >]`) | Next Theme | Cycle forward through Walkman themes |
| **D-Pad Left** | — | Previous Theme | Cycle backward through Walkman themes |
| **D-Pad Up** | — | Volume Up | Increase playback volume (+5%) |
| **D-Pad Down** | — | Volume Down | Decrease playback volume (-5%) |
| **Select** | — | Reset Pairing | Wipe configuration & enter Setup Mode |
| **Start** | — | Force Refresh | Force immediate state refresh / reload config |
| **Circle ($\bigcirc$)** | — | Dismiss Error Modal | Clear on-screen error dialogue |

---

## Themes

Switch themes on the fly using **D-Pad Left / Right** or by tapping the red circular **THEME** button on the bottom deck. Your selection is automatically saved to `ux0:data/psvitaman/config.ini`:

- **TPS-L2 (1979)**: The original metallic blue and brushed silver styling that started it all, accented with the classic "Hot Line" orange button.
- **Sports WM-F5 (1983)**: Bold Action Yellow body with matte black accents and turquoise highlights, celebrating Sony's legendary rugged cassette player.
- **Graphite WM-DD (1982)**: Premium dark charcoal anthracite cassette chassis with chrome and platinum trim.
- **Stealth AMOLED**: 0-IRE true black background with neon phosphor green accents, eliminating OLED power draw on the PS Vita 1000 display.
- **WM-2 Red (1981)**: Vivid Japanese Crimson Red body, matte black piano-key buttons, and amber gold accents inspired by the bestselling second-generation Walkman.
- **WM-D6C Pro (1984)**: Studio anodized matte gunmetal black chassis, Dolby gold lettering, Type IV metal cassette, and studio red LED peak indicators honoring Sony's direct-drive quartz field recorder.
- **My First Sony (1987)**: Playful pop culture classic featuring pure primary red chassis, cobalt blue transport buttons, sunflower yellow accents, and iconic sky blue spools.
- **WM-701C 10th Anniv. Gold (1989)**: Ultra-sleek champagne titanium gold chassis, royal navy deck keys, and polished brass spools celebrating 10 years of Walkman innovation.

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
