#!/usr/bin/env python3
"""
Read TOF frames from USART6 text protocol and dump JSONL.

Usage:
  python tof_usart6_receiver.py --port /dev/ttyUSB0 --baud 921600 --out tof_frames.jsonl
"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
import sys
import time
from datetime import datetime
from dataclasses import dataclass
from typing import List

try:
    import serial
    from serial.tools import list_ports
except ModuleNotFoundError as exc:
    if exc.name == "serial":
        print(
            "Missing dependency: pyserial. Install it with: python -m pip install pyserial",
            file=sys.stderr,
        )
        raise SystemExit(1) from exc
    raise


@dataclass
class TofFrame:
    frame_id: int
    mcu_ts_ms: int
    resolution: int
    fps: int
    distance_mm: List[int]
    target_status: List[int]
    signal_per_spad: List[int]
    ambient_per_spad: List[int]
    valid: List[int]
    host_ts_ns: int


def _parse_int_list(tokens: List[str], start: int, count: int) -> List[int]:
    end = start + count
    if end > len(tokens):
        raise ValueError("not enough numeric tokens")
    return [int(x) for x in tokens[start:end]]


def parse_tof_line(line: str) -> TofFrame:
    # Expected:
    # TOF,<frame_id>,<ts_ms>,<resolution>,<fps>,D,<...>,S,<...>,G,<...>,A,<...>
    tokens = [t.strip() for t in line.strip().split(",") if t.strip() != ""]
    if len(tokens) < 10:
        raise ValueError("line too short")
    if tokens[0] != "TOF":
        raise ValueError("not a TOF frame")

    frame_id = int(tokens[1])
    mcu_ts_ms = int(tokens[2])
    resolution = int(tokens[3])
    fps = int(tokens[4])

    idx = 5
    if tokens[idx] != "D":
        raise ValueError("missing D marker")
    idx += 1
    distance_mm = _parse_int_list(tokens, idx, resolution)
    idx += resolution

    if tokens[idx] != "S":
        raise ValueError("missing S marker")
    idx += 1
    target_status = _parse_int_list(tokens, idx, resolution)
    idx += resolution

    if tokens[idx] != "G":
        raise ValueError("missing G marker")
    idx += 1
    signal_per_spad = _parse_int_list(tokens, idx, resolution)
    idx += resolution

    if tokens[idx] != "A":
        raise ValueError("missing A marker")
    idx += 1
    ambient_per_spad = _parse_int_list(tokens, idx, resolution)
    idx += resolution

    if idx < len(tokens):
        if tokens[idx] != "V":
            raise ValueError("missing V marker")
        idx += 1
        valid = _parse_int_list(tokens, idx, resolution)
    else:
        valid = [1 if s in (5, 9) else 0 for s in target_status]

    return TofFrame(
        frame_id=frame_id,
        mcu_ts_ms=mcu_ts_ms,
        resolution=resolution,
        fps=fps,
        distance_mm=distance_mm,
        target_status=target_status,
        signal_per_spad=signal_per_spad,
        ambient_per_spad=ambient_per_spad,
        valid=valid,
        host_ts_ns=time.monotonic_ns(),
    )


def frame_to_features(frame: TofFrame) -> dict:
    # Basic ready-to-use features for early fusion with camera.
    valid = [v == 1 for v in frame.valid]
    valid_dist = [d for d, ok in zip(frame.distance_mm, valid) if ok]
    valid_count = sum(valid)
    total_count = len(valid)

    if valid_dist:
        min_dist = min(valid_dist)
        avg_dist = sum(valid_dist) / len(valid_dist)
    else:
        min_dist = -1
        avg_dist = -1.0

    return {
        "valid_count": valid_count,
        "total_count": total_count,
        "valid_ratio": valid_count / total_count if total_count else 0.0,
        "min_distance_mm": min_dist,
        "avg_distance_mm": avg_dist,
        "avg_signal": sum(frame.signal_per_spad) / len(frame.signal_per_spad),
        "avg_ambient": sum(frame.ambient_per_spad) / len(frame.ambient_per_spad),
    }


def _format_grid(title: str, values: List[int], resolution: int, unit: str = "") -> list[str]:
    side = math.isqrt(resolution)
    if side * side != resolution:
        suffix = unit if unit else ""
        return [f"{title}: " + " ".join(f"{value}{suffix}" for value in values)]

    lines = [f"{title}:"]
    for row in range(side):
        start = row * side
        row_values = values[start:start + side]
        lines.append("  " + " ".join(f"{value:>4}{unit}" for value in row_values))
    return lines


def _print_frame_grid(frame: TofFrame) -> None:
    for line in _format_grid("distance", frame.distance_mm, frame.resolution, ""):
        print(line)
    for line in _format_grid("status", frame.target_status, frame.resolution, ""):
        print(line)
    for line in _format_grid("valid", frame.valid, frame.resolution, ""):
        print(line)


def _scan_serial_ports() -> list[list_ports.ListPortInfo]:
    return sorted(list(list_ports.comports()), key=lambda item: item.device)


def _format_port_details(port: list_ports.ListPortInfo) -> list[str]:
    details: list[str] = []

    if port.description:
        details.append(f"description: {port.description}")
    if getattr(port, "manufacturer", None):
        details.append(f"manufacturer: {port.manufacturer}")
    if getattr(port, "product", None):
        details.append(f"product: {port.product}")
    if getattr(port, "interface", None):
        details.append(f"interface: {port.interface}")
    if getattr(port, "vid", None) is not None and getattr(port, "pid", None) is not None:
        details.append(f"vid:pid: {port.vid:04X}:{port.pid:04X}")
    if getattr(port, "serial_number", None):
        details.append(f"serial: {port.serial_number}")
    if getattr(port, "location", None):
        details.append(f"location: {port.location}")
    if port.hwid:
        details.append(f"hwid: {port.hwid}")

    if not details:
        details.append("description: unknown")

    return details


def _choose_serial_port(cli_port: str | None) -> str:
    if cli_port:
        return cli_port

    ports = _scan_serial_ports()
    if not ports:
        raise RuntimeError("no serial ports found")

    print("Available serial ports:")
    for index, port in enumerate(ports, start=1):
        print(f"  {index}. {port.device}")
        for detail in _format_port_details(port):
            print(f"     - {detail}")

    while True:
        choice = input("Select serial port number: ").strip()
        if not choice:
            print("Please enter a port number.")
            continue
        if not choice.isdigit():
            print("Invalid input. Enter a numeric port number.")
            continue

        index = int(choice)
        if 1 <= index <= len(ports):
            selected = ports[index - 1].device
            print(f"Using serial port: {selected}")
            return selected

        print(f"Out of range. Choose 1 to {len(ports)}.")


def _format_rate(frame_delta: int, time_delta_s: float) -> str:
    if frame_delta <= 0 or time_delta_s <= 0.0:
        return "n/a"
    return f"{frame_delta / time_delta_s:.2f}"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", help="Serial port, e.g. /dev/ttyUSB0 or COM3")
    parser.add_argument("--baud", type=int, default=921600)
    parser.add_argument("--out", default="tof_frames.jsonl")
    parser.add_argument("--print-every", type=int, default=30, help="Console print period by frame count")
    parser.add_argument("--print-grid", action="store_true", help="Print distance/status/valid as 2D grids")
    args = parser.parse_args()

    try:
        port = _choose_serial_port(args.port)
    except RuntimeError as exc:
        print(f"serial setup error: {exc}", file=sys.stderr)
        raise SystemExit(1) from exc

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    n = 0
    last_print_host_ts_ns: int | None = None
    last_print_mcu_ts_ms: int | None = None
    last_print_frame_id: int | None = None
    last_frame_id: int | None = None
    dropped_frames = 0
    with serial.Serial(port, args.baud, timeout=1) as ser, out_path.open("a", encoding="utf-8") as f:
        while True:
            raw = ser.readline()
            if not raw:
                continue

            try:
                line = raw.decode("utf-8", errors="replace").strip()
                frame = parse_tof_line(line)
                data = {
                    "frame": frame.__dict__,
                    "features": frame_to_features(frame),
                }

                frame_gap = 0
                if last_frame_id is not None:
                    frame_gap = frame.frame_id - last_frame_id
                    if frame_gap > 1:
                        dropped_frames += frame_gap - 1

                f.write(json.dumps(data, ensure_ascii=True) + "\n")
                n += 1
                last_frame_id = frame.frame_id

                if n % args.print_every == 0:
                    feat = data["features"]
                    host_wall_time = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                    rx_fps = "n/a"
                    mcu_fps = "n/a"
                    if last_print_frame_id is not None and last_print_host_ts_ns is not None and last_print_mcu_ts_ms is not None:
                        frame_delta = frame.frame_id - last_print_frame_id
                        host_dt_s = (frame.host_ts_ns - last_print_host_ts_ns) / 1_000_000_000.0
                        mcu_dt_s = (frame.mcu_ts_ms - last_print_mcu_ts_ms) / 1000.0
                        rx_fps = _format_rate(frame_delta, host_dt_s)
                        mcu_fps = _format_rate(frame_delta, mcu_dt_s)

                    print(
                        f"[{host_wall_time}] "
                        f"frame={frame.frame_id} "
                        f"gap={frame_gap} dropped={dropped_frames} "
                        f"valid={feat['valid_count']}/{feat['total_count']} ({feat['valid_ratio']:.2f}) "
                        f"min={feat['min_distance_mm']}mm avg={feat['avg_distance_mm']:.1f}mm "
                        f"rx_fps={rx_fps} mcu_fps={mcu_fps}"
                    )
                    last_print_host_ts_ns = frame.host_ts_ns
                    last_print_mcu_ts_ms = frame.mcu_ts_ms
                    last_print_frame_id = frame.frame_id
                    if args.print_grid:
                        _print_frame_grid(frame)
            except Exception as e:
                print(f"parse error: {e}")


if __name__ == "__main__":
    main()
