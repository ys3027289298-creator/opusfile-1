#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
banner "Segment 2/4: Generate tiny materials and inspect embedded tags"
pause
echo '$ ./output/make_materials.sh'
pause
"$HERE/make_materials.sh"
pause
echo '$ ls -l output/materials   (each file ~1.2 KB)'
ls -l "$HERE/materials"
pause
echo '$ python3 output/oggdump.py materials/valid_tags.opus'
pause
python3 "$HERE/oggdump.py" "$HERE/materials/valid_tags.opus"
pause
echo '$ python3 output/oggdump.py materials/with_gain_head.opus (header gain = 768 q8)'
pause
python3 "$HERE/oggdump.py" "$HERE/materials/with_gain_head.opus"
pause
