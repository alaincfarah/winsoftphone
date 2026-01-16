# WinSoftphone

Windows softphone client built on PJSIP with a Win32 GUI. It targets FreePBX,
supports OpenVPN-based connectivity, and includes call control, recording, and
CRM popups on incoming calls.

## Features
- SIP calling via PJSIP with FreePBX registration.
- Codecs: G.729, G.711 ulaw (PCMU), and G.711 alaw (PCMA).
- Call control: Hold, Warm Transfer, Blind Transfer, Hangup.
- Call recording to WAV.
- DTMF keypad during active calls.
- Audio device selection, volume sliders, and mute.
- Contact storage and call history in JSON.
- URL launch on incoming calls based on caller CNAME.

## Project Layout
- `src/`: C source code (Win32 UI, SIP stack, storage).
- `config/`: Default JSON configs copied to `%APPDATA%\WinSoftphone` on first run.
- `installer.iss`: Inno Setup script for packaging.

## Build (Windows)
1. Build PJSIP with required codecs (G.729 requires a licensed plugin).
2. Set `PJSIP_DIR` to the PJSIP build root (contains `include/` and `lib/`).
3. Configure and build:

```
cmake -S . -B build -DPJSIP_DIR="C:\path\to\pjsip"
cmake --build build --config Release
```

## Runtime Configuration
The app copies defaults on first launch:
- `%APPDATA%\WinSoftphone\config.json`
- `%APPDATA%\WinSoftphone\contacts.json`
- `%APPDATA%\WinSoftphone\history.json`

### Incoming Call URL
If a contact entry contains a matching `cname`, its `url` is opened.
Otherwise, the `incoming_url_template` in `config.json` is used with `{cname}`
replaced by a URL-encoded caller name.

### Transfer Flow
Enter the target in the dial field:
- Warm transfer: first click starts consult call; second click completes transfer.
- Blind transfer: click once to transfer immediately.

### Recording
Recordings are written to:
`%APPDATA%\WinSoftphone\recordings\call-YYYYMMDD-HHMMSS.wav`

## Installer
Edit `installer.iss` to match your build output and PJSIP DLL paths, then run
Inno Setup to generate the installer.