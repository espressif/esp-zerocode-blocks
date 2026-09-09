#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

# Print the products a diff can actually affect — one per line.
#
#   scripts/ci-affected.sh <base-ref>
#
# WHY: a full framework x chip grid is ~90 builds, and almost every change
# touches one block used by one or two products. Building the other 88 asks a
# question the diff cannot have changed the answer to.
#
# The ESP-IDF 6 migration is the evidence for the shape of this. Of the dozen
# distinct compile failures it turned up, only FOUR were chip-specific (the
# a missing per-chip Zigbee archive, esp32-camera's target gate, MALLOC_CAP_EXEC on parts
# with no executable heap, and a voice binary overflowing an 8 MB table). The
# other eight — the mqtt/json component moves, the esp_driver_* split,
# io_loop_back, i2s_port_t, led_strip 3.x, two gpio_num_t retypings, a volatile
# increment — failed IDENTICALLY on every chip. One chip would have caught them
# all. So: per-change builds go wide on PRODUCTS and narrow on CHIPS, and the
# full grid runs on main and on a schedule, where its cost is amortised.
#
# Prints the token FULL when the change is one whose blast radius is everything.
set -uo pipefail
BASE="${1:-origin/main}"
cd "$(dirname "$0")/.."

# An unresolvable base is NOT "nothing changed". The runner fetches the PR ref
# at depth 20 and not the target branch, so a stacked PR (target = another
# feature branch) lands here with $BASE missing. Swallowing that printed
# nothing, and the caller read nothing as "no product touched" and passed the
# job GREEN without building anything. Say FULL instead: a wasted sweep is
# recoverable, a false pass is not.
if ! git rev-parse --verify --quiet "$BASE" >/dev/null; then
  echo "cannot resolve base '$BASE' — assuming everything is affected" >&2
  echo FULL
  exit 0
fi

# Three-dot diff needs a MERGE-BASE, and both sides are shallow (depth 20) —
# so the ref can resolve while the fork point is beyond the horizon. That
# failure must not read as "nothing changed" (the same false green the
# rev-parse guard above closes, through a different door): say FULL.
if ! CHANGED="$(git diff --name-only "$BASE"...HEAD 2>/dev/null)"; then
  echo "cannot diff against '$BASE' (no merge-base in shallow history) — assuming everything is affected" >&2
  echo FULL
  exit 0
fi
[ -z "$CHANGED" ] && exit 0

# Anything under these rebuilds the world: the scaffold every product is
# assembled onto, the generator that assembles it, or the CI that runs it.
#
# scripts/ is deliberately NOT swept wholesale. Only three scripts shape what is
# built — assemble_blocks.py (builds the catalog the engine reads), generate.mjs
# (drives generation) and build-product.sh (drives the compiler). The rest are
# validators and CI plumbing (check.py, validate.mjs, test-loader.mjs, the
# ci-*.sh files, this one): they can fail a pipeline but cannot change a byte
# of generated firmware, so a full framework sweep proves nothing about them.
if echo "$CHANGED" | grep -qE '^(base_firmware/|engine/|scripts/(assemble_blocks\.py|generate\.mjs|build-product\.sh)|Dockerfile\.ci|\.gitlab-ci\.yml|\.github/workflows/|chips\.txt|pins\.env|package(-lock)?\.json)'; then
  echo FULL
  exit 0
fi

# A plain list, not `declare -A`: associative arrays are bash 4+, macOS ships
# bash 3.2, and under `set -u` the failure surfaced as an unbound-variable
# error that still exited 0 with no output — a contributor running this
# locally was told "nothing affected" when it had not run at all.
PICKED=""

# ONE product per changed block, not every product that uses it.
#
# drivers/status_led is in 65 products. Building all 65 compiles the same block
# 65 times in near-identical surroundings — the 2nd through 65th cannot fail
# where the 1st passed, because the block's slots land in the same component
# either way. What varies between products is composition, and composition is
# what `validate` checks WITHOUT a compiler.
#
# So: pick one representative product per changed block. Prefer a chip-portable
# one (no ci_chips) and, among those, the one with the FEWEST instances — the
# smallest product that still contains the block is the fastest way to compile
# it, and a leaner tree makes a failure easier to read.
pick_for_block() {
  local blk="$1" best="" best_n=99999
  local baselines="" fw=""
  # A framework block is never named by `- block:` — a product opts into it via
  # its `frameworks:` list. Matching only on block references would silently
  # find nothing for the twelve most important blocks in the repo.
  case "$blk" in frameworks/*) fw="${blk#frameworks/}" ;; esac
  for b in baselines/*.yml; do
    grep -qE "block: *$blk( |$)" "$b" 2>/dev/null && baselines="$baselines $(basename "$b" .yml)"
  done
  for d in product_configurations/*/; do
    local p n hit=0
    p="$(basename "$d")"
    grep -q '^ci_chips:' "$d/product.yml" 2>/dev/null && continue
    if [ -n "$fw" ]; then
      local line
      line="$(grep -m1 '^frameworks:' "$d/product.yml" 2>/dev/null | sed 's/frameworks: *//; s/[][]//g' | tr -d ' ')"
      case ",$line," in *",$fw,"*) hit=1 ;; esac
    fi
    [ $hit -eq 0 ] && grep -qE "block: *$blk( |$)" "$d/product.yml" 2>/dev/null && hit=1
    if [ $hit -eq 0 ]; then
      for bl in $baselines; do
        grep -qE "include: *$bl( |$)" "$d/product.yml" 2>/dev/null && { hit=1; break; }
      done
    fi
    [ $hit -eq 0 ] && continue
    n="$(grep -cE '^\s+- (block|include):' "$d/product.yml")"
    if [ "$n" -lt "$best_n" ]; then best_n="$n"; best="$p"; fi
  done
  # Nothing chip-portable uses it — the block exists only on a specific board
  # (the ES8311 codec is the case: it is the Mosaico's audio and nowhere else).
  # Fall back to the board product, which the caller builds on the chips it
  # declares. Returning nothing here would quietly leave the block untested.
  if [ -z "$best" ]; then
    for d in product_configurations/*/; do
      grep -q '^ci_chips:' "$d/product.yml" 2>/dev/null || continue
      if grep -qE "block: *$blk( |$)" "$d/product.yml" 2>/dev/null; then
        best="$(basename "$d")"; break
      fi
    done
  fi
  [ -n "$best" ] && echo "$best"
}

# A changed product builds itself — its composition is what changed.
while read -r d; do
  [ -n "$d" ] && [ -d "product_configurations/$d" ] && PICKED="$PICKED $d"
done < <(echo "$CHANGED" | sed -n 's|^product_configurations/\([^/]*\)/.*|\1|p' | sort -u)

while read -r blk; do
  [ -z "$blk" ] && continue
  rep="$(pick_for_block "$blk")"
  [ -n "$rep" ] && PICKED="$PICKED $rep"
done < <(echo "$CHANGED" | sed -n 's|^code_blocks/\(.*\)/[^/]*$|\1|p' | sed 's|/slots$||' | sort -u)

# A changed baseline: one product that includes it, same reasoning.
while read -r bl; do
  [ -z "$bl" ] && continue
  for d in product_configurations/*/; do
    if grep -qE "include: *$bl( |$)" "$d/product.yml" 2>/dev/null; then
      grep -q '^ci_chips:' "$d/product.yml" 2>/dev/null || { PICKED="$PICKED $(basename "$d")"; break; }
    fi
  done
done < <(echo "$CHANGED" | sed -n 's|^baselines/\(.*\)\.yml$|\1|p' | sort -u)

for p in $PICKED; do echo "$p"; done | sort -u
