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
import re
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
            for _ in range(100):  # the watch takes its first reading and starts learning
                kb.run()
                kb.matrix.inject(kb)
            # jump the millisecond clock past the learning time (4 s)
            now_ms = kb.sym["timer_now_ms"]
            kb.cmd("set mem xram 0x%x 0x00 0x20 0x00 0x00" % now_ms)  # 0x2000 = 8192 ms
            for _ in range(60):
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
        # the simulator holds P7.0 low in every column too, so "e00=0 ... e15=0 " come with it
        want = [KEY_P, KEY_DIGIT[7], KEY_DIGIT[0], KEY_EQUAL, KEY_DIGIT[0], KEY_SPACE]  # "p70=0 "
        self.assertTrue(any(typed[i:i + len(want)] == want for i in range(len(typed))), typed)

    def test_two_digit_column(self):
        self.assertEqual(self.typed(12, 4), [KEY_C, KEY_DIGIT[1], KEY_DIGIT[2], KEY_R, KEY_DIGIT[4], KEY_SPACE])


class TestB68DefaultLayout(unittest.TestCase):
    """The shipped layout: cells found with the probe, keycodes of a 65% ANSI board."""

    FIRMWARE_UNDER_TEST = None  # a subclass can run the same checks on another build

    def keys(self, *cells, held=()):
        """The last keyboard report after pressing `cells`, with `held` already down for a few scans
        (a key pressed in the same scan as Fn would be read before Fn, in column order)."""
        kb = Air60Sim(firmware=self.FIRMWARE_UNDER_TEST or FIRMWARE)
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
        return self.normalize(reports[-1])

    def normalize(self, report):
        return report

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


def find_rawhid_firmware():
    for d in ("build-b68", "build"):
        f = REPO_ROOT / d / "yunzii-b68_rawhid_smk.hex"
        if f.exists():
            return str(f)
    return None


RAWHID_FIRMWARE = find_rawhid_firmware()


class TestB68RawHid(unittest.TestCase):
    """VIA/Vial over the raw HID interface: a 32-byte packet in on EP2 OUT, the reply on EP2 IN."""

    EP2_OUT_BUF, OEP2CNT, USBIF2, OEP2IF = 0x1140, 0xA4, 0x93, 0x40

    def exchange(self, *requests):
        """Sends each request in turn (waiting for its reply first) and returns the replies."""
        if not RAWHID_FIRMWARE:
            self.skipTest("yunzii-b68_rawhid_smk.hex not built")
        kb = Air60Sim(firmware=RAWHID_FIRMWARE)
        kb.matrix = B68KeyMatrix()
        replies = []
        try:
            kb.boot(usb=True)
            kb.mark_usb_configured()
            read_rows = kb._a("user_matrix_read_rows")
            kb.brk(read_rows)
            for request in requests:
                packet = list(request) + [0] * (32 - len(request))
                before = len(self.ep2_packets(kb))
                kb.cmd("set mem xram 0x%x " % self.EP2_OUT_BUF + " ".join("0x%02x" % b for b in packet))
                kb.set_sfr(self.OEP2CNT, 32)
                # The simulator raises the USB interrupt only for a SETUP, so a harmless GET_DESCRIPTOR(device)
                # carries it, and the same interrupt finds the EP2 OUT flag (what the SIE would set for a packet).
                setup = [0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x40, 0x00]
                kb.cmd("set mem xram 0x1100 " + " ".join("0x%02x" % b for b in setup))
                kb.set_sfr(self.USBIF2, self.OEP2IF)
                kb.set_sfr(0x92, 0x10)  # USBIF1.SETUPIF
                for _ in range(300):
                    kb.run()
                    kb.matrix.inject(kb)
                    if len(self.ep2_packets(kb)) > before:
                        break
                packets = self.ep2_packets(kb)
                self.assertGreater(len(packets), before, "no reply on EP2 IN")
                replies.append(packets[-1])
            kb.cmd("delete")
            self.last_sizes = [int(n) for n in re.findall(r"\[SIE\] EP2 IN (\d+) bytes", kb.stderr_text())]
        finally:
            kb.close()
        return replies

    @staticmethod
    def ep2_packets(kb):
        return [[int(x, 16) for x in m.split()]
                for m in re.findall(r"\[SIE\] EP2 IN \d+ bytes:((?: [0-9a-f]{2})*)", kb.stderr_text())]

    def test_via_protocol_version(self):
        (reply,) = self.exchange([0x01])
        self.assertEqual(reply[:3], [0x01, 0x00, 0x09])  # the simulator's log shows the first 24 bytes of a packet
        self.assertEqual(self.last_sizes, [32])  # ...but the packet is the full 32

    def test_vial_keyboard_id(self):
        (reply,) = self.exchange([0xFE, 0x00])
        self.assertEqual(reply[:4], [6, 0, 0, 0])  # Vial protocol 6
        self.assertEqual(reply[4:12], [0x46, 0xEC, 0xD0, 0x56, 0x5D, 0xF8, 0x4F, 0xA8])

    def test_keycode_round_trip(self):
        replies = self.exchange([0x05, 0, 0, 0, 0x12, 0x34], [0x04, 0, 0, 0])
        self.assertEqual(replies[1][:6], [0x04, 0, 0, 0, 0x12, 0x34])

    def test_two_requests_in_a_row_both_answered(self):
        replies = self.exchange([0x01], [0x11])
        self.assertEqual(replies[0][:3], [0x01, 0x00, 0x09])
        self.assertEqual(replies[1][:2], [0x11, 8])  # eight layers


class TestB68RawHidBuildKeys(TestB68DefaultLayout):
    """The raw HID build types and switches layers exactly like the default one."""

    FIRMWARE_UNDER_TEST = RAWHID_FIRMWARE

    def normalize(self, report):
        # interface 0 reports carry their ID here: [3, mods, reserved, keys...]
        self.assertEqual(report[0], 3)
        return report[1:]

    def setUp(self):
        if not RAWHID_FIRMWARE:
            self.skipTest("yunzii-b68_rawhid_smk.hex not built")


class TestB68RawHidBuildInterface0(unittest.TestCase):
    """What interface 0 sends in the raw HID build: IDs in report protocol, none in boot protocol,
    and the media keys that used to go out on EP2."""

    def run_keys(self, cells, boot_protocol=False):
        if not RAWHID_FIRMWARE:
            self.skipTest("yunzii-b68_rawhid_smk.hex not built")
        kb = Air60Sim(firmware=RAWHID_FIRMWARE)
        kb.matrix = B68KeyMatrix()
        try:
            kb.boot(usb=True)
            kb.mark_usb_configured()
            if boot_protocol:
                kb.cmd("set mem xram 0x%x 0x00" % kb.sym["interface0_protocol"])
            for col, row in cells:
                kb.matrix.press(col, row)
            reports = kb.scan_for_report(max_hits=600)
        finally:
            kb.close()
        return reports

    def test_report_protocol_has_the_id(self):
        self.assertEqual(self.run_keys([(1, 1)])[-1], [3, 0, 0, 0x14, 0, 0, 0, 0, 0])  # Q

    def test_boot_protocol_has_no_id(self):
        self.assertEqual(self.run_keys([(1, 1)], boot_protocol=True)[-1], [0, 0, 0x14, 0, 0, 0, 0, 0])

    def test_media_key_goes_out_on_ep1_with_its_id(self):
        # Fn + the arrow-down key is volume down on the Fn layer
        kb = Air60Sim(firmware=RAWHID_FIRMWARE) if RAWHID_FIRMWARE else None
        if not kb:
            self.skipTest("yunzii-b68_rawhid_smk.hex not built")
        kb.matrix = B68KeyMatrix()
        try:
            kb.boot(usb=True)
            kb.mark_usb_configured()
            kb.matrix.press(9, 4)  # Fn
            read_rows = kb._a("user_matrix_read_rows")
            kb.brk(read_rows)
            for _ in range(200):
                kb.run()
                kb.matrix.inject(kb)
            kb.cmd("delete")
            kb.matrix.press(14, 4)  # Down: volume down
            kb.scan_for_report(max_hits=600)
            reports = kb.ep1_reports()
        finally:
            kb.close()
        self.assertIn([2, 0xEA, 0x00], reports)  # consumer report ID 2, usage 0x00EA (volume down)


class TestB68Encoder(unittest.TestCase):
    """The encoder on P0.6 (A) and P0.7 (B): a full cycle of the two pins is one volume step."""

    def volume_reports(self, cycle):
        kb = Air60Sim(firmware=FIRMWARE)
        kb.matrix = B68KeyMatrix()
        try:
            kb.boot(usb=True)
            kb.mark_usb_configured()
            read_rows = kb._a("user_matrix_read_rows")
            kb.brk(read_rows)
            for state in cycle:  # (A, B) levels, each held across a couple of timer ticks
                # the simulator takes the level of the P0 input pins from the SFR itself (P0.2 is an output, kept high)
                kb.cmd("set mem sfr 0x80 0x%02x" % (0x04 | (state[0] << 6) | (state[1] << 7)))
                for _ in range(80):
                    kb.run()
                    kb.matrix.inject(kb)
            for _ in range(200):  # let the main loop send what was counted
                kb.run()
                kb.matrix.inject(kb)
            kb.cmd("delete")
            text = kb.stderr_text()
        finally:
            kb.close()
        return [[int(x, 16) for x in m.split()]
                for m in re.findall(r"\[SIE\] EP2 IN \d+ bytes:((?: [0-9a-f]{2})*)", text)]

    # (A, B): 00 -> 10 -> 11 -> 01 -> 00 is the direction seen first on the board
    FIRST_WAY = [(0, 0), (1, 0), (1, 1), (0, 1), (0, 0)]
    OTHER_WAY = [(1, 1), (1, 0), (0, 0), (0, 1), (1, 1)]  # B falls first, as in the second half of the board capture

    def test_first_way_is_volume_up(self):
        self.assertIn([0x02, 0xE9, 0x00], self.volume_reports([(0, 0)] + self.FIRST_WAY))

    def test_other_way_is_volume_down(self):
        self.assertIn([0x02, 0xEA, 0x00], self.volume_reports([(1, 1)] + self.OTHER_WAY))


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
