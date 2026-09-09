---
name: new-product-template
description: Guide a contributor through creating a new product template (product configuration) — compose existing blocks into a complete reference product, build it for real, open the PR. Use when someone wants to add a ready-made product like "smart curtain" or "CO2 monitor".
---

# Add a new product template

A product template is one file — `product_configurations/<id>/product.yml` —
that composes existing blocks into a complete, buildable product. Users pick
it as a starting point in ZeroCode Studio.

## Steps

1. **Scaffold**:
   ```bash
   scripts/new-product.sh <product-id>
   ```
   Read 2–3 similar products in `product_configurations/` first — they show
   the composition idioms.

2. **Compose** in `product.yml`:
   - `name`, `description`, `keywords` — user-facing; the studio's template
     picker searches these.
   - `frameworks: [matter]` — which framework blocks to apply
     (matter/rainmaker/zigbee/ble_mesh; `[]` = standalone).
   - `instances:` — the blocks, each with a `prefix` (UPPERCASE, unique per
     block) and `cfg:` (pins + params). Wire the param bus by giving a
     driver's published param and a device_type's consumed param the same
     `APP_DRIVER_PARAM_<NAME>` id.
   - `- include: <baseline>` — pull in the standard groups from
     `baselines/` (connectivity, console diagnostics) instead of repeating
     them.

3. **Pick pins that work everywhere**: avoid flash pads and strapping pins
   (the engine's chip/pin validator warns — heed it). Templates are
   chip-agnostic; users pick the chip.

4. **Verify with a real build**:
   ```bash
   python3 scripts/check.py
   scripts/test-product.sh <product-id> --build
   ```

5. **Open the PR.** After merge + catalog publish it appears as a template
   on the studio's New-product page and in the Library.
