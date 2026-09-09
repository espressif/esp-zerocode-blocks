---
name: new-framework
description: Guide a contributor through adding a new framework — a whole subsystem products compose (a connectivity stack, an audio/video pipeline, a display stack). Spans this repo and possibly the ZeroCode AI repo. Use when someone wants to add e.g. an audio framework, a display stack, or a new protocol stack.
---

# Add a new framework

A framework is the biggest kind of block: a component subtree + init slots
that products opt into via `frameworks: [<id>]`. Connectivity stacks
(matter, rainmaker, zigbee, ble_mesh) are frameworks; audio/video/display
stacks are added the same way.

## Steps

1. **Start from the closest existing framework**:
   ```bash
   cp -r code_blocks/frameworks/rainmaker code_blocks/frameworks/<id>
   ```
   The shape: `block.yml` (description + `main_includes`/`main_init`
   slots), `components/app_<id>/` (the framework's component source),
   sidecars (`idf_component.yml` deps, `sdkconfig.defaults`,
   `requires.cmake`).

2. **Self-contained vs binding framework**:
   - Self-contained (init + own component, e.g. an audio pipeline the
     product configures via params) → **zero engine change needed**.
   - If device types must contribute per-device code into it (like the
     `matter_*` slot family) → the engine needs a small codegen addition in
     `engine/src/generator.ts`; the existing matter/rainmaker/zigbee
     handling is the pattern. Loop in the ZeroCode AI team for this part.

3. **Platform side (ZeroCode AI repo — esp-zerocode-ai)**, an PR anyone can
   raise: add a profile in `shared/src/frameworks/profiles.ts` + the
   registry (chip support, incompatibilities) and the option on the create
   form (`frontend-next/src/pages/CreateProduct.tsx`). The AI agents need
   nothing — they are framework-independent.

4. **Verify**: a product that composes the framework, built for real:
   ```bash
   python3 scripts/check.py
   scripts/test-product.sh <product-using-it> --build
   ```

5. **Open the MRs** (this repo; ZeroCode AI repo if engine/UI changed).
   After merge: catalog publish, plus a platform deploy only if engine/UI
   changed — the ZeroCode AI team drives that with you.
