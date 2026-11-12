#!/bin/bash
#Demo/verification run, recorded with asciinema.
#Segments are titled and separated by pauses; no keyboard input is needed.
cd "$(dirname "$0")/.." || exit 1

pause(){ sleep 2; }
title(){ printf '\n===== %s =====\n' "$1"; pause; }

title "1/5 改动概览: git diff --stat"
git diff --stat
pause

title "2/5 构建 (cmake, 仓库自带构建)"
PKG_CONFIG_PATH=$HOME/deps/lib/pkgconfig cmake -B build \
  -DOP_DISABLE_DOCS=ON > /dev/null
cmake --build build -j4 2>&1 | grep -E "Built target"
pause

title "3/5 造多段拼接素材: 3 x 1.2s 正弦 -> chained.opus"
for i in 0 1 2; do
  f=$((440*(i+1)))
  ffmpeg -hide_banner -loglevel error \
    -f lavfi -i "sine=frequency=$f:sample_rate=48000:duration=1.2" \
    -ac 1 -c:a libopus -b:a 64k -y "output/seg$i.opus"
done
cat output/seg0.opus output/seg1.opus output/seg2.opus > output/chained.opus
ls -l output/*.opus
pause

title "4/5 仓库自带入口验证: examples/seeking_example"
echo "覆盖: 按段枚举 / 按时间跨段定位(样本级) / 边界不取上一段尾部 / 越界报错"
./build/seeking_example output/chained.opus 2>&1 | tr '\r' '\n' \
  > output/verify_full.log
status=${PIPESTATUS[0]}
grep -vE '^[[:space:]]*[0-9]+ \[' output/verify_full.log
pause

title "5/5 结果汇总"
grep -E "link [0-9]|Max time-seek error|^OK" output/verify_full.log \
  | tee output/summary.txt
echo "seeking_example exit status: $status" | tee -a output/summary.txt
pause
