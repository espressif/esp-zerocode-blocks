# coreMQTT — fetched, not vendored

This directory carries **no third-party source**. `CMakeLists.txt` fetches
`espressif/esp-aws-iot` at a pinned commit and registers coreMQTT's sources and
its esp-tls `network_transport` port directly.

| file | origin | why it is here |
|---|---|---|
| `CMakeLists.txt` | ZeroCode AI, Apache-2.0 | upstream's own reaches outside its component directory — see the comment at the top of the file |
| `Kconfig` | esp-aws-iot, verbatim | `core_mqtt_config.h` dereferences `CONFIG_MQTT_*`, and IDF collects component `Kconfig` files **before** any component `CMakeLists.txt` runs, so the fetch cannot supply it in time |

Upstream: <https://github.com/espressif/esp-aws-iot>, commit
`9c879feedb699611e7c220b93aed84c84322ff84` (2026-07-31). The coreMQTT submodule
inside it is FreeRTOS coreMQTT **2.3.1**. Both licences travel with the fetched
checkout.

## Where the source lands

`../third_party/esp-aws-iot`, a **sibling** of the firmware tree — `apply_template`
deletes and recreates the firmware directory, so a checkout inside it would not
survive a re-scaffold. `base_firmware/CMakeLists.txt` already treats
`../third_party` as the home for fetched source.

## Building against a pre-cloned tree

```bash
export ZC_AWS_IOT_SRC=/path/to/esp-aws-iot     # must be at the pinned commit
idf.py build
```

**It must be the environment variable, not `-DZC_AWS_IOT_SRC=...`.** IDF
processes component `CMakeLists.txt` in two passes and the early one does not
see `-D`. With `-D` the first pass still fetches and only the second pass honours
the override: the build succeeds while silently re-cloning on every run. The
environment reaches both passes. Verified on ESP-IDF 6.x — with the environment
variable set, a clean build performs zero fetches and creates no `third_party`.

## Bumping the pin

Change `ZC_AWS_IOT_PIN` in `CMakeLists.txt`, delete any existing
`third_party/esp-aws-iot`, and re-check this README and `../../idf_component.yml`
— the git-pinned libraries there must name the **same** commit, or the fetched
coreMQTT and the managed libraries drift apart.
