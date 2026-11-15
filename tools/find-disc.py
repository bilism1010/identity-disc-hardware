#!/usr/bin/env python3
"""Print the serial port of the Identity Disc bench board, found by its MAC.

The ESP32-C3's USB serial/JTAG unit reports the chip MAC as its USB serial
number. This reads the macOS I/O registry only; it does not open any port, so
it cannot reset this board or disturb any other.

Usage: find-disc.py [MAC]      (default: the bench unit, or $DISC_MAC)
Exit status: 0 and the port on stdout if exactly one match, 1 otherwise.
"""
import os
import plistlib
import subprocess
import sys

BENCH_MAC = "48:31:B7:3D:40:10"


def walk(node, found):
    """Yield every registry node, depth first."""
    yield node
    for child in node.get("IORegistryEntryChildren", []):
        yield from walk(child, found)


def ports_under(node):
    return [n["IOCalloutDevice"] for n in walk(node, None) if "IOCalloutDevice" in n]


def main():
    mac = (sys.argv[1] if len(sys.argv) > 1 else os.environ.get("DISC_MAC", BENCH_MAC)).upper()
    raw = subprocess.run(
        ["ioreg", "-a", "-r", "-c", "IOUSBHostDevice", "-l"],
        check=True, capture_output=True,
    ).stdout
    roots = plistlib.loads(raw) if raw.strip() else []

    ports = set()
    for root in roots:
        for node in walk(root, None):
            if str(node.get("USB Serial Number", "")).upper() == mac:
                # Espressif VID 0x303A, USB JTAG/serial PID 0x1001
                if (node.get("idVendor"), node.get("idProduct")) != (0x303A, 0x1001):
                    continue
                ports.update(ports_under(node))

    if len(ports) != 1:
        print(f"find-disc: expected one port for {mac}, found {len(ports)}: {sorted(ports)}",
              file=sys.stderr)
        return 1
    print(ports.pop())
    return 0


if __name__ == "__main__":
    sys.exit(main())
