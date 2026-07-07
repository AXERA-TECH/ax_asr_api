"""Tests for the ax_asr Python package."""

import os
import pytest


class TestModelValidation:
    """Pure Python logic — no native library required."""

    def test_model_type_names(self):
        from ax_asr.recognizer import _MODEL_TYPE_NAMES
        assert "whisper_tiny" in _MODEL_TYPE_NAMES
        assert "sensevoice" in _MODEL_TYPE_NAMES
        assert "unknown_model" not in _MODEL_TYPE_NAMES

    def test_reject_unknown_model_type(self):
        from ax_asr import AX_ASR
        with pytest.raises(ValueError, match="Unknown model_type"):
            AX_ASR("nonexistent_model", model_path="/tmp")

    def test_missing_model_path(self):
        from ax_asr import AX_ASR
        old = os.environ.pop("AX_ASR_MODEL_PATH", None)
        try:
            with pytest.raises(ValueError, match="model_path"):
                AX_ASR("sensevoice")
        finally:
            if old is not None:
                os.environ["AX_ASR_MODEL_PATH"] = old

    def test_pcm_method_exists(self):
        from ax_asr.recognizer import AX_ASR as _AX_ASR
        assert hasattr(_AX_ASR, "transcribe_pcm")


@pytest.fixture(scope="module")
def native_binding():
    try:
        import ax_asr._ax_asr_core
        return ax_asr._ax_asr_core
    except ImportError:
        pytest.skip("libax_asr_api.so not available")


class TestNativeBinding:
    """Tests requiring the compiled pybind11 module."""

    def test_enum_values(self, native_binding):
        assert int(native_binding.AsrType.WHISPER_TINY) == 0
        assert int(native_binding.AsrType.SENSEVOICE) == 4

    def test_asr_type_names(self, native_binding):
        expected = {
            "WHISPER_TINY", "WHISPER_BASE", "WHISPER_SMALL",
            "WHISPER_TURBO", "SENSEVOICE",
        }
        names = {n for n in dir(native_binding.AsrType) if not n.startswith("_")}
        assert expected.issubset(names)

    def test_status_enum(self, native_binding):
        assert int(native_binding.AsrStatus.SUCCESS) == 0
