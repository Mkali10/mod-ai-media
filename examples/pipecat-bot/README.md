# Pipecat calling example

This example replaces the old `mod_audio_stream` file-playback path. It does not use `esl_client.py`, temporary `.r8` files, base64 playback messages or `uuid_broadcast`.

Files used at runtime:

- `bot.py`
- `mod_ai_media_serializer.py`
- `.env`
- FreeSWITCH dialplan file `76543_mod_ai_media.xml`

Run the bot from the existing virtual environment:

```bash
cd /root/freeswitch-ai-bot
source .venv/bin/activate
python bot.py
```

The same-host bot listens only on `127.0.0.1:8765`. Remote media services must use trusted `wss://` endpoints.
