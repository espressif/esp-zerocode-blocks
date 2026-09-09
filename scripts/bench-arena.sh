#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

# Measure the tensor arena each ML model actually needs, and print what the
# manifest should say.
#
#   scripts/bench-arena.sh <built-project-dir> [--write]
#
# WHY THIS EXISTS. A model's arena can only be learned by RUNNING it: TFLM
# computes it inside AllocateTensors and there is no offline equivalent. Left
# to guesswork the number is wrong in both directions — person-detect shipped
# at 100 KB when esp32s3 needs 160, and ZeroCode AI burned build cycles
# rediscovering that by hand. So: run it, read it, write it down.
#
# WHAT IT READS. frameworks/ml logs one greppable line per session create:
#
#   ZC_ARENA model=person_detect configured=163840 used=158016 want=158032 input=9216 psram=1
#
# `want` is used+16 — TFLM's own rule, because the arena needs 16-byte
# alignment to be fully usable (micro_interpreter.h).
#
# HOW IT RUNS IT. idf.py qemu, when the target is emulable. Note the gap:
# esp32s3 CANNOT be emulated (esp-emu runs p4 but not s3), and s3 is a chip
# where esp-nn's optimized kernels add scratch. So s3 numbers need real
# hardware — run the same product on a board and pipe `idf.py monitor` through
# this script's parser, or accept p4's optimized figure as an approximation and
# say so in the manifest's `source:` field. Do not silently interpolate.
set -uo pipefail
cd "$(dirname "$0")/.."
ROOT="$(pwd)"

DIR="${1:-}"
[ -n "$DIR" ] && [ -d "$DIR/build" ] || {
  echo "usage: scripts/bench-arena.sh <built-project-dir> [--write]" >&2
  echo "  the dir must already be built (idf.py set-target <chip> && idf.py build)" >&2
  exit 2
}
WRITE=0
[ "${2:-}" = "--write" ] && WRITE=1

: "${IDF_PATH:?bench-arena needs ESP-IDF (set IDF_PATH, or source export.sh)}"

CHIP="$(sed -n 's/^CONFIG_IDF_TARGET="\(.*\)"/\1/p' "$DIR/sdkconfig" 2>/dev/null | head -1)"
[ -n "$CHIP" ] || { echo "cannot read CONFIG_IDF_TARGET from $DIR/sdkconfig" >&2; exit 2; }

# esp-emu's set, from the monorepo's EMULATOR_SUPPORTED_CHIPS. Kept as a
# literal because this repo has no import path to it; if it drifts the worst
# case is a run that times out with no ZC_ARENA lines, which is legible.
case "$CHIP" in
  esp32c3|esp32c6|esp32h2|esp32p4|esp32s31) ;;
  *) echo "note: $CHIP has no virtual device — run this on real hardware and" >&2
     echo "      pipe 'idf.py monitor' output through: grep ZC_ARENA" >&2
     exit 3 ;;
esac

LOG="$(mktemp)"
echo "== running $DIR on $CHIP under qemu (30s)"
( cd "$DIR" && timeout 30 idf.py qemu monitor >"$LOG" 2>&1 ) || true

if ! grep -q "ZC_ARENA" "$LOG"; then
  echo "no ZC_ARENA lines — the product may not create a session at boot," >&2
  echo "or qemu failed. Last 15 lines:" >&2
  tail -15 "$LOG" >&2
  exit 1
fi

printf '\n%-22s %10s %10s %10s  %s\n' MODEL CONFIGURED USED WANT VERDICT
grep -o 'ZC_ARENA model=[^ ]* configured=[0-9]* used=[0-9]* want=[0-9]*' "$LOG" | sort -u |
while read -r line; do
  m=${line#*model=};   m=${m%% *}
  c=${line#*configured=}; c=${c%% *}
  u=${line#*used=};    u=${u%% *}
  w=${line#*want=};    w=${w%% *}
  if   [ "$c" -lt "$w" ]; then v="UNDER by $((w - c)) B — would fail on a bigger input"
  elif [ "$c" -gt $((w * 2)) ]; then v="over-provisioned $((c / 1024)) KB vs $((w / 1024)) KB"
  else v="ok"
  fi
  printf '%-22s %10s %10s %10s  %s\n' "$m" "$c" "$u" "$w" "$v"

  if [ "$WRITE" -eq 1 ] && [ -f "$ROOT/models/$m.yml" ]; then
    python3 - "$ROOT/models/$m.yml" "$w" "$CHIP" "$c" "$u" <<'PY'
import sys, datetime, re, pathlib
path, want, chip, configured, used = sys.argv[1], int(sys.argv[2]), sys.argv[3], int(sys.argv[4]), int(sys.argv[5])
p = pathlib.Path(path); t = p.read_text()
# Only the generic-C path defines the base; an esp-nn chip's figure is
# base+scratch, so record it as a measurement and let a human split it rather
# than guessing which half moved.
stamp = datetime.date.today().isoformat()
entry = f"    - {{chip: {chip}, configured: {configured}, used: {used}, want: {want}, date: {stamp}}}\n"
t = re.sub(r"(  measured_on: )\[\]\n", r"\1\n" + entry, t, count=1)
if entry not in t:  # already had entries
    t = re.sub(r"(  measured_on:\n)", r"\1" + entry, t, count=1)
t = t.replace("  source: upstream-declared", "  source: measured", 1)
p.write_text(t)
print(f"    -> recorded in models/{pathlib.Path(path).stem}.yml")
PY
  fi
done

echo
echo "manifest arena should be the WANT column (used + 16, TFLM's alignment rule)."
[ "$WRITE" -eq 0 ] && echo "re-run with --write to record these in models/*.yml"
rm -f "$LOG"
