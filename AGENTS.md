# AGENTS.md — Developer & AI Agent Context Guide

This document provides complete architectural context, design patterns, debugging lessons, and operational procedures for **PSVitaman**. Any AI agent or developer continuing work in this codebase should read this document first.

---

## 1. Project Overview & Philosophy

**PSVitaman** is a retro-styled Spotify remote client for the PlayStation Vita (ARM Cortex-A9), packaged as a standalone `.vpk` homebrew.
- Modeled after iconic Sony Walkman cassette tape decks (TPS-L2, WM-F5 Sports, WM-DD).
- Features rotating tape spools with dynamic tape radius physics, marquee track scrolling, vector audio transport buttons, and authentic Walkman themes.
- Functions as an active desk clock/player: keeps the PS Vita display awake (`sceKernelPowerTick`) and incorporates micro-orbiting pixel shift to protect OLED panels on PS Vita 1000 models.
- **Zero-friction pairing**: phone scans an on-screen QR code, authorizes via Spotify PKCE OAuth on GitHub Pages, and posts tokens directly to the Vita over local Wi-Fi.

---

## 2. System Architecture

```
+---------------------------------------------------------------------------------+
|                               Main Thread (60 FPS)                              |
|  - main.c: Event loop, delta time, power tick (sceKernelPowerTick)              |
|  - input.c: Physical controls (SCE_CTRL) + Capacitive touch hit testing         |
|  - ui.c: vita2d GPU rendering, vector icons, tape animations, AMOLED orbit      |
|  - error.c: In-app modal diagnostics & crash recovery overlay                   |
+---------------------------------------------------------------------------------+
                                      |
                         Thread-Safe Command Queue / State Snapshot
                                      |
+---------------------------------------------------------------------------------+
|                            Worker Thread (worker.c)                             |
|  - Polling loop (every 1-2s): /v1/me/player                                     |
|  - Token auto-refresh via /api/token when access_token expires                  |
|  - Command execution: play/pause, next, prev, shuffle, repeat, volume           |
|  - spotify.c: REST client using embedded MbedTLS over BSD SceNet sockets        |
+---------------------------------------------------------------------------------+
                                      |
+---------------------------------------------------------------------------------+
|                        Pairing HTTP Server (http_server.c)                      |
|  - Runs on port 8888 during setup or when SELECT is pressed                     |
|  - Serves status, receives tokens at POST /save, writes ux0:data/.../config.ini |
+---------------------------------------------------------------------------------+
```

### Key Subsystems

1. **Network Engine (`src/spotify.c`)**:
   - Uses embedded **MbedTLS 3.6.5** (`mbedtls_ssl_context`, `mbedtls_net_context`).
   - Does **NOT** depend on Sony `SceHttp`, `SceSsl`, or `iTLS-Enso`. Works natively on 3.60, 3.65 Enso, and 3.68+ firmwares.
   - Sockets run through Vita's native `SceNet` stack (`sceNetSocket`, `sceNetConnect`, `sceNetSend`, `sceNetRecv`).
   - Bundles Mozilla root CA certificates (`src/ca_cert.h`) for full TLS verification.

2. **Setup & Pairing Server (`src/http_server.c`)**:
   - Embedded non-blocking socket server on port 8888.
   - Generates QR code targeting `https://biodam.github.io/psvitaman/?ip=<vita_ip>&port=8888`.
   - Web application handles PKCE OAuth, exchanges code for refresh token, and pushes it directly to `POST http://<vita_ip>:8888/save`.
   - Multi-chunk receiver buffers incoming HTTP headers and JSON/URL-encoded bodies completely.

3. **Background Worker (`src/worker.c`)**:
   - Runs as a dedicated Vita kernel thread (`sceKernelCreateThread("psvitaman_worker", ...)` with a 128 KB stack).
   - Manages token lifetimes and maintains atomic playback state snapshots (`SpotifyPlaybackState`).
   - Command ring buffer receives playback inputs asynchronously from the main thread without blocking rendering.

4. **UI & Graphics (`src/ui.c`)**:
   - Powered by `vita2d` (GPU-accelerated PVR/SGX543 rendering, 960x544).
   - Uses system PGF font (`sceSysmoduleLoadModule(SCE_SYSMODULE_PGF)`).
   - **Authentic Cassette Tape Physics**:
     - **Conservation of Linear Speed**: $v = \omega \cdot r$ ($4.76 \text{ cm/s}$). Empty spools turn $\sim 2.6\times$ faster than full tape packs ($200^\circ/\text{s}$ vs $78^\circ/\text{s}$). As supply depletes, left reel accelerates; as take-up fills, right reel decelerates.
     - **Motor Inertia**: Smooth exponential acceleration/deceleration on play and pause.
     - **Wow & Flutter**: Micro-harmonic speed fluctuation ($\pm 2\%$) modeling realistic belt-drive slip and motor vibrations.
     - **Fast Whir Cueing**: Brief 3.5x speed burst on track skips and seeking.
     - **Tangential Ribbon Routing**: Tape peels tangentially off the supply reel, loops around flanged guide rollers, and feeds into the take-up reel dynamically.
     - **Mechanical Tape Counter**: Geared to cumulative revolutions of the take-up reel rather than flat wall-clock seconds.
   - 100% procedurally drawn vector icons: Play, Pause, Prev, Next, Shuffle, Repeat, and the authentic circular Theme button.

5. **AMOLED Burn-in Protection (`src/ui.c`)**:
   - PS Vita 1000 units feature Samsung Super AMOLED displays vulnerable to static image burn-in.
   - Implements 8-point orbital pixel shifting (`s_shift_x`, `s_shift_y`) updated every 45 seconds by $\pm 2$ pixels.
   - Shifts HUD text, cassette window, and control deck coordinates imperceptibly while keeping pixels active.

6. **Screen Awake Keepalive (`src/main.c`)**:
   - Calls `sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT)` every frame to prevent Vita auto-dim and standby during long listening sessions.

7. **Tactile Sound Engine (`src/sound.c`, `src/sound_data.h`)**:
   - Uses embedded `SceAudioOut` stereo playback at 44.1 kHz.
   - Dedicated background audio thread (`psvitaman_audio`, priority `0x10000100 + 10`, 64 KB stack) streaming PCM buffers asynchronously without blocking the 60 FPS main thread.
   - 100% procedural embedded PCM waveform tables (`sound_data.h`) with zero external file dependencies:
     - Heavy mechanical cassette lever latch clack (50 ms) for transport controls (Play, Pause, Skip, Prev, Shuffle, Repeat, Reset).
     - Crisp tactile rotary detent click (20 ms) for Volume adjustments, Theme cycling, and Refresh.

---

## 3. Themes

Configured in `src/ui.h` & `src/ui.c`, persisted in `ux0:data/psvitaman/config.ini` (`[ui] theme = <id>`):

| Theme ID | Name | Inspiration / Colorway |
|---|---|---|
| `0` (`THEME_TPS_L2`) | **TPS-L2 (1979)** | Classic Sony Walkman Metallic Blue, Silver brushed aluminum, Hot Line Orange theme button. |
| `1` (`THEME_SPORTS_YELLOW`) | **Sports WM-F5 (1983)** | Vivid Action Yellow, Rubber Black bumper accents, Turquoise/Teal theme button. |
| `2` (`THEME_GRAPHITE_DD`) | **Graphite WM-DD (1982)** | Dark Charcoal Anthracite, Brushed Chrome/Platinum, Crimson Ruby theme button. |
| `3` (`THEME_STEALTH_OLED`) | **Stealth AMOLED** | True 0-IRE Pitch Black, High-Contrast Phosphor Green accents, lowest power draw. |

**Controls to Switch Themes**:
- Press **D-Pad Right** (next theme) or **D-Pad Left** (previous theme).
- Tap the **THEME button** (`[< >]`) on the bottom touchscreen bar.

---

## 4. Control Scheme & UI Layout

```
+--------------------------------------------------------------------+
| [HUD]  TRACK TITLE - ARTIST                      VOL: 85%  [WIFI]  |  Y: 6..54
+--------------------------------------------------------------------+
|                                                                    |
|                      CASSETTE TAPE BAY                             |
|          [ (O)           === TAPE ===           (O) ]              |  Y: 58..434
|               Supply Reel             Take-up Reel                 |
|                                                                    |
+--------------------------------------------------------------------+
| [THEME]   [SHUFFLE]    [PREV]      [PLAY]      [NEXT]    [REPEAT]  |  Y: 446..532
|  [< >]      [SQ]        [L]         [X]         [R]       [TRI]    |
+--------------------------------------------------------------------+
```

| Physical Button | Touchscreen Hitbox | Action |
|---|---|---|
| **Cross ($\times$)** | **PLAY / PAUSE** (Center) | Toggle Playback |
| **R-Trigger** | **NEXT** | Skip Track |
| **L-Trigger** | **PREV** | Previous Track |
| **Square ($\square$)** | **SHUFFLE** | Toggle Shuffle |
| **Triangle ($\triangle$)** | **REPEAT** | Cycle Repeat (Off / Context / Track) |
| **D-Pad Right** | **THEME** (Far Left) | Next Theme |
| **D-Pad Left** | — | Previous Theme |
| **D-Pad Up / Down** | — | Volume $\pm 5\%$ |
| **Select** | — | Reset Pairing / Clear Config / Enter Setup Mode |
| **Start** | — | Force State Refresh (or reload config in Setup Mode) |
| **Circle ($\bigcirc$)** | — | Dismiss Error Modal |

---

## 5. Critical Technical Lessons & Gotchas

> [!WARNING]
> These are hard-won debugging lessons specific to the PS Vita toolchain. Pay close attention:

1. **VitaSDK Newlib `%zu` Bug**:
   - The newlib implementation in VitaSDK `vitasdk/vitasdk:latest` **does NOT format `%zu` correctly** in `snprintf` / `sprintf`. It outputs literal `"zu"`.
   - In HTTP requests, `Content-Length: zu\r\n` causes Spotify and other servers to reject requests with `400 Bad Request`.
   - **Rule**: ALWAYS cast `size_t` to `(unsigned long)` with `%lu` or `(unsigned int)` with `%u`.

2. **HTTP Server Socket Fragmentation**:
   - Modern phone browsers send HTTP POST headers and body in separate TCP packets.
   - Never assume a single `recv()` call returns the entire HTTP body. Always inspect `Content-Length` and loop `recv()` until the body is complete before parsing JSON or URL parameters.

3. **Spotify 403 "Restricted Player Control" Errors**:
   - When calling shuffle or repeat on certain Spotify playlists (e.g. Daily Mixes, podcasts, radio contexts), Spotify returns HTTP 403 with `PLAYER_COMMAND_RESTRICTED`.
   - This occurs even for Premium subscribers! Do not display a fatal error modal for this; log as a warning and keep playback active.

4. **Vita Thread Stack Limits**:
   - Default thread stacks on PS Vita are small. The worker thread uses `128 * 1024` bytes (128 KB).
   - Never allocate huge local arrays (e.g. 64 KB buffers) on the stack inside worker functions; use static buffers or heap allocation.

5. **Resetting Pairing Cleanly**:
   - When a user presses **SELECT**, delete `ux0:data/psvitaman/config.ini`, zero the `AppConfig` struct, stop the worker, and start `http_server_start(8888, &config)`.

---

## 6. Directory Structure

```
psvitaman/
├── .github/workflows/
│   └── build.yml               # GitHub Actions VitaSDK CI build
├── assets/                     # LiveArea assets (icon0.png, bg0.png, template.xml)
├── docs/                       # Web QR pairing bridge (GitHub Pages)
│   ├── index.html              # Pairing gateway & PKCE OAuth handler
│   └── callback.html           # Spotify OAuth redirect target
├── scripts/
│   └── ftp_sync.ps1            # VitaShell FTP upload automation script
├── src/
│   ├── ca_cert.h               # Mozilla Root CA bundle for MbedTLS
│   ├── config.c / .h           # INI configuration manager (ux0:data/psvitaman/config.ini)
│   ├── error.c / .h            # In-app diagnostic error modal system
│   ├── http_server.c / .h      # Micro-HTTP pairing server (port 8888)
│   ├── input.c / .h            # Physical buttons + Capacitive touch hit testing
│   ├── logger.c / .h           # Diagnostic file & UART logger (ux0:data/psvitaman/psvitaman.log)
│   ├── main.c                  # Application entry point, event loop, power tick
│   ├── qrcodegen.c / .h        # QR code generator for screen display
│   ├── sound.c / .h            # Mechanical sound effects audio engine (SceAudioOut)
│   ├── sound_data.h            # Procedural 44.1 kHz PCM audio tables (clack, click)
│   ├── spotify.c / .h          # Spotify Web API client (MbedTLS over BSD SceNet)
│   ├── ui.c / .h               # vita2d renderer, cassette physics, themes, AMOLED orbit
│   └── worker.c / .h           # Background sync & command worker thread
├── tools/
│   └── get_token.py            # Optional PC companion pairing script
├── CMakeLists.txt              # VitaSDK CMake build specification
├── AGENTS.md                   # This developer & AI guide
└── README.md                   # User-facing documentation & guide
```

---

## 7. Build & Deployment

### Building with VitaSDK
```bash
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake ..
make -j$(nproc)
```
Output: `build/PSVitaman.vpk`

### Deploying to PS Vita (VitaShell FTP)
Target Vita IP is configured in `scripts/ftp_sync.ps1` (default `192.168.1.88:1337`):
```powershell
powershell -ExecutionPolicy Bypass -File scripts/ftp_sync.ps1
```
- Uploads `PSVitaman.vpk` to `ux0:data/PSVitaman.vpk`.
- Automatically downloads diagnostic log `ux0:data/psvitaman/psvitaman.log` for immediate debugging.
