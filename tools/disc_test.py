#!/usr/bin/env python3
"""Host test script for the Identity Disc prototype (serial transport, M1).

The port is found by MAC through find-disc.py, never guessed. Opening the port
can reset the board (see docs/FIRMWARE_SPEC.md). Measured on the bench unit: opening with
DTR/RTS held low, as this script does by default, does not reset it. `--wait-ready`
opens normally and waits for the `ready` event instead.

  disc_test.py info
  disc_test.py prf "deploy prod" [--salt HEX64]
  disc_test.py demo                    show the ring visuals (standby, presence, warning, alert, verified)
  disc_test.py check                   guided M1 acceptance run (needs the owner at the disc)
"""
import argparse
import json
import os
import subprocess
import sys
import time

import serial

HERE = os.path.dirname(os.path.abspath(__file__))
SALT_A = "00" * 31 + "01"
SALT_B = "00" * 31 + "02"


def find_port():
    r = subprocess.run([sys.executable, os.path.join(HERE, "find-disc.py")],
                       capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit(r.stderr.strip() or "disc not found")
    return r.stdout.strip()


class Disc:
    def __init__(self, port, no_reset=False):
        self.next_id = 1
        self.ser = serial.Serial()
        self.ser.port = port
        self.ser.baudrate = 115200
        self.ser.timeout = 0.2
        if no_reset:
            self.ser.dtr = False
            self.ser.rts = False
        t0 = time.time()
        self.ser.open()
        self.ready_seen = False
        if no_reset:
            # No reset expected: the disc is already running, so probe with a ping.
            time.sleep(0.5)
            self.ser.reset_input_buffer()
            r = self.call("ping", wait=2)
            self.reset_avoided = r is not None
            print(f"# no-reset open: ping {'answered' if r else 'NOT answered'} "
                  f"({time.time() - t0:.1f}s)")
        else:
            self.reset_avoided = False
            ev = self.read_until(lambda m: m.get("evt") == "ready", 10)
            self.ready_seen = ev is not None
            print(f"# ready event {'seen' if ev else 'NOT seen'} after {time.time() - t0:.1f}s")
            if not ev:
                sys.exit("disc did not announce ready")

    def verify(self):
        """Refuse to go on unless the port answers as the Identity Disc firmware."""
        r = self.call("info", wait=3)
        if not (r and r.get("ok") and r.get("device") == "identity-disc"):
            self.ser.close()
            sys.exit(f"REFUSING: {self.ser.port} did not identify as identity-disc: {r}")
        self.info = r
        print(f"# verified identity-disc fw {r.get('fw')} disc_id {r.get('disc_id')}")

    def read_line(self, timeout):
        end = time.time() + timeout
        buf = b""
        while time.time() < end:
            c = self.ser.read(1)
            if not c:
                continue
            if c == b"\n":
                return buf.decode(errors="replace").strip()
            buf += c
        return None

    def read_until(self, pred, timeout):
        end = time.time() + timeout
        while time.time() < end:
            line = self.read_line(max(0.05, end - time.time()))
            if line is None:
                continue
            if line.startswith("#") or not line:
                print("  " + line)
                continue
            try:
                m = json.loads(line)
            except ValueError:
                print("  ?? non-JSON line:", line)
                continue
            if pred(m):
                return m
            print("  <", line)
        return None

    def send_raw(self, text):
        self.ser.write(text.encode() + b"\n")

    def send(self, op, **fields):
        i = self.next_id
        self.next_id += 1
        msg = {"v": 1, "id": i, "op": op, **fields}
        self.send_raw(json.dumps(msg, separators=(",", ":")))
        return i

    def call(self, op, wait=5, **fields):
        i = self.send(op, **fields)
        return self.read_until(lambda m: m.get("id") == i, wait)


def demo(d):
    r = d.call("demo")
    print(r)
    if r and r.get("ok"):
        print("# demo running on the disc: standby, presence, warning, alert, verified (~4.5 s each)")


def expect(name, cond, detail=""):
    print(f"{'PASS' if cond else 'FAIL'}  {name} {detail}")
    return cond


def check(d):
    results = []
    r = d.call("info")
    results.append(expect("info", bool(r and r.get("ok") and len(r.get("disc_id", "")) == 16), str(r)))
    r = d.call("ping")
    results.append(expect("ping", bool(r and r.get("ok"))))

    # malformed input
    d.send_raw("not json")
    r = d.read_until(lambda m: "id" in m, 3)
    results.append(expect("garbage -> bad_request", bool(r and r.get("err") == "bad_request"), str(r)))
    i = d.next_id; d.next_id += 1
    d.send_raw(json.dumps({"v": 2, "id": i, "op": "ping"}))
    r = d.read_until(lambda m: m.get("id") == i, 3)
    results.append(expect("v=2 -> bad_version", bool(r and r.get("err") == "bad_version"), str(r)))
    r = d.call("prf", salt="zz" * 32, label="x")
    results.append(expect("bad salt -> bad_request", bool(r and r.get("err") == "bad_request"), str(r)))
    r = d.call("prf", salt=SALT_A, label="")
    results.append(expect("empty label -> bad_request", bool(r and r.get("err") == "bad_request"), str(r)))
    r = d.call("prf", salt=SALT_A, label="x", timeout_s=2)
    results.append(expect("timeout_s=2 -> bad_request", bool(r and r.get("err") == "bad_request"), str(r)))
    r = d.call("nope")
    results.append(expect("unknown op -> bad_request", bool(r and r.get("err") == "bad_request"), str(r)))

    print("\n>>> On the disc: PRESS AND HOLD the centre ring until it fills (about 1.2 s).")
    r1 = d.call("prf", wait=40, salt=SALT_A, label="check: same salt 1")
    ok1 = bool(r1 and r1.get("ok") and len(r1.get("prf", "")) == 64)
    results.append(expect("prf approved (salt A)", ok1, str(r1)))

    time.sleep(1.5)
    print("\n>>> Hold again.")
    r2 = d.call("prf", wait=40, salt=SALT_A, label="check: same salt 2")
    results.append(expect("same salt -> same value", bool(ok1 and r2 and r2.get("prf") == r1.get("prf"))))

    time.sleep(1.5)
    print("\n>>> Hold again.")
    r3 = d.call("prf", wait=40, salt=SALT_B, label="check: different salt")
    results.append(expect("different salt -> different value",
                          bool(ok1 and r3 and r3.get("ok") and r3.get("prf") != r1.get("prf"))))

    time.sleep(1.5)
    print("\n>>> Do NOT touch: waiting 5 s for the timeout.")
    r = d.call("prf", wait=15, salt=SALT_A, label="check: timeout", timeout_s=5)
    results.append(expect("no touch -> timeout, no prf", bool(r and r.get("err") == "timeout" and "prf" not in r), str(r)))

    r = d.call("prf", wait=3, salt=SALT_A, label="check: cooldown")
    results.append(expect("right after timeout -> cooldown", bool(r and r.get("err") == "cooldown"), str(r)))
    time.sleep(4)

    print("\n>>> TAP the 'tap here to deny' zone at the bottom.")
    r = d.call("prf", wait=40, salt=SALT_A, label="check: deny me")
    results.append(expect("tap deny -> denied, no prf", bool(r and r.get("err") == "denied" and "prf" not in r), str(r)))
    time.sleep(4)

    print("\n>>> Do NOT touch: testing busy and cancel.")
    i1 = d.send("prf", salt=SALT_A, label="check: busy/cancel", timeout_s=30)
    d.read_until(lambda m: m.get("evt") == "touch_wait", 3)
    r = d.call("prf", wait=3, salt=SALT_B, label="second request")
    results.append(expect("second prf while pending -> busy", bool(r and r.get("err") == "busy"), str(r)))
    d.send("cancel")
    r = d.read_until(lambda m: m.get("id") == i1, 3)
    results.append(expect("cancel -> pending replies cancelled", bool(r and r.get("err") == "cancelled"), str(r)))

    print()
    print(f"{sum(results)}/{len(results)} checks passed")
    return all(results)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", choices=["info", "prf", "check", "demo"])
    ap.add_argument("label", nargs="?", default="test request")
    ap.add_argument("--salt", default=SALT_A)
    ap.add_argument("--wait-ready", action="store_true",
                    help="open the port normally and wait for the ready event (the default holds DTR/RTS low, which does not reset the board)")
    a = ap.parse_args()

    port = find_port()
    print(f"# disc port: {port}")
    d = Disc(port, no_reset=not a.wait_ready)
    d.verify()
    if a.cmd == "demo":
        demo(d)
    elif a.cmd == "info":
        print(d.call("info"))
    elif a.cmd == "prf":
        print("# hold the centre of the disc to approve")
        print(d.call("prf", wait=130, salt=a.salt, label=a.label))
    else:
        sys.exit(0 if check(d) else 1)


if __name__ == "__main__":
    main()
