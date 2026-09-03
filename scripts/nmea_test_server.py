#!/usr/bin/env python3
"""Standalone NMEA 0183 test server for exercising the eData instrument
display. NAV sentences only (no AIS/engine) - forked from eNMEA's test
server, see IMPLEMENTATION_PLAN.md for what's shared vs. new.

Speaks both transports the device supports, because picking the wrong one is
the most common reason the display shows nothing:

    python3 scripts/nmea_test_server.py                 # TCP server (device dials in)
    python3 scripts/nmea_test_server.py --proto udp     # UDP broadcast (device listens)

TCP mode listens on --port and accepts one client at a time. UDP mode
broadcasts to the subnet, which is what the device's UDP mode expects - it
listens on the port and never dials out, so nothing needs to know its address.

Prints this machine's addresses at startup: in TCP mode that is exactly what
goes in the device's Host field, and it changes whenever you move between
networks.

Emits a live RMB (bearing/distance/XTE to waypoint) every tick, with a
distance that counts down over time and a cross-track error that oscillates
across zero - enough to exercise Distance/Bearing/XTE/ETA/Time-To-Go without
a real chartplotter's route. ETA and TTG are not sentences themselves - see
DataCatalog.cpp - so there's nothing to emit for them directly; watch them
change on-device as distance and speed do.

No third-party dependencies - stdlib only.
"""
import argparse
import math
import socket
import time


def checksum(body: str) -> str:
    cs = 0
    for ch in body:
        cs ^= ord(ch)
    return f"{cs:02X}"


def sentence(body: str) -> str:
    prefix = body[0]
    inner = body[1:]
    return f"{prefix}{inner}*{checksum(inner)}\r\n"


def nmea_lat(deg: float) -> tuple[str, str]:
    hemi = "N" if deg >= 0 else "S"
    deg = abs(deg)
    d = int(deg)
    m = (deg - d) * 60
    return f"{d:02d}{m:07.4f}", hemi


def nmea_lon(deg: float) -> tuple[str, str]:
    hemi = "E" if deg >= 0 else "W"
    deg = abs(deg)
    d = int(deg)
    m = (deg - d) * 60
    return f"{d:03d}{m:07.4f}", hemi


def local_addresses():
    """(ip, broadcast) for each non-loopback IPv4 the OS will actually route from."""
    found = []
    try:
        import fcntl
        import struct
    except ImportError:  # non-Linux fallback: whatever the default route uses
        fcntl = None
    # The connect-to-a-remote-address trick returns the IP the kernel would use
    # as a source for outbound traffic - no packet is sent (UDP connect is local).
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        probe.connect(("8.8.8.8", 53))
        ip = probe.getsockname()[0]
        found.append((ip, ip.rsplit(".", 1)[0] + ".255"))
    except OSError:
        pass
    finally:
        probe.close()
    return found


def sentence_batch(tick, t0, state):
    """One tick's worth of sentences, as a list of complete NMEA lines."""
    now = time.time()
    elapsed = now - t0
    hhmmss = time.strftime("%H%M%S", time.gmtime(now))

    # Slowly drift position/course/speed/heading so STALE never fires and the
    # dashboard visibly updates each redraw.
    lat_now = state["lat"] + 0.0006 * math.sin(elapsed / 40.0)
    lon_now = state["lon"] + 0.0006 * math.cos(elapsed / 40.0)
    course_now = (state["course"] + elapsed * 0.5) % 360
    speed_now = state["speed"] + 0.8 * math.sin(elapsed / 15.0)
    heading_now = (state["heading"] + elapsed * 0.5) % 360
    wind_dir_now = (state["wind_dir"] + elapsed * 1.5) % 360
    water_temp = state["water_temp"]
    depth = state["depth"]
    wind_speed = state["wind_speed"]

    lat_s, lat_h = nmea_lat(lat_now)
    lon_s, lon_h = nmea_lon(lon_now)

    lines = []
    lines.append(sentence(
        f"$GPGGA,{hhmmss}.00,{lat_s},{lat_h},{lon_s},{lon_h},1,08,0.9,3.2,M,-19.6,M,,"))
    lines.append(sentence(
        f"$GPRMC,{hhmmss}.00,A,{lat_s},{lat_h},{lon_s},{lon_h},"
        f"{speed_now:.1f},{course_now:.1f},{time.strftime('%d%m%y', time.gmtime(now))},,"))
    lines.append(sentence(
        f"$GPVTG,{course_now:.1f},T,,M,{speed_now:.1f},N,{speed_now * 1.852:.1f},K"))

    if tick % 2 == 0:
        lines.append(sentence(f"$HEHDT,{heading_now:.1f},T"))
    if tick % 3 == 0:
        lines.append(sentence(f"$YXMTW,{water_temp:.1f},C"))
        lines.append(sentence(f"$SDDBT,{depth * 3.281:.1f},f,{depth:.1f},M,{depth * 0.5468:.1f},F"))
    if tick % 2 == 1:
        lines.append(sentence(f"$WIMWV,{wind_dir_now:.1f},T,{wind_speed:.1f},N,A"))
    if tick % 5 == 0:
        lines.append(sentence(
            f"$WIMWD,{wind_dir_now:.1f},T,{wind_dir_now:.1f},M,{wind_speed:.1f},N,{wind_speed * 0.5144:.1f},M"))
    # Extra "other" sentence types so the OTHER: line has something to show.
    if tick % 4 == 0:
        lines.append(sentence("$GPGSA,A,3,04,05,09,12,,,,,,,,,1.8,0.9,1.6"))
    if tick % 6 == 0:
        lines.append(sentence("$GPGLL,{},{},{},{},{},A,A".format(lat_s, lat_h, lon_s, lon_h, hhmmss)))

    # RMB: a route to a fixed waypoint, distance counting down over time (so
    # ETA/Time-To-Go visibly change) and XTE oscillating across zero (so both
    # L and R steer directions get exercised). Never reaches exactly zero -
    # min() floors it well above the arrival threshold real equipment would
    # use, since eData doesn't act on the arrival flag anyway.
    distance_nm = max(0.2, 8.0 - elapsed * 0.03)
    bearing_to_wp = (course_now + 15.0) % 360
    xte_nm = 0.15 * math.sin(elapsed / 20.0)
    xte_dir = "L" if xte_nm < 0 else "R"
    dest_lat_s, dest_lat_h = nmea_lat(state["lat"] + 0.05)
    dest_lon_s, dest_lon_h = nmea_lon(state["lon"] + 0.05)
    lines.append(sentence(
        f"$GPRMB,A,{abs(xte_nm):.2f},{xte_dir},ORIG,{state['waypoint_id']},"
        f"{dest_lat_s},{dest_lat_h},{dest_lon_s},{dest_lon_h},"
        f"{distance_nm:.1f},{bearing_to_wp:.1f},{speed_now:.1f},V"))

    if state["bad_every"] and now - state["last_bad"] >= state["bad_every"]:
        state["last_bad"] = now
        good = sentence(f"$GPGGA,{hhmmss}.00,{lat_s},{lat_h},{lon_s},{lon_h},1,08,0.9,3.2,M,-19.6,M,,")
        lines.append(good[:-4] + "FF\r\n")  # corrupt the checksum on purpose

    return lines


def serve_tcp(args, state, t0):
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((args.bind, args.port))
    srv.listen(1)
    print(f"[nmea-test-server] TCP listening on {args.bind}:{args.port}")
    for ip, _ in local_addresses():
        print(f"[nmea-test-server]   -> set the device's Host to {ip}, Port {args.port}, Protocol TCP")
    print("[nmea-test-server]   the device dials in; nothing happens here until it does")

    while True:
        print("[nmea-test-server] waiting for a connection...")
        conn, addr = srv.accept()
        print(f"[nmea-test-server] client connected: {addr}")
        try:
            tick = 0
            while True:
                conn.sendall("".join(sentence_batch(tick, t0, state)).encode("ascii"))
                tick += 1
                time.sleep(1.0)
        except OSError as exc:
            # Any OSError, not just the clean-disconnect pair. A device that
            # reboots mid-stream - which eData does on every Wi-Fi/profile save, and
            # on every reflash - drops off the network without closing the
            # socket, and the send fails with EHOSTUNREACH/ETIMEDOUT instead.
            # Catching only BrokenPipeError/ConnectionResetError took the whole
            # server down exactly when it was most needed.
            print(f"[nmea-test-server] client gone ({exc.__class__.__name__}: {exc}) - waiting for reconnect")
        finally:
            conn.close()


def serve_udp(args, state, t0):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    targets = [args.udp_target] if args.udp_target else [bcast for _, bcast in local_addresses()]
    if not targets:
        targets = ["255.255.255.255"]
    print(f"[nmea-test-server] UDP broadcasting to {', '.join(targets)} port {args.port}")
    print(f"[nmea-test-server]   -> set the device to Protocol UDP, Port {args.port}; Host is ignored")

    tick = 0
    while True:
        payload = "".join(sentence_batch(tick, t0, state)).encode("ascii")
        # One datagram per sentence: NMEA-over-UDP sources send whole sentences,
        # and a single oversized datagram risks fragmentation on the way in.
        for line in payload.splitlines(keepends=True):
            for target in targets:
                try:
                    sock.sendto(line, (target, args.port))
                except OSError as exc:
                    # Broadcasting to a subnet that briefly has no route (Wi-Fi
                    # dropping, an interface going down) must not end the run.
                    print(f"[nmea-test-server] send to {target} failed ({exc}) - continuing")
        if tick % 10 == 0:
            print(f"[nmea-test-server] tick {tick}: sent {len(payload)} bytes")
        tick += 1
        time.sleep(1.0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--proto", choices=("tcp", "udp"), default="tcp",
                    help="tcp: run a server the device dials into. udp: broadcast to the device.")
    ap.add_argument("--port", type=int, default=10110)
    ap.add_argument("--bind", default="0.0.0.0", help="TCP bind address")
    ap.add_argument("--udp-target", default=None,
                    help="UDP destination (default: this machine's subnet broadcast address)")
    ap.add_argument("--bad-checksum-every", type=int, default=20,
                    help="seconds between deliberately-corrupted GGA sentences (0 to disable)")
    args = ap.parse_args()

    t0 = time.time()
    state = {
        "lat": 47.6062, "lon": -122.3321,  # Elliott Bay, Seattle - arbitrary start point
        "course": 45.0, "speed": 6.2, "heading": 47.0,
        "water_temp": 16.8, "depth": 24.3,
        "wind_dir": 210.0, "wind_speed": 12.4,
        "waypoint_id": "WPT01",
        "bad_every": args.bad_checksum_every, "last_bad": t0,
    }

    if args.proto == "tcp":
        serve_tcp(args, state, t0)
    else:
        serve_udp(args, state, t0)


if __name__ == "__main__":
    main()
