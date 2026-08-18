# mod_ai_media

Secure, language-neutral, bidirectional raw-audio streaming for FreeSWITCH.

`mod_ai_media` connects an active FreeSWITCH call to an external real-time media service over TLS WebSockets. It sends decoded caller audio as raw PCM and accepts raw PCM for direct injection into the call—without temporary playback files.

> **Project status:** Development preview. The module builds and loads successfully on FreeSWITCH `1.10.12-release` (64-bit, source installation under `/usr/local/freeswitch`). Core automated tests pass. Live-call audio quality, long-duration stability and production-scale concurrency are still being validated.

## Why it exists

File-based playback adds disk I/O, queueing delay and cleanup complexity to conversational calling. This module provides a direct media layer suitable for:

- Real-time voice agents and conversational IVR
- Speech recognition and speech analytics
- Streaming text-to-speech playback
- Barge-in and interruption handling
- Live transcription, quality monitoring and compliance workflows
- Python, .NET, Node.js, Go, Java, Rust or any WebSocket-capable backend

## Current capabilities

- Per-call bidirectional PCM16 streaming
- Native FreeSWITCH decoded audio capture
- Direct audio injection through a media bug write-replace frame
- Bounded per-call playback ring buffer
- Playback `clear`, `pause`, `resume` and `stop` controls
- Caller capture remains active during playback pause for reliable barge-in
- Manual status reporting with queue depth
- Secure `wss://` transport for remote services; plaintext is allowed only on exact OS loopback addresses
- No changes to an existing dialplan are required for manual API-driven testing
- Native `ai_media` dialplan application for normal call routing
- Can coexist with `mod_audio_stream`; it does not replace or modify that module

## Media path

```text
Caller/SIP
    │ decoded PCM16
    ▼
FreeSWITCH call ── mod_ai_media ── WSS binary frames ── Media service
FreeSWITCH call ◀─ mod_ai_media ◀─ WSS binary PCM16  ◀─ TTS/AI/backend
```

The backend language is irrelevant. The integration boundary is the documented WebSocket protocol, not a Python- or vendor-specific API.

## Requirements

- Linux x86-64
- FreeSWITCH development headers and `freeswitch.pc`
- CMake 3.16+
- C++17 compiler
- pkg-config
- libevent development package
- OpenSSL development package
- zlib development package
- A trusted `wss://` media endpoint for live testing

The WebSocket dependency is fetched from `amigniter/libwsc` at a pinned commit. TLS is enabled during its build.

## Build from Git

The following commands were used successfully on Ubuntu 22.04 with FreeSWITCH installed in `/usr/local/freeswitch`:

```bash
cd /usr/src
git clone https://github.com/Mkali10/mod-ai-media.git
cd /usr/src/mod-ai-media

apt-get update
apt-get install -y \
  build-essential \
  cmake \
  pkg-config \
  libevent-dev \
  libssl-dev \
  zlib1g-dev

export PKG_CONFIG_PATH=/usr/local/freeswitch/lib/pkgconfig
bash scripts/build.sh
```

A successful build ends with:

```text
[100%] Built target mod_ai_media
100% tests passed, 0 tests failed out of 1
```

The resulting module is:

```text
/usr/src/mod-ai-media/build/mod_ai_media.so
```

## Verify before installation

```bash
file /usr/src/mod-ai-media/build/mod_ai_media.so
ldd /usr/src/mod-ai-media/build/mod_ai_media.so
```

Do not continue if `ldd` reports any dependency as `not found`.

## Install and load without restarting FreeSWITCH

Copying and manually loading the module does not edit the dialplan, SIP profiles or `modules.conf.xml`:

```bash
install -m 755 \
  /usr/src/mod-ai-media/build/mod_ai_media.so \
  /usr/local/freeswitch/mod/mod_ai_media.so

/usr/local/freeswitch/bin/fs_cli -x "load mod_ai_media"
/usr/local/freeswitch/bin/fs_cli -x "module_exists mod_ai_media"
/usr/local/freeswitch/bin/fs_cli -x "uuid_ai_media"
```

Expected verification:

```text
true
-USAGE uuid_ai_media <uuid> <start <ws-url> [metadata]|stop|clear|pause|resume|status>
```

Manual loading is intentionally used during validation. Automatic loading should be configured only after live-call and stability testing is complete.

## Command reference

All commands operate on an active FreeSWITCH call UUID.

```bash
# Attach the call to a secure media service
uuid_ai_media <uuid> start wss://voice.example.com/media [metadata]

# Display WebSocket/playback state
uuid_ai_media <uuid> status

# Immediately discard queued AI audio (barge-in primitive)
uuid_ai_media <uuid> clear

# Pause AI playback; caller capture continues
uuid_ai_media <uuid> pause

# Resume AI playback
uuid_ai_media <uuid> resume

# Detach the media service from the call
uuid_ai_media <uuid> stop
```

Use `show channels` in `fs_cli` to locate the UUID of an active test call.

For normal routing, use the native dialplan application:

```xml
<action application="ai_media" data="start ws://127.0.0.1:8765/ws pipecat_test"/>
```

The complete extension and Pipecat server are available in [`examples/pipecat-bot`](examples/pipecat-bot).

## Live-call test sequence

1. Start a trusted WSS media service implementing [Protocol v1](docs/PROTOCOL.md).
2. Establish a test call and keep it connected.
3. Run `show channels` and copy the call UUID.
4. Run `uuid_ai_media <uuid> start wss://host/media`.
5. Confirm `uuid_ai_media <uuid> status` reports `connected=true`.
6. Verify caller-to-server PCM and server-to-caller PCM independently.
7. While playback is active, run `clear` and verify immediate interruption.
8. Run `stop` before ending the test, then review FreeSWITCH logs.

## WebSocket protocol

The complete wire contract is documented in [docs/PROTOCOL.md](docs/PROTOCOL.md).

Summary:

- First module-to-server message: JSON `start` metadata
- Module-to-server audio: binary signed 16-bit little-endian mono PCM
- Server-to-module audio: binary signed 16-bit little-endian mono PCM
- Sample rates are announced in the `start` message
- Text control messages manage playback state
- `playback.clear` drops buffered playback without interrupting caller capture

Backends must send audio at the announced output sample rate. Incorrect rate, sample format or pacing can cause distorted, fast, slow or choppy playback.

## Barge-in behavior

Barge-in is application-controlled:

1. Caller audio continues streaming to the backend during AI playback.
2. Backend VAD/STT detects that the caller started speaking.
3. Backend sends `{"type":"playback.clear"}`.
4. The module atomically drops queued playback.
5. Backend cancels the current TTS/response and generates the next response.

Emotion, intent, language detection and conversational policy belong to the backend. The module remains a fast, language-neutral media transport.

## Security

- Remote endpoints must use `wss://`. Plain `ws://` is accepted only for exact same-host loopback (`127.0.0.1` or `::1`), where traffic never leaves the server.
- Call audio and metadata must never be sent over plaintext WebSockets.
- Use a certificate trusted by the server operating system.
- Keep media endpoints private or authenticated at the reverse proxy/service layer.
- Never place API secrets in command metadata or logs.
- Apply network allowlists, rate limits, observability and retention policies before production use.
- Treat raw call audio as sensitive data and follow applicable consent, privacy and recording laws.

## Production and scaling notes

The current implementation uses one session and bounded playback buffer per attached call. Before offering a production or paid service, validate:

- Concurrent calls and calls-per-second targets
- CPU and memory usage at each codec/sample rate
- Network jitter and WebSocket backpressure
- Reconnect and backend-failure behavior
- Long-duration call stability
- Graceful shutdown and FreeSWITCH reload behavior
- Audio latency, packet pacing and barge-in response time
- Monitoring, metrics, alerting and audit logs
- Tenant isolation, authentication, quotas and billing integration
- Data retention, consent and regional compliance requirements

No concurrency or latency claim should be published until it has been measured on the intended hardware and call profile.

## Commercial deployment model

The module can serve as the native media component of a commercial platform. A typical paid architecture adds:

- Tenant-aware media gateway
- API-key or signed-token authentication
- Usage metering by call/minute/channel
- Plan limits and concurrency enforcement
- Provider-agnostic STT, TTS and conversational engines
- Webhooks, call events and reporting
- Central monitoring and customer support tooling
- High availability and regional media nodes

Commercial packaging, support terms, SLA, pricing and licensing must be defined before external distribution. The repository currently represents development source and does not claim production certification or an SLA.

## Updating the source

```bash
cd /usr/src/mod-ai-media
git pull origin main
export PKG_CONFIG_PATH=/usr/local/freeswitch/lib/pkgconfig
bash scripts/build.sh
```

After rebuilding, repeat `file` and `ldd` validation before replacing a loaded module. Do not overwrite a module while it is loaded; unload it during a controlled maintenance/test window first.

## Troubleshooting

### `build: No such file or directory`

The build folder is inside the repository. Use the absolute path:

```bash
find /usr/src/mod-ai-media/build -type f -name "mod_ai_media.so" -ls
```

### Module does not load

```bash
ldd /usr/local/freeswitch/mod/mod_ai_media.so
tail -n 100 /usr/local/freeswitch/log/freeswitch.log
```

### API returns `session not found`

The UUID must belong to a currently active call. Run `show channels` again.

### API returns `connected=false`

Check DNS, certificate trust, firewall access and WSS service logs. The asynchronous connection may also still be starting; check status again after the handshake.

### Playback is choppy or distorted

Verify exact PCM16 little-endian mono format, output sample rate, frame pacing, backend buffer size and network jitter. Do not send WAV headers, MP3, Opus or encoded telephony payloads as binary playback frames.

## Compatibility and non-interference

`mod_ai_media` is installed as a separate module and API. It does not alter existing FreeSWITCH configuration and can coexist with `mod_audio_stream`. Do not attach multiple media/playback modules to the same production call until their media-bug interaction has been tested.

## License and support

No open-source or commercial license grant is currently included in this repository. All rights remain with the repository owner unless a separate written license is provided. Define the final commercial license, support channel and SLA before customer distribution.
