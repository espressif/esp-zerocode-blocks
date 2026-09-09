## What this adds or changes

<!-- A block, a product, a framework, an engine change — one per PR. -->

## How it was verified

- [ ] `python3 scripts/check.py` passes
- [ ] `scripts/test-product.sh <product>` passes
- [ ] `scripts/test-product.sh <product> --build <chip>` built — chip: `…`
- [ ] Tested on hardware (say which board) — or: emulator only / not tested

## Contract checklist (blocks)

- [ ] Actuators are born safe (idle level asserted before any bus value)
- [ ] `requires_bus:` / `provides_bus:` declared for shared I2C/SPI
- [ ] `bmgr:` declared for pins a board should see
- [ ] Third-party code keeps its own licence header
