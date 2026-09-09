# Firmware Templates Inventory

Catalog of all code_blocks and product_configurations under
`firmware/templates/`. Update when entries are added, removed, or renamed.

- esp_matter pinned to **1.6.0** (Matter spec 1.6) in
  `frameworks/matter/idf_component.yml`
- esp-zigbee-lib pinned to **~2.0.0** in
  `frameworks/zigbee/idf_component.yml`
- ESP-IDF **master @ e9da155** (self-reports 6.2.0) — a commit, not a release,
  because ESP32-S31 is an IDF PREVIEW target and exists in no tag. `pins.env`
  holds it (with the esp-board-manager pin): every CI build, GitLab and GitHub,
  runs on exactly that commit, the same one the ZeroCode AI backend builds with.
- CI is in THIS repo (`.gitlab-ci.yml`, `.github/workflows/`), which is where it belongs now the
  products do. Three tiers, sized by what each can actually tell you:
  - `validate:blocks` — every push, GATES. All 73 products, no compiler.
  - `build:affected` — every PR. One product per CHANGED block, one chip
    (esp32c6). Typically 1–3 builds.
  - `build:firmware` — main + nightly. The full framework × chip grid,
    ~12 builds per chip across eight chips.

  73 × 8 is ~584 builds asking maybe 20 distinct questions. A second Matter
  light cannot fail where the first passed, and on an PR nothing outside the
  diff can have changed behaviour at all.

## Block kinds

| Kind | Count | Contributes |
|---|---|---|
| `driver` | 63 | Hardware peripherals — relay, PWM LED, fan, button, I2C sensors, encoder, I2S mic/speaker, etc. |
| `device_type` | 60 | Cross-framework device types — light, dimmable_light, fan, thermostat, occupancy_sensor, … (per-framework bindings via matter_* slots; rainmaker_* planned) |
| `behavior` | 36 | Runtime patterns — NVS persistence, OTA, factory reset, LED patterns, console commands, watchdogs, … |
| `peripheral` | 7 | Shared bus/unit init — I2C, SPI, OneWire, UART, ADC, GPIO ISR service, PCNT |
| `framework` | 15 | matter / rainmaker / zigbee / ble_mesh / audio / mqtt / display / vision / ble_hid / espnow / webui / agents / sound / ml / lm — each owns its `app_<framework>/` component |
| `partition_table` | 5 | 2mb / 4mb / 8mb / 8mb-voice / 16mb-voice partition CSVs |
| `sdkconfig_fragment` | 13 | Named sdkconfig modes (production, secure_boot, coredump, power_management, matter_thread, …) |

---

## Product configurations (91)

Catalog grouped by category. All product_configurations are **chip-agnostic**
— there's no `chips:` field in product.yml. Compatibility surfaces at build
time (matrix run on 5 chips: esp32 / esp32c3 / esp32c6 / esp32s3 / esp32h2).
esp32c5 and esp32p4 have per-chip defaults + a devkit GPIO map here, but the
platform's CI matrix (`BUILD_CHIP` in esp-zerocode-ai) does not build them
yet — verify those two with `scripts/test-product.sh <id> --build <chip>`.
Both carry the whole catalog — Matter included, verified by build on each. On
esp32p4 that rides ESP-Hosted for both Wi-Fi and BLE, which brings an
esp_hosted version pin and a one-line esp_matter patch with it (CLAUDE.md →
Chips).

### Outlets / on-off (8)
`smart-plug` (full-featured), `smart-plug-metered`, `usb-multi-plug`,
`night-light`, `wall-switch-1gang`, `wall-switch-2gang`, `wall-switch-3gang`,
`wall-switch-dimmer-combo`

### Lighting (8)
`dimmable-bulb`, `tunable-bulb`, `color-bulb`, `led-strip-rgb`,
`led-strip-cct`, `ceiling-panel-light`, `outdoor-flood-light`,
`in-wall-dimmer`

### Sensors (9)
`temp-humidity-sensor`, `motion-sensor`, `contact-sensor`,
`water-leak-sensor`, `ambient-light-sensor`, `air-quality-monitor`,
`smoke-co-alarm`, `thread-contact-sensor`, `environment-sensor-hub`

`environment-sensor-hub` is a multi-probe station on ONE I2C bus: three
temperature/humidity probes at different heights (SHT4x @0x44, SHT3x @0x45,
AHT21 @0x38), CO2 (SCD4x), VOC (SGP40), light (BH1750), the supply rail's
current and voltage (INA219) and a 128×64 OLED — nine devices, two GPIOs. It
is a real archetype, and it is in the CATALOG because those eight sensor
blocks were in no product at all, so CI compiled none of them. A block nothing
builds keeps passing every gate right up to the day somebody's product is the
first to instantiate it.

`thread-contact-sensor` is `contact-sensor` carried over **Thread** instead of
Wi-Fi — same driver and device type, plus `sdkconfig_fragments:
[matter_thread]`, and no `baseline-connectivity` (that group is Wi-Fi-coupled).

### Climate / fans (6)
`smart-fan`, `air-purifier`, `portable-ac`, `thermostat`,
`ceiling-fan-light`, `water-heater`

### Switches / controllers (4)
`scene-button`, `smart-remote`, `rotary-dial` (encoder),
`wall-switch-dimmer-combo`

### Other appliances / actuators (5)
`smart-door-lock`, `curtain-motor`, `robotic-vacuum`, `ev-charger`,
`outdoor-flood-motion`

### Combos (2 device-level + 2 FRAMEWORK-level)
`bath-fan-light` (light + fan + humidity), `bath-exhaust-fan` (fan + humidity)
— device combos within one framework.

**Framework combos** (frameworks: [a, b] — the same device-type instance
renders BOTH bindings; the param bus is the join, source-skip + change
dedup prevent update loops):
- `thermostat-touch-panel` — [matter, display]: a Matter climate device
  that is also a touchscreen; Home-app writes move the on-screen slider and
  screen drags update the Matter attribute.
- `camera-motion-ha` — [vision, mqtt]: on-device motion detection published
  to Home Assistant as an occupancy binary_sensor; the image never leaves
  the device.
- `mosaico-room-panel` — [matter, display]: first esp32s31 product, all
  onboard ESP-Mosaico hardware (CO5300 480×480 QSPI panel + CST9220 touch +
  status LED as the light card). Mosaico-specific by construction; not yet
  hardware-verified. On the 8mb table — Matter + LVGL on the S31 overruns a
  1.875 MB slot by 15 KB, and the board has 16 MB of NOR.
- `mosaico-room-panel-board` — [matter, display]: the SAME product with its
  panel and touch coming from the `esp_mosaico` board definition instead of
  two driver blocks and sixteen GPIO numbers (see CLAUDE.md → Boards). It
  carries a `board.yaml`; `mosaico-room-panel` deliberately does not, and is
  kept as the CONTROL the two are measured against. Measured on the S31 (one
  run, both sides): 1,992,816 B hand-pinned vs 2,125,584 B on the board — the
  board costs +132.8 KB, because it brings up hardware this product does not
  use (the ES8311 codec and its I2S) as well as the panel it does.
  The two are now behaviourally the same product: the board twin drives the
  board's OWN status LED through `drivers/board_gpio_ctrl` (CLAUDE.md →
  Driving a board's OWN device) instead of an externally-wired relay on
  GPIO 4, so the only remaining difference is who owns the panel, the touch
  and the I2C bus.
- `mosaico-touch-hub` — [matter, mqtt, display, webui, sound]: the ESP-Mosaico
  with EVERY onboard peripheral in the user guide's pin table — panel, touch,
  AI button (GPIO7), haptic motor (GPIO8), ES8311 codec + NS4150B amp, BMI270
  IMU, both BMM150 magnetometers, BQ27220 fuel gauge. Four frameworks at once
  is the S31's point: three radios on one die, so a panel need not choose
  between a Matter controller, a local broker and its own web page. Sensors are
  bound to honest device types — the IMU's "being moved" is occupancy, a
  magnetometer's "magnet near" is a contact sensor.
- `camera-motion-v4l2` — [vision]: camera-motion-sensor's twin on the esp_video
  (V4L2) stack, for chips espressif/esp32-camera does not build for. Same
  detector, different capture path.

Compatibility is now a TRUE-conflict matrix in the monorepo's
FRAMEWORK_PROFILES (radio/stack ownership, Wi-Fi ownership, flash budget) —
not a blanket standalone rule. Notable blocks that remain: mqtt↔matter/
rainmaker (both own Wi-Fi — the "mqtt rides an existing transport" engine
work lifts this later), audio↔matter (8MB flash: voice models + a Matter
app don't both fit the 2.25MB OTA slots; needs a 16MB table), ble_hid↔BLE
users (NimBLE held permanently).

### Audio (1)
`sound-monitor-chime` — I2S microphone (RMS loudness) + I2S chime speaker,
console-controlled. The catalog's first **`frameworks: []`** product: no
framework has a speaker or microphone device type (verified against the SDK
headers — esp_matter 1.4.2 ships no media/AV endpoints, and neither the
RainMaker nor Zigbee HA vocabularies used here cover audio), so these drivers
bind to no endpoint and are driven over the console instead. Also the first
coverage of the documented-but-untested framework-less path.

Six GPIOs for two I2S devices does not fit cleanly in any chip: after
excluding classic-esp32 flash pins (6-11), esp32c3 USB-JTAG/UART0 (18-21) and
the status LED, only ~3 fully-clean GPIOs remain across the validator's
pin-check set. The defaults are all *usable* on c3/c6/s3 (no pin warning) but
some are strapping pins on some chips — safe here because all are driven only
after reset, and a reason to reassign per board.

### RainMaker (4)
`rainmaker-fan-light` (light + fan, real-board verified), `rainmaker-smart-plug`
(relay + button, bool actuator reference), `rainmaker-temp-sensor` (I2C temp +
humidity, read-only telemetry reference), `rainmaker-color-bulb` (PWM RGB,
multi-param actuator reference). All `frameworks: [rainmaker]`, 4mb partition
table (fctry for self-claiming), status LED on GPIO 10 (15 is a flash pin on
esp32c3).

47 of 53 device_type blocks carry `rainmaker_*` bindings (same-block
multi-framework slots). The 6 controller-side / event-emitting types
(color_remote, dimmer_remote, thermostat_controller, pump_controller,
control_bridge, scene_button) are Matter-only — RainMaker models
state-holding devices, not controllers.

### Voice / audio framework (1)
`voice-lamp` — first `frameworks: [audio]` product. On-device wake word
(WakeNet 9 "hi esp") plus English speech commands (MultiNet 7) via esp-sr,
driving a PWM lamp. No cloud, no hub.

**ESP32-S3 only, and MultiNet is the constraint** — not esp-sr as a whole.
The component builds on esp32/c3/c5/c6/p4/s2/s3 and WakeNet runs on all of
them, but every English model in esp-sr's `ENGLISH_SR_MN_MODEL_SEL` choice is
`depends on IDF_TARGET_ESP32S3` (mn7 also allows P4). A voice framework with
no command vocabulary isn't this framework.

Requires the **8mb-voice** partition table — `esp_srmodel_init("model")` needs
a `model` partition, and a build using the plain 8mb table succeeds and then
fails at boot with "no models found".

The device-type binding is **inverted** vs every other framework: instead of
"how does this device appear on the network," `audio_commands_register`
contributes phrases and `audio_command_cases` maps a recognized phrase id to
`app_driver_set_param`. 29 of 53 device_type blocks carry `audio_*` bindings
(101 phrases total — comfortably inside MultiNet's `ESP_MN_MAX_PHRASE_NUM`
of 400 / 63-char cap). The 24 without are `# Voice-absent` by category:
16 sensors (read-only telemetry, no TTS path to answer questions), 6
controllers (a controller is itself an input — binding voice to it is
meaningless), 2 with no driver_params (extractor_hood, heat_pump).
`door_lock` binds **lock only** — a wake word is not authentication, so
unlock stays on the authenticated paths (see its block.yml).
Per-instance phrase ids come from `audio_cmd_base` (same uniqueness
discipline as `zb_endpoint`).

Audio is standalone for now: it is additive rather than an alternative to the
transports (a voice-controlled Matter lamp is the obvious next product), but
combos aren't supported yet, so its profile marks it incompatible with all of
them — same staging Zigbee used.

### ESP-NOW (2 — a KIT)
`voice-matter-lamp` — first voice+TRANSPORT product: frameworks:
[audio, matter] on the 16mb-voice table (engine-enforced — the pair is a
validation error on any other table; measured 37% OTA headroom in the
3.5MB slots). Partition-table blocks own their flash-size sdkconfig
(8mb-voice → FLASHSIZE_8MB, 16mb-voice → FLASHSIZE_16MB) so table and
flash size never drift. 16MB S3 boards only.

`mqtt-color-light` — first HA **JSON-schema** light: plain-topic lights
carry state and brightness on separate topics and cannot express colour,
so color_light's mqtt binding publishes ONE retained payload with state +
brightness + hs colour and parses commands with cJSON (no value_template —
jinja braces collide with the slot engine). Driver hue/sat are Matter units
(0-254); both conversion directions round, or HA sends h240/s80 and gets
back h239/s79. VERIFIED on the emu+mosquitto rig: discovery, retained
state, and a four-command round-trip (colour, brightness-only, colour-only,
off) all exact. Uses pwm_rgb — ws2812_strip crashes esp-emu in
rmt_tx_mark_eof (RMT gap, for the emu team).

`espnow-remote` + `espnow-light` — devices that control each other DIRECTLY:
no router, no hub, no app, nothing to pair. The wire carries an FNV-1a hash
of the param NAME (zc-now/1, 14-byte broadcast, channel 1, group filter), so
separately built products interoperate when both sides bind the same names —
the remote's level_param and the light's brightness_param are both
APP_DRIVER_PARAM_BRIGHTNESS on purpose. Controllers get their third home:
dimmer_remote + scene_button broadcast (espnow_driver_cb_cases); light /
dimmable_light / smart_plug listen (espnow_recv_cases). v1 trust model is
open broadcast for trusted spaces — pairing + per-peer LMK is v2. Blocked
with router-bound transports (channel coherence). Emu-boots clean.

`webui-smart-plug` — the smart_plug webui binding, added after the first
live webui run rendered an EMPTY s_entities[] (smart_plug had no
webui_entities slot) and code_writing hand-replaced the whole generated
component. The engine now THROWS on an empty manifest rather than
emitting a control page with no controls.

`audio-doorbell` — first `frameworks: [sound]` product (the Audio
framework: the speaker half of the Voice/Audio split; id `sound` because
Voice holds `audio` historically). Press the button, the speaker plays a
two-tone chime: sound_event_cases on device types map param changes to
tone patterns; app_sound sequences them through the existing speaker_i2s
tone driver. EMU: esp-emu does not drain I2S TX, so ALL speaker products
(incl. the framework-less sound-monitor-chime) hang emu boot — gated via
emulatorSupported, filed with the emu team.

`agent-room-companion` — first `frameworks: [agents]` product: the device
is an ESP Private Agents client (TEXT mode v1 — websocket + local tools;
no esp-sr). Device types contribute agents_statics handlers +
agents_tool_register rows against the param bus. Console onboarding
(agent-wifi / agent-id / agent-token); the SDK's `agent` component is a
git-PINNED idf_component.yml dependency (repo has no tags). Carries a
generator workaround for an upstream NULL-deref in esp_agent_init (text
mode with NULL audio configs crashes; dummy configs passed).

`webui-room-node` — first `frameworks: [webui]` product: the device serves
its OWN control page. Standalone SoftAP ("ZeroCode-<id>", password
"zerocode") at http://192.168.4.1 — or shared-net when a Wi-Fi transport
is co-selected (mDNS zerocode-<id>.local either way). Device types
contribute `webui_entities` manifest rows; one embedded plain-ASCII page
renders them (toggle/slider/value/binary) against GET /api/state +
GET /api/set, params addressed BY NAME. esp32c3, dimmable pwm light +
DHT22 temperature.

### BLE HID (1)
`ble-media-remote` — first `frameworks: [ble_hid]` product, and the first
built ON the controller device types every other framework leaves orphaned:
encoder deltas → volume up/down, button toggle → mute, sent to the paired
phone/PC/TV as standard HID consumer-control (report id 1). NimBLE (matches
base_firmware's host — no Bluedroid switch to fight Matter), esp_hid over
GATT; needs CONFIG_BT_NIMBLE_HID_SERVICE=y (the framework block carries it).
GAP/security boilerplate lives in the component's ble_hid_gap.c (BLE-only,
adapted from the IDF esp_hid_device example); pairing is Just Works, bonds
persist, re-advertises on disconnect. EMULATOR-verified: NimBLE up and
LE_Set_Advertising_Enable = 1 on a virtual esp32c3.

### Vision (1)
`camera-motion-sensor` — first `frameworks: [vision]` product: frame-diff
motion detection on the AI-Thinker ESP32-CAM, on-board red LED as the
indicator, occupancy on the param bus. Privacy by construction: grayscale
QQVGA frames are diffed in DRAM and discarded — no storage, no transmission,
no network stack in the product. Detectors live in DRIVER blocks
(camera_motion_dvp) reporting through the generated zc_vision_emit(event,
value) hook; the framework is just the event spine, so esp-who person/face
detection later = a sibling detector block with its own chip/partition
story, zero generator change. Chips: esp32 + esp32s3 (DVP interface).

## Model catalogue (`models/`)

`models/<id>.yml` describes every model `frameworks/ml` can run — where the
bytes come from, what shape the tensors are, which op profile it needs, and
what tensor arena it costs. Emitted into `library.json` as a top-level
`models[]` array, so the AI can size a product WITHOUT a build-fail cycle.

| model | bytes | consumed by | note |
|---|---|---|---|
| `hello_world` | 2 488 | `drivers/ml_classifier` | vendored SMOKE TEST — output is meaningless |
| `person_detect` | 300 568 | `drivers/ml_person_detect` | 96x96 grayscale presence |
| `micro_speech` | 18 800 | — | needs a second front-end model whose ops sit outside every profile |
| `mobilenet_v3` | 2 879 832 | — | `blocked_by: oversized_for_ota, needs_new_partition_table, ops_outside_profiles` |

Two fields carry the weight. `arena` is what the model actually needs, split
into `base_bytes` and `scratch_bytes` (the extra esp-nn's optimized kernels
want on esp32s3/p4), with a `source:` of `upstream-declared` or `measured` so
nobody mistakes a quoted number for a benchmarked one —
`scripts/bench-arena.sh` flips it. `blocked_by` says why a listed model cannot
be chosen, because an unexplained absence invites the same question forever.

`source.kind` is `component | vendored | user`; the third value is where
bring-your-own-model will land without a schema change.

### Machine learning — on-device (2)
`ml-inference-demo` — first `frameworks: [ml]` product: on-device machine-
learning inference driving the param bus, with Espressif's esp-nn
supplying the optimized kernels. Same split as vision — the framework owns the
interpreter (PREPARE / INVOKE / POST-PROCESS in app_tflite.h — the framework
id is the CAPABILITY, `ml`; TensorFlow Lite Micro is the implementation it
hides, and a second backend would live behind the same id) and the driver
block owns the model, the input feed and the param bus. Ships the TFLM
reference model (2.5 KB, one FullyConnected graph) on zeroed input, so the
reading is meaningless by construction: what it proves is that the interpreter
builds, allocates, invokes and reaches the param bus. Real products override
the weak `<prefix_lc>_fill_input()` hook and point `model_symbol` at their own.

`person-detect-camera` — the first product whose verdict comes from a REAL
trained model: `drivers/ml_person_detect` runs the upstream TensorFlow
person-detection CNN on 96x96 grayscale frames and publishes presence +
confidence to the param bus, bound to a Matter occupancy sensor. The model is
NOT vendored: it is 1.85 MB of hex, seven times the catalog tarball, and
esp-tflite-micro (which every `ml` product already depends on) ships it at
examples/person_detection — so frameworks/ml's component CMakeLists compiles
it from there and the catalog carries a reference. Apache-2.0, © The
TensorFlow Authors. Chips: esp32 + esp32s3 (DVP camera), same constraint as
camera-motion-sensor. Verified: esp32 build 267s, 1689 KB image with the
293 KB model linked in.

Deliberately carries NO connectivity framework — `ci-affected.sh` picks this
product whenever a TFLite block changes, and an earlier `frameworks: [matter]`
draft made that pull esp_matter on every such PR. `device_types/occupancy_sensor`
stays for the param wiring; its slots are all framework-specific, so it emits no
code until a framework is added.

Reach for this framework when **esp-dl cannot be used** — a target outside
esp-dl's ESP32/S3/P4 operator support, an operator esp-dl does not implement, a
model that will not export to ONNX for esp-ppq, or a `.tflite` you were handed
and cannot retrain. esp-dl is the first choice everywhere else.

Chips: unrestricted. esp-tflite-micro builds on any target with IDF >= 5.1 and
esp-nn picks kernels per chip — assembly on esp32s3 (vector) and esp32p4
(PIE/QACC), generic C on esp32 and esp32c3, reference kernels elsewhere. Built
and verified on esp32c6, esp32s3 and esp32p4 (IDF v6.0.2).

### Display (2)
`touch-room-panel` — first `frameworks: [display]` product: LVGL touchscreen
UI (ILI9341 SPI panel + FT5x06 capacitive touch), a touchable light card + a
live temperature card. Fully local, no network stack. The binding is
inverted like audio's: display_widget_create = "how does this device SHOW
itself"; the generated driver_cb centralizes lvgl_port_lock (slot code must
never lock — nested would deadlock). Panel/touch pins live in DRIVER blocks
(display_ili9341_spi, display_touch_ft5x06) because framework blocks carry
no cfg. Pin defaults target esp32s3-class boards (11 GPIOs don't fit c3
cleanly). **44 of 53 device types carry `display_*` bindings** — toggle cards,
brightness/speed/position/setpoint sliders, live value labels (units per
type), binary status labels (LEAK!/Dry, SMOKE!/OK), appliance mode
dropdowns. The 9 without are `# Display-absent` (controllers: the screen IS
the controller; no-param types).
C-macro trap: ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG()'s designator order is
C-legal but rejected by C++ — fill the struct field-by-field.

`mosaico-room-panel` — the ESP-Mosaico (esp32s31) as a [matter, display]
room panel using only onboard hardware. Two new driver blocks carry the
board: `display_co5300_qspi` (QSPI panel — quad data lines, no D/C, 32-bit
cmd phase, brightness over the command channel so no backlight GPIO) and
`display_touch_cst9220` (Hynitron CST92xx via waveshare/esp_lcd_touch_cst9217,
16-bit register addressing, addr 0x5A on the board's shared I2C bus 0/1).
Same field-by-field C++ macro discipline as the FT5x06 block.

### MQTT (1)
`mqtt-room-node` — first `frameworks: [mqtt]` product: light + DHT22
temperature over Home Assistant MQTT discovery. Wi-Fi + broker configured on
the serial console (`mqtt-wifi`, `mqtt-broker`), persisted in NVS;
unconfigured boot idles and logs, never blocks app_main. esp-mqtt + esp_wifi
ship with IDF — no managed components. Every Wi-Fi chip (not esp32h2); on
esp32p4 the Wi-Fi is the companion radio's, reached over esp_wifi_remote.
**44 of 53 device types carry `mqtt_*` bindings** — full HA coverage:
switch/outlet, light (brightness + color-temp), sensor (temperature,
humidity, power, energy, air quality…), binary_sensor (motion, contact,
leak, freeze, smoke/CO), fan, cover, climate (thermostat heat / AC cool),
lock (LOCK+UNLOCK — unlike voice, HA is an authenticated controller),
number (setpoints), select (appliance modes), and **device_triggers** for
the input controllers (scene_button: press; dimmer_remote: up/down/toggle —
events for HA automations, not stateful entities). Every entity carries a
`device` object, so a node's entities group under ONE HA device. The 9
without are `# MQTT-absent` with reasons (4 remaining controllers, 2 with
no params, device_energy_management). Full key names in discovery JSON,
plain payload values, never value_template. First framework the EMULATOR verifies end-to-end
(esp-emu --net user; boot + unconfigured path proven on a virtual esp32c3).

### Display panels (2)
`drivers/display_ili9341_spi` — 240x320 SPI TFT, the ubiquitous cheap panel;
partial buffers in internal RAM, so it works on every chip.
`drivers/display_ek79007_mipi` — **1024x600 MIPI-DSI**, the 7" screen on the
ESP32-P4-Function-EV-Board. esp32p4 only (nothing else has a DSI host). Claims
NO GPIOs — the D-PHY pads are dedicated — and turns PSRAM on itself, because a
frame is 1.2 MB against 768 KB of SRAM. BUILD-verified on esp32p4; not yet
brought up on a physical panel (that board's reset + backlight sit behind a
TCA9554 expander, and its GT911 touch controller has no block yet).
Product: `p4-mipi-panel`.

### Board samples (5 — the only products carrying `board.yaml`)
The CI exercisers of the esp-board-manager path (CLAUDE.md → Boards):
- `mosaico-bmgr-probe` — board path with NO external parts.
- `c6-devkit-board-relay` — the AMEND path: one externally wired relay.
- `mosaico-room-panel-board` — the board CONTRIBUTES the display (no panel
  block at all); board twin of `mosaico-room-panel`.
- `c6-devkit-plug` — the ADAPTER sample: the devkit's own BOOT button
  (`drivers/board_button`) and WS2812 (`drivers/board_led_strip`, an
  observer-style indicator) plus an external relay via the amend. It is the
  fix the validator names when smart-plug's GPIO 8/9 collide with this board.
- `mosaico-pinned-panel-board` — the STEP-ASIDE sample: hand-pinned CO5300 +
  CST9220 blocks on the board that already carries both; the blocks step
  aside, and the validator (sharing the generator's `boardTakesOver`
  predicate) does not count their pins as conflicts.

### BLE Mesh (2 — a KIT)
`ble-mesh-light`, `ble-mesh-switch` — the first `frameworks: [ble_mesh]`
products, and a pair: the switch provisions the light. Both build on esp32 /
esp32c3 / esp32c5 / esp32c6 / esp32h2 / esp32s3 (the framework's `chips:`
list, one real build each).

The framework (`frameworks/ble_mesh`) generates a whole Generic OnOff/Level
node — a Config Server and Client, OnOff/Level servers and clients each with
their own publication context, PB-ADV + PB-GATT provisioning under a device
UUID whose first two bytes are a fixed marker, `CONFIG_BLE_MESH_SETTINGS` so
keys and bindings survive a reboot, and a `mesh` console command
(status | join | reset | on | off). Commands leave through the CLIENT model
with a full message context and an incrementing TID; "am I provisioned" is
always `esp_ble_mesh_node_is_provisioned()`, never a local flag.

4 device_type blocks carry `ble_mesh_*` bindings so far: `light` and
`dimmable_light` bind the Generic servers to their power/brightness params
(Generic Level's signed 16-bit range and the driver's 0-254 share one
conversion, used both ways), and `dimmer_remote` and `scene_button` publish
through the clients to the shared group `0xC000` (`ZC_MESH_GROUP_ADDR`).

`behaviors/ble_mesh_provisioner` turns one device into the network's
provisioner: it matches unprovisioned beacons on the marker, adds them, then
drives the Config Client through adding the application key, binding it to the
node's OnOff/Level SERVERS **and CLIENTS**, subscribing the servers to the
group and pointing the clients' publication at it — advancing past a step a
node refuses rather than stalling the rest. Params: `group_addr`, `app_key`
(32 hex characters; the default is a development key), `max_nodes`.

### Zigbee (1)
`zigbee-light` — first `frameworks: [zigbee]` product (any 802.15.4 SoC;
esp-zigbee-lib 2.0.x router, joins hubs via network steering;
NVRAM in default `nvs`).

44 of 53 device_type blocks carry `zigbee_*` bindings — ZCL server clusters
(On/Off, Level, Color, Thermostat, Fan Control, measurement clusters, Door
Lock, Window Covering, Multistate, Electrical Measurement), IAS Zone for
the five boolean detectors (per-detector zone types, status-change
notifications + ZoneStatus attr, enroll-response handled), five CONTROLLER
bindings (scene_button, dimmer_remote, color_remote, thermostat_controller,
pump_controller: client clusters sending commands to Finding & Binding
targets — `zb-bind` console cmd / `app_zigbee_start_finding_binding()`;
board-verified via `zigbee-remote`), and Zigbee OTA via `behaviors/ota_basic`
(OTA Upgrade client endpoint 80 → inactive esp_ota partition, board-verified
boot). Not bound, documented in each block.yml: control_bridge
(gateway-scale), 6 appliances (no ZCL equivalent), 2 param-less stubs.
Door lock / window covering are state-reporting only until blocks use the
`zigbee_action_cases` command surface.

---

## Universal baseline behaviors

Every product gets the following blocks via its `product.yml`, on top of
its product-specific drivers/device types:

**Diagnostics + lifecycle**
- `behaviors/boot_reason_logger` — logs `esp_reset_reason()` at boot
- `behaviors/boot_count` — NVS-persisted boot counter
- `behaviors/espnow_pair_trigger` — opens the ESP-NOW pairing window when a bus param fires (bind to a button); ESP-NOW products only
- `behaviors/task_watchdog` — TWDT enabled, 10s timeout, idle subscribed
- `behaviors/uptime_log` — periodic uptime + free heap log
- `behaviors/network_status_log` — Wi-Fi / IP transition logging
- `behaviors/sntp_time` — wall clock: timezone + SNTP, optional bus bool on first sync; coexists with RainMaker's own SNTP (observes it)
- `behaviors/wifi_disconnect_recovery` — log a Wi-Fi outage; reboot after N seconds offline only when `reboot_after_seconds > 0` (opt-in)

**Status LED + identify**
- `drivers/status_led` — LED hardware (default GPIO 15)
- `behaviors/blink_pattern` — pattern driver
- `behaviors/commissioning_led` — commissioning state → pattern
- `behaviors/identify_blink` — Matter Identify cluster → pattern

**State preservation**
- `behaviors/ota_basic` — Matter OTA Requestor
- `behaviors/factory_reset_power_cycle` — 5 rapid power cycles → wipe NVS
- `behaviors/factory_reset_long_press` (where button exists) — long-hold reset
- `behaviors/nvs_persist_<type>` per stateful actuator param

**Console diagnostics** (5 universal + per-param set/get)
- `behaviors/console_uptime` — `uptime`
- `behaviors/console_chip_info` — `chip` (model, revision, MAC, features)
- `behaviors/console_version` — `version` (firmware + IDF)
- `behaviors/console_log_level` — `log <tag> <NONE|...|VERBOSE>`
- `behaviors/console_heap_stats` — `heap` (per-region detail)
- `behaviors/console_param_cmd` per stateful param — `<name> <value>` (bench override on the bus; actuator/setpoint params, not sensor params)
- `behaviors/console_param_get` per stateful param — `get-<name>`

The console base also ships 3 always-on built-ins (not blocks):
`reboot`, `factory_reset`, `mem`.

---

## Sdkconfig fragment blocks (11)

`product.yml` selects via `sdkconfig_fragments: [name, ...]`.
Each fragment is a block under `templates/code_blocks/sdkconfig-fragments/<name>/block.yml`
with a `sdkconfig:` map.

### Connectivity choices
| Fragment | Description |
|---|---|
| `matter_thread` | Matter over Thread (802.15.4) instead of Wi-Fi. esp32c5 / esp32c6 / esp32h2. |
| `c6_thread_only` | Superseded by `matter_thread`; no product uses it. Its `CONFIG_ESP_WIFI_ENABLED="n"` is inert — that symbol is promptless in IDF 6.x and cannot be set from sdkconfig. |

### Build modes
| Fragment | Description |
|---|---|
| `production` | Release flags: bootloader compression, app rollback + anti-rollback, no log colors. **Production-phase only** — anti-rollback writes the secure_version eFuse. |
| `no_app_logs` | Strip `ESP_LOGI` / `LOGW` / `LOGE` from app code. Saves 20–50 KB. |

### Security hardening — production-phase only
Touch eFuses; **irreversible**. Not applied during dev. A future
*Production Considerations* agent picks the right combination per device.

| Fragment | Description |
|---|---|
| `secure_boot` | Secure Boot v2 — bootloader verifies app signature against eFuse-burned key. ONE-WAY. |
| `flash_encryption` | Flash Encryption (release mode) — bootloader + app encrypted on flash. ONE-WAY. Combine with secure_boot. |
| `nvs_encryption` | Encrypt NVS partition (Wi-Fi creds, fabric keys, persisted params). Pair with flash_encryption. |

### Power profiles
| Fragment | Description |
|---|---|
| `power_management` | IDF PM + tickless idle + Wi-Fi sleep IRAM. ~50-70% idle power reduction. |
| `wifi_min_memory` | Reduce Wi-Fi RX/TX buffer pools. ~10-15 KB RAM saved at cost of latency. |

### Diagnostics & I/O
| Fragment | Description |
|---|---|
| `coredump` | ELF core-dump-to-flash on crash. Needs a `coredump` partition. |
| `usb_serial_console` | Route console + logs to USB-Serial-JTAG (c3/c6/s3/h2). |

---

## Partition table blocks (5)

Selected per-product via `partition_table: <name>`. Default 4 MB layout
copied from `firmware/base_firmware/partitions.csv` when omitted.

| Name | Flash | OTA slots | Use case |
|---|---|---|---|
| `2mb` | 2 MB | none | esp32c2 / 2MB-only chips, smallest products |
| `4mb` | 4 MB | 2 × 1.875 MB | Default for most products |
| `8mb` | 8 MB | 2 × 3 MB | Rich production builds (heavy Matter + many behaviors) |
| `8mb-voice` | 8 MB | 2 × 2.25 MB | Voice — carries the `model` partition esp-sr needs |
| `16mb-voice` | 16 MB | 2 × 3.5 MB | Voice + a transport, and voice on the S31 |

Each table ships the `CONFIG_ESPTOOLPY_FLASHSIZE_*` that matches its layout, so
choosing the table is all a product has to do. (Before 2026-08-24 the plain
`8mb` one did not, and could not build on any chip — the per-chip defaults all
set 4 MB and won.)

---

## Layered sdkconfig precedence (last writer wins)

```
firmware/base_firmware/sdkconfig.defaults          ← base universal defaults
firmware/base_firmware/sdkconfig.defaults.<chip>   ← per-chip — hardware reality
matterProfile.sdkconfigDefaults                    ← framework — Matter optimizations + cluster excludes
block.sdkconfig (every code_block)                 ← per-block contributions
sdkconfig_fragments: [...]                         ← opt-in named modes
extra_sdkconfig: { ... }                           ← per-product final overrides
```

**Per-chip files vs sdkconfig fragments — they coexist with different
roles.** Per-chip files describe what the chip *is* (esp32h2 has no
Wi-Fi; esp32c2 has 2 MB flash; esp32p4 has no radio at all and borrows
one from a companion chip). Fragments are *choices* the product
makes (Thread vs Wi-Fi on c5/c6, power profile, dev vs production).

## Dev phase vs production phase

Product manifests describe the **functional** firmware: drivers,
device types, behaviors, partition layout. They do not enable production
hardening (`secure_boot`, `flash_encryption`, `nvs_encryption`,
`production`-anti-rollback) — those write to one-way eFuses and belong
to a separate **Production Considerations** agent phase that runs
after the device is functionally validated. The fragments stay
available as reference for that agent to discover and apply.

---

## Browsing the catalog

Block descriptions, params, and slot contributions live inline in each
`block.yml`. Same for product compositions in each `product.yml`.

```bash
# List everything
find firmware/templates/code_blocks -name block.yml | sort
find firmware/templates/product_configurations -name product.yml | sort

# Read one
cat firmware/templates/code_blocks/drivers/relay/block.yml
cat firmware/templates/product_configurations/smart-plug/product.yml
```

Agents browse the same catalog through the `list_templates`,
`search_templates`, and `get_template` tools (backed by
`firmware/services/src/templates/`).

The validator catches missing producer/consumer relationships across
blocks (param-type conflicts, dangling refs):

```bash
pnpm exec tsx scripts/validate-templates.ts --strict
```

## Known gaps

- **More real I2C sensor drivers** (already have sht4x/scd4x/aht21/bh1750/ina219/ds18b20).
  Still missing: bme280 (temp+humidity+pressure), tsl2591 (extended lux),
  mlx90614 (IR thermometer), scd30 (older NDIR CO2).
- **Capacitive touch button** — needs chip-conditional code (touch_pad
  API isn't on C3/C6/H2). Open design question for chip-conditional blocks.
- **OLED display** — basic ssd1306 block exists; rich graphics layer (lvgl/u8g2) not done.
- **Seven-segment displays** — TM1637 / MAX7219 drivers.
- **GPIO expanders** — PCF8574 / MCP23017 for products needing >chip-GPIO-count loads.
- **Binding command emitters** — `scene-button`, `smart-remote`, `rotary-dial`
  expose controller endpoints but emit no commands. Need
  `behaviors/emit_onoff_toggle`, `behaviors/emit_level_step`,
  `behaviors/emit_color_step`.
- **Matter 1.5/1.6 device types** — `soil_moisture_sensor`, cameras. NO LONGER
  BLOCKED: esp_matter is 1.6.0 (spec 1.6) since the ESP-IDF 6 move. This is now
  work someone can just do.
- **ESP-Mosaico, remaining** — every peripheral in the user guide's pin table is
  wired except the 1 Gbit SPI NAND (no block exposes flash storage yet) and the
  camera on the 2×10P module headers (`drivers/camera_motion_v4l2` builds; the
  pins depend on which module is attached).
- **BMI270 gyro** — the accelerometer path needs no firmware blob and covers
  tilt + motion. Gyro rates would need Bosch's licensed 8 KB config blob, i.e.
  pulling in their driver as a managed component.
- **Hardware verification** — every ESP-Mosaico block (CO5300, CST9220, ES8311,
  BMI270, BMM150, BQ27220, haptics) is BUILD-verified only. They need a real
  board.

## How it's invoked

- **CLI / CI:** `scripts/generate-template.ts <product-id>` — thin
  wrapper around the service; used by `ci-build-products.sh` and
  `smoke-build-products.sh`.
- **Agent:** the `product_manager` agent picks (or composes) a
  product_configuration via the `list_templates` / `search_templates` /
  `get_template` tools, then calls `apply_template`. The host
  (backend/CLI) pre-binds the output directory; the agent doesn't pass
  a path.

Both paths go through the same `TemplateService` in
`firmware/services/src/templates/`.
