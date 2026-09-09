# Firmware Templates — Block-Based Architecture

> **This repo is the canonical home** of ZeroCode AI's blocks + product
> configurations. The platform never vendors them: `scripts/publish-catalog.sh`
> publishes a versioned catalog that every deployment fetches at runtime, so
> a merged change reaches the product without a platform release.
> `_generated/` (build output) never belongs here.
>
> **This repo = the whole firmware template system:** `base_firmware/`
> (the buildable scaffold, platform-team owned), `code_blocks/` (fill
> its slots), `product_configurations/` (compose blocks),
> `baselines/` (shared instance groups). It assembles a complete tree
> on its own.
>
> **Authoring format (in esp-zerocode-blocks):** `block.yml` carries
> description + `params:` (knobs) + `driver_params:` (param-bus
> connections; replaces `param_refs`) — no `id:`/`kind:` (derived from
> path). Slot code lives in `slots/<name>.c|.h`; build needs in sidecar
> files (`requires.cmake`, `sdkconfig.defaults`, `idf_component.yml`).
> Products may pull shared instance groups from `baselines/` via
> `- include: <name>` items. `scripts/assemble_blocks.py` expands
> blocks and products to the single-file format documented below.

This directory is the block system. Every concrete contribution to a
buildable firmware tree — hardware drivers, device types, runtime
behaviors, frameworks, partition tables, sdkconfig modes — is a block.
A product is a YAML manifest that composes blocks. The generator
stitches them into `_generated/<product>/`.

## Layout

```
templates/
  code_blocks/                         # reusable building blocks
    drivers/<id>/block.yml             # hardware peripherals
    device_types/<id>/block.yml        # device types (per-framework bindings in slots)
    behaviors/<id>/block.yml           # runtime patterns (NVS persist,
                                       #   OTA, factory reset, LED
                                       #   patterns, console cmds, …)
    frameworks/<id>/                   # matter, rainmaker, zigbee, ble_mesh, …
      block.yml
      components/app_<id>/             # per-framework component sources
    partition-tables/<id>/             # 2mb / 4mb / 8mb
      block.yml
      partitions.csv
    sdkconfig-fragments/<id>/          # production, secure_boot,
      block.yml                        #   coredump, power_management,
                                       #   no_app_logs, wifi_min_memory, …
  product_configurations/<id>/product.yml  # composes code_blocks → a product
  _generated/<id>/                     # generator output (gitignored)
  INVENTORY.md                         # catalog of blocks + products
```

## Block kinds

| kind | What it contributes | Where it lands |
|---|---|---|
| `driver` | Hardware init, apply_param() cases, params | `components/app_driver/` |
| `device_type` | Per-framework device bindings (today: Matter endpoint creation, attribute/driver callbacks) | `components/app_matter/` |
| `behavior` | Runtime tasks, NVS, button handlers, OTA, LED patterns, console cmds | `components/app_logic/` (or wherever its slots target) |
| `framework` | Framework component source + main_init/main_includes slots (connectivity stacks, media pipelines, UI stacks) | `components/app_<framework>/` + `main/app_main.cpp` |
| `partition_table` | `partitions.csv` file | `partitions.csv` |
| `sdkconfig_fragment` | A `sdkconfig:` map of `CONFIG_X=value` flags | appended to `sdkconfig.defaults` |
| `peripheral` / `binding` | (reserved for future use) | n/a |

## How a product is composed

A `product.yml` lists block **instances**. Each entry has:

```yaml
- block: drivers/relay        # which block
  prefix: APP                 # uppercased → #define prefix, lowercased → C-symbol prefix
  cfg:                        # block-defined params
    gpio: 7
    active_level: 1
    param_id: APP_DRIVER_PARAM_POWER
```

Multiple instances of the same block can coexist with different
prefixes — that's how `wall-switch-3gang` has three relays + three
buttons + three device-type instances.

Other product.yml fields:
- `frameworks: [matter]` — selects which framework blocks to apply
- `partition_table: 4mb` — picks the partition-table block
- `sdkconfig_fragments: [coredump]` — picks sdkconfig-fragment blocks
- `extra_sdkconfig: { CONFIG_X: y }` — final per-product overrides
- `ci_chips: [esp32c6]` — chips CI builds this product on (absent = CI picks)

**A product.yml NEVER names a board.** The catalog says what a product DOES;
what hardware it runs on is a property of a user's product, and lives in a
separate `board.yaml` — see **Boards** below.

**Instance ORDER is init order.** `app_driver_init()` calls each instance's
`driver_init` in `instances:` order, so anything shared — a bus, a bank of
pins — must be listed above the blocks that use it. See **Shared buses**.

## Driver param IDs

Drivers and device-type blocks communicate through `app_driver_param_id_t` enum
values + the `app_driver_param_val_t` union. Each block declares its
`param_refs` (which `cfg` keys are param-id references and what value
type the param carries — `bool` / `u8` / `i16` / `u16` / `u32`).

The generator collects every param ID referenced by any block in the
product and emits `app_driver_types.h` with a per-product enum. A
smart-plug only has `APP_DRIVER_PARAM_POWER`; a `ceiling-fan-light`
has `APP_DRIVER_PARAM_LIGHT_POWER` and `APP_DRIVER_PARAM_FAN_SPEED`.

Blocks that only **consume** a param (like `console_param_get` or
`nvs_persist_*`) intentionally omit `param_refs` — the producer
establishes the type.

### State vs events — pick the right call

The bus has two write calls, and choosing wrong is the most common way a
generated product ends up broken:

- **`app_driver_set_param(id, val, source)` — STATE.** Change-detected: writing
  the value the param already holds is a no-op, no hardware re-apply, no
  notifications. That is deliberate — a repeated "unlock" must not re-stamp a
  deadbolt's auto-lock countdown, and a controller re-asserting "on" must not
  re-arm a heater's cutoff.
- **`app_driver_fire_event(id, val, source)` — EVENTS.** Always applies, always
  notifies, even with an identical value.

Rule of thumb: **if two identical calls in a row should produce two effects, it
is an event.** A doorbell press, a button tap, an alarm ringing, a timer
expiring, a motion re-trigger, a media key, a dispensed treat — all events.

Routing an event through `set_param` fails silently and completely: the second
and every later occurrence disappears. Products have shipped a chime that rang
once per boot, a manual override the light ignored because motion had already
set the same value, a preset that did nothing when re-tapped, and a media button
that sent nothing on the second press. A deduped write now logs at DEBUG
(`set_param(N) unchanged — not propagated`) so the swallow is at least visible
in a device log.

### Actuators must be born safe

`gpio_config()` and `ledc_channel_config()` drive the pin the moment they are
called, and both start from a register reset value of 0. An actuator's **init
code is its boot behaviour**, and no later code can undo it:

- Write the idle level BEFORE switching a GPIO to output (`drivers/relay`,
  `relay_with_button`, `relay_latching` all do this now). Configuring first and
  deasserting after energizes an active-LOW load for the gap between — on a
  garage-door opener that is a pulse to the motor on every boot.
- Derive a PWM channel's initial duty from the block's own rest/closed constant,
  never from the middle of travel (`drivers/servo_pwm` has `rest_deg`, default
  0). A cat feeder shipped with that channel hardcoded at the 90° centre and
  dispensed a portion on every power event, silently.

## Block slots

Each block contributes code via named `slots` in `block.yml`. The
generator substitutes `{{cfg.X}}`, `{{prefix}}`, `{{prefix_lc}}` and
inserts the result into the appropriate shell file at the appropriate
indent.

| Slot | Goes into | Purpose |
|---|---|---|
| `config_defines` | `app_config.h` | GPIO numbers, channel IDs, `#define`s |
| `driver_includes` / `driver_statics` / `driver_init` / `driver_apply_cases` | `app_driver.cpp` | hardware init + per-param HW writes |
| `matter_includes` / `matter_statics` | `app_matter.cpp` | per-endpoint includes + state |
| `matter_endpoint_create` | inside `app_matter_init()` | endpoint creation |
| `matter_driver_cb_cases` | `matter_driver_cb()` | param → attribute push |
| `matter_attr_cb_cases` | `app_attribute_update_cb()` | attribute → param push |
| `matter_identify_cases` / `matter_event_cases` | identify + chip event handlers | |
| `logic_includes` / `logic_statics` / `logic_init` | `app_logic/<concern>.cpp` — grouped by concern (persist / indicator / console / diagnostics / factory_reset / control / …); each behavior's `logic_init` becomes a `static <name>_init()`, run by the slim `app_logic.cpp` orchestrator | sensor poll tasks, button handlers, console cmd registration, NVS persist |
| `main_includes` / `main_init` | `app_main.cpp` | per-framework `#include` + init call (framework blocks only) |

## Block top-level fields

Beyond slots, a block can also contribute:

| Field | Effect |
|---|---|
| `idf_components` | Adds entries to `main/idf_component.yml` for the IDF component manager |
| `cmake_priv_requires` | Per-component PRIV_REQUIRES additions, e.g. `app_logic: [console]` or `main: [app_matter]` |
| `sdkconfig` | Adds key/value pairs to the product's `sdkconfig.defaults` |
| `param_refs` | Declares the value type of a `cfg` key that holds a driver param ID |
| `params` | Schema for `cfg` (with `type`, `required`, `default`, `enum`, `description`) |
| `provides_bus` / `requires_bus` | Which shared bus this block configures, or only talks on — see **Shared buses** |
| `bmgr` | esp-board-manager mapping, read only when a board file is supplied — see **Boards** |

## Shared buses (`provides_bus:` / `requires_bus:`)

A block that performs I2C (or SPI) transactions against a port it takes as a
param but never configures is making an assumption about a *different* block in
the same product. Undeclared, that assumption is held by convention only, and
when it does not hold the product **generates, compiles, links and boots**, and
is simply silent on the device — nothing in any report says why. Declare it:

```yaml
# in a sensor block that only TALKS on the bus
requires_bus:
  type: i2c            # bus kind: the word bmgr's peripheral `type:` uses
  port_param: i2c_port # the cfg key holding the bus index

# in the block that CONFIGURES it (peripherals/i2c_bus)
provides_bus:
  type: i2c
  port_param: port
```

Both accept a list, for a block on two buses at once (an SPI panel with an I2C
touch controller). The validator then requires, for every `requires_bus` block:

1. **a provider on the same port**, and
2. **earlier in `instances:`** — init order *is* product.yml order
   (`app_driver_init()` calls each instance's `driver_init` in sequence), so a
   bus configured after its first transaction is the same bug as no bus at all;
3. unless the instance runs on a **board** whose own definition carries that
   bus (`type: i2c` on the matching `config.port` in `board_peripherals.yaml`)
   — a board's peripherals come up in `esp_board_manager_init()`, which runs
   before `app_driver_init()`, so it is in time by construction.

Two things this deliberately does NOT do:

- **A block that configures its bus inside a display/audio node-init slot is
  not a provider.** Those slots run in their own task (`display_setup_task`),
  which is not ordered against `driver_init` — blessing them would bless a
  race. `drivers/display_touch_*` are in this category.
- It does not check the *pins*. Two blocks disagreeing about a bus's SDA is a
  `bmgr` shared-key error when a board is supplied (below), and invisible without one.

Twelve blocks declare `requires_bus` today (`aht21`, `bh1750`,
`fuel_gauge_bq27220`, `imu_bmi270`, `ina219`, `magnetometer_bmm150`,
`oled_ssd1306`, `scd4x`, `sgp40`, `sht3x`, `sht4x`, `speaker_es8311`), and
three declare `provides_bus` (`peripherals/i2c_bus`,
`drivers/i2c_temp_humidity`, `drivers/i2c_air_quality_stub`). All fifteen
speak `driver/i2c_master.h`.

**Declaring is not optional, and it is not on the honour system.**
`scripts/check.py` fails any block whose slots call `i2c_master_*` (or
`audio_codec_new_i2c_ctrl`, which is the same thing behind a codec API) while
declaring neither. The two `display_touch_*` blocks are the only exemptions,
for the node-init reason above, and they are named in the check. That rule is
how `drivers/speaker_es8311` was found: it configured an ES8311 over I2C in
`driver_init` and sat ABOVE `peripherals/i2c_bus` in `mosaico-touch-hub`, so
the codec was talking to a bus that did not exist yet — a build that succeeds
and a board that is silent, which is exactly what these declarations exist to
stop. Undeclared, the ordering rule could not see it.

### How an I2C block gets its bus

**The PORT is the bus's identity, and it is the only identity.** A block names
an integer port in `requires_bus.port_param` / `provides_bus.port_param`, and
at init it resolves that port to a live bus with ESP-IDF's own lookup:

```c
i2c_master_bus_handle_t bus = NULL;
esp_err_t err = i2c_master_get_bus_handle((i2c_port_num_t)PORT, &bus);   /* whoever made it */
i2c_device_config_t dev = {};                                            /* addr + speed */
dev.dev_addr_length = I2C_ADDR_BIT_LEN_7;
dev.device_address  = ADDR;
dev.scl_speed_hz    = FREQ;
err = i2c_master_bus_add_device(bus, &dev, &s_dev);                      /* one handle, kept static */
```

Transactions are then `i2c_master_transmit` / `i2c_master_receive` /
`i2c_master_transmit_receive` against `s_dev` — timeouts in **milliseconds**,
not ticks.

Why the port and not a bus NAME, which is the obvious alternative and was
rejected:

- **The registry already exists, inside the driver.** `i2c_new_master_bus()`
  records the bus against its port whether the caller was `peripherals/i2c_bus`
  or esp-board-manager's `periph_i2c`, and `i2c_master_get_bus_handle()` hands
  it back to anyone who knows the port. A name would be a *second* index over
  the same thing, kept in sync by hand, and the day the two disagree the
  product is silent on the device — the exact failure this section exists to
  stop.
- **A name is board-specific and a port is not.** Our amend calls its shared
  bus `i2c_zc_0`; the ESP-Mosaico's own definition calls the same physical bus
  `i2c_master`; the next board will call it something else again. A sensor
  naming a bus would have to be re-pinned per board, which is precisely the
  coupling `board.yaml` exists to keep out of `product.yml`. The port is the
  same number on every board, and it is already what the validator matches
  against the board's `config.port`.
- **No product.yml churn.** `i2c_port` kept its meaning, so not one instance
  had to be re-cfg'd for the migration.

Also rejected: `esp_board_manager_get_periph_handle("<name>")`. It resolves
only on the board path, so every sensor would carry two code paths chosen at
generation time — and it needs the name, so it inherits the problem above.

Three consequences worth not re-deriving:

- **Clock speed is now per DEVICE, not per bus** (`i2c_device_config_t.
  scl_speed_hz`). Every consumer gained an `i2c_freq_hz` param, default
  100000. A board-owned bus running fast and a slow sensor on it is fine.
- **Providers ADOPT rather than collide.** `i2c_new_master_bus()` fails on a
  port that already has a bus, so every block that wants to *create* one —
  the three providers and both `display_touch_*` blocks — looks the port up
  first and only creates it if nobody has. Two blocks on one port is then
  benign whichever runs first, which is what removed `mosaico-touch-hub`'s
  long-standing double-configuration of I2C0.
- **A missing bus logs and fails closed; it never aborts.** The block's
  `attach()` returns the error, the block skips its poll timer, and the log
  names the port. A sensor that isn't there must not take the product down
  with it — but it must not be silent either.

There is **no mixed state and no way back into one**: `scripts/check.py` fails
any block whose slots `#include <driver/i2c.h>`. A single block written the old
way would build, link, boot and be silent on a bus every other block now
creates with `i2c_new_master_bus()`, so it is an error rather than a warning.
`driver/i2c.h` being EOL in IDF 6 and removed in IDF 7 is the smaller reason.

One thing this migration did NOT change, and it is worth knowing: the legacy
header pulled FreeRTOS in transitively (its timeouts are `TickType_t`) and
`driver/i2c_master.h` does not. Seven blocks were relying on that and compiled
only in products where something else happened to include it. They now include
`freertos/task.h` themselves.

**FreeRTOS is not transitive, and `check.py` now says so.** A block whose slots
call `vTaskDelay`, `xTaskCreate`, `pdMS_TO_TICKS` and friends must include
`freertos/task.h` — checked PER SLOT FAMILY, because `driver_*` lands in
`app_driver.cpp`, `logic_*` in `app_logic/<concern>.cpp` and `matter_*` in
`app_matter.cpp`, and an include in one family does nothing for another. The
gap is latent by nature: the block is fine in every product it has been tried
in and breaks the day it is the only one in its file. Two blocks still carried
it when the rule was written — `drivers/ds18b20` and
`behaviors/identify_relay_click` — and the rule found exactly those two.

## Boards (`board.yaml` + `bmgr:`)

**A product.yml never names a board.** A product_configuration says what a
product DOES; what hardware it runs on is a property of one user's product —
the same smart plug is a bare C6 module for one person and an M5 NanoC6 for
the next. Welding a `board:` key into the catalog made 76 general entries
describe somebody's desk, and dragged `ci_chips` along with it.

The board is a SEPARATE document, `board.yaml` (schema `zc-board/1`), handed
to the generator and the validator beside the product:

```yaml
schema: zc-board/1
selected: esp32-c6-devkitc-1   # what the user picked: an esp-virtual-parts board id
chip: esp32c6
bmgr:
  board: esp32_c6_devkitc_1    # resolved bmgr board directory, or null
  resolved_from: exact         # exact | fallback | custom
```

- **`selected` is ALWAYS present.** The user picks hardware even when it is a
  bare module; there is no such thing as a product with no board.
- **`bmgr.board` may be null.** That means no esp-board-manager definition
  exists for that hardware yet — a coverage fact about our packs, not a fact
  about their board. A null degrades to the chip-agnostic path: same tree as
  before, no error.
- **`resolved_from: fallback`** means we resolved to a minimal `<chip>_module`
  board — i.e. we assert only the chip, and nothing on the board is claimed.
  `custom` means the definition travels in the product tree instead of being
  named from a pack.
- **The mapping is DATA, not string munging.** esp-virtual-parts carries
  `bmgr_board` / `bmgr_fallback` in `boards/<id>/board.json` for all 128
  boards. Read it: `m5-*` maps to `m5stack_*`, and the naive
  dash-to-underscore rule only resolves 63 of 128.

When `bmgr.board` resolves, on-board hardware comes from the board definition
and the product's own externally-wired parts are generated into an amend
overlay (`board_amend/`) that `idf.py bmgr -a` merges into the board before
code generation — which is what puts product pins and board pins in front of
one IO-conflict check. The build step reads the resolved name from `.zc-board`
in the generated tree.

**The validator's board pin-conflict check reads INTENT, not just pins.** A
product pin landing on a board-claimed pin is an error — with two exemptions,
each the composition working rather than an accident:

- **Step-aside** (`boardTakesOver` in `engine/src/types.ts` — ONE predicate,
  used by `renderInstances` to drop `replaces_slots` AND by the validator to
  exempt the pins): a `provided_by` block on a board that carries the device
  contributes no pins anywhere, so its cfg pins cannot conflict. The two sides
  used to re-derive this separately and diverged — the generator stepped a
  hand-pinned display aside while the validator threw an 11-pin wall for the
  same product + board. On a board WITHOUT the device the block keeps its init
  and its pins conflict-check normally. A mapping with no `provided_by` (an
  amend part like drivers/relay) is never exempt: its pins are real claims.
- **Adapter binding**: an instance whose mapping binds a board device by name
  (`provided_by` + `device_param`) exempts the pins THAT device claims (its
  own config plus the peripherals it references), so a companion block sharing
  the bound button's GPIO is asking the owner, not fighting it. And a plain
  pin conflict with a bindable device names the adapter as the fix
  ("use drivers/board_button (cfg.device: boot_button) instead of wiring your
  own on gpio 9") — derived from the catalog, so a new adapter names itself.

**A bus PROVIDER on a board-provided port is an error, not a merge.**
Consumers adopt a board-owned bus; a `provides_bus` instance (or any bmgr
peripheral fragment with a `config.port`) whose type+port the board already
provides means `esp_board_manager_init()` runs `i2c_new_master_bus()` twice on
one port and aborts at boot — a product that validates, builds and dies.
Checked in the validator AND in `buildBoardAmend` (for a product generated
without validation); the fix line is always "drop the instance — consumers
adopt the board's bus".

**The catalog carries board files for the SAMPLE products only** — the ones
that exist to exercise this path in CI (`mosaico-bmgr-probe`,
`c6-devkit-board-relay`, `mosaico-room-panel-board`, `c6-devkit-plug` — the
adapter sample: the devkit's own BOOT button + WS2812 through
`drivers/board_button` / `drivers/board_led_strip`, relay via the amend —
and `mosaico-pinned-panel-board`, the step-aside sample: hand-pinned CO5300 +
CST9220 blocks on the board that already carries both). `generate()` and
`scripts/validate.mjs` pick a board file up from there when the caller passes
none; a host that owns the user's board file passes it explicitly. No other
product has one, and none should grow one — `scripts/check.py` refuses a
`board:` key in any product.yml.

A block joins the amend by DECLARING a `bmgr:` mapping. Nothing is inferred
from param names — a guess here writes pin claims and init code for hardware
the engine does not know, and a wrong guess is a board definition that is
confidently wrong. A block with no `bmgr:` contributes nothing and behaves
exactly as it always did, board or no board.

```yaml
bmgr:
  init: board_manager        # required word: who owns the hardware at boot
  replaces_slots: [driver_init]   # dropped when a board owns the line — ONE owner
  provided_by: display_lcd   # …but ONLY on a board that already HAS this
  requires:                  # hard gates; an instance outside them FAILS
    - cfg: active_level
      equals: 1
      reason: <printed verbatim as the error>
  peripherals:
    - name: gpio_{{prefix_lc}}_relay
      type: gpio
      config:
        pin: '{{cfg.gpio}}'
  devices: [...]             # same substitution rules
```

- **Two shapes of mapping, and `provided_by` tells them apart.** A relay is
  wired TO a board and never on it, so its pins belong in the amend and its
  `replaces_slots` applies on ANY board. A PANEL is the opposite: it arrives
  soldered, the board definition already describes it, and the block has
  nothing to add — it must step aside so the board's own device is the one
  owner. `provided_by: <board device type>` marks that second shape, and it is
  checked against the board's own `board_devices.yaml`: on a board with no such
  device (a bare devkit with a panel wired to its header) the mapping does not
  apply and the block keeps its init, pins and all. Without the gate, that
  product would lose its screen with nothing in the tree to say why.
- **`requires:` is a gate, not a filter.** A mapping hands a physical line to
  code the block author did not write; an instance outside what the author
  vouched for stops generation instead of silently dropping out of the amend
  (which would look exactly like a block with no mapping).
- **`periph_gpio` cannot drive an ACTIVE-LOW output safely.** It calls
  `gpio_config()` *before* it writes `default_level`, and the output register's
  reset value is 0 — so an active-low load is ENERGIZED for the gap between the
  two calls, i.e. a pulse to a garage-door opener on every boot. This is the
  same rule as *Actuators must be born safe* above, and it is why
  `drivers/relay` guards `active_level: 1`: an active-low instance keeps the
  block's own init, which writes the idle level first. Any new output mapping
  needs the same guard until bmgr can set a level before driving.
- **Quote any value that starts with `{{`.** `pin: {{cfg.gpio}}` is a YAML
  *flow mapping*, not a placeholder, and fails to parse. Write
  `pin: '{{cfg.gpio}}'`. A placeholder that is the WHOLE value keeps its type,
  so that emits `pin: 4` — an int, which is what bmgr's typed schema wants —
  while `gpio_{{prefix_lc}}_relay` is ordinary string interpolation.

### Shared bmgr fragments (`shared_key:`)

The mapping above is per-INSTANCE, which is right for a relay (one instance,
one line) and wrong for a bus: six sensors on one I2C port would emit six `i2c`
peripherals, and bmgr merges same-named entries **field by field**, so which
SDA pin the bus really runs on would be decided by instance order.

An entry carrying `shared_key:` describes a resource several instances share:

```yaml
peripherals:
  - shared_key: 'i2c-{{cfg.port}}'     # templated: identity of the RESOURCE
    name: 'i2c_zc_{{cfg.port}}'        # NOT {{prefix_lc}} — same name for all
    type: i2c
    config:
      port: '{{cfg.port}}'
      pins: { sda: '{{cfg.sda_gpio}}', scl: '{{cfg.scl_gpio}}' }
```

The first instance resolving a given key emits the fragment; later ones
resolving the SAME key emit nothing; a later one resolving the same key to
**different YAML fails generation**, naming both instances and every field they
disagree on. "One bus, two opinions" is a real disagreement about hardware, and
the one thing that must never be resolved by last-writer-wins. `shared_key` is
ours: it is stripped before the fragment is written, because bmgr's schema has
no such field. Nothing about it is I2C-specific — an SPI host, a shared LEDC
timer or a shared UART use it identically.

> **The phase-3 caveat is GONE.** It used to read: bmgr's `i2c` peripheral
> speaks `driver/i2c_master.h`, the `requires_bus` sensors spoke the legacy
> `driver/i2c.h`, the two cannot own one port, so on a board those sensors
> could not ride the bus and the validator refused the composition. Every I2C
> block moved to the new API, so a board-owned bus and a product-owned one are
> now the same bus to a consumer. `mosaico-room-panel-board` is the product
> that proves it: two sensors on a bus it does not own, no `peripherals/i2c_bus`
> instance and no SDA/SCL anywhere in its file.

### The board as a display CONTRIBUTOR

The mapping above is how a block gets OUT of the way. Something still has to
drive the screen, and on a board that carries one, that something is the board.

When a board's `board_devices.yaml` declares a `display_lcd` device, the
generator has the display framework FETCH its handle
(`esp_board_manager_get_device_handle("display_lcd", …)`) rather than run an
`esp_lcd_*` bring-up of its own; an `lcd_touch` device is registered against
that display the same way. The code renders into the ordinary
`display_node_init` slot from the same synthetic board contributor that emits
`{{board_init}}`, so ordering is already right: board init runs before
`app_driver_init()`, and `app_display_init()` after both.

Three things this rests on:

- **A handle, not a re-init.** `esp_board_manager_init()` has already made the
  bus, the panel IO, the reset, the mirror/swap and `disp_on`. Doing it again
  is not a redundancy — `spi_bus_initialize()` on a host the board already
  claimed returns `ESP_ERR_INVALID_STATE` into an `ESP_ERROR_CHECK`.
- **Read from the board, never from its name.** The device list comes out of
  the board's own YAML at generation time. Matched on `type:`, called by
  `name:` — usually the same word (bmgr's canonical `display_lcd`), but they
  are different things and a board may name its panel something else.
- **A display product with no panel fails GENERATION.** Not at runtime, and
  not on a bench: if no block and no board provides one, the tree is refused,
  and the error says which of the two it was (including "the board pack could
  not be read").

Measured on the ESP-Mosaico, `mosaico-room-panel` vs `mosaico-room-panel-board`
(esp32s31, same frameworks): 1,980,976 B → 2,114,048 B. Re-measured after the
board twin moved to the board's own status LED (`bmgr.device_param`, below),
same pair, same run: **1,992,816 B → 2,125,584 B** — the gap is unchanged at
+132.8 KB, and both sides drifted the same ~11.6 KB upward with the toolchain.
The collapse itself is
nearly free — `app_display` and `app_driver` lose 138 B between them, because
the init MOVED into `esp_board_manager` rather than disappearing. The +130 KB
is the board bringing up everything ELSE it carries: the ES8311 codec
(+22.8 KB) and its I2S driver (+28 KB), the board's buttons (+4.8 KB), PSRAM
the board correctly asserts (+4.4 KB), the board manager itself (+20 KB) and
`gen_bmgr_codes` (+3 KB). What a board product pays for is the WHOLE board.
Trimming that is phase 4's `init_skip` question, and this is the number it
should be measured against.

### Driving a board's OWN device (`bmgr.device_param`)

The display path works because the display framework has a slot the board can
fill. Most on-board hardware has no framework behind it: a board's status LED
is a `gpio_ctrl` device with a handle sitting there and nothing that fetches
it. An **adapter block** is that fetcher — three exist:
`drivers/board_gpio_ctrl` (a `gpio_ctrl` output), `drivers/board_button`
(a `button` device — dev_button is built ON iot_button, its handles ARE
`button_handle_t`, so the adapter registers an ordinary `iot_button_register_cb`
and creates nothing; it must NOT go through `app_button_get_or_create`, which
would make a second iot_button device on a pin the board's device owns), and
`drivers/board_led_strip` (a `led_strip` device — an OBSERVER via
`app_driver_register_solution`, never a `driver_apply_cases` owner, because an
indicator's param is usually owned by the relay whose state it shows and a
second `case` on one param is a duplicate-case compile error).

```yaml
bmgr:
  init: board_manager
  provided_by: gpio_ctrl   # board device TYPE — which handle struct comes back
  device_param: device     # cfg key holding the NAME — which of them
```

- **The block states NO pin**, and that is the whole mechanism. The board
  claims the line for its own device, so a product claiming it too is an IO
  conflict — correctly. `mosaico-room-panel-board` could not drive the
  Mosaico's orange LED at all before this: pointing `drivers/relay` at GPIO 3
  collides with the board's `gpio_status_led`, so the product drove an
  externally-wired relay on GPIO 4 instead. The way to use someone else's pin
  is to ask them for it.
- **TYPE and NAME are both needed**, unlike the display case where "the first
  panel" is a fair answer. A board carries several devices of one type — the
  ESP-Mosaico declares `status_led` AND `vibration_motor`, both `gpio_ctrl` —
  so first-of-type would silently drive the wrong one.
- **A board-device adapter is board-ONLY and CHECKED.** No board, an unreadable
  pack, or no such device on the board all fail GENERATION, and the error names
  the devices the board does have (the usual mistake is a spelling). Unchecked,
  every one of those generates, compiles, links, boots and reports itself in a
  single `ESP_LOGE` at startup.
- **`dev_gpio_ctrl_init()` drives the line to `active_level` at board init**,
  whatever `default_level` says — so the device is ON by the time
  `app_driver_init()` runs. The adapter deasserts first thing. On an LED that
  is a boot flash; on a relay or a motor the same adapter can drive, it is a
  pulse to the load. Same rule as *Actuators must be born safe*.

## Generator

```bash
pnpm exec tsx scripts/generate-template.ts <product-id>
```

Reads `product_configurations/<id>/product.yml` → loads instance blocks + framework
blocks → renders slots → copies the base `firmware/` tree + selected
framework components → emits a buildable firmware tree at
`_generated/<id>/`.

```bash
# Build the generated tree
cd firmware/templates/_generated/<product-id>
idf.py set-target esp32c6 && idf.py build
```

## Validation

```bash
pnpm exec tsx scripts/validate-templates.ts --strict
```

Catches:
- Param-type conflicts across blocks (e.g. one block says `bool`,
  another says `u8` for the same `APP_DRIVER_PARAM_*`)
- Params published-with-no-consumer or consumed-with-no-producer
- Missing block references in product.yml

## Chips

Products are chip-agnostic *as catalog entries*, but a generated tree is built
for ONE chosen chip: `generate()` takes a required `chip`, and only that chip's
sdkconfig is merged into the single `sdkconfig.defaults` — no per-chip files are
emitted. A chip is supported by its entry in `engine/src/hardware.ts` — the
default devkit, its usable GPIOs (flash/PSRAM/board-reserved pins already
excluded) and radio features. `chipCapabilities()` reads it, the validator's pin
check reads it, and the agents get their pin budget from it. `features` is what
is on the die; a radio that comes from a companion chip goes in `hostedFeatures`
instead, never in `features`. `generate()` validates `chip` against
`CHIP_MODULES`.

**`target: <chip>`** scopes ANY block to one chip — of any kind (driver,
behavior, framework, sdkconfig fragment). A block with `target:` contributes
only when generating for that chip and is skipped otherwise; a block with no
`target:` is chip-agnostic and always applies (the default). It is the general
"this is meaningful on one chip only" filter, honored uniformly across
instances, frameworks and product-selected fragments (`targetMatches` in the
generator). `check.py` fails a `target:` that is not in `chips.txt`.

The built-in users of it are the **chip base blocks**
`code_blocks/sdkconfig-fragments/<chip>/` — an OPTIONAL `sdkconfig_fragment`
marked `target: <chip>` carrying only what is silicon-specific in its sidecar
`sdkconfig.defaults`, which the generator AUTO-emits for the selected chip by
that name (no product lists it). Only two chips have one: `esp32h2` (the
Thread-only stack — no Wi-Fi) and `esp32p4` (the hosted-radio wiring). Universal
defaults (flash size, size opt, logging) live in `base_firmware/sdkconfig.defaults`,
BLE/Wi-Fi enablement lives in the frameworks that use them, and a key that only
restates a Kconfig default is left out — so every other chip is fully covered by
the common base + frameworks and has no chip base at all. The generator merges a
present chip base in low in the file (a floor the block/fragment/product
contributions sit on top of), so it decides which choice symbols a product can
win. `y`/`n` in the sidecar MUST be quoted (`="y"`) — an unquoted value is
parsed as an int by `assemble_blocks.py`.

Current set: esp32, esp32c3, esp32c5, esp32c6, esp32h2, esp32p4, esp32s3,
esp32s31 — `chips.txt`, the one list `check.py`, `build-product.sh`, the CI
matrix and `Dockerfile.ci` read. A chip joins it only after a real build.

**A framework declares the chips it builds for** — `chips:` in
`code_blocks/frameworks/<fw>/block.yml`, every name from `chips.txt`
(`check.py` enforces both). CI builds a framework only on the chips it claims,
so proving a framework on a new chip is: build it for real, add the chip here.
The platform's framework registry (`supportedChips`) must say the same thing;
a change to one is a change to both.

**A framework also declares `radio: true|false`** — does it need a radio (a
transport, a mesh, a BLE link, the internet) or is it entirely on-device (a
screen, a speaker, a microphone)? The platform's `frameworks/radio.ts` split,
written here too and checked the same way. The generator acts on it for the
**ESP32-P4**: the base manifest pins the ESP-Hosted stack (`esp_wifi_remote` +
`esp_hosted`) for that chip so a networked product keeps the `esp_wifi` API,
but a tree whose frameworks are all local gets the two dropped
(`P4_HOSTED_PACKAGES`). Linked whole-archive, the stack cost the voice product
278 KB of app (measured, ESP-IDF 6.2) for nothing it could send.

**Voice fits 8mb-voice on every chip — on esp-sr 2.4.x.** The audio block pins
`~2.4.7` on purpose: 2.5 added an esp-dl dependency that is 860 KB of code the
framework never calls, and it turned a 1.33 MB S3 image into 2.44 MB (over the
2.25 MB slot). Every "the S31/P4 overflows 8mb-voice" reading of Aug–Sep 2026
was that payload, not the chip. Bumping esp-sr is a measured change.

**esp32p4 has no radio of any kind** — no Wi-Fi, no BLE, no 802.15.4 — which
is why its `features` list is empty. All connectivity belongs to a companion
chip (a C6 on the Function EV board) reached over ESP-Hosted:
`esp_wifi_remote`, pulled in by a `target in [esp32p4]` rule in
`base_firmware/main/idf_component.yml`, keeps the esp_wifi API working, and the
companion's slave firmware must be flashed separately (see the header of the
`sdkconfig-fragments/esp32p4` chip block). The `target in [esp32p4]` component
rules and the `BLEManagerImpl.cpp` CMake patch are NOT sdkconfig, so they stay
in `base_firmware/` (`main/idf_component.yml`, `CMakeLists.txt`) — only the P4
sdkconfig keys live in the chip block. Three consequences:

- `CONFIG_SOC_WIFI_SUPPORTED` is **false** on P4, so Wi-Fi-conditional slot
  code needs
  `#if CONFIG_SOC_WIFI_SUPPORTED || CONFIG_SOC_WIRELESS_HOST_SUPPORTED`.
- `CONFIG_BT_*` symbols exist, but the controller is remote, never local.
- Matter runs **dual-stack** here where every other chip is IPv6-only:
  `eppp_link`, which comes along with `esp_wifi_remote`, does not compile
  against an IPv4-less lwIP. IPv4 is on by default (`CONFIG_LWIP_IPV4=y`) and the
  matter block does not disable it, so P4 is dual-stack with no extra keys — but
  a P4 product must never add an IPv6-only / Thread override (e.g. the
  `matter_thread` fragment, which is meaningless on P4 anyway) or eppp_link
  fails to compile.

**BLE on P4 costs two extra pieces**, both already in place — worth knowing
about before touching either. `CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE=y` builds
esp_hosted's VHCI glue, which implements NimBLE's `ble_transport_*` entry
points against the companion's controller; esp_matter then drives
`nimble_port_init()` as it does anywhere else. That glue only self-binds on
esp_hosted's **2.x** line, which is why `main/idf_component.yml` pins it there
for P4 — 3.x turned it into a feature the app has to start explicitly, and
esp_matter never makes that call. And `base_firmware/CMakeLists.txt` patches
one line of esp_matter's `BLEManagerImpl.cpp`: it hardcodes a
`ble_transport_ll_deinit` stub for P4 that current esp_hosted also defines, so
the guard is narrowed to keep the stub only when the glue is absent.

## When you need a new block

> **Matter device_type blocks: generate the slots, don't hand-write them.**
> The `esp-code-blocks` MCP server emits the five `matter_*` slots
> (endpoint creation, feature claims, attribute↔driver wiring) spec-checked for
> any device type × feature combo. Generate with it first and hand-edit only
> what it can't know; catalog device_type blocks are the fallback, hand-written
> data-model code the last resort. Drivers, behaviors, and non-Matter blocks are
> hand-authored as before.

> **Some Matter device types need a delegate — and the catalog already says which.**
> Some clusters require an application-defined *delegate* (OperationalState, a
> ModeBase mode cluster, ValveConfigurationAndControl, and the like): the generic
> server handles the data model, and calls the app's delegate for commands and for
> list data it cannot know. A device type carrying one needs that C++ delegate, not
> just endpoint creation. An unimplemented one is not silently
> incomplete — it is flagged in `block.yml` by `matter_wiring: endpoint_only`
> with a `matter_wiring_note` naming what it still needs (often "…delegates").
> `scripts/check.py` keeps that field honest against the slots: a device_type
> with no `matter_*_cb_cases` slots MUST declare `endpoint_only`, and one that
> has them MUST drop it — so the marker can never drift from reality. Scan the
> catalog for the stubs with `grep -rl 'matter_wiring: endpoint_only'
> code_blocks/device_types`.
>
> To implement one, do NOT hand-write the delegate contract from memory: a wrong
> terminator convention, a missing command declaration, or a mis-threaded report
> compiles and links cleanly and is then silent on the device — the exact failure
> the block system exists to prevent, and one only hardware testing catches. The
> canonical reference delegates live in the **Matter KB**; the agent retrieves one
> through its Matter knowledge tool and adapts it. The KB covers the esp_matter
> side only — the ZeroCode side (param-bus wiring, the reporting path) is shown by
> any device_type block that already carries a delegate; read its `slots/`. Once a
> stub gets its delegate, drop `matter_wiring: endpoint_only` — `check.py` insists
> on it the moment `matter_*_cb_cases` slots exist.

Use an existing block as a template. The pattern is consistent: a
`block.yml` with metadata + slots. For drivers/device_types/behaviors,
the slot targets are documented above. For frameworks, supply a
`components/app_<name>/` subdir; the generator copies it whole.

Watch out for these common authoring pitfalls:
1. **Cross-TU statics.** `logic_*` slots are now grouped by concern into
   separate files under `app_logic/` (e.g. `persist.cpp`, `indicator.cpp`),
   and `driver_*` slots land in `app_driver.cpp` — each a separate
   translation unit, so a `static` in one isn't visible in another. A
   behavior's own `logic_statics` + `logic_init` land together in its
   concern file, so keep state + the functions that touch it in the same
   slot family.
2. **Missing `cmake_priv_requires`.** A `#include <foo.h>` in a slot
   needs the relevant component listed under
   `cmake_priv_requires.<component>` or you'll get
   `fatal error: foo.h: No such file or directory` at compile time
   even though the dependency was downloaded.
3. **Matter attribute writes off the CHIP thread.** Prefer
   `esp_matter::attribute::update(...)` for attribute reporting — it takes
   the CHIP stack lock internally, so it is safe to call from a driver
   callback, a sensor poll timer, or a console command. Do NOT reach for a
   raw `*Server::Set*` cluster setter or a direct CHIP data-model call from
   those contexts: they do not lock, so off the Matter event-loop thread they
   trip `AssertChipStackLockedByCurrentThread` and abort at runtime (compiles
   fine, so only functional_testing catches it). If you genuinely need a raw
   setter, wrap it in `esp_matter::lock::chip_stack_lock()` /
   `chip_stack_unlock()` or dispatch it via `PlatformMgr().ScheduleWork(...)`.
4. **Zigbee ZCL writes off the Zigbee task need the stack lock.** Any
   `ezb_zcl_*` call from a driver callback, timer, or console command
   must be wrapped in `esp_zigbee_lock_acquire(portMAX_DELAY)` /
   `esp_zigbee_lock_release()` — it compiles fine without and corrupts stack
   state at runtime (same class as the Matter chip-stack-lock rule above).
   The generated `zigbee_driver_cb` scaffolding does this; keep the
   discipline in any hand-written additions.
5. **`MANAGED_INTERNALLY` attributes are served by the cluster's server, not
   esp-matter storage.** `attribute::update()` refuses them with
   `ESP_ERR_NOT_SUPPORTED` — check that return, or the failure is silent in
   practice. (On the generated data model this covers every attribute a delegate
   cluster serves, not only the measurement values.) How to change one depends
   on how the server's `Read()` sources that attribute — look it up per
   attribute, not per cluster; the same cluster mixes both:
   - **The server keeps it itself** and exposes a setter (`OperationalState` via
     `SetOperationalState`, `CurrentMode` via `UpdateCurrentMode`): call the
     setter; it stores and reports. Setters are `[[nodiscard]]` with differing
     return types (`Status` vs `CHIP_ERROR`); an unchecked one fails the build.
   - **The server asks the Delegate** on read (`ActivePower` via
     `GetActivePower`, `OperationalStateList` via `GetOperationalStateAtIndex`,
     `SupportedModes`): you own the value. Change your backing data, then mark
     the attribute dirty — the cluster's own helper if it has one
     (`ReportOperationalStateListChange`), else
     `MatterReportingAttributeChangeCallback(...)` under the chip stack lock —
     see `device_types/electrical_sensor`.
6. **Zigbee device factories already include Groups + Scenes.** The
   `ezb_zha_create_*()` factories (`ezb_zha_create_on_off_switch()`,
   `ezb_zha_create_on_off_light()`, …) create endpoints that ALREADY contain the
   mandatory Basic / Identify / Groups / Scenes clusters. Adding Groups/Scenes to
   those endpoints again is a **duplicate cluster registration** — compiles and passes
   review, but the stack **crashes at boot** (only functional_testing catches it,
   same class as #3/#4). Add only the EXTRA clusters the device needs on top of
   the endpoint, or build its cluster list manually if you need full control.

See `INVENTORY.md` for the current catalog.

## Where this sits (the four repos)

ZeroCode AI is one platform plus three runtime-fetched content repos. Know which
one owns what before proposing a change:

| repo | owns | goes live by |
|---|---|---|
| **esp-zerocode-blocks** (here) | firmware vocabulary — blocks, product configurations, frameworks, the generator engine | `scripts/publish-catalog.sh` |
| esp-virtual-parts | diagram vocabulary — virtual parts + boards the whiteboard renders | `scripts/publish.sh` |
| esp-zerocode-agents | agent vocabulary — per-stage prompts, tool lists, models | `scripts/publish.sh` |
| esp-zerocode-ai | the platform — studio, runner, tools, providers, chip + framework registries | image build + deploy |

Contributor entry points here: `AGENTS.md` (+ `.claude/skills/`) for the guided
flows; the in-product version is the studio's **Library → + Contribute** page.
