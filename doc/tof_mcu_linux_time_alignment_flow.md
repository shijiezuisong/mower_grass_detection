# TOF 时间轴对齐流程（MCU -> Linux）

本文档给出一套先可用、再优化的时间对齐流程，目标是让 MCU 发送的 TOF 数据稳定映射到 Linux 时间轴，便于与相机数据融合。

## 1. 目标

1. MCU 侧持续发送 TOF 帧，带 MCU 时间戳。
2. Linux 侧为每帧补充接收时间戳。
3. Linux 建立 MCU 时间到 Linux 时间的映射关系。
4. 输出可直接用于相机对齐的 TOF 时间戳。

## 2. 前提（当前协议已满足）

TOF 每帧至少包含：

- frame_id
- ts_ms（MCU 毫秒时基）
- resolution
- fps
- D/S/G/A/V 数据

Linux 接收到每一帧后，额外记录：

- host_rx_ns（Linux 单调时钟，建议 CLOCK_MONOTONIC）

## 3. 一级方案（先跑通，立即可用）

这是最快可上线方案，直接使用接收时间作为 TOF 时间。

流程：

1. MCU 按 30fps 连续发送 TOF 行协议。
2. Linux 串口线程按行解析，每帧打 host_rx_ns。
3. 将 TOF 时间暂定为 tof_ts_ns = host_rx_ns。
4. 与相机 cam_ts_ns 做最近邻配对。

优点：

- 实现简单，立刻可用。

缺点：

- 包含串口传输与调度抖动，单帧会有几毫秒级波动。

## 4. 二级方案（推荐，提升对齐精度）

在一级方案基础上，用滑动窗口拟合 MCU 时钟到 Linux 时钟映射。

### 4.1 建立锚点

每帧形成锚点对：

- x_i = ts_ms_i × 1e6（转为 ns）
- y_i = host_rx_ns_i

### 4.2 线性映射

拟合关系：

$$
y = a x + b
$$

其中：

- x：MCU 时间（ns）
- y：Linux 时间（ns）
- a：时钟比例因子（补偿主机与 MCU 的微小频偏）
- b：时钟偏移

### 4.3 生成对齐时间

对每个 TOF 帧计算：

$$
tof\_aligned\_ns = a \cdot (ts\_ms \times 10^6) + b
$$

该时间用于和相机 cam_ts_ns 对齐，而不是直接用 host_rx_ns。

### 4.4 拟合建议参数

- 滑动窗口长度：100 到 300 帧。
- 更新周期：每 10 帧更新一次 a,b。
- 异常剔除：丢弃离群锚点（例如残差超过 3 倍中位绝对偏差）。

## 5. 与相机对齐流程

1. 相机每帧记录 cam_ts_ns。
2. TOF 每帧计算 tof_aligned_ns。
3. 对每帧相机做最近邻匹配：

$$
\hat{j} = \arg\min_j |cam\_ts\_ns(i) - tof\_aligned\_ns(j)|
$$

4. 计算时间差：

$$
\Delta t = |cam\_ts\_ns - tof\_aligned\_ns|
$$

5. 若 Delta t 大于阈值（建议 50ms），该样本丢弃或降权。

## 6. MCU 侧执行要求（确保可对齐）

1. ts_ms 必须单调递增，不回跳。
2. frame_id 连续递增，用于检测丢帧。
3. 输出帧率稳定在目标值（30fps）。
4. 保持固定协议字段顺序，避免 Linux 解析歧义。

## 7. Linux 侧执行要求（确保映射稳定）

1. 串口接收线程与对齐线程解耦（队列或环形缓冲）。
2. 使用单调时钟打时间戳，禁止使用 wall clock。
3. 定期监控映射残差，残差异常时重置拟合窗口。
4. 记录 dt_ms 分布，用于在线健康监测。

## 8. 产出字段建议（写入 paired 索引）

每条样本建议至少包含：

- image.cam_ts_ns
- tof.frame_id
- tof.ts_ms
- tof.host_rx_ns
- tof.aligned_ns
- dt_ms
- tof_valid_ratio

## 9. 先后实施顺序

1. 先上线一级方案，保证全链路稳定采集。
2. 再切二级方案，加入线性拟合与离群剔除。
3. 最后根据 dt_ms 分布调阈值和窗口参数。

## 10. 验收标准（建议）

1. 连续 10 分钟无明显丢帧（frame_id 基本连续）。
2. 对齐后中位时间差小于 20ms。
3. 95% 样本时间差小于 50ms。
4. TOF 有效点比例异常帧可被识别并过滤。
