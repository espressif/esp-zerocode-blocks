#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

# Contributor test loop — runs on JUST this repo:
#   scripts/test-product.sh                 validate all blocks + products
#   scripts/test-product.sh <product-id>    validate one product
#   scripts/test-product.sh <id> --build [chip]   also do a real ESP-IDF build
#                                           (default esp32c3; a preview target
#                                           like esp32s31 needs an IDF that
#                                           carries it — set IDF_PATH to it)
#
# Validation needs nothing beyond node + python3. --build additionally needs
# the IDF toolchain (IDF_PATH) and, for a product with a board.yaml, an
# esp-board-manager checkout (BMGR_PATH) — see scripts/build-product.sh.
set -euo pipefail
cd "$(dirname "$0")/.."

id="${1:-}"
mode="${2:-}"
chip="${3:-esp32c3}"

npm run --silent build                 # ensure engine/dist is current
python3 scripts/check.py               # fast structural check
node scripts/test-loader.mjs           # engine read-path unit test
node scripts/validate.mjs ${id:+--product "$id"}   # semantic, via the in-repo engine

if [ -n "$id" ] && [ "$mode" = "--build" ]; then
  scripts/build-product.sh "$id" "$chip"
fi
