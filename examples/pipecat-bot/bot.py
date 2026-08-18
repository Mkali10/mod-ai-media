"""FreeSWITCH mod_ai_media <-> Pipecat realtime voice bot."""

import asyncio
import os

from dotenv import load_dotenv
from fastapi import FastAPI, WebSocket
import uvicorn

from pipecat.audio.vad.silero import SileroVADAnalyzer
from pipecat.audio.vad.vad_analyzer import VADParams
from pipecat.frames.frames import LLMRunFrame
from pipecat.pipeline.pipeline import Pipeline
from pipecat.pipeline.runner import PipelineRunner
from pipecat.pipeline.task import PipelineParams, PipelineTask
from pipecat.processors.aggregators.llm_context import LLMContext
from pipecat.processors.aggregators.llm_response_universal import (
    LLMContextAggregatorPair,
    LLMUserAggregatorParams,
)
from pipecat.transports.websocket.fastapi import (
    FastAPIWebsocketParams,
    FastAPIWebsocketTransport,
)

from mod_ai_media_serializer import ModAiMediaSerializer

load_dotenv()

AI_PROVIDER = os.getenv("AI_PROVIDER", "gemini").strip().lower()
GOOGLE_API_KEY = os.getenv("GOOGLE_API_KEY")
OPENAI_API_KEY = os.getenv("OPENAI_API_KEY")
GEMINI_MODEL = os.getenv("GEMINI_MODEL", "models/gemini-3.1-flash-live-preview")
GEMINI_VOICE = os.getenv("GEMINI_VOICE", "Charon")
BOT_HOST = os.getenv("BOT_HOST", "127.0.0.1")
BOT_PORT = int(os.getenv("BOT_PORT", "8765"))
SYSTEM_PROMPT = os.getenv(
    "SYSTEM_PROMPT",
    "You are a friendly, helpful voice assistant on a phone call. "
    "Keep replies short and conversational. Respond in the caller's language.",
)

app = FastAPI(title="mod_ai_media Pipecat Voice Bot")


def create_phone_vad() -> SileroVADAnalyzer:
    return SileroVADAnalyzer(
        params=VADParams(
            confidence=0.5,
            start_secs=0.1,
            stop_secs=0.5,
            min_volume=0.1,
        )
    )


def build_gemini_pipeline(transport: FastAPIWebsocketTransport) -> Pipeline:
    if not GOOGLE_API_KEY:
        raise RuntimeError("GOOGLE_API_KEY is missing from .env")

    from pipecat.services.google.gemini_live.llm import (
        GeminiLiveLLMService,
        GeminiVADParams,
        InputParams,
    )

    llm = GeminiLiveLLMService(
        api_key=GOOGLE_API_KEY,
        model=GEMINI_MODEL,
        voice_id=GEMINI_VOICE,
        system_instruction=SYSTEM_PROMPT,
        params=InputParams(vad=GeminiVADParams(disabled=True)),
    )
    context = LLMContext()
    user, assistant = LLMContextAggregatorPair(
        context,
        user_params=LLMUserAggregatorParams(vad_analyzer=create_phone_vad()),
    )
    return Pipeline([transport.input(), user, llm, transport.output(), assistant])


def build_openai_pipeline(transport: FastAPIWebsocketTransport) -> Pipeline:
    if not OPENAI_API_KEY:
        raise RuntimeError("OPENAI_API_KEY is missing from .env")

    from pipecat.services.openai.llm import OpenAILLMService
    from pipecat.services.openai.stt import OpenAISTTService
    from pipecat.services.openai.tts import OpenAITTSService

    stt = OpenAISTTService(api_key=OPENAI_API_KEY)
    llm = OpenAILLMService(api_key=OPENAI_API_KEY)
    tts = OpenAITTSService(api_key=OPENAI_API_KEY, voice="alloy")
    context = LLMContext(messages=[{"role": "system", "content": SYSTEM_PROMPT}])
    user, assistant = LLMContextAggregatorPair(
        context,
        user_params=LLMUserAggregatorParams(vad_analyzer=create_phone_vad()),
    )
    return Pipeline(
        [transport.input(), stt, user, llm, tts, transport.output(), assistant]
    )


def build_pipeline(transport: FastAPIWebsocketTransport) -> Pipeline:
    if AI_PROVIDER == "gemini":
        return build_gemini_pipeline(transport)
    if AI_PROVIDER == "openai":
        return build_openai_pipeline(transport)
    raise RuntimeError("AI_PROVIDER must be gemini or openai")


@app.get("/health")
async def health() -> dict:
    return {"status": "ok", "provider": AI_PROVIDER}


@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    peer = f"{websocket.client.host}:{websocket.client.port}" if websocket.client else "unknown"
    print(f"[WS] FreeSWITCH connected: {peer}", flush=True)

    serializer = ModAiMediaSerializer()
    transport = FastAPIWebsocketTransport(
        websocket=websocket,
        params=FastAPIWebsocketParams(
            audio_in_enabled=True,
            audio_out_enabled=True,
            serializer=serializer,
        ),
    )

    try:
        pipeline = build_pipeline(transport)
        task = PipelineTask(pipeline, params=PipelineParams(allow_interruptions=True))
        await task.queue_frames([LLMRunFrame()])
        await PipelineRunner().run(task)
    except asyncio.CancelledError:
        raise
    except Exception as exc:
        print(f"[BOT ERROR] {type(exc).__name__}: {exc}", flush=True)
        raise
    finally:
        print(f"[WS] FreeSWITCH disconnected: {peer}", flush=True)


if __name__ == "__main__":
    print(f"[BOT] provider={AI_PROVIDER} listening={BOT_HOST}:{BOT_PORT}", flush=True)
    uvicorn.run(app, host=BOT_HOST, port=BOT_PORT, log_level="info")
