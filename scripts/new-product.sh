#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

# Scaffold a new product: scripts/new-product.sh my-widget "My Widget"
set -euo pipefail
cd "$(dirname "$0")/.."

id="${1:?usage: scripts/new-product.sh <product-id> [name]}"
name="${2:-$id}"
dir="product_configurations/$id"
[ -e "$dir" ] && { echo "already exists: $dir" >&2; exit 1; }
mkdir -p "$dir"

cat > "$dir/product.yml" <<YML
id: $id
name: $name
description: One line describing the product.
keywords: [$id]
frameworks: [matter]

instances:
  # ── What makes THIS product itself — its hardware + device type ──
  - block: drivers/relay
    prefix: MAIN
    cfg:
      gpio: 7
      active_level: 1
      param_id: APP_DRIVER_PARAM_POWER

  - block: device_types/light
    prefix: MAIN
    cfg:
      power_param: APP_DRIVER_PARAM_POWER

  # ── Standard baselines (see baselines/*.yml for what's inside) ──
  - include: baseline-connectivity

  - include: console-diagnostics

  - include: diagnostics-core

product_metadata:
  vendor_name: ZeroCode AI
  product_name: $name
YML

echo "scaffolded $dir"
echo "next: scripts/test-product.sh $id        (add --build for a real IDF build)"
