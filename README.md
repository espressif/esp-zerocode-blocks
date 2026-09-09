# esp-zerocode-blocks

The **canonical home of ZeroCode AI's building blocks**: every code
block (drivers, device types, behaviors, frameworks, partition tables,
sdkconfig fragments) and every product configuration the ZeroCode AI
pipeline composes firmware from.

The folder structure *is* the documentation. This repo is the whole
firmware template **system** — the buildable base, the blocks that fill
it, the products that compose blocks, and the shared baselines — so it
can assemble a complete firmware tree on its own.

```
base_firmware/                            the buildable IDF scaffold blocks
                                          plug into — its components carry the
                                          slot anchors (platform-team owned)
code_blocks/
  drivers/<id>/                       hardware peripherals (bh1750, relay, dht22, …)
    block.yml                           description + params (knobs) +
                                        driver_params (param-bus connections);
                                        id/kind come from the path
    slots/<slot>.c|.h                   THE CODE — real C files, one per slot
    requires.cmake                      (optional) extra PRIV_REQUIRES (real CMake syntax)
    sdkconfig.defaults                  (optional) Kconfig the block needs
    idf_component.yml                   (optional) managed component deps
  device_types/<id>/                  device types with per-framework bindings
  behaviors/<id>/                     runtime patterns (NVS persist, OTA, reset, …)
  frameworks/<id>/                    matter, rainmaker, zigbee, ble_mesh, …
  partition-tables/<id>/              2mb / 4mb / 8mb
  sdkconfig-fragments/<id>/           production, secure_boot, coredump, …
product_configurations/<id>/product.yml   composes code_blocks → a product;
                                          shared groups come from baselines/
                                          via `- include: <name>`
baselines/<name>.yml                      the standard instance groups every
                                          product ships (connectivity,
                                          console + core diagnostics)
CLAUDE.md                             the block contract + authoring guide
INVENTORY.md                          catalog of everything here
```

**The param bus in one picture** — the only concept a block author must
learn. Blocks never call each other; they meet on a shared bus of named
runtime values:

```
drivers ──publish──▶  param bus (APP_DRIVER_PARAM_*)  ──consume──▶ behaviors, device_types
                      the product.yml picks the names
```

`params:` are ordinary config knobs. `driver_params:` are the block's
bus connections — each entry names a driver-param and says what it
carries (`type: u16`). Example: the `bh1750` driver publishes lux on
whatever name the product wires into its `value_param`; a light
device_type consumes `power_param` the same way. In `product.yml`,
both kinds are set through the instance's single `cfg:` map — the block
declares them separately, the product fills them in together.

**Authoring vs consumption format:** `id:`/`kind:` are derived from the
path; slot code lives in `slots/` (see `SLOTS.md` for every slot and
where it renders; `{{prefix}}`/`{{cfg.*}}` placeholders as before);
build needs live in sidecar files (`requires.cmake`,
`sdkconfig.defaults`, `idf_component.yml`).
`scripts/assemble_blocks.py` expands everything back into the
single-file format the ZeroCode generator consumes — the sync script
does this automatically, so the platform never changes.

**Start a new block:** `scripts/new-block.sh drivers/my_sensor` — a
commented skeleton that teaches the contract.

## Verify — on just this repo

```bash
python3 scripts/check.py       # offline structural check (pyyaml only) — no
                               # engine, no network. Run this first.
scripts/test-product.sh <id>   # full semantic validation via the in-repo engine
scripts/test-product.sh <id> --build [chip]   # + a real ESP-IDF build (IDF_PATH)
```

`check.py` catches the common mistakes (bad yaml, unknown block reference,
unresolved `include:`, missing type) instantly and standalone. Deep
semantic + build validation still needs the engine — see "self-verification"
below.

## Contribute in three steps

1. Scaffold: `scripts/new-block.sh drivers/my_sensor` or
   `scripts/new-product.sh my-widget` (or copy the closest neighbor).
   `CLAUDE.md` is the contract; `SLOTS.md` lists every slot.
2. Test it yourself: `scripts/test-product.sh <product-id>` — assembles
   and runs the semantic validator. Add `--build` for a real ESP-IDF build
   (the review gate); that needs the IDF toolchain (`IDF_PATH`) and, for a
   product with a `board.yaml`, an esp-board-manager checkout (`BMGR_PATH`).
3. Open a pull request. The folder you added *is* the submission.

## Repo lifecycle

This is the public home of the catalog. PR CI runs the validator on every
push and a real ESP-IDF build of the products a change touches; a merged
block or product is published to the catalog and listed in the ZeroCode AI
Library with attribution.

## Relationship to the platform

- **This repo is canonical.** The platform vendors nothing: it fetches the
  published catalog at runtime, and its generator IS `engine/` (a pinned git
  dependency on this repo). Edit here, then `scripts/publish-catalog.sh`.
- **ZeroCode AI's Library** lists blocks/templates (from the published
  library.json) and links back here as their home.
- `_generated/` trees are build output and never belong in this repo.

## Release (maintainers)

```bash
CATALOG_BUCKET=<bucket> scripts/publish-catalog.sh   # AWS credentials for the deployment
```

Bundles the assembled blocks + products + base_firmware into a
content-hash-versioned tarball on the CDN. The platform fetches it
at runtime (pinned by version) — a merged change reaches the cloud
with no image rebuild and no mirror sync.

## Self-contained

Everything here runs on this repo alone: the structural check, the engine's
validator and generator (`engine/`), and the build executor
(`scripts/build-product.sh`). CI needs only an ESP-IDF image plus an
esp-board-manager checkout for the board products, both checked out to the
commits in `pins.env` — the same ones the ZeroCode AI backend builds with.
GitHub Actions (`.github/workflows/ci.yml`) does that inside the public
`espressif/idf` image at job start (`scripts/ci-pin-idf.sh`); GitLab
(`.gitlab-ci.yml`) bakes it into a worker image (`Dockerfile.ci`). `chips.txt`
is the one list of chips a product builds for, and each framework's
`block.yml` says which of them it has been built on.
