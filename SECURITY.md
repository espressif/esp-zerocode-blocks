# Security

The code in this repository is generated INTO firmware that runs on real
devices, so a defect here can reach many products at once.

Please do **not** open a public issue for a security problem. Report it
through Espressif's coordinated disclosure process instead —
<https://www.espressif.com/en/support/security> — and we will acknowledge the
report and follow up privately.

Two properties every generated image is built to keep, and which a change
must not weaken:

- `CONFIG_EFUSE_VIRTUAL=y` stays in `base_firmware/sdkconfig.defaults`, so a
  generated image can never permanently burn a one-time-programmable eFuse.
- Actuators are born safe: a driver's idle level is asserted before any bus
  value can reach it.
