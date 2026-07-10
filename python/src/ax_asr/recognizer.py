"""High-level Python wrapper for Axera ASR speech recognition."""

from __future__ import annotations

import os
from typing import Optional

import numpy as np

_MODEL_TYPES = {
    "whisper_tiny": 0,
    "whisper_base": 1,
    "whisper_small": 2,
    "whisper_turbo": 3,
    "sensevoice": 4,
}

_DEFAULT_MODEL_PATH_ENV = "AX_ASR_MODEL_PATH"

_MODEL_TYPE_NAMES = frozenset(_MODEL_TYPES)


class AX_ASR:
    """Speech recognition wrapper for libax_asr_api.

    Parameters
    ----------
    model_type : str
        One of ``whisper_tiny``, ``whisper_base``, ``whisper_small``,
        ``whisper_turbo``, ``sensevoice``.
    model_path : str or None
        Root directory containing model files. If None, reads the
        ``AX_ASR_MODEL_PATH`` environment variable.

    Example
    -------
    >>> with AX_ASR("sensevoice", "./models-ax650") as asr:
    ...     text = asr.transcribe_file("demo.wav", language="zh")
    """

    def __init__(self, model_type: str, model_path: Optional[str] = None):
        if model_type not in _MODEL_TYPES:
            raise ValueError(
                f"Unknown model_type '{model_type}'. "
                f"Choose from: {sorted(_MODEL_TYPES)}"
            )
        self._model_type = model_type
        self._model_path = model_path or os.environ.get(_DEFAULT_MODEL_PATH_ENV)
        if not self._model_path:
            raise ValueError(
                "model_path must be provided or set via "
                f"{_DEFAULT_MODEL_PATH_ENV} environment variable"
            )
        self._handle = None

        from ._ax_asr_core import AsrType, init as _init

        native_type = getattr(AsrType, _MODEL_TYPES[model_type].upper())
        self._handle = _init(native_type, self._model_path)

    def __enter__(self) -> AX_ASR:
        return self

    def __exit__(self, *args) -> None:
        self.close()

    def close(self) -> None:
        """Release the underlying ASR handle. Safe to call multiple times."""
        if self._handle is None:
            return
        from ._ax_asr_core import uninit as _uninit

        _uninit(self._handle)
        self._handle = None

    def __del__(self) -> None:
        if getattr(self, "_handle", None) is not None:
            self.close()

    @property
    def model_type(self) -> str:
        return self._model_type

    @property
    def model_path(self) -> str:
        return self._model_path

    def transcribe_file(self, audio_path: str, language: str = "zh") -> str:
        """Transcribe an audio file (.wav or .mp3)."""
        from ._ax_asr_core import run_file as _run_file

        return _run_file(self._handle, audio_path, language)

    def transcribe_pcm(
        self,
        pcm: np.ndarray,
        sample_rate: int,
        language: str = "zh",
    ) -> str:
        """Transcribe raw PCM float32 audio data."""
        if pcm.dtype != np.float32:
            pcm = pcm.astype(np.float32)
        if pcm.ndim != 1:
            raise ValueError("PCM data must be 1-dimensional")
        from ._ax_asr_core import run_pcm as _run_pcm

        return _run_pcm(self._handle, pcm, sample_rate, language)        return _run_pcm(self._handle, pcm, sample_rate, language)

    def stream_init(self) -> None:
        """Initialize streaming recognition state.
        Call once before feeding audio chunks. Clears any previous partial results."""
        from ._ax_asr_core import stream_init as _stream_init
        _stream_init(self._handle)

    def stream_feed(self, pcm: np.ndarray, sample_rate: int) -> None:
        """Feed an audio chunk for streaming recognition.

        Parameters
        ----------
        pcm:
            float32 numpy array (1-D), range [-1.0, 1.0]. 
            Typical chunk sizes: 400-1600 samples at 16kHz (25-100ms).
        sample_rate:
            Sample rate of the audio. Resampled to 16kHz internally.
        """
        if pcm.dtype != np.float32:
            pcm = pcm.astype(np.float32)
        if pcm.ndim != 1:
            raise ValueError("PCM data must be 1-dimensional")
        from ._ax_asr_core import stream_feed as _stream_feed
        _stream_feed(self._handle, pcm, sample_rate)

    def stream_result(self) -> str:
        """Get the current partial streaming recognition result.

        Returns the best partial hypothesis so far. Call periodically 
        after stream_feed() to get incremental updates.
        """
        from ._ax_asr_core import stream_result as _stream_result
        return _stream_result(self._handle)

    def stream_reset(self) -> None:
        """Reset streaming state. Call to start a new utterance."""
        from ._ax_asr_core import stream_reset as _stream_reset
        _stream_reset(self._handle)
