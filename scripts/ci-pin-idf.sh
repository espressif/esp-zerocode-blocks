#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

# Put an ESP-IDF checkout on the commit pins.env names, and install the tools
# for one chip — so a build in the public espressif/idf image runs on exactly
# the IDF the ZeroCode AI backend runs on, not whatever master the image was
# built from.
#
#   IDF_PATH=/opt/esp/idf scripts/ci-pin-idf.sh <chip>
#
# Also clones esp-board-manager at its pin into BMGR_PATH when that is set and
# empty. Idempotent: a checkout already on the pin is left alone.
set -euo pipefail
cd "$(dirname "$0")/.."
CHIP="${1:?usage: ci-pin-idf.sh <chip>}"
: "${IDF_PATH:?IDF_PATH must point at an ESP-IDF checkout}"
# shellcheck disable=SC1091
. ./pins.env

if [ "$(git -C "$IDF_PATH" rev-parse HEAD)" != "$IDF_REF" ]; then
  echo "idf: $(git -C "$IDF_PATH" rev-parse --short HEAD) → $IDF_REF"
  # A commit cannot be `git fetch --branch`; fetch it by sha (GitHub serves
  # reachable commits) and check out detached. Submodules follow the commit.
  git -C "$IDF_PATH" fetch -q --depth 1 origin "$IDF_REF"
  git -C "$IDF_PATH" checkout -q FETCH_HEAD
  git -C "$IDF_PATH" submodule update -q --init --depth 1 --recursive --jobs 4
else
  echo "idf: already on $IDF_REF"
fi
# Tool versions the pinned commit wants that the image lacks are downloaded;
# the rest are already in IDF_TOOLS_PATH. The python env is per IDF version.
"$IDF_PATH/install.sh" "$CHIP" >/dev/null
echo "idf: tools installed for $CHIP ($("$IDF_PATH"/tools/idf.py --version 2>/dev/null | tail -1 || true))"

if [ -n "${BMGR_PATH:-}" ] && [ ! -d "$BMGR_PATH/esp_board_manager" ]; then
  git clone -q https://github.com/espressif/esp-board-manager.git "$BMGR_PATH"
  git -C "$BMGR_PATH" checkout -q "$BMGR_REF"
  echo "esp-board-manager: $BMGR_REF → $BMGR_PATH"
fi
