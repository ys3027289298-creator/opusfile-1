#!/usr/bin/env bash
# Produce segmented, titled, non-interactive asciinema recordings under
# output/recordings. Each segment is a separate file with a metadata title and
# an in-terminal banner; explicit sleeps create visible pauses between commands.
# Nothing waits for keyboard input.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
REC="$HERE/recordings"
mkdir -p "$REC"

asciinema rec --headless --overwrite --return --title "Segment 1 - Build" \
  --window-size 100x30 "$REC/seg1_build.cast" -c "$HERE/seg/seg1.sh"
sleep 2
asciinema rec --headless --overwrite --return --title "Segment 2 - Materials" \
  --window-size 100x30 "$REC/seg2_materials.cast" -c "$HERE/seg/seg2.sh"
sleep 2
asciinema rec --headless --overwrite --return --title "Segment 3 - Acceptance" \
  --window-size 100x30 "$REC/seg3_acceptance.cast" -c "$HERE/seg/seg3.sh"
sleep 2
asciinema rec --headless --overwrite --return --title "Segment 4 - Example" \
  --window-size 100x30 "$REC/seg4_example.cast" -c "$HERE/seg/seg4.sh"

echo
echo "Recordings written to $REC:"
ls -1 "$REC"
