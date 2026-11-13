#!/usr/bin/env bash
# Build (via the repo's own CMake entry) and run all acceptance checks.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
DEPS="$ROOT/output/deps/usr"
export PKG_CONFIG_PATH="$DEPS/lib/x86_64-linux-gnu/pkgconfig"

echo "== Building library and bundled examples with CMake =="
cmake -S "$ROOT" -B "$HERE/build" -DCMAKE_BUILD_TYPE=Debug \
  -DOP_DISABLE_HTTP=ON -DOP_DISABLE_DOCS=ON >/dev/null
cmake --build "$HERE/build" -j"$(nproc)"

echo
echo "== (Re)generating small test materials =="
"$HERE/make_materials.sh"

echo
echo "== Compiling acceptance test against the built library =="
gcc -std=c99 -Wall -Wextra -I"$ROOT/include" -I"$DEPS/include" \
  -I"$DEPS/include/opus" \
  "$HERE/tests/loudness_test.c" \
  "$HERE/build/libopusfile.a" \
  "$DEPS/lib/x86_64-linux-gnu/libopus.a" \
  "$DEPS/lib/x86_64-linux-gnu/libogg.a" -lm -o "$HERE/tests/loudness_test"

echo
echo "== Running acceptance checks =="
"$HERE/tests/loudness_test" \
  "$HERE/materials/valid_tags.opus" \
  "$HERE/materials/no_tags.opus" \
  "$HERE/materials/invalid_tags.opus" \
  "$HERE/materials/with_gain_head.opus" | tee "$HERE/test_results.txt"

echo
echo "== Bundled example: loudness dump (valid file), gain enabled =="
"$HERE/build/opusfile_example" "$HERE/materials/valid_tags.opus" \
  > "$HERE/valid_gain_on.wav" 2> "$HERE/example_valid.log"
grep -E "Gain adjustment|Loudness|R128|ReplayGain" "$HERE/example_valid.log"

echo
echo "== Bundled example: no-tags file returns an empty structure =="
"$HERE/build/opusfile_example" "$HERE/materials/no_tags.opus" \
  > "$HERE/notags.wav" 2> "$HERE/example_notags.log"
grep -E "Loudness" "$HERE/example_notags.log"

echo
echo "== Bundled example: --no-gain byte-identity check =="
"$HERE/build/opusfile_example" --no-gain "$HERE/materials/with_gain_head.opus" \
  > "$HERE/gain_off.wav" 2> "$HERE/example_gainoff.log"
"$HERE/build/opusfile_example" "$HERE/materials/with_gain_head.opus" \
  > "$HERE/gain_on.wav" 2> "$HERE/example_gainon.log"
cmp "$HERE/gain_off.wav" <("$HERE/build/opusfile_example" --no-gain \
  "$HERE/materials/with_gain_head.opus" 2>/dev/null) \
  && echo "  PASS: two --no-gain decodes are byte-identical"
if cmp -s "$HERE/gain_off.wav" "$HERE/gain_on.wav"; then
  echo "  FAIL: gain on/off outputs differ"; exit 1
else
  echo "  PASS: default (gain on) output differs from --no-gain output"
fi
grep -E "Gain adjustment|disabled" "$HERE/example_gainoff.log"
