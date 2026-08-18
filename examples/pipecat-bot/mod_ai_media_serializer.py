"""Pipecat serializer for mod_ai_media Protocol v1.

Caller audio arrives as binary PCM16. AI audio is resampled and returned
immediately as binary PCM16, without files or base64 wrapping.
"""

import audioop
import json

from pipecat.frames.frames import AudioRawFrame, Frame, InputAudioRawFrame, StartFrame
from pipecat.serializers.base_serializer import FrameSerializer


class ModAiMediaSerializer(FrameSerializer):
    class InputParams(FrameSerializer.InputParams):
        input_sample_rate: int = 16000
        output_sample_rate: int = 8000
        sample_rate: int | None = None
        sample_width: int = 2
        num_channels: int = 1

    def __init__(self, params: "ModAiMediaSerializer.InputParams | None" = None):
        params = params or ModAiMediaSerializer.InputParams()
        super().__init__(params)
        self._params = params
        self._input_rate = params.input_sample_rate
        self._output_rate = params.output_sample_rate
        self._pipecat_rate = params.sample_rate or self._input_rate
        self._sample_width = params.sample_width
        self._channels = params.num_channels
        self._input_state = None
        self._output_state = None
        self._input_frames = 0
        self._output_frames = 0

    async def setup(self, frame: StartFrame):
        self._pipecat_rate = (
            self._params.sample_rate or frame.audio_in_sample_rate or self._input_rate
        )
        self._input_state = None
        self._output_state = None
        print(
            f"[AUDIO] initial input={self._input_rate}Hz "
            f"pipecat={self._pipecat_rate}Hz output={self._output_rate}Hz",
            flush=True,
        )

    async def serialize(self, frame: Frame) -> str | bytes | None:
        frame_name = type(frame).__name__
        if frame_name in {"InterruptionFrame", "UserStartedSpeakingFrame"}:
            self._output_state = None
            return '{"type":"playback.clear"}'

        if not isinstance(frame, AudioRawFrame) or not frame.audio:
            return None

        source_rate = frame.sample_rate or self._output_rate
        try:
            if source_rate == self._output_rate:
                converted = frame.audio
            else:
                converted, self._output_state = audioop.ratecv(
                    frame.audio,
                    self._sample_width,
                    self._channels,
                    source_rate,
                    self._output_rate,
                    self._output_state,
                )
        except audioop.error as exc:
            print(f"[AUDIO OUT ERROR] {exc}", flush=True)
            return None

        if not converted:
            return None
        self._output_frames += 1
        if self._output_frames <= 5 or self._output_frames % 100 == 0:
            print(
                f"[AUDIO OUT] frame={self._output_frames} "
                f"{source_rate}Hz/{len(frame.audio)} -> "
                f"{self._output_rate}Hz/{len(converted)} bytes",
                flush=True,
            )
        return converted

    async def deserialize(self, data: str | bytes) -> Frame | None:
        if isinstance(data, str):
            try:
                message = json.loads(data)
            except json.JSONDecodeError:
                return None
            if message.get("type") != "start":
                return None
            self._input_rate = int(message.get("input", {}).get("sampleRate", self._input_rate))
            self._output_rate = int(message.get("output", {}).get("sampleRate", self._output_rate))
            self._input_state = None
            self._output_state = None
            print(
                f"[MEDIA START] call={message.get('callId')} "
                f"input={self._input_rate}Hz output={self._output_rate}Hz",
                flush=True,
            )
            return None

        if not data:
            return None
        try:
            if self._input_rate == self._pipecat_rate:
                converted = data
            else:
                converted, self._input_state = audioop.ratecv(
                    data,
                    self._sample_width,
                    self._channels,
                    self._input_rate,
                    self._pipecat_rate,
                    self._input_state,
                )
        except audioop.error as exc:
            print(f"[AUDIO IN ERROR] {exc}", flush=True)
            return None

        if not converted:
            return None
        self._input_frames += 1
        if self._input_frames <= 5 or self._input_frames % 100 == 0:
            print(
                f"[AUDIO IN] frame={self._input_frames} "
                f"{self._input_rate}Hz/{len(data)} -> "
                f"{self._pipecat_rate}Hz/{len(converted)} bytes",
                flush=True,
            )
        return InputAudioRawFrame(
            audio=converted,
            sample_rate=self._pipecat_rate,
            num_channels=self._channels,
        )
