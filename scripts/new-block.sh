#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

# Scaffold a new block with a commented skeleton:
#   scripts/new-block.sh drivers/my_sensor
# The skeleton teaches the contract; delete the comments once you don't
# need them. See SLOTS.md for every slot name and where it renders.
set -euo pipefail
cd "$(dirname "$0")/.."

path="${1:?usage: scripts/new-block.sh <kind-folder>/<block_id>   e.g. drivers/my_sensor}"
dir="code_blocks/$path"
[ -e "$dir" ] && { echo "already exists: $dir" >&2; exit 1; }
case "$path" in
  drivers/*|behaviors/*|device_types/*|peripherals/*) ;;
  *) echo "scaffold supports drivers/, behaviors/, device_types/, peripherals/ (copy a neighbor for other kinds)" >&2; exit 1 ;;
esac

mkdir -p "$dir/slots"

cat > "$dir/block.yml" <<'YML'
# No id/kind here — both come from this folder's path.
description: One line saying what this block does and when to pick it. Shown to the AI and in search.

# Config knobs the product manifest can set. {{cfg.<name>}} in slot code
# becomes the configured value.
params:
  poll_interval_ms: { type: int, default: 1000 }

# Connections to the param bus — each entry NAMES a driver-param (the
# shared runtime key-value bus; type = what it carries). Drivers publish
# driver-params; behaviors/device_types consume them. The product
# manifest picks the actual names (e.g. APP_DRIVER_PARAM_LUX).
driver_params:
  value_param:
    type: u16
    required: true
    description: Where this block publishes its reading.

# Sidecar files (create only if needed — see any block that has them):
#   requires.cmake       extra PRIV_REQUIRES, e.g. list(APPEND app_logic_PRIV_REQUIRES nvs_flash)
#   sdkconfig.defaults   Kconfig this block needs
#   idf_component.yml    managed component dependencies
YML

cat > "$dir/slots/config_defines.h" <<'H'
/* Rendered into the generated config header. {{prefix}} is this block's
 * instance name (SO two instances never collide) — always prefix your
 * defines with it. */
#define {{prefix}}_POLL_MS  {{cfg.poll_interval_ms}}
H

cat > "$dir/slots/logic_includes.h" <<'H'
#include <esp_timer.h>
H

cat > "$dir/slots/logic_statics.c" <<'C'
/* File-scope statics + helpers. Prefix statics with {{prefix_lc}} —
 * blocks share a translation unit, unprefixed statics collide. */
static void {{prefix_lc}}_poll_cb(void *arg)
{
    /* read your hardware, then publish: */
    app_driver_param_val_t v = { .u16 = 0 };
    app_driver_set_param({{cfg.value_param}}, v, APP_DRIVER_SOURCE_LOCAL);
}
C

cat > "$dir/slots/logic_init.c" <<'C'
/* Runs once at startup, inside a { } scope of the shared init function —
 * locals are fine, but anything that must outlive init goes in statics. */
{
    const esp_timer_create_args_t {{prefix_lc}}_args = {
        .callback = &{{prefix_lc}}_poll_cb,
        .name = "{{prefix_lc}}",
    };
    esp_timer_handle_t {{prefix_lc}}_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&{{prefix_lc}}_args, &{{prefix_lc}}_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic({{prefix_lc}}_timer, (uint64_t){{prefix}}_POLL_MS * 1000ULL));
}
C

echo "scaffolded $dir"
echo "next: edit the files, then validate:"
echo "  scripts/test-product.sh <a-product-using-it>   (add --build for a real IDF build)"
