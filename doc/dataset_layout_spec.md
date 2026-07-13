# 图像 + TOF 数据集目录规范

本文档定义联合采集阶段的数据目录、文件格式和命名规则，便于多人协作、复现实验和后续训练。

## 1. 顶层结构建议

```text
dataset/
  session_YYYYMMDD_HHMMSS/
    images/
    tof/
    paired/
    labels/
    meta/
```

说明：

- `images/`：相机图像
- `tof/`：原始 TOF 串口帧落盘
- `paired/`：图像与 TOF 对齐索引
- `labels/`：人工标签或自动标签
- `meta/`：会话配置、设备信息、标定参数

## 2. 文件定义

### 2.1 images/

命名建议：

- `img_<cam_frame_id>_<cam_ts_ns>.jpg`

示例：

- `img_00012345_1720001234567890123.jpg`

### 2.2 tof/

原始 TOF 推荐 JSONL（每行一帧）：

- `tof_raw.jsonl`

每行字段建议：

- frame.frame_id
- frame.mcu_ts_ms
- frame.resolution
- frame.fps
- frame.distance_mm[]
- frame.target_status[]
- frame.signal_per_spad[]
- frame.ambient_per_spad[]
- frame.valid[]
- frame.host_ts_ns

### 2.3 paired/

对齐索引推荐 JSONL：

- `paired_index.jsonl`

每行字段建议：

- sample_id
- image.path
- image.cam_frame_id
- image.cam_ts_ns
- tof.frame_id
- tof.host_ts_ns
- dt_ms
- tof_valid_ratio
- tof_min_valid_mm
- tof_avg_valid_mm

### 2.4 labels/

阶段一可先空置；阶段二建议使用：

- `labels_grass_density.jsonl`

每行建议字段：

- sample_id
- grass_density_level（如 0/1/2/3）
- 堵转风险（如 low/medium/high）
- annotator
- note

### 2.5 meta/

建议包含：

- `session_info.json`：场地、天气、设备版本、采样目标
- `camera_intrinsics.json`：相机内参
- `tof_mount_extrinsics.json`：TOF 到相机外参
- `protocol_version.txt`：串口协议版本

## 3. 样本唯一键建议

统一使用 `sample_id` 关联多文件，格式示例：

- `s_<cam_frame_id>_<tof_frame_id>`

例如：

- `s_12345_67890`

## 4. 对齐质量与筛选规则建议

写入 paired 时同步写入质量字段，并提供采样后筛选：

- `dt_ms <= 50`
- `tof_valid_ratio >= 0.25`
- `tof_min_valid_mm` 在任务可用范围

## 5. 版本与兼容

建议在 `meta/session_info.json` 中记录：

- firmware_git_commit
- linux_parser_git_commit
- tof_protocol_version
- camera_driver_version

这样后续模型复现可准确定位数据来源。

## 6. 最小可用示例

```text
dataset/
  session_20260702_143000/
    images/
      img_00000001_1720000000001000000.jpg
    tof/
      tof_raw.jsonl
    paired/
      paired_index.jsonl
    labels/
      labels_grass_density.jsonl
    meta/
      session_info.json
      camera_intrinsics.json
      tof_mount_extrinsics.json
      protocol_version.txt
```

## 7. 实操建议

1. 先保证 `tof_raw.jsonl` 和 `images/` 稳定产出。
2. 再启用 `paired_index.jsonl` 的在线对齐。
3. 最后再做标签闭环（人工 + 规则辅助）。
4. 每次采集一个 session，避免多场景混写。
