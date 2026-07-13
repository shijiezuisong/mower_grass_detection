# TOF Grass Density Workflow

This file gives a direct path from TOF jsonl to grass-density features.

## 1. Collect raw TOF data

Use receiver script to save frames:

```powershell
& "D:\Program Files\Python313\python.exe" ".\python_scripts\tof_usart6_receiver.py" --out "output\tof_frames_dense.jsonl"
```

Repeat for different scenes, for example:

- sparse grass
- medium grass
- dense grass

## 2. Extract features

Use feature extractor on each file and attach label.

```powershell
& "D:\Program Files\Python313\python.exe" ".\python_scripts\tof_grass_density_features.py" --in "output\tof_frames_dense.jsonl" --out "output\tof_features_dense.jsonl" --window-size 15 --label dense
```

```powershell
& "D:\Program Files\Python313\python.exe" ".\python_scripts\tof_grass_density_features.py" --in "output\tof_frames_sparse.jsonl" --out "output\tof_features_sparse.jsonl" --window-size 15 --label sparse
```

## 3. Key features produced

Per-frame:

- valid_ratio
- dist_min_mm, dist_mean_mm, dist_std_mm
- dist_p10_mm, dist_p50_mm, dist_p90_mm
- near_ratio_100mm, near_ratio_200mm, near_ratio_300mm
- center_mean_mm, edge_mean_mm, center_edge_diff_mm
- roughness_mm
- avg_signal, avg_ambient

Sliding window:

- w_valid_ratio_mean, w_valid_ratio_std
- w_dist_mean_mm_mean, w_dist_mean_mm_std
- w_roughness_mm_mean, w_roughness_mm_std

## 4. Practical notes

- Current setup is 8x8, so each frame has 64 zones.
- Keep sensor height and tilt fixed when collecting datasets.
- Mix static and dynamic mower states in each class.
- If valid_ratio is very low for long periods, inspect scene and mounting first.

## 5. Suggested next step

Train a simple classifier first (random forest) with the extracted jsonl rows.

```powershell
& "D:\Program Files\Python313\python.exe" ".\python_scripts\tof_grass_density_train.py" --inputs "output\tof_features_sparse.jsonl" "output\tof_features_medium.jsonl" "output\tof_features_dense.jsonl" --model-out "output\tof_grass_density_model.pkl"
```

The trainer prints:

- accuracy
- confusion matrix
- classification report

And saves a model package to:

- `output/tof_grass_density_model.pkl`

## 6. Run inference

Use trained model to predict density labels from feature jsonl.

```powershell
& "D:\Program Files\Python313\python.exe" ".\python_scripts\tof_grass_density_infer.py" --model "output\tof_grass_density_model.pkl" --in "output\tof_features_dense.jsonl" --out "output\tof_density_pred_dense.jsonl"
```

Console output includes periodic prediction summaries, and all predictions can be saved to a JSONL file.

## 7. Run real-time inference

Use serial stream and trained model to print online density prediction.

```powershell
& "D:\Program Files\Python313\python.exe" ".\python_scripts\tof_grass_density_realtime.py" --model "output\tof_grass_density_model.pkl" --port COM3
```

If `--port` is omitted, script will scan ports and ask for selection.
