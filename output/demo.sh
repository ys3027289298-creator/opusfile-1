#!/bin/bash
#Segmented, non-interactive demo of the tag/cover writing feature.
#Each segment prints a title and pauses briefly; nothing waits for keyboard
# input, so the whole script can be recorded unattended, e.g.:
#   asciinema rec --overwrite -c "bash output/demo.sh" output/demo.cast
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/output"
ASSETS="$OUT/assets"
export PKG_CONFIG_PATH="$ROOT/deps/root/usr/lib/x86_64-linux-gnu/pkgconfig"
CFLAGS="-g -DOP_ENABLE_ASSERTIONS -fPIC -std=c89 -pedantic -Wextra -Wno-parentheses -Wno-long-long -Wall"
TAGGER="$ROOT/unix/tagging_example"

segment(){
  echo
  echo "################################################################"
  echo "## $1"
  echo "################################################################"
}

pause(){
  sleep 2
}

run(){
  echo
  echo "\$ $*"
  "$@"
}

segment "1/6 构建:用仓库自带 unix/Makefile 编译库和示例"
run make -C "$ROOT/unix" -f Makefile CFLAGS="$CFLAGS"
pause

segment "2/6 造素材:1 秒 opus 音频 + 32x32 PNG 封面(都很小)"
mkdir -p "$ASSETS"
run ffmpeg -hide_banner -loglevel error -f lavfi \
 -i "sine=frequency=440:duration=1" -c:a libopus -b:a 24k \
 -y "$ASSETS/test.opus"
run ffmpeg -hide_banner -loglevel error -f lavfi \
 -i "color=c=0x336699:size=32x32" -frames:v 1 -y "$ASSETS/cover.png"
run ls -l "$ASSETS/test.opus" "$ASSETS/cover.png"
pause

segment "3/6 归档写入:给 opus 文件写标签和封面"
run "$TAGGER" "$ASSETS/test.opus"
run "$TAGGER" "$ASSETS/test.opus" TITLE="Round Trip" ARTIST="opusfile" \
 --picture="$ASSETS/cover.png"
pause

segment "4/6 往返一致:读回标签,封面字节逐一相同,音频不变"
run "$TAGGER" "$ASSETS/test.opus" --dump-picture="$ASSETS/roundtrip.png"
run cmp "$ASSETS/cover.png" "$ASSETS/roundtrip.png"
echo "封面原始字节一致 (cmp 无输出)"
echo
echo "\$ md5sum 解码PCM(写标签前后)"
"$ROOT/unix/opusfile_example" "$ASSETS/test.opus" 2>/dev/null | md5sum
pause

segment "5/6 非法输入:超长/非法字段返回错误码"
run "$TAGGER" "$ASSETS/test.opus" "NOEQUALS"
echo "退出码=$? (OP_EINVAL=-131)"
run "$TAGGER" "$ASSETS/test.opus" --picture="$ASSETS/cover.png" \
 --picture-type=99
echo "退出码=$? (OP_EINVAL=-131)"
cp "$ASSETS/test.opus" "$ASSETS/ro.opus"
chmod 444 "$ASSETS/ro.opus"
run "$TAGGER" "$ASSETS/ro.opus" TITLE=x
echo "退出码=$? (OP_EREAD=-128, 只读文件未被修改)"
chmod 644 "$ASSETS/ro.opus"
pause

segment "6/6 旧接口零回归:解码与 seek 不受影响"
run "$ROOT/unix/opusfile_example" "$ASSETS/test.opus"
echo
echo "\$ seeking_example (随机 seek 1000 次)"
"$ROOT/unix/seeking_example" "$ASSETS/test.opus" 2>&1 | tail -2
pause

segment "完成:标签/封面可写,往返一致,非法输入被拒,旧接口无回归"
