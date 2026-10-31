#!/usr/bin/env python3
"""Talk to the keyboard's raw HID interface (usage page 0xFF60, usage 0x61) with the `hidapi` package.

    pip install hidapi
    python tools/rawhid_test.py            # a few VIA/Vial questions, then 1000 round trips

The same checks as tools/rawhid-test.html, for a machine without Chrome.
"""

import sys
import time

import hid

USAGE_PAGE, USAGE = 0xFF60, 0x61


def find():
    for d in hid.enumerate():
        if d["usage_page"] == USAGE_PAGE and d["usage"] == USAGE:
            return d
    sys.exit("no raw HID interface found (is the keyboard running a RAW_HID_ENABLE build?)")


def exchange(dev, request):
    dev.write(b"\x00" + bytes(request).ljust(32, b"\x00"))  # report id 0 first
    reply = dev.read(32, timeout_ms=1000)
    if len(reply) != 32:
        raise RuntimeError("no reply" if not reply else "short reply: %d bytes" % len(reply))
    return bytes(reply)


def main():
    info = find()
    dev = hid.device()
    dev.open_path(info["path"])
    print("connected:", info["product_string"], "%04x:%04x" % (info["vendor_id"], info["product_id"]))

    for name, request in (("VIA protocol", [0x01]), ("Vial id", [0xFE, 0x00]), ("layers", [0x11]),
                          ("HUD info", [0xF0, 0x01, 0, 1]), ("HUD state", [0xF0, 0x02, 0, 2]),
                          ("HUD matrix", [0xF0, 0x04, 0, 3])):
        print("%-14s" % name, exchange(dev, request).hex(" "))

    bad = 0
    start = time.time()
    for i in range(1000):
        request = [0x04, 0, i % 5, i % 16]
        reply = exchange(dev, request)
        if reply[0] != 0x04 or reply[2] != request[2] or reply[3] != request[3]:
            bad += 1
    elapsed = time.time() - start
    print("1000 round trips in %.0f ms, %d bad" % (elapsed * 1000, bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
