---
name: new-block
description: Guide a contributor through creating a new code block (driver, behavior, or device type) — scaffold, author the C + block.yml contract, verify locally, open the PR. Use when someone wants to add support for a sensor, actuator, peripheral, runtime behavior, or device type.
---

# Add a new code block

A block is a directory: `block.yml` (the contract) + `slots/*.c|.h` (the real
C) + optional sidecars. The generator composes blocks into firmware; blocks
never call each other — they meet on the param bus.

> **Matter device_type blocks:** the `matter_*` slots are generator-produced,
> not hand-written. Generate them with the `esp-code-blocks` MCP server
> (spec-checked for any device type × feature combo), hand-edit only what it
> can't know, and resolve every `WARNING:` (unclaimed choose-one features abort
> at boot). See CLAUDE.md § "When you need a new block". Drivers and behaviors
> are hand-authored as before.

## Steps

1. **Scaffold** from the commented skeleton (it teaches the contract):
   ```bash
   scripts/new-block.sh drivers/<name>     # or behaviors/<name>, device_types/<name>
   ```
   Before writing anything, find the closest existing block and read it —
   ~150 blocks under `code_blocks/` are working examples (`bh1750` for I2C
   sensors, `relay` for GPIO actuators, `auto_off_timer` for behaviors).

2. **Author the contract** in `block.yml`:
   - `description:` — one line; the AI uses it to pick blocks, make it count.
   - `params:` — ordinary config knobs (gpio, thresholds), each with
     `type`, `description`, and `required: true` or a `default`.
   - `driver_params:` — the block's param-bus connections: what runtime
     values it publishes (drivers) or consumes (device_types/behaviors),
     each with a bus `type` (bool/u8/i16/u16/u32).

3. **Write the C** in `slots/<slot>.c|.h` — one file per slot the block
   fills. `SLOTS.md` lists every slot and where it renders. Use
   `{{prefix}}` / `{{cfg.<param>}}` placeholders. Build needs go in
   sidecars: `idf_component.yml` (managed deps), `sdkconfig.defaults`,
   `requires.cmake` (extra PRIV_REQUIRES).

4. **Review for the classic pitfalls** before verifying:
   - No cross-translation-unit statics — each slot file may land in a
     different TU; share state via the param bus, not `static` globals.
   - Components referenced from slot code need `requires.cmake`
     (missing PRIV_REQUIRES compiles locally but breaks products).
   - `set_param` writes should be change-detected if repeated commands are
     possible (avoid re-stamping timers).

5. **Verify locally** — both gates, in order:
   ```bash
   python3 scripts/check.py               # instant structural check
   scripts/test-product.sh                # semantic validation via the in-repo engine
   ```
   To prove it compiles, reference the block from a product and run:
   ```bash
   scripts/test-product.sh <product-id> --build   # real ESP-IDF build
   ```

6. **Open the PR.** The directory is the submission. After merge, a
   maintainer publishes the catalog (`scripts/publish-catalog.sh`) and the
   block is live in the next pipeline run — no platform deploy.
