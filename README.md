# mod_ai_media

Language-neutral bidirectional raw-audio module for FreeSWITCH.

> Status: Milestone 0 scaffold. Do not install on production FreeSWITCH yet.

## Goals

- Stream decoded caller PCM16 to a WebSocket service.
- Receive PCM16 as binary WebSocket frames.
- Inject playback through a bounded per-call queue.
- Support `start`, `stop`, `clear`, `pause`, `resume`, and `status`.
- Keep the server implementation language-neutral (Python, .NET, Node.js, Go, Java, or Rust).

## Safety

This module is developed side-by-side with `mod_audio_stream`; it does not replace it. The current scaffold only registers the `uuid_ai_media` API and returns `-ERR not implemented` for media actions.

## Build

```bash
export PKG_CONFIG_PATH=/usr/local/freeswitch/lib/pkgconfig
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Protocol

See [docs/PROTOCOL.md](docs/PROTOCOL.md).

