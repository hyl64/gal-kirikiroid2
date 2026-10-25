#!/bin/bash
# run-video-check.sh - a movie decodes, paints, and advances.
#
# tests/media/twocolor.mp4 is 0.7s red then 0.7s green, so the frame on screen
# tells us whether the decoder really advanced or just painted once.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
KRKRRUN="${1:-$ROOT/build/krkrrun}"
TMP="$(mktemp -d /tmp/krkr-video.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

[ -x "$KRKRRUN" ] || { echo "FAIL: no krkrrun at $KRKRRUN"; exit 2; }
[ -f "$HERE/media/twocolor.mp4" ] || { echo "FAIL: test movie missing"; exit 2; }

cp "$HERE/media/twocolor.mp4" "$TMP/"
cd "$TMP"

fail=0
note() { echo "  $*"; }

SDL_VIDEODRIVER=dummy KRKR_LOOP_SECONDS=1.6 "$KRKRRUN" "$HERE/video-probe.tjs" \
	--screenshot "$TMP/end.bmp" >"$TMP/run.log" 2>&1
[ $? -eq 0 ] || { note "FAILED: the run exited non-zero"; fail=1; }
strings "$TMP/run.log" | grep -aq "VIDEO-PROBE-STARTED" \
	|| { note "MISSING: the probe never started"; fail=1; }
strings "$TMP/run.log" | grep -aq "opened: duration=1\." \
	|| { note "MISSING: the container duration was not read"; fail=1; }

# sample the middle of the movie rectangle in the final frame
read -r r g b <<<"$(python3 - "$TMP/end.bmp" <<'PY'
import struct, sys
d = open(sys.argv[1], 'rb').read()
off = struct.unpack_from('<I', d, 10)[0]
w, h = struct.unpack_from('<ii', d, 18)
i = off + (h - 1 - 60) * w * 4 + 80 * 4
print(d[i + 2], d[i + 1], d[i])
PY
)"
note "final frame pixel at the movie centre: rgb($r,$g,$b)"
if [ "${g:-0}" -gt 100 ] && [ "${r:-0}" -lt 60 ]; then
	note "the movie advanced to its second colour"
else
	note "FAILED: the frame did not advance (expected green, got rgb($r,$g,$b))"
	fail=1
fi

[ "$fail" = "0" ] && echo "PASS: a movie decodes, paints and advances" || { echo "FAIL"; exit 1; }
