# Contributing to esp-zerocode-blocks

Thanks for adding to the catalog. A contribution here is a **directory**: a
block under `code_blocks/`, a product under `product_configurations/`, or a
framework — and the folder you add *is* the submission. `CLAUDE.md` is the
contract every block follows; `SLOTS.md` lists every slot; `INVENTORY.md` is
what already exists (read it first, the closest neighbour is your template).

## The loop

```bash
scripts/new-block.sh drivers/my_sensor     # or behaviors/…, device_types/…
scripts/new-product.sh my-widget           # a ready-made product
python3 scripts/check.py                   # structural, instant, pyyaml only
scripts/test-product.sh <product-id>       # semantic validation via the engine
scripts/test-product.sh <id> --build [chip] # a real ESP-IDF build (the review gate)
```

`--build` needs ESP-IDF on the machine (`IDF_PATH`) and, for products that
carry a `board.yaml`, a board-pack checkout (`ZC_BOARDS_DIR`, an
esp-board-manager `boards/` directory). Everything else runs on this repo
alone.

## What a pull request needs

- **It builds.** Say which chip you built on. CI builds one product per
  changed block on the canary chip; the full chip grid runs on `main`.
- **It is born safe.** Actuators idle at their safe level before any bus
  value arrives; see "Actuators must be born safe" in `CLAUDE.md`.
- **It declares what it touches.** `requires_bus:` / `provides_bus:` for
  shared I2C/SPI, `bmgr:` for pins a board should know about. All three fail
  silently on hardware when missing, and none is a compiler error.
- **A product never names a board.** The catalog says what a product does;
  a separate `board.yaml` says what one user's product runs on.
- **Third-party code carries its licence.** Vendored sources keep their own
  header; everything new is Apache-2.0 with an SPDX header.

Keep one change per pull request: a new driver and a fix to an unrelated
behaviour are two reviews, not one.

## Licence

By contributing you agree that your contribution is licensed under the
Apache License 2.0 (see `LICENSE`).

## Commit messages

Imperative mood, one change per commit, and **no AI-assistant attribution**:
no "Claude", no `Co-Authored-By` trailer naming an assistant, no session
links. `npm ci` installs a `commit-msg` hook that refuses such a message, and
CI checks every commit in a PR the same way.

## Using an AI assistant

`AGENTS.md` and `.claude/skills/` carry guided flows for adding a block, a
product or a framework. They read the same contract you would, so a
generated contribution is held to the same review.
