#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

# Compile ONE representative product per framework, for one chip.
#
#   scripts/ci-build-frameworks.sh <chip>
#
# WHY NOT every product x every chip: 73 products x 8 chips is ~584 builds, and
# they are not 584 different questions. What the compiler can tell us that the
# validator cannot is "does this FRAMEWORK's generated code build for this
# CHIP" — and a second Matter light answers that no better than the first.
# Product-level correctness (params published/consumed, pin legality, block
# wiring) is checked for ALL products by `validate`, without a compiler.
#
# So: the compiler covers the framework x chip grid, the validator covers the
# catalog. Between them nothing is unchecked, and CI finishes in minutes.
#
# The chip list for each framework is the framework block's own `chips:`
# (code_blocks/frameworks/<fw>/block.yml) — what it has been built for.
set -uo pipefail

CHIP="${1:?usage: ci-build-frameworks.sh <chip>}"
cd "$(dirname "$0")/.."

# Parallel lanes (CI_NODE_INDEX 1-based / CI_NODE_TOTAL): each lane builds its
# share of the sweep (round-robin over the would-be builds) so the wall time is
# the slowest share, not the sum. Outside CI both default to a single lane.
LANE="${CI_NODE_INDEX:-1}"
LANES="${CI_NODE_TOTAL:-1}"
BUILD_I=0
mine() { BUILD_I=$((BUILD_I + 1)); [ $(( (BUILD_I - 1) % LANES )) -eq $(( LANE - 1 )) ]; }

# Representative product per framework, DERIVED: prefer a product whose
# frameworks list is exactly [F] (isolates the framework), else the first that
# includes F. Deriving means a new framework gets covered the day someone adds
# a product for it, with no CI edit — the failure mode of the matrix this
# replaces was precisely that its hardcoded list stopped matching the catalog.
mapfile -t FRAMEWORKS < <(
  grep -h '^frameworks:' product_configurations/*/product.yml \
    | sed 's/frameworks: *//; s/[][]//g; s/, */\n/g' | tr -d ' ' | grep -v '^$' | sort -u
)

representative() {
  local fw="$1" solo="" any=""
  for d in product_configurations/*/; do
    local p line
    p="$(basename "$d")"
    # A board-specific product pins one board's wiring (its own panel, its own
    # camera) and is not chip-portable, so it can never stand in for its
    # framework generally. It is covered separately, on the chips it declares.
    grep -q '^ci_chips:' "$d/product.yml" 2>/dev/null && continue
    line="$(grep -m1 '^frameworks:' "$d/product.yml" 2>/dev/null | sed 's/frameworks: *//; s/[][]//g' | tr -d ' ')"
    [ -z "$line" ] && continue
    case ",$line," in *",$fw,"*) ;; *) continue ;; esac
    [ "$line" = "$fw" ] && { [ -z "$solo" ] && solo="$p"; }
    [ -z "$any" ] && any="$p"
  done
  echo "${solo:-$any}"
}

# Board-specific products declared for THIS chip. These are the ones a generic
# framework representative cannot cover: the Mosaico's panel, the P4's MIPI
# display, the two camera stacks.
board_products_for_chip() {
  local chip="$1"
  for d in product_configurations/*/; do
    local line
    line="$(grep -m1 '^ci_chips:' "$d/product.yml" 2>/dev/null | sed 's/ci_chips: *//; s/[][]//g' | tr -d ' ')"
    [ -z "$line" ] && continue
    case ",$line," in *",$chip,"*) basename "$d" ;; esac
  done
}

# Does the framework block claim this chip? Exit 2 means "no such framework
# block, or it declares no chips" — a different thing from "not on this chip",
# and reported differently.
supports() {
  ZC_FW="$1" ZC_CHIP="$2" python3 -c '
import os, sys, yaml
try:
    b = yaml.safe_load(open("code_blocks/frameworks/" + os.environ["ZC_FW"] + "/block.yml"))
    chips = b["chips"]
except Exception:
    sys.exit(2)
sys.exit(0 if os.environ["ZC_CHIP"] in chips else 1)
  '
}

npm run --silent build

# Both loops below build a product and record one table row; the only thing
# that differs is the label column ("$fw" vs "(board)"). One helper so the row
# format and the log path are each written once.
#
# `.ci.log`, not `.log`: build-product.sh writes its own
# _generated/_logs/<product>-<chip>.log (the compiler output); this wrapper log
# is the wider capture (generate + validate too), and CI collects both.
mkdir -p _logs
declare -a ROWS
OVERALL=0

build_row() {                       # label, product
  # Not this lane's build → skip silently; another lane's table carries it.
  mine || return 0
  local label="$1" product="$2" start
  local log="_logs/$product-$CHIP.ci.log"
  start=$(date +%s)
  local row
  if scripts/test-product.sh "$product" --build "$CHIP" >"$log" 2>&1; then
    row="$(printf '%-6s %-12s %-26s %ss' PASS "$label" "$product" "$(( $(date +%s) - start ))")"
  else
    row="$(printf '%-6s %-12s %-26s %ss  log=%s' FAIL "$label" "$product" "$(( $(date +%s) - start ))" "$log")"
    OVERALL=1
  fi
  ROWS+=("$row")
  # Say it NOW, not only in the table at the end: a job that dies mid-sweep
  # (runner out of disk, timeout) otherwise leaves an empty trace, and the
  # rows it did finish are the whole diagnosis.
  echo "  $row"
  # A built tree is 1-2 GB (build/ + managed_components/); ten of them in one
  # job is more than a hosted runner's disk. The logs are all a row needs.
  rm -rf "_generated/$product/build" "_generated/$product/managed_components"
}

for fw in "${FRAMEWORKS[@]}"; do
  product="$(representative "$fw")"
  # No chip-portable product for this framework — every one of its products
  # is board-specific (vision is the case: both camera stacks are). Those are
  # built below, per chip. Said out loud so it reads as covered, not missed.
  if [ -z "$product" ]; then
    [ "$LANE" = 1 ] && ROWS+=("$(printf '%-6s %-12s %-26s %s' NOTE "$fw" "-" "no chip-portable product; covered by board builds")")
    continue
  fi
  if ! supports "$fw" "$CHIP"; then
    rc=$?
    if [ $rc -eq 2 ]; then
      [ "$LANE" = 1 ] && ROWS+=("$(printf '%-6s %-12s %-26s %s' WARN "$fw" "$product" 'framework unknown to the registry')")
    else
      [ "$LANE" = 1 ] && ROWS+=("$(printf '%-6s %-12s %-26s %s' SKIP "$fw" "$product" "not claimed for $CHIP")")
    fi
    continue
  fi
  # The representative may compose OTHER frameworks too (voice-matter-lamp is
  # audio + matter), and a combo is buildable only where every one of them
  # is: matter pins an esp_wifi_remote that has no ESP-IDF 6.2 sources, so
  # audio + matter on the esp32p4 fails on matter, not on audio — a build the
  # platform never offers, since matter does not claim the P4. Skip, naming the
  # framework that rules the chip out; the board products cover what is left.
  other=""
  for f2 in $(grep -m1 '^frameworks:' "product_configurations/$product/product.yml" | sed 's/frameworks: *//; s/[][]//g; s/,/ /g'); do
    [ "$f2" = "$fw" ] && continue
    supports "$f2" "$CHIP" || { other="$f2"; break; }
  done
  if [ -n "$other" ]; then
    [ "$LANE" = 1 ] && ROWS+=("$(printf '%-6s %-12s %-26s %s' SKIP "$fw" "$product" "composes $other, not claimed for $CHIP")")
    continue
  fi
  build_row "$fw" "$product"
done

# Board packs live outside this repo (an esp-board-manager checkout). Without
# one a product with a board.yaml cannot resolve its board — not a failure any
# PR can fix, so report SKIP naming what is missing rather than a FAIL that
# keeps the sweep permanently red.
BOARDS_DIR="${ZC_BOARDS_DIR:-${BMGR_PATH:-}}"

# Board-specific products, on the chips they name.
for product in $(board_products_for_chip "$CHIP"); do
  if [ -f "product_configurations/$product/board.yaml" ] && [ ! -d "$BOARDS_DIR" ]; then
    [ "$LANE" = 1 ] && ROWS+=("$(printf '%-6s %-12s %-26s %s' SKIP "(board)" "$product" "no board packs; set BMGR_PATH (esp-board-manager checkout)")")
    continue
  fi
  build_row "(board)" "$product"
done

echo
df -h . 2>/dev/null | tail -1 | awk '{print "disk: " $4 " free of " $2}'
if [ "$LANES" -gt 1 ]; then
  echo "========== $CHIP — lane $LANE/$LANES of the framework sweep =========="
else
  echo "========== $CHIP — one product per framework, plus this chip's boards =========="
fi
# A lane whose share was all NOTE/SKIP has no rows; printf would emit one
# empty line under set -u with an empty array on old bash — guard it.
[ ${#ROWS[@]} -gt 0 ] && printf '%s\n' "${ROWS[@]}"
exit $OVERALL
