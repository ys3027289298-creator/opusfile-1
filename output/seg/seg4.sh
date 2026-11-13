#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
banner "Segment 4/4: Bundled example, loudness dump and --no-gain identity"
pause
echo '$ opusfile_example materials/valid_tags.opus   (stderr metadata)'
pause
"$HERE/build/opusfile_example" "$HERE/materials/valid_tags.opus" \
  > /dev/null 2> /tmp/seg4_valid.log
grep -E "Gain adjustment|Loudness|R128|ReplayGain" /tmp/seg4_valid.log
pause
echo '$ opusfile_example --no-gain with_gain_head.opus twice -> byte-identical'
pause
"$HERE/build/opusfile_example" --no-gain "$HERE/materials/with_gain_head.opus" \
  > /tmp/a.wav 2>/dev/null
"$HERE/build/opusfile_example" --no-gain "$HERE/materials/with_gain_head.opus" \
  > /tmp/b.wav 2>/dev/null
cmp -s /tmp/a.wav /tmp/b.wav && echo "cmp: identical (zero difference)"
pause
echo '$ default (gain on) vs --no-gain -> must differ'
"$HERE/build/opusfile_example" "$HERE/materials/with_gain_head.opus" \
  > /tmp/c.wav 2>/dev/null
if ! cmp -s /tmp/a.wav /tmp/c.wav; then echo "cmp: outputs differ as expected"; fi
pause
banner "All segments finished"
