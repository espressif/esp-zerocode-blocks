#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

# Build ONE product for one or more chips, from this working tree.
#
#   IDF_PATH=/path/to/esp-idf scripts/build-product.sh <product-id> [chip ...]
#
#   1. generates the product into _generated/<product-id> (scripts/generate.mjs)
#   2. per chip: fullclean → `--preview set-target` → build
#   3. prints a summary; exits non-zero on any failure
#
# Toolchain, not repo content — all three are host paths on the same footing:
#   IDF_PATH        ESP-IDF checkout (required)
#   BMGR_PATH       esp-board-manager checkout — needed only by a product that
#                   carries a board.yaml: it supplies the `idf.py bmgr` action
#                   and, unless ZC_BOARDS_DIR says otherwise, the board packs
#   ZC_BOARDS_DIR   a directory of board packs (<pack>/<board>/board_info.yaml);
#                   defaults to the esp-board-manager checkout itself, whose
#                   esp_boards/ m5stack_boards/ esp_friends_boards/ are packs
set -u
set -o pipefail
cd "$(dirname "$0")/.."
ROOT="$(pwd)"

[[ -n "${IDF_PATH:-}" && -f "${IDF_PATH}/export.sh" ]] || { echo "ERROR: IDF_PATH must point at an ESP-IDF checkout" >&2; exit 2; }
[[ $# -ge 1 ]] || { echo "usage: $0 <product-id> [chip ...]" >&2; exit 2; }

PRODUCT="$1"; shift
if [[ $# -gt 0 ]]; then CHIPS=("$@"); else CHIPS=("$(grep -v '^#' chips.txt | head -1)"); fi
for CHIP in "${CHIPS[@]}"; do
  grep -qx "$CHIP" chips.txt || { echo "ERROR: '$CHIP' is not in chips.txt" >&2; exit 2; }
done

GEN_DIR="${ROOT}/_generated/${PRODUCT}"
LOG_DIR="${ROOT}/_generated/_logs"
mkdir -p "${LOG_DIR}"

npm run --silent build || exit 1

echo "=================================================================="
echo "Generating ${PRODUCT} (${CHIPS[0]})"
echo "=================================================================="
# The tree is single-chip (only the target chip's sdkconfig is merged), so it
# is regenerated when the chip changes. A board product has one chip and never
# does. Generation also writes .zc-board when the product has a board.yaml.
node scripts/generate.mjs "${PRODUCT}" --chip "${CHIPS[0]}" --out "${GEN_DIR}" || { echo "ERROR: generation failed" >&2; exit 1; }
LAST_GEN_CHIP="${CHIPS[0]}"

# shellcheck disable=SC1091
source "${IDF_PATH}/export.sh" >/dev/null 2>&1 || { echo "ERROR: failed to source ${IDF_PATH}/export.sh" >&2; exit 2; }
echo "IDF: $(idf.py --version 2>&1 | head -1)"

# ── esp-board-manager (`board.yaml` products) ────────────────────────────
# The generator writes .zc-board when the product was generated WITH a board.
# The marker's presence is the whole switch: no marker, nothing below runs.
BOARD=""
BMGR_AMEND=()
[[ -f "${GEN_DIR}/.zc-board" ]] && BOARD="$(head -1 "${GEN_DIR}/.zc-board" | tr -d '[:space:]')"
if [[ -n "${BOARD}" ]]; then
  BMGR="${BMGR_PATH:-}"
  [[ -d "${BMGR}/esp_board_manager" ]] || { echo "ERROR: '${PRODUCT}' runs on board '${BOARD}', which needs an esp-board-manager checkout (set BMGR_PATH)" >&2; exit 2; }
  BOARDS_DIR="${ZC_BOARDS_DIR:-$BMGR}"
  BOARD_PACK=""
  for _p in "${BOARDS_DIR}"/*/; do
    [[ -f "${_p}${BOARD}/board_info.yaml" ]] && BOARD_PACK="${_p%/}"
  done
  [[ -n "${BOARD_PACK}" ]] || { echo "ERROR: board '${BOARD}' not found under ${BOARDS_DIR}/*/ (set ZC_BOARDS_DIR)" >&2; exit 2; }
  # `idf.py bmgr` is an idf_ext.py action that ships INSIDE the component, and
  # the component only lands in managed_components/ after a reconfigure — which
  # is after we need it. So the ACTION comes from the checkout while the
  # component the firmware links against still comes from the registry pin.
  export IDF_EXTRA_ACTIONS_PATH="${BMGR}/esp_board_manager"
  echo "Board: ${BOARD}  (pack ${BOARD_PACK})"
  # The product's externally wired parts, as a patch on the board. Absolute on
  # purpose: bmgr resolves a bare name against the board's own directory first.
  if [[ -f "${GEN_DIR}/board_amend/board_amend.yaml" ]]; then
    BMGR_AMEND=(-a "${GEN_DIR}/board_amend")
    echo "Amend: ${GEN_DIR}/board_amend"
  fi
  BOARD_CHIP="$(awk '/^chip:/{print $2}' "${BOARD_PACK}/${BOARD}/board_info.yaml")"
  for CHIP in "${CHIPS[@]}"; do
    [[ "${CHIP}" == "${BOARD_CHIP}" ]] || { echo "ERROR: '${PRODUCT}' is a board product (${BOARD}); its chip is ${BOARD_CHIP}, not ${CHIP}" >&2; exit 2; }
  done
fi

declare -a RESULTS
OVERALL_RC=0
for CHIP in "${CHIPS[@]}"; do
  echo
  echo "=================================================================="
  echo "Building ${PRODUCT} for ${CHIP}"
  echo "=================================================================="
  if [[ "${CHIP}" != "${LAST_GEN_CHIP}" ]]; then
    node scripts/generate.mjs "${PRODUCT}" --chip "${CHIP}" --out "${GEN_DIR}" || {
      RESULTS+=("FAIL  ${CHIP}  (generation)"); OVERALL_RC=1; continue
    }
    LAST_GEN_CHIP="${CHIP}"
  fi
  LOG="${LOG_DIR}/${PRODUCT}-${CHIP}.log"
  START=$(date +%s)
  (
    cd "${GEN_DIR}"
    rm -rf build
    if [[ -n "${BOARD}" ]]; then
      # bmgr FIRST, and no set-target: the action appends the board's
      # board_manager.defaults (CONFIG_IDF_TARGET + every device symbol) to
      # SDKCONFIG_DEFAULTS only while `sdkconfig` does not exist yet, and
      # set-target would create it. The target comes from the board.
      rm -f sdkconfig sdkconfig.old
      # ${a[@]+"${a[@]}"}: bash 3.2 (macOS) trips `set -u` on an empty array.
      idf.py bmgr -b "${BOARD}" -c "${BOARD_PACK}" ${BMGR_AMEND[@]+"${BMGR_AMEND[@]}"} 2>&1
      idf.py build 2>&1
    else
      # --preview unconditionally: ESP32-S31 is an ESP-IDF PREVIEW target and
      # set-target refuses it without the flag; a no-op on a supported target.
      idf.py --preview set-target "${CHIP}" 2>&1
      idf.py build 2>&1
    fi
  ) > "${LOG}" 2>&1
  RC=$?
  DURATION=$(( $(date +%s) - START ))
  if [[ ${RC} -eq 0 ]]; then
    echo "  ✓ ${CHIP} build succeeded (${DURATION}s)  log=${LOG#${ROOT}/}"
    RESULTS+=("PASS  ${CHIP}  ${DURATION}s")
  else
    echo "  ✗ ${CHIP} build FAILED (${DURATION}s)  log=${LOG#${ROOT}/}"
    tail -n 40 "${LOG}" | sed 's/^/    /'
    RESULTS+=("FAIL  ${CHIP}  ${DURATION}s")
    OVERALL_RC=1
  fi
done

echo
echo "Summary: ${PRODUCT}"
for R in "${RESULTS[@]}"; do echo "  ${R}"; done
exit ${OVERALL_RC}
