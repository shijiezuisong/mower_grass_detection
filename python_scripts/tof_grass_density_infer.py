#!/usr/bin/env python3
"""
Run grass-density inference from TOF feature JSONL using a trained model.

Example:
    python python_scripts/tof_grass_density_infer.py \
    --model output/tof_grass_density_model.pkl \
    --in output/tof_features_dense.jsonl \
    --out output/tof_density_pred_dense.jsonl
"""

from __future__ import annotations

import argparse
import json
import pickle
from pathlib import Path
from typing import Any


def _to_float(value: Any) -> float:
    if value is None:
        return -1.0
    try:
        return float(value)
    except (TypeError, ValueError):
        return -1.0


def _read_jsonl(path: Path) -> list[dict]:
    rows: list[dict] = []
    with path.open("r", encoding="utf-8") as rf:
        for line in rf:
            text = line.strip()
            if not text:
                continue
            try:
                obj = json.loads(text)
            except json.JSONDecodeError:
                continue
            if isinstance(obj, dict):
                rows.append(obj)
    return rows


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", required=True, help="Model path from tof_grass_density_train.py")
    parser.add_argument("--in", dest="input_path", required=True, help="Input feature JSONL")
    parser.add_argument("--out", dest="output_path", default="", help="Optional prediction JSONL output")
    parser.add_argument("--print-every", type=int, default=30, help="Console print period")
    args = parser.parse_args()

    model_path = Path(args.model)
    input_path = Path(args.input_path)

    with model_path.open("rb") as rf:
        payload = pickle.load(rf)

    model = payload.get("model")
    feature_names: list[str] = list(payload.get("feature_names", []))

    if model is None or not feature_names:
        raise SystemExit("Invalid model package: missing model or feature_names")

    rows = _read_jsonl(input_path)
    if not rows:
        raise SystemExit("No valid input rows found")

    output_path: Path | None = None
    wf = None
    if args.output_path:
        output_path = Path(args.output_path)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        wf = output_path.open("w", encoding="utf-8")

    try:
        for i, row in enumerate(rows, start=1):
            x = [_to_float(row.get(name)) for name in feature_names]
            pred = model.predict([x])[0]

            confidence = -1.0
            if hasattr(model, "predict_proba"):
                proba = model.predict_proba([x])[0]
                confidence = float(max(proba))

            rec = {
                "frame_id": int(row.get("frame_id", -1)),
                "mcu_ts_ms": int(row.get("mcu_ts_ms", -1)),
                "pred_label": str(pred),
                "confidence": confidence,
            }

            if wf is not None:
                wf.write(json.dumps(rec, ensure_ascii=True) + "\n")

            if i % args.print_every == 0:
                print(
                    f"row={i} frame={rec['frame_id']} "
                    f"pred={rec['pred_label']} conf={rec['confidence']:.3f}"
                )
    finally:
        if wf is not None:
            wf.close()

    if output_path is not None:
        print(f"prediction_saved={output_path}")


if __name__ == "__main__":
    main()
