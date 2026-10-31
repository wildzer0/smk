#!/usr/bin/env python3
"""Layer engine unit tests (src/smk/layer.c), compiled with the host C compiler.

The engine has no hardware dependency, so it is built natively against a 2x3 stand-in
board (tests/host/inc/kbdef.h) and exercised by tests/host/test_layer.c.
"""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
CC = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")


class TestLayerEngine(unittest.TestCase):
    def test_layer_engine(self):
        if not CC:
            self.skipTest("no host C compiler")
        with tempfile.TemporaryDirectory() as tmp:
            exe = Path(tmp) / "test_layer"
            build = subprocess.run(
                [CC, "-std=c11", "-Wall", "-Wextra", "-Werror",
                 "-I", str(REPO_ROOT / "tests/host/inc"), "-I", str(REPO_ROOT / "src/smk"),
                 "-o", str(exe),
                 str(REPO_ROOT / "tests/host/test_layer.c"), str(REPO_ROOT / "src/smk/layer.c")],
                capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stderr)
            run = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == "__main__":
    unittest.main()
