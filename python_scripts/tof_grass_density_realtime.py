#!/usr/bin/env python3
"""
Real-time grass-density inference from TOF serial stream.

Example:
    python python_scripts/tof_grass_density_realtime.py \
    --model output/tof_grass_density_model.pkl \
    --port COM3
"""

from __future__ import annotations

import argparse
import pickle
import sys
from collections import Counter, deque
from datetime import datetime
from pathlib import Path
from typing import Any

try:
    import serial
except ModuleNotFoundError as exc:
    print("Missing dependency: pyserial. Install with: python -m pip install pyserial", file=sys.stderr)
    raise SystemExit(1) from exc

from tof_grass_density_features import extract_features
from tof_usart6_receiver import _choose_serial_port, parse_tof_line


def _to_float(value: Any) -> float:
    if value is None:
        return -1.0
    try:
        return float(value)
    except (TypeError, ValueError):
        return -1.0


def _stable_label(history: deque[str]) -> str:
    if not history:
        return "n/a"
    count = Counter(history)
    return count.most_common(1)[0][0]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", required=True, help="Model path from tof_grass_density_train.py")
    parser.add_argument("--port", help="Serial port, e.g. COM3 or /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=921600)
    parser.add_argument("--min-mm", type=int, default=20)
    parser.add_argument("--max-mm", type=int, default=2000)
    parser.add_argument("--print-every", type=int, default=30, help="Console print period by frame count")
    parser.add_argument("--smooth-window", type=int, default=15, help="Prediction smoothing window")
    args = parser.parse_args()

    model_path = Path(args.model)
    with model_path.open("rb") as rf:
        payload = pickle.load(rf)

    model = payload.get("model")
    feature_names: list[str] = list(payload.get("feature_names", []))
    if model is None or not feature_names:
        raise SystemExit("Invalid model package: missing model or feature_names")

    try:
        port = _choose_serial_port(args.port)
    except RuntimeError as exc:
        print(f"serial setup error: {exc}", file=sys.stderr)
        raise SystemExit(1) from exc

    pred_history: deque[str] = deque(maxlen=max(1, args.smooth_window))
    n = 0

    with serial.Serial(port, args.baud, timeout=1) as ser:
        while True:
            raw = ser.readline()
            if not raw:
                continue

            try:
                line = raw.decode("utf-8", errors="replace").strip()
                frame = parse_tof_line(line)
                frame_dict = {
                    "frame_id": frame.frame_id,
                    "mcu_ts_ms": frame.mcu_ts_ms,
                    "host_ts_ns": frame.host_ts_ns,
                    "resolution": frame.resolution,
                    "fps": frame.fps,
                    "distance_mm": frame.distance_mm,
                    "target_status": frame.target_status,
                    "signal_per_spad": frame.signal_per_spad,
                    "ambient_per_spad": frame.ambient_per_spad,
                    "valid": frame.valid,
                }

                feat = extract_features(frame_dict, args.min_mm, args.max_mm)
                x = [_to_float(feat.get(name)) for name in feature_names]

                pred = str(model.predict([x])[0])
                confidence = -1.0
                if hasattr(model, "predict_proba"):
                    confidence = float(max(model.predict_proba([x])[0]))

                pred_history.append(pred)
                stable_pred = _stable_label(pred_history)
                n += 1

                if n % args.print_every == 0:
                    now = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                    print(
                        f"[{now}] frame={frame.frame_id} pred={pred} conf={confidence:.3f} "
                        f"stable={stable_pred} valid_ratio={feat['valid_ratio']:.2f} "
                        f"dist_mean={feat['dist_mean_mm']:.1f}mm"
                    )
            except Exception as exc:
                print(f"infer error: {exc}")


if __name__ == "__main__":
    main()
