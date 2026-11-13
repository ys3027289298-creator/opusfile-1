#!/bin/bash
#End-to-end verification for the tag/cover writing feature.
#Uses only the repository's own build entry points (unix/Makefile) and
# example programs, plus tiny self-made assets.
#Exits 0 only if every check passes.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/output"
ASSETS="$OUT/assets"
export PKG_CONFIG_PATH="$ROOT/deps/root/usr/lib/x86_64-linux-gnu/pkgconfig"
CFLAGS="-g -DOP_ENABLE_ASSERTIONS -fPIC -std=c89 -pedantic -Wextra -Wno-parentheses -Wno-long-long -Wall"
TAGGER="$ROOT/unix/tagging_example"
DECODER="$ROOT/unix/opusfile_example"
SEEKER="$ROOT/unix/seeking_example"
PASS=0
FAIL=0

ok(){
  if [ "$1" -eq 0 ];then
    PASS=$((PASS+1));echo "PASS: $2"
  else
    FAIL=$((FAIL+1));echo "FAIL: $2"
  fi
}

echo "== 1. build (unix/Makefile) =="
make -C "$ROOT/unix" -f Makefile CFLAGS="$CFLAGS" >"$OUT/build.log" 2>&1
ok $? "library and examples build"
grep -cE "warning|error" "$OUT/build.log" | grep -q "^0$"
ok $? "zero compiler warnings"

echo "== 2. unit checks (serialization round-trip + rejection) =="
cc -g -I"$ROOT/include" -I"$ROOT/deps/root/usr/include" \
 -I"$ROOT/deps/root/usr/include/opus" "$OUT/tag_limits_test.c" \
 "$ROOT/unix/libopusfile.a" \
 -L"$ROOT/deps/root/usr/lib/x86_64-linux-gnu" -logg -lopus \
 -o "$OUT/tag_limits_test" 2>>"$OUT/build.log"
ok $? "unit test builds"
"$OUT/tag_limits_test" >"$OUT/unit.log" 2>&1
ok $? "unit test: 20/20 checks"

echo "== 3. make tiny assets =="
mkdir -p "$ASSETS"
ffmpeg -hide_banner -loglevel error -f lavfi -i "sine=frequency=440:duration=1" \
 -c:a libopus -b:a 24k -y "$ASSETS/test.opus"
ok $? "make 1s opus file ($(stat -c%s "$ASSETS/test.opus") bytes)"
ffmpeg -hide_banner -loglevel error -f lavfi -i "color=c=0x336699:size=32x32" \
 -frames:v 1 -y "$ASSETS/cover.png"
ok $? "make 32x32 png cover ($(stat -c%s "$ASSETS/cover.png") bytes)"

echo "== 4. tag + cover round-trip =="
PCM_BEFORE=$("$DECODER" "$ASSETS/test.opus" 2>/dev/null | md5sum | cut -d' ' -f1)
"$TAGGER" "$ASSETS/test.opus" TITLE="Round Trip" ARTIST="opusfile" \
 --picture="$ASSETS/cover.png" >"$ASSETS/write.log" 2>&1
ok $? "write tags and cover"
"$TAGGER" "$ASSETS/test.opus" --dump-picture="$ASSETS/roundtrip.png" \
 >"$ASSETS/read.log" 2>&1
ok $? "read tags back"
grep -q "^  TITLE=Round Trip$" "$ASSETS/read.log"
ok $? "TITLE round-trips"
grep -q "^  ARTIST=opusfile$" "$ASSETS/read.log"
ok $? "ARTIST round-trips"
grep -q 'mime="image/png"' "$ASSETS/read.log"
ok $? "cover mime type preserved"
cmp -s "$ASSETS/cover.png" "$ASSETS/roundtrip.png"
ok $? "cover bytes identical after round-trip"
PCM_AFTER=$("$DECODER" "$ASSETS/test.opus" 2>/dev/null | md5sum | cut -d' ' -f1)
[ "$PCM_BEFORE" = "$PCM_AFTER" ]
ok $? "audio data untouched by tag write"

echo "== 5. idempotent re-write =="
"$TAGGER" "$ASSETS/test.opus" TITLE="Round Trip" ARTIST="opusfile" \
 --picture="$ASSETS/cover.png" >"$ASSETS/rewrite.log" 2>&1
"$TAGGER" "$ASSETS/test.opus" 2>&1 | grep -vE "^Vendor|^Comments" \
 >"$ASSETS/list1.log"
"$TAGGER" "$ASSETS/test.opus" TITLE="Round Trip" ARTIST="opusfile" \
 --picture="$ASSETS/cover.png" >/dev/null 2>&1
"$TAGGER" "$ASSETS/test.opus" 2>&1 | grep -vE "^Vendor|^Comments" \
 >"$ASSETS/list2.log"
cmp -s "$ASSETS/list1.log" "$ASSETS/list2.log"
ok $? "second write yields identical tags"

echo "== 6. invalid input is rejected =="
"$TAGGER" "$ASSETS/test.opus" "NOEQUALS" 2>&1 | grep -q -- "-131"
ok $? "comment without '=' -> OP_EINVAL(-131)"
"$TAGGER" "$ASSETS/test.opus" "$(printf 'BAD\x01TAG=x')" 2>&1 | grep -q -- "-131"
ok $? "control char in tag name -> OP_EINVAL(-131)"
"$TAGGER" "$ASSETS/test.opus" --picture="$ASSETS/cover.png" \
 --picture-type=99 2>&1 | grep -q -- "-131"
ok $? "picture type 99 -> OP_EINVAL(-131)"
cp "$ASSETS/test.opus" "$ASSETS/ro.opus"
chmod 444 "$ASSETS/ro.opus"
RO_BEFORE=$(md5sum "$ASSETS/ro.opus" | cut -d' ' -f1)
"$TAGGER" "$ASSETS/ro.opus" TITLE=x 2>&1 | grep -q -- "-128"
ok $? "read-only file -> OP_EREAD(-128)"
RO_AFTER=$(md5sum "$ASSETS/ro.opus" | cut -d' ' -f1)
[ "$RO_BEFORE" = "$RO_AFTER" ]
ok $? "read-only file left untouched"
chmod 644 "$ASSETS/ro.opus"
echo "not an opus file" >"$ASSETS/notopus.txt"
"$TAGGER" "$ASSETS/notopus.txt" TITLE=x 2>&1 | grep -q -- "-132"
ok $? "non-opus file -> OP_ENOTFORMAT(-132)"

echo "== 7. old interface regression =="
"$DECODER" "$ASSETS/test.opus" >/dev/null 2>"$ASSETS/decode.log"
ok $? "opusfile_example still decodes tagged file"
grep -q "Done: played 1.000s" "$ASSETS/decode.log"
ok $? "decoded length unchanged (1.000s)"
"$SEEKER" "$ASSETS/test.opus" 2>&1 | grep -q "^OK\.$"
ok $? "seeking_example still works"

echo
echo "RESULT: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
