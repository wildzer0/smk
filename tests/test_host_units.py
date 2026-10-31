#!/usr/bin/env python3
"""Host unit tests: the layer engine (src/smk/layer.c) and the keymap store
(src/smk/keymap_store.c), compiled with the host C compiler.

Neither has a hardware dependency, so they are built natively against a 2x3 stand-in board
(tests/host/inc/kbdef.h). The keymap store runs on a simulated flash that loses power at
every step of a save (tests/host/test_keymap_store.c).
"""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
CC = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")


def build_and_run(test_case, test_source, *sources, extra_flags=()):
    if not CC:
        test_case.skipTest("no host C compiler")
    with tempfile.TemporaryDirectory() as tmp:
        exe = Path(tmp) / "host_test"
        build = subprocess.run(
            [CC, "-std=c11", "-Wall", "-Wextra", "-Werror",
             "-I", str(REPO_ROOT / "tests/host/inc"), "-I", str(REPO_ROOT / "src/smk"),
             "-I", str(REPO_ROOT / "src/sino51lib"), *extra_flags,
             "-o", str(exe), str(REPO_ROOT / test_source), *[str(REPO_ROOT / s) for s in sources]],
            capture_output=True, text=True)
        test_case.assertEqual(build.returncode, 0, build.stderr)
        run = subprocess.run([str(exe)], capture_output=True, text=True)
        test_case.assertEqual(run.returncode, 0, run.stdout + run.stderr)


class TestLayerEngine(unittest.TestCase):
    def test_layer_engine(self):
        build_and_run(self, "tests/host/test_layer.c", "src/smk/layer.c")


class TestKeymapStore(unittest.TestCase):
    def test_keymap_store(self):
        # __xdata / __code are SDCC storage classes: empty on the host
        build_and_run(self, "tests/host/test_keymap_store.c", "src/smk/keymap_store.c",
                      extra_flags=("-D__xdata=", "-D__code="))


class TestVia(unittest.TestCase):
    def test_via_and_vial(self):
        build_and_run(self, "tests/host/test_via.c", "src/smk/via.c", "src/smk/vial.c",
                      extra_flags=("-D__xdata=", "-D__code="))


class TestTimer(unittest.TestCase):
    def test_timer(self):
        build_and_run(self, "tests/host/test_timer.c", "src/smk/timer.c")


if __name__ == "__main__":
    unittest.main()
