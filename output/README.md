# 验收产物说明

本目录存放“归档时写标签和封面”功能的全部验证产物。

## 文件清单

- `verify.sh` — 端到端验收脚本(可重复运行,23 项检查,全部通过才退出 0)。
- `verify.log` — 最近一次验收运行结果(`RESULT: 23 passed, 0 failed`)。
- `demo.sh` — 分段演示脚本(6 段,段间停顿 2 秒,全程不等键盘)。
- `demo.cast` — `asciinema` 录屏(`asciinema play output/demo.cast` 回放)。
- `tag_limits_test.c` / `tag_limits_test` — 序列化层单元验证(20 项:
  标签/封面往返一致、二进制后缀保留、各类非法字段返回 `OP_EINVAL`)。
- `unit.log` — 单元验证输出。
- `build.log` — 构建日志(零警告)。
- `assets/` — 自造的小素材(`test.opus` 约 3.9KB,`cover.png` 120B)及
  各步骤的读写回读日志。

## 复现

```sh
output/verify.sh          # 构建 + 全部验收检查
bash output/demo.sh       # 分段演示(可配 asciinema rec 录制)
```

依赖:`deps/` 下是用 `apt-get download` 本地解包的 libogg/libopus 开发
头文件(环境无 root,无法系统安装),脚本已通过 `PKG_CONFIG_PATH` 指向它。
