# mod_ai_media

Language-neutral bidirectional raw-audio module for FreeSWITCH.

> Status: development preview. Do not install on production FreeSWITCH yet.

## Goals

- Stream decoded caller PCM16 to a WebSocket service.
- Receive PCM16 as binary WebSocket frames.
- Inject playback through a bounded per-call queue.
- Support `start`, `stop`, `clear`, `pause`, `resume`, and `status`.
- Keep the server implementation language-neutral (Python, .NET, Node.js, Go, Java, or Rust).
- Require TLS (`wss://`) for call audio and metadata in transit.

## Safety

This module is developed side-by-side with `mod_audio_stream`; it does not replace it. Native-rate PCM capture/playback and lifecycle code are under active validation. Build and test only on a non-production call path.

## Build

```bash
export PKG_CONFIG_PATH=/usr/local/freeswitch/lib/pkgconfig
sudo apt-get install -y cmake g++ pkg-config libevent-dev libssl-dev zlib1g-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Protocol

See [docs/PROTOCOL.md](docs/PROTOCOL.md).
