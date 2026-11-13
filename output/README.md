# 响度（母带）信息解析与可选增益 — 验证产物

本目录包含为 `libopusfile` 增加的响度元数据解析、可选增益开关的全部验证产物。
所有验证均通过仓库自带入口（CMake 构建 + 自带 `opusfile_example`）完成，
素材为本地生成的小型 Ogg Opus 文件（每个约 1.2 KB），全程无键盘交互。

## 代码改动（位于仓库根，不在本目录）

- `include/opusfile.h`
  - 新增 `struct OpusLoudness` / `OpusLoudness` 与字段掩码
    `OP_LOUDNESS_TRACK_GAIN/ALBUM_GAIN/TRACK_PEAK/ALBUM_PEAK`。
  - 新增 `opus_tags_get_loudness()`：一次解析 R128 增益与 ReplayGain 峰值，
    缺失/非法字段只清对应掩码位，整体始终返回成功（空结构而非报错）。
  - 新增 `op_set_gain_enabled()` / `op_get_gain_enabled()`：增益应用可选，
    关闭时向解码器下发 0 dB，返回未加增益的原始样本。
- `src/info.c`：实现 `opus_tags_get_loudness()` 与严格的峰值数值解析
  （有限、非负；拒绝 NaN/无穷/超长整数；支持小数与指数）。
- `src/internal.h` / `src/opusfile.c`：新增 `gain_enabled` 字段，默认开启；
  在 `op_update_gain()` 中于关闭时强制 0 dB。
- `examples/opusfile_example.c`：打印解析后的响度结构，并支持 `--no-gain`。

单位约定：R128 增益保持标签原生单位 1/256 dB（Q8 dB）；峰值保持线性满刻度
振幅（1.0 = 0 dBFS）。dB→线性换算为 `pow(10, gain_q8/(20*256))`，测试中校验。

## 目录结构

- `run_tests.sh`：一键构建并运行全部验收检查（生成日志与 `test_results.txt`）。
- `make_materials.sh` / `set_head_gain.py`：生成小型素材；后者在不重新编码的
  前提下改写 OpusHead 的 `output_gain` 并重算 Ogg 页 CRC。
- `oggdump.py`：最小 Ogg/Opus 标签与头部增益查看器。
- `tests/loudness_test.c`：C 验收程序（字段回读、空结构、非法值、增益开关、
  单位换算），链接本仓库构建出的 `libopusfile.a`。
- `materials/`：测试素材（valid / no_tags / invalid / with_gain_head / 边界用例）。
- `build/`：仓库 CMake 入口的构建产物（含 `opusfile_example`）。
- `deps/`：本地解包的 libopus/libogg 开发文件（环境无 root，仅用于编译）。
- `recordings/`：4 段带标题、段内停顿的 asciinema 录屏。
- `seg/`：每段录屏实际执行的脚本。
- `*.log` / `test_results.txt` / `*.wav`：示例输出与解码对照文件。

## 录屏分段（recordings/）

1. `seg1_build.cast` — Segment 1 - Build：CMake 配置与构建。
2. `seg2_materials.cast` — Segment 2 - Materials：生成并查看小素材与标签。
3. `seg3_acceptance.cast` — Segment 3 - Acceptance：运行验收检查。
4. `seg4_example.cast` — Segment 4 - Example：自带示例响度转储与 `--no-gain`
   字节一致性。

重新生成录屏：`./record_segments.sh`（headless、自动结束、不等待键盘）。

## 复现步骤

```sh
export PKG_CONFIG_PATH=$PWD/deps/usr/lib/x86_64-linux-gnu/pkgconfig
./run_tests.sh
```

## 验收结论（与需求逐条对应）

- 字段值与写入值一致：R128 增益 Q8 值与 ReplayGain 峰值逐字段相等（见
  `test_results.txt` 第 [1] 组）。
- 缺失返回空结构而非报错：无标签文件 `fields=0`、返回 0（第 [2] 组）。
- 非法值被忽略：非数字/负数/超长/NaN 均被丢弃，同结构内合法值仍保留
  （第 [3] 组，及 `materials/edge_tags.opus`）。
- 增益可选、关闭零差异：`--no-gain` 两次解码逐样本/逐字节一致；默认开启时
  输出确实改变（第 [4] 组与 seg4）。
- 单位换算正确：实际增益比例与 `pow(10,q8/(20*256))` 吻合，覆盖头部增益与
  R128 track 增益两条路径。
