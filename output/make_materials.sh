#!/usr/bin/env bash
# Generate tiny Ogg Opus test materials (no interactive input).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
MAT="$HERE/materials"
mkdir -p "$MAT"

# A 0.3 s, 48 kHz, mono, low-bitrate tone -> keeps each file well under 4 kB.
SRC_ARGS=( -hide_banner -loglevel error -f lavfi
  -i "sine=frequency=440:sample_rate=48000:duration=0.3"
  -c:a libopus -application voip -vbr off -b:a 24k )

# 1) Full valid loudness metadata.
ffmpeg "${SRC_ARGS[@]}" \
  -metadata "R128_TRACK_GAIN=512" \
  -metadata "R128_ALBUM_GAIN=-256" \
  -metadata "REPLAYGAIN_TRACK_PEAK=1.234567" \
  -metadata "REPLAYGAIN_ALBUM_PEAK=0.987654" \
  -y "$MAT/valid_tags.opus"

# 2) No loudness tags at all.
ffmpeg "${SRC_ARGS[@]}" -y "$MAT/no_tags.opus"

# 3) Invalid values mixed with valid ones.
ffmpeg "${SRC_ARGS[@]}" \
  -metadata "R128_TRACK_GAIN=not_a_number" \
  -metadata "R128_ALBUM_GAIN=300" \
  -metadata "REPLAYGAIN_TRACK_PEAK=abc" \
  -metadata "REPLAYGAIN_ALBUM_PEAK=-1.5" \
  -y "$MAT/invalid_tags.opus"

# 4) Same encoded audio as (1) but with a non-zero OpusHead output gain,
#    used for the gain on/off comparison.
ffmpeg "${SRC_ARGS[@]}" \
  -metadata "R128_TRACK_GAIN=512" \
  -y "$MAT/with_gain_head.opus"
python3 "$HERE/set_head_gain.py" "$MAT/with_gain_head.opus" 768
