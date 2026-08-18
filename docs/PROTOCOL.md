# WebSocket Protocol v1

The protocol is language-neutral. A server may be implemented in Python, .NET, Node.js, Go, Java, Rust, or any runtime that supports WebSockets and binary frames.

## Start metadata

The module sends one UTF-8 JSON text frame after connecting:

```json
{
  "type": "start",
  "version": 1,
  "callId": "freeSWITCH-uuid",
  "input": {"encoding":"pcm_s16le","sampleRate":16000,"channels":1,"frameDurationMs":20},
  "output": {"encoding":"pcm_s16le","sampleRate":8000,"channels":1}
}
```

## Audio

- Module to server: binary PCM16 caller audio.
- Server to module: binary PCM16 playback audio.
- PCM samples are signed 16-bit little-endian mono.

## Controls

```json
{"type":"playback.clear"}
{"type":"playback.pause"}
{"type":"playback.resume"}
{"type":"playback.stop"}
{"type":"ping","timestamp":123456789}
```

`playback.clear` is the barge-in primitive. It must atomically discard queued playback without stopping caller capture.

