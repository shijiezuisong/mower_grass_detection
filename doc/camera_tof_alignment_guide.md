# 相机与 TOF 对齐指南（Linux 侧）

本文档用于指导 Linux 侧将相机帧与 MCU 通过 USART6 上报的 TOF 帧进行时间对齐，并产出可用于训练与验证的数据。

## 1. 输入数据

### 1.1 相机侧

每帧至少包含：

- image_frame_id：相机帧号
- cam_ts_ns：相机单调时钟时间戳（建议 `CLOCK_MONOTONIC`，单位 ns）
- image_path：图像文件路径（或内存中的图像对象）

### 1.2 TOF 侧

每帧来自串口协议（见 [doc/tof_usart6_protocol.md](doc/tof_usart6_protocol.md)）：

- frame_id
- ts_ms（MCU 时间戳）
- resolution
- fps
- D/S/G/A/V 数组
- host_ts_ns（Linux 接收时补充）

## 2. 时间基准建议

为减少跨设备时钟差异影响，建议采用 Linux 接收时刻 `host_ts_ns` 作为 TOF 与相机对齐基准。

- 相机：`cam_ts_ns`
- TOF：`host_ts_ns`

不要直接用 `ts_ms` 与 `cam_ts_ns` 对齐，`ts_ms` 主要用于 MCU 内部排序和诊断。

## 3. 对齐方法（最近邻）

对每一帧相机图像，选择时间差最小的 TOF 帧：

$$
\hat{j} = \arg\min_j |cam\_ts\_ns(i) - host\_ts\_ns(j)|
$$

并定义：

$$
\Delta t = |cam\_ts\_ns(i) - host\_ts\_ns(\hat{j})|
$$

若 $\Delta t > T_{max}$（建议初始 50ms），则该样本标记为未对齐成功并丢弃。

## 4. 实时流程建议

1. 串口线程持续解析 TOF，写入内存环形缓冲（按 `host_ts_ns` 升序）。
2. 相机线程取到新图像后，查询 TOF 缓冲做最近邻匹配。
3. 匹配成功则产出一条 paired 样本（图像 + TOF + 时间差）。
4. 将 paired 样本写入 JSONL 索引，图像按帧落盘。

## 5. 质量控制建议

对每条 paired 样本增加质量字段：

- dt_ms：对齐时间差（毫秒）
- tof_valid_ratio：TOF 有效点比例（`V=1` 占比）
- tof_min_valid_mm：有效点最小距离
- tof_avg_valid_mm：有效点平均距离

推荐过滤条件：

- `dt_ms <= 50`
- `tof_valid_ratio >= 0.25`

## 6. TOF 有效点使用建议

TOF 数组计算时优先使用 `V=1` 的点；`V=0` 的点不要直接参与建模。

对于 `D` 的离群值（负值、极大值），即使状态看似可用也建议结合 `G`（信号）与时序滤波做二次筛选。

## 7. 调试检查清单

1. 串口帧号连续：`frame_id` 无大幅跳变。
2. 对齐时间差分布：`dt_ms` 应集中在小范围内。
3. 有效率分布：`tof_valid_ratio` 在合理范围（非长期接近 0）。
4. 图像与 TOF 场景一致：目标出现位置与 TOF 距离变化一致。

## 8. 产出内容

至少产出两类文件：

- 图像文件（jpg/png）
- paired 索引（jsonl/csv），每行记录图像与 TOF 的对应关系

目录规范建议见 [doc/dataset_layout_spec.md](doc/dataset_layout_spec.md)。
