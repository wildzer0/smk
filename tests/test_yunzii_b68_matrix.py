#!/usr/bin/env python3
"""Yunzii B68 key matrix, driven through the simulator.

The wiring below is the one read from the stock firmware (see
src/keyboards/yunzii-b68/kbdef.h). Every (column, row) cell is pressed in turn on the
test-side KeyMatrix and the firmware's real scan must report exactly that cell in
its `matrix[]` array: this checks the column/row macros, user_matrix_col_select /
_deselect and user_matrix_read_rows against the pin tables, not just that it builds.

It also covers the recovery chord (src/smk/recovery.c): holding the chord at power-on must
end in isp_jump() (A=0x5A, B=0xA5 at 0xFF00), anything less must boot on to the main loop.

Run from the repo root (inside `nix develop`, after building):

    meson compile -C build-b68 yunzii-b68_default_smk.hex
    python3 -m unittest discover -s tests

Override the firmware with SMK_B68_FIRMWARE.
"""

import os
import unittest
from pathlib import Path

from devices import Air60Sim, KeyMatrix, P5, P7

P4, P6 = 0xb0, 0xc0

REPO_ROOT = Path(__file__).resolve().parent.parent
MATRIX_COLS, MATRIX_ROWS = 16, 5

# column -> (port SFR, bit): strobes, active low
B68_COL_PIN = {0: (P6, 0), 1: (P6, 1), 2: (P6, 2), 3: (P6, 3),
               4: (P6, 4), 5: (P6, 5), 6: (P6, 6), 7: (P6, 7),
               8: (P5, 0), 9: (P5, 1), 10: (P5, 2), 11: (P5, 7),
               12: (P4, 0), 13: (P4, 2), 14: (P4, 3), 15: (P4, 5)}


class B68KeyMatrix(KeyMatrix):
    COL_PIN = B68_COL_PIN


def find_b68_firmware():
    env = os.environ.get("SMK_B68_FIRMWARE")
    if env:
        return env if Path(env).exists() else None
    for d in ("build-b68", "build"):
        f = REPO_ROOT / d / "yunzii-b68_default_smk.hex"
        if f.exists():
            return str(f)
    return None


FIRMWARE = find_b68_firmware()


def setUpModule():
    if not FIRMWARE:
        raise unittest.SkipTest("yunzii-b68_default_smk.hex not built")


class TestB68Matrix(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.kb = Air60Sim(firmware=FIRMWARE)  # the session class is board-agnostic
        cls.kb.matrix = B68KeyMatrix()
        cls.kb.boot(usb=True)
        cls.matrix_addr = cls.kb.sym["matrix"]

    @classmethod
    def tearDownClass(cls):
        cls.kb.close()

    def scan_once(self):
        """Serve every row read of one full scan (2 per column), then read matrix[]."""
        kb = self.kb
        rr = kb._a("user_matrix_read_rows")
        kb.brk(rr)
        try:
            for _ in range(2 * MATRIX_COLS + 8):
                kb.run()
                kb.matrix.inject(kb)
        finally:
            kb.cmd("delete")
        return kb.get_xram(self.matrix_addr, MATRIX_COLS)

    def test_idle_matrix_is_empty(self):
        self.kb.matrix.clear()
        self.assertEqual(self.scan_once(), [0] * MATRIX_COLS)

    def test_every_cell_maps_to_its_own_column_and_row(self):
        """One full scan per pattern, each pressing one cell in EVERY column (rows are
        independent bits, so several columns can be held at once). The row picked for a
        column comes from two families, (col + k) % 5 and (col // 5 + k) % 5 for
        k = 0..4: together they cover all 80 cells, and no two columns share a pattern in
        both families, so swapped columns or rows cannot cancel out."""
        families = (lambda col, k: (col + k) % MATRIX_ROWS,
                    lambda col, k: (col // MATRIX_ROWS + k) % MATRIX_ROWS)
        seen = set()
        for family in families:
            for k in range(MATRIX_ROWS):
                with self.subTest(family=families.index(family), k=k):
                    self.kb.matrix.clear()
                    want = [0] * MATRIX_COLS
                    for col in range(MATRIX_COLS):
                        row = family(col, k)
                        self.kb.matrix.press(col, row)
                        want[col] = 1 << row
                        seen.add((col, row))
                    self.assertEqual(self.scan_once(), want)
        self.kb.matrix.clear()
        self.assertGreater(len(seen), MATRIX_COLS * 3)


def find_probe_firmware():
    for d in ("build-b68", "build"):
        f = REPO_ROOT / d / "yunzii-b68_probe_smk.hex"
        if f.exists():
            return str(f)
    return None


PROBE_FIRMWARE = find_probe_firmware()

# HID usage of the characters the probe types
KEY_C, KEY_R, KEY_SPACE = 0x06, 0x15, 0x2C
KEY_DIGIT = [0x27] + [0x1E + n for n in range(9)]  # 0 then 1..9


class TestB68ProbeLayout(unittest.TestCase):
    """The probe layout types the coordinates of the cell pressed: "c03r2 "."""

    def typed(self, col, row):
        if not PROBE_FIRMWARE:
            self.skipTest("yunzii-b68_probe_smk.hex not built")
        kb = Air60Sim(firmware=PROBE_FIRMWARE)
        kb.matrix = B68KeyMatrix()
        try:
            kb.boot(usb=True)
            kb.mark_usb_configured()
            kb.matrix.press(col, row)
            kb.scan_for_report(max_hits=600)
            read_rows = kb._a("user_matrix_read_rows")  # let the main loop finish typing
            kb.brk(read_rows)
            for _ in range(400):
                kb.run()
                kb.matrix.inject(kb)
            kb.cmd("delete")
            reports = kb.ep1_reports()
        finally:
            kb.close()
        # a key goes down in one report and up in the next
        return [r[2] for r in reports if r[2]]

    def test_single_digit_column(self):
        self.assertEqual(self.typed(3, 2), [KEY_C, KEY_DIGIT[0], KEY_DIGIT[3], KEY_R, KEY_DIGIT[2], KEY_SPACE])

    def test_input_watch_types_a_pin_that_changes(self):
        if not PROBE_FIRMWARE:
            self.skipTest("yunzii-b68_probe_smk.hex not built")
        kb = Air60Sim(firmware=PROBE_FIRMWARE)
        kb.matrix = B68KeyMatrix()
        try:
            kb.boot(usb=True)
            kb.mark_usb_configured()
            read_rows = kb._a("user_matrix_read_rows")
            kb.brk(read_rows)
            for _ in range(100):  # let the watch take its first reading
                kb.run()
                kb.matrix.inject(kb)
            kb.matrix.p7_forced_low = 0x01  # P7.0 goes low
            for _ in range(200):
                kb.run()
                kb.matrix.inject(kb)
            kb.cmd("delete")
            typed = [r[2] for r in kb.ep1_reports() if r[2]]
        finally:
            kb.close()
        KEY_P, KEY_EQUAL = 0x13, 0x2E
        self.assertEqual(typed, [KEY_P, KEY_DIGIT[7], KEY_DIGIT[0], KEY_EQUAL, KEY_DIGIT[0], KEY_SPACE])  # "p70=0 "

    def test_two_digit_column(self):
        self.assertEqual(self.typed(12, 4), [KEY_C, KEY_DIGIT[1], KEY_DIGIT[2], KEY_R, KEY_DIGIT[4], KEY_SPACE])


class TestB68DefaultLayout(unittest.TestCase):
    """The shipped layout: cells found with the probe, keycodes of a 65% ANSI board."""

    def keys(self, *cells, held=()):
        """The last keyboard report after pressing `cells`, with `held` already down for a few scans
        (a key pressed in the same scan as Fn would be read before Fn, in column order)."""
        kb = Air60Sim(firmware=FIRMWARE)
        kb.matrix = B68KeyMatrix()
        try:
            kb.boot(usb=True)
            kb.mark_usb_configured()
            if held:
                for col, row in held:
                    kb.matrix.press(col, row)
                read_rows = kb._a("user_matrix_read_rows")
                kb.brk(read_rows)
                for _ in range(200):
                    kb.run()
                    kb.matrix.inject(kb)
                kb.cmd("delete")
            for col, row in cells:
                kb.matrix.press(col, row)
            reports = kb.scan_for_report(max_hits=600)
        finally:
            kb.close()
        return reports[-1]

    def test_letter(self):
        self.assertEqual(self.keys((1, 1))[2], 0x14)  # c1r1 = Q

    def test_right_column(self):
        self.assertEqual(self.keys((15, 1))[2], 0x4C)  # c15r1 = Delete
        self.assertEqual(self.keys((15, 2))[2], 0x4B)  # c15r2 = Page Up
        self.assertEqual(self.keys((15, 3))[2], 0x4E)  # c15r3 = Page Down

    def test_shifted_key(self):
        report = self.keys((0, 3), (1, 3))  # left Shift + Z
        self.assertEqual(report[0], 0x02)
        self.assertEqual(report[2], 0x1D)

    def test_fn_layer_gives_f1(self):
        self.assertEqual(self.keys((1, 0), held=[(9, 4)])[2], 0x3A)  # Fn + c1r0 = F1


RECOVERY_CHORD = [(0, 0), (1, 0), (2, 0)]  # RECOVERY_CHORD_KEYS in kbdef.h
ISP_ENTRY = 0xFF00
MAX_STOPS = 400


class TestB68RecoveryChord(unittest.TestCase):
    def power_on(self, pressed):
        """Boot from reset with `pressed` held; return 'isp' (with A, B) or 'app'."""
        kb = Air60Sim(firmware=FIRMWARE)
        kb.matrix = B68KeyMatrix()
        for col, row in pressed:
            kb.matrix.press(col, row)
        try:
            kb.cmd("reset")
            kb.cmd("set mem rom 0x%x 0x22" % kb._a("delay_us"))   # RET: only the delays are skipped
            kb.cmd("set mem rom 0x%x 0x22" % kb._a("delay_ms"))
            read_rows = kb._a("user_matrix_read_rows")
            main_loop = kb._a("kb_update_switches")
            for addr in (read_rows, main_loop, ISP_ENTRY):
                kb.brk(addr)
            for _ in range(MAX_STOPS):
                at = kb.stopped_at(kb.run())
                if at == read_rows:
                    kb.matrix.inject(kb)
                elif at == ISP_ENTRY:
                    return "isp", kb.get_sfr(0xE0), kb.get_sfr(0xF0)
                elif at == main_loop:
                    return "app", None, None
            self.fail("neither the ISP entry nor the main loop was reached")
        finally:
            kb.close()

    def test_chord_held_jumps_to_isp(self):
        outcome, acc, b = self.power_on(RECOVERY_CHORD)
        self.assertEqual((outcome, acc, b), ("isp", 0x5A, 0xA5))

    def test_no_keys_boots_the_app(self):
        self.assertEqual(self.power_on([])[0], "app")

    def test_partial_chord_boots_the_app(self):
        for missing in range(len(RECOVERY_CHORD)):
            held = [k for i, k in enumerate(RECOVERY_CHORD) if i != missing]
            with self.subTest(missing=missing):
                self.assertEqual(self.power_on(held)[0], "app")

    def test_other_keys_boot_the_app(self):
        self.assertEqual(self.power_on([(5, 2), (9, 4)])[0], "app")


if __name__ == "__main__":
    unittest.main()
