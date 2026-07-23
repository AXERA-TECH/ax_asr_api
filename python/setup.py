#!/usr/bin/env python3
"""setup.py for ax_asr — build platform-specific aarch64 wheels.

Usage:
    # Build a platform-specific wheel
    python setup.py bdist_wheel --chip=ax650

    # Build with custom BSP SDK location
    python setup.py bdist_wheel --chip=ax8850 --bsp-dir=/opt/axcl_bsp_sdk

    # Set chip via environment (useful for pip install .)
    AX_ASR_CHIP=ax650 pip install .
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

from setuptools import setup
from setuptools.command.bdist_wheel import bdist_wheel
from setuptools.command.build_ext import build_ext

HERE = Path(__file__).resolve().parent
REPO_ROOT = HERE.parent

# ── Platform configuration ────────────────────────────────────────────
# Maps each chip to its BSP SDK location and linker libraries.
CHIP_CONFIG: dict[str, dict[str, str]] = {
    "ax650": {
        "bsp_default": "ax650n_bsp_sdk",
        "bsp_lib_subdir": "msp/out/lib",
        "msp_libs": "ax_sys;ax_engine;ax_interpreter;ax_dmadim",
    },
    "ax630c": {
        "bsp_default": "ax620e_bsp_sdk",
        "bsp_lib_subdir": "msp/out/arm64_glibc/lib",
        "msp_libs": "ax_sys;ax_engine;ax_interpreter",
    },
    "ax620q": {
        "bsp_default": "ax620e_bsp_sdk",
        "bsp_lib_subdir": "msp/out/arm_uclibc/lib",
        "msp_libs": "ax_sys;ax_engine;ax_interpreter",
    },
    "ax8850": {
        "bsp_default": "axcl_bsp_sdk",
        "bsp_lib_subdir": "out/lib",
        "msp_libs": (
            "axcl_rt;axcl_pkg;axcl_comm;axcl_npu;spdlog;"
            "axcl_token;axcl_pcie_msg;axcl_pcie_dma"
        ),
    },
}


# ── Helpers ───────────────────────────────────────────────────────────

def _resolve_chip() -> str | None:
    """Return chip from environment, if set."""
    return os.environ.get("AX_ASR_CHIP")


def _resolve_bsp_dir(chip: str, override: str | None = None) -> Path:
    """Resolve the BSP SDK root directory for *chip*."""
    if override:
        return Path(override)
    return REPO_ROOT / CHIP_CONFIG[chip]["bsp_default"]


def _validate_paths(chip: str, bsp_dir: Path) -> tuple[Path, Path, Path]:
    """Validate and return (lib_dir, include_dir, msp_lib_dir).

    Exits with a clear message when required paths are missing.
    """
    lib_dir = REPO_ROOT / "install" / chip / "lib"
    include_dir = REPO_ROOT / "install" / chip / "include"
    msp_lib_dir = bsp_dir / CHIP_CONFIG[chip]["bsp_lib_subdir"]

    if not (lib_dir / "libax_asr_api.a").exists():
        sys.exit(
            f"ERROR: libax_asr_api.a not found in {lib_dir}\n"
            f"  Run  ./build_{chip}.sh  first to build the C++ library."
        )
    if not msp_lib_dir.exists():
        sys.exit(
            f"ERROR: BSP library directory not found: {msp_lib_dir}\n"
            f"  Run  ./download_bsp.sh  first, or pass --bsp-dir to override."
        )
    return lib_dir, include_dir, msp_lib_dir


def _compile_extension(
    chip: str,
    lib_dir: Path,
    include_dir: Path,
    msp_lib_dir: Path,
) -> None:
    """Run cmake + make to build ``_ax_asr_core``, then copy .so into the
    package source tree so it is picked up by ``package_data``."""
    config = CHIP_CONFIG[chip]
    build_dir = HERE / "build" / f"cmake_{chip}"
    build_dir.mkdir(parents=True, exist_ok=True)

    cmake_args = [
        "cmake", str(HERE),
        f"-DAX_ASR_LIB_DIR={lib_dir}",
        f"-DAX_ASR_INCLUDE_DIR={include_dir}",
        f"-DMSP_LIB_DIR={msp_lib_dir}",
        f"-DMSP_LIBS={config['msp_libs']}",
        "-DCMAKE_BUILD_TYPE=Release",
    ]

    print(f"[ax_asr] Configuring {chip} build ...")
    subprocess.run(cmake_args, cwd=build_dir, check=True)

    nproc = os.cpu_count() or 4
    print(f"[ax_asr] Compiling ({nproc} jobs) ...")
    subprocess.run(["make", f"-j{nproc}"], cwd=build_dir, check=True)

    # Locate the built shared library and stage it for packaging.
    so_files = list(build_dir.glob("_ax_asr_core*.so"))
    if not so_files:
        sys.exit(f"ERROR: Built .so not found in {build_dir}")

    dest_dir = HERE / "src" / "ax_asr"
    for so in so_files:
        shutil.copy2(so, dest_dir / so.name)
        print(f"[ax_asr]   staged {so.name} -> src/ax_asr/")


# ── Custom setuptools commands ────────────────────────────────────────

class SkipBuildExt(build_ext):
    """Skip native compilation when a pre-built ``.so`` already exists."""

    def run(self) -> None:
        so_files = list((HERE / "src" / "ax_asr").glob("_ax_asr_core*.so"))
        if so_files:
            print(f"[ax_asr] Using existing extension: {so_files[0].name}")
            return
        print("[ax_asr] No pre-built .so found — use --chip to compile, "
              "or set AX_ASR_CHIP.")
        super().run()


class ChipBdistWheel(bdist_wheel):
    """``bdist_wheel`` extended with ``--chip`` and ``--bsp-dir`` options.

    When a chip is specified this command will:

    1. Validate that ``libax_asr_api.a`` and the BSP libraries exist.
    2. Run cmake + make to compile the pybind11 extension.
    3. Stage the ``.so`` for packaging.
    4. Append a local version label (``+ax650``) to the wheel filename.
    """

    user_options = bdist_wheel.user_options + [
        ("chip=", None, "Target chip: ax650, ax630c, ax620q, ax8850"),
        ("bsp-dir=", None, "Override BSP SDK root directory"),
    ]

    def initialize_options(self) -> None:
        super().initialize_options()
        self.chip: str | None = None
        self.bsp_dir: str | None = None

    def finalize_options(self) -> None:
        super().finalize_options()
        chip = self.chip or _resolve_chip()
        if not chip:
            return
        if chip not in CHIP_CONFIG:
            sys.exit(
                f"ERROR: Unknown chip '{chip}'. "
                f"Choose from: {sorted(CHIP_CONFIG)}"
            )
        self._active_chip: str = chip

    def run(self) -> None:
        chip: str | None = getattr(self, "_active_chip", None)
        if chip:
            bsp_dir = _resolve_bsp_dir(chip, self.bsp_dir)
            lib_dir, include_dir, msp_lib_dir = _validate_paths(chip, bsp_dir)

            print(f"[ax_asr] Building for {chip}")
            print(f"  libax_asr_api: {lib_dir}")
            print(f"  include:       {include_dir}")
            print(f"  bsp libs:      {msp_lib_dir}")
            print(f"  msp libs:      {CHIP_CONFIG[chip]['msp_libs']}")

            _compile_extension(chip, lib_dir, include_dir, msp_lib_dir)

            # Append local version label so wheels are distinguishable.
            base = self.distribution.metadata.version
            self.distribution.metadata.version = f"{base}+{chip}"

        super().run()


# ── Setup ─────────────────────────────────────────────────────────────

setup(
    name="ax_asr",
    version="0.1.1",
    description="Python binding for Axera ASR speech recognition",
    long_description="",
    author="Axera Semiconductor",
    license="MIT",
    python_requires=">=3.8",
    install_requires=["numpy>=1.20"],
    packages=["ax_asr"],
    package_dir={"ax_asr": "src/ax_asr"},
    package_data={"ax_asr": ["_ax_asr_core*.so"]},
    ext_modules=[],  # compilation handled by ChipBdistWheel
    cmdclass={
        "build_ext": SkipBuildExt,
        "bdist_wheel": ChipBdistWheel,
    },
    classifiers=[
        "Development Status :: 3 - Alpha",
        "License :: OSI Approved :: MIT License",
        "Programming Language :: Python :: 3",
    ],
)
