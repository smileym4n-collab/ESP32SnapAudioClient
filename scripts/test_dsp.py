#!/usr/bin/env python3
"""Dependency-free host DSP checks; does not emulate ESP32 timing or real NVS."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="speaker-dsp-tests-") as directory:
    for suite in ("core", "control"):
        binary = str(Path(directory) / suite)
        subprocess.run([
            os.environ.get("CXX", "c++"), "-std=c++11", "-O2", "-Wall", "-Wextra", "-Werror",
            "-pthread", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
            "-I" + str(root / "tests/dsp/stubs"), "-I" + str(root / "include"),
            str(root / "src/audio_dsp.cpp"), str(root / f"tests/dsp/{suite}_test.cpp"),
            "-o", binary,
        ], check=True)
        subprocess.run([binary], check=True)
