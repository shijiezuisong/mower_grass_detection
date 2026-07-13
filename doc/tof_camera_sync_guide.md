# TOF 与相机时间同步完整指南

本文档是端到端的操作手册，覆盖从问题原理到可直接运行的 Python 实现。  
相关文档：[tof_usart6_protocol.md](tof_usart6_protocol.md)、[tof_mcu_linux_time_alignment_flow.md](tof_mcu_linux_time_alignment_flow.md)、[camera_tof_alignment_guide.md](camera_tof_alignment_guide.md)

---

## 1. 为什么需要同步

MCU 和 Linux 是两套独立时钟，互不感知：

```
MCU    tx_time_get() → TICKS_TO_MS() → ts_ms
       上电从 0 开始，100 Hz tick，与 Linux 毫无关系

Linux  time.monotonic_ns()
       系统启动后单调递增，与 MCU 完全独立

相机   cam_ts_ns
       来自 Linux 单调时钟，与 Linux 同源，但与 MCU 不同源
```

TOF 帧头里的 `ts_ms` 是 MCU 自己的时间，**不能直接和 `cam_ts_ns` 做差值比较**。

需要建立 MCU 时钟 → Linux 时钟的映射关系，才能准确对齐。

---

## 2. 整体数据流

```
MCU 侧 (STM32F401RE)                    Linux 侧
─────────────────────────────────────────────────────────────────
VL53L8CX @30fps
  ↓ frame_id++  ts_ms = TICKS_TO_MS(tx_time_get())
  ↓ bsp_uart_send(USART6)  ──921600baud──→  串口接收线程
                                              ↓ readline()
                                              ↓ host_ts_ns = monotonic_ns()  ← 关键时刻
                                              ↓ 写入内存队列
                                              ↓
                            相机采集线程      TOF 对齐线程
                            cam_frame         ClockAligner.add_frame(ts_ms, host_ts_ns)
                            cam_ts_ns  ──→   最近邻匹配
                                              ↓
                                         paired sample → 落盘
```

---

## 3. 两级同步方案

### 3.1 一级方案：接收时刻直接对齐（立即可用）

以 Linux 收到每帧的时刻 `host_ts_ns` 作为该 TOF 帧的时间戳，直接与相机时间做最近邻匹配。

**误差组成**：

| 来源 | 典型值 |
|------|--------|
| 串口传输（~600B @921600baud） | ~6 ms（固定） |
| Linux 线程调度抖动 | 1～5 ms |
| **合计** | **~10～20 ms** |

文档验收标准（95% 样本 dt < 50ms）可以满足。

### 3.2 二级方案：线性时钟拟合（推荐，精度更高）

用滑动窗口对 `(ts_ms, host_ts_ns)` 锚点对做最小二乘线性拟合：

$$y = a \cdot x + b$$

- $x = ts\_ms \times 10^6$（MCU 时间，转为 ns 量纲）
- $y = host\_ts\_ns$（Linux 单调时钟，ns）
- $a$：时钟比例因子（补偿两边晶振微小频差，通常 ≈ 1.000xx）
- $b$：固定偏移（包含串口传输延迟 ~6ms）

拟合后计算每帧的对齐时间：

$$tof\_aligned\_ns = a \cdot (ts\_ms \times 10^6) + b$$

用 `tof_aligned_ns` 替代 `host_ts_ns` 与相机对齐，**精度可提升到 < 5ms**。

---

## 4. Python 实现

### 4.1 ClockAligner：时钟拟合器

```python
# clock_aligner.py
"""
滑动窗口线性时钟拟合器：将 MCU ts_ms 映射到 Linux monotonic ns。
依赖：numpy
"""
from __future__ import annotations

from collections import deque
from typing import Optional

import numpy as np


class ClockAligner:
    """
    维护最近 window 帧的 (mcu_ns, host_ns) 锚点对，
    每 update_every 帧用最小二乘拟合 y = a*x + b。
    离群锚点（残差 > 3×MAD）会被自动剔除。
    """

    def __init__(self, window: int = 200, update_every: int = 10) -> None:
        self._window = window
        self._update_every = update_every
        self._anchors: deque[tuple[float, float]] = deque(maxlen=window)
        self._frame_count = 0
        # 初始参数：假设 a=1（无频差），b=0（无偏移）
        self.a: float = 1.0
        self.b: float = 0.0
        self.ready: bool = False          # 至少拟合过一次后才置 True
        self.last_residual_ms: float = 0.0  # 上次拟合的中位残差（ms），用于健康监测

    # ------------------------------------------------------------------
    def add_frame(self, mcu_ts_ms: int, host_ts_ns: int) -> None:
        """每收到一帧调用一次，更新锚点并按需重新拟合。"""
        mcu_ns = float(mcu_ts_ms) * 1_000_000.0
        self._anchors.append((mcu_ns, float(host_ts_ns)))
        self._frame_count += 1
        if self._frame_count % self._update_every == 0:
            self._refit()

    # ------------------------------------------------------------------
    def to_linux_ns(self, mcu_ts_ms: int) -> int:
        """把 MCU 毫秒时间戳映射为 Linux 单调时钟 ns。"""
        return int(self.a * (float(mcu_ts_ms) * 1_000_000.0) + self.b)

    # ------------------------------------------------------------------
    def _refit(self) -> None:
        if len(self._anchors) < 10:
            return

        xs = np.array([a[0] for a in self._anchors], dtype=np.float64)
        ys = np.array([a[1] for a in self._anchors], dtype=np.float64)

        # 第一次全量拟合，剔除离群点后再次拟合
        a0, b0 = np.polyfit(xs, ys, 1)
        residuals = np.abs(ys - (a0 * xs + b0))
        mad = float(np.median(residuals))
        threshold = max(3.0 * mad, 1e6)  # 至少 1ms 容忍，防止全部被剔除

        mask = residuals <= threshold
        if mask.sum() >= 10:
            self.a, self.b = np.polyfit(xs[mask], ys[mask], 1)
        else:
            self.a, self.b = a0, b0

        # 计算拟合后残差，供外部健康监测
        final_residuals = np.abs(ys - (self.a * xs + self.b))
        self.last_residual_ms = float(np.median(final_residuals)) / 1_000_000.0
        self.ready = True
```

### 4.2 PairedBuffer：滑动 TOF 缓存 + 最近邻匹配

```python
# paired_buffer.py
"""
维护最近 N 帧 TOF，为每帧相机做最近邻 TOF 匹配。
"""
from __future__ import annotations

from collections import deque
from dataclasses import dataclass
from typing import Optional


@dataclass
class TofEntry:
    frame_id: int
    mcu_ts_ms: int
    host_ts_ns: int
    aligned_ns: int           # 经 ClockAligner 计算的对齐时间
    valid_ratio: float
    distance_mm: list[int]
    target_status: list[int]
    signal_per_spad: list[int]
    ambient_per_spad: list[int]
    valid: list[int]


@dataclass
class PairedSample:
    # 相机侧
    cam_frame_id: int
    cam_ts_ns: int
    image_path: str
    # TOF 侧
    tof_frame_id: int
    tof_mcu_ts_ms: int
    tof_host_ts_ns: int
    tof_aligned_ns: int
    tof_valid_ratio: float
    tof_min_valid_mm: int
    tof_avg_valid_mm: float
    tof_distance_mm: list[int]
    tof_target_status: list[int]
    tof_signal_per_spad: list[int]
    tof_ambient_per_spad: list[int]
    tof_valid: list[int]
    # 对齐质量
    dt_ms: float


class PairedBuffer:
    """
    保存最近 window 帧 TOF，对外提供 match() 接口。
    线程安全需由调用方保证（或使用 threading.Lock）。
    """

    def __init__(self, window: int = 90) -> None:
        self._buf: deque[TofEntry] = deque(maxlen=window)

    def push(self, entry: TofEntry) -> None:
        self._buf.append(entry)

    def match(
        self,
        cam_frame_id: int,
        cam_ts_ns: int,
        image_path: str,
        dt_max_ms: float = 50.0,
        valid_ratio_min: float = 0.25,
    ) -> Optional[PairedSample]:
        """
        为一帧相机找最近邻 TOF，返回 PairedSample 或 None（不满足质量条件时）。
        使用 aligned_ns 做时间差计算；若 aligner 尚未就绪则退回 host_ts_ns。
        """
        if not self._buf:
            return None

        best = min(self._buf, key=lambda e: abs(cam_ts_ns - e.aligned_ns))
        dt_ns = abs(cam_ts_ns - best.aligned_ns)
        dt_ms = dt_ns / 1_000_000.0

        if dt_ms > dt_max_ms:
            return None
        if best.valid_ratio < valid_ratio_min:
            return None

        valid_dists = [
            d for d, v in zip(best.distance_mm, best.valid) if v == 1
        ]
        min_valid_mm = min(valid_dists) if valid_dists else -1
        avg_valid_mm = sum(valid_dists) / len(valid_dists) if valid_dists else -1.0

        return PairedSample(
            cam_frame_id=cam_frame_id,
            cam_ts_ns=cam_ts_ns,
            image_path=image_path,
            tof_frame_id=best.frame_id,
            tof_mcu_ts_ms=best.mcu_ts_ms,
            tof_host_ts_ns=best.host_ts_ns,
            tof_aligned_ns=best.aligned_ns,
            tof_valid_ratio=best.valid_ratio,
            tof_min_valid_mm=min_valid_mm,
            tof_avg_valid_mm=avg_valid_mm,
            tof_distance_mm=best.distance_mm,
            tof_target_status=best.target_status,
            tof_signal_per_spad=best.signal_per_spad,
            tof_ambient_per_spad=best.ambient_per_spad,
            tof_valid=best.valid,
            dt_ms=dt_ms,
        )
```

### 4.3 接收线程骨架

```python
# 串口接收线程（与相机线程并行运行）
import time
import serial
import threading
import json

from clock_aligner import ClockAligner
from paired_buffer import PairedBuffer, TofEntry
from tof_usart6_receiver import parse_tof_line  # 复用已有解析器

aligner = ClockAligner(window=200, update_every=10)
paired_buf = PairedBuffer(window=90)  # 保留最近 3s TOF（@30fps）
buf_lock = threading.Lock()

def tof_receiver_thread(port: str, baud: int = 921600) -> None:
    with serial.Serial(port, baud, timeout=1) as ser:
        while True:
            raw = ser.readline()
            if not raw:
                continue
            host_ts_ns = time.monotonic_ns()   # ← 尽早打戳，减少调度抖动
            try:
                line = raw.decode("utf-8", errors="replace").strip()
                frame = parse_tof_line(line)
                frame.host_ts_ns = host_ts_ns  # 覆盖为此处精确打戳的值

                # 更新时钟拟合器
                aligner.add_frame(frame.mcu_ts_ms, host_ts_ns)

                # 计算对齐时间
                aligned_ns = (
                    aligner.to_linux_ns(frame.mcu_ts_ms)
                    if aligner.ready
                    else host_ts_ns     # 拟合未就绪时退回接收时刻
                )

                valid_count = sum(frame.valid)
                valid_ratio = valid_count / len(frame.valid) if frame.valid else 0.0

                entry = TofEntry(
                    frame_id=frame.frame_id,
                    mcu_ts_ms=frame.mcu_ts_ms,
                    host_ts_ns=host_ts_ns,
                    aligned_ns=aligned_ns,
                    valid_ratio=valid_ratio,
                    distance_mm=frame.distance_mm,
                    target_status=frame.target_status,
                    signal_per_spad=frame.signal_per_spad,
                    ambient_per_spad=frame.ambient_per_spad,
                    valid=frame.valid,
                )
                with buf_lock:
                    paired_buf.push(entry)

            except Exception as e:
                print(f"[TOF] parse error: {e}")
```

### 4.4 相机线程骨架

```python
# 相机采集线程（示意，替换为实际相机 SDK 调用）
import dataclasses
import json
from pathlib import Path

PAIRED_OUT = Path("output/paired_index.jsonl")
PAIRED_OUT.parent.mkdir(parents=True, exist_ok=True)

def camera_thread(output_dir: Path) -> None:
    cam_frame_id = 0
    with PAIRED_OUT.open("a", encoding="utf-8") as f_paired:
        while True:
            # --- 替换为实际相机帧获取 ---
            image, cam_ts_ns = camera_grab_frame()
            image_path = output_dir / f"img_{cam_frame_id:08d}_{cam_ts_ns}.jpg"
            image_path.parent.mkdir(parents=True, exist_ok=True)
            image.save(str(image_path))
            # ----------------------------

            with buf_lock:
                sample = paired_buf.match(
                    cam_frame_id=cam_frame_id,
                    cam_ts_ns=cam_ts_ns,
                    image_path=str(image_path),
                    dt_max_ms=50.0,
                    valid_ratio_min=0.25,
                )

            if sample is not None:
                f_paired.write(json.dumps(dataclasses.asdict(sample)) + "\n")
                f_paired.flush()

            cam_frame_id += 1
```

---

## 5. 误差预期

| 方案 | 典型中位误差 | 95% 分位误差 |
|------|------------|------------|
| 一级（`host_ts_ns` 直接匹配） | 8～15 ms | < 30 ms |
| 二级（线性拟合 `aligned_ns`） | 2～5 ms | < 10 ms |

验收标准（来自 `tof_mcu_linux_time_alignment_flow.md`）：

- 中位时间差 < 20 ms ✅（两级均满足）
- 95% 样本时间差 < 50 ms ✅（两级均满足）

---

## 6. 健康监测建议

```python
# 每隔一段时间检查拟合质量
if aligner.ready and aligner.last_residual_ms > 20.0:
    print(f"[WARN] 时钟拟合残差偏大: {aligner.last_residual_ms:.1f} ms，考虑重置拟合窗口")
    aligner = ClockAligner()   # 重置，重新积累锚点
```

其他监测指标：

- `frame_id` 跳变 → 丢帧（串口拥塞或 Linux 处理慢）
- `valid_ratio` 持续 < 0.25 → 传感器遮挡或安装异常
- `dt_ms` 持续 > 50ms → 相机或 TOF 帧率异常

---

## 7. MCU 侧前提（已满足）

| 要求 | 实现 | 文件 |
|------|------|------|
| `ts_ms` 单调递增 | `TICKS_TO_MS(tx_time_get())` | `app_threadx.c` |
| `frame_id` 连续递增 | `s_frame_id++` | `vl53l8cx_app.c` |
| 发送不阻塞 TOF 采集 | `bsp_uart_send()`（DMA） | `vl53l8cx_app.c` |
| 帧率稳定 30fps | 传感器硬件配置 | `vl53l8cx_app.c` |

---

## 8. 快速启动检查清单

- [ ] MCU 固件已烧录，USART6 连接 Linux（USB-UART 或直连）
- [ ] 运行 `tof_usart6_receiver.py`，确认 `frame_id` 连续、`mcu_fps` 接近 30
- [ ] 运行 `clock_aligner.py` 单元测试，确认 `ready=True` 后残差 < 10ms
- [ ] 相机驱动打 `CLOCK_MONOTONIC` 时间戳（非 wall clock）
- [ ] 联调时检查 `dt_ms` 分布，中位值 < 20ms 则对齐正常
- [ ] 落盘 `paired_index.jsonl`，验证 `tof_valid_ratio >= 0.25` 样本占比

---

## 9. 参考

- [tof_usart6_protocol.md](tof_usart6_protocol.md)：串口协议格式
- [tof_mcu_linux_time_alignment_flow.md](tof_mcu_linux_time_alignment_flow.md)：时间对齐理论
- [camera_tof_alignment_guide.md](camera_tof_alignment_guide.md)：Linux 侧对齐流程
- [dataset_layout_spec.md](dataset_layout_spec.md)：落盘目录规范
