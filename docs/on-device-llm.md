# Small language models on ESP32

**Verdict: this works today, on hardware we already support, at interactive
speed.** `esp-tinylm` runs a coherent language model on an ESP32-S31 at
**26.7 tok/s**, and a natural-language → JSON tool-call parser at **88.8%
exact match** from a **270 KB** model — smaller than the person-detection CNN
this catalogue already ships.

Reference implementation: **esp-tinylm** (Vikram Dattu, Espressif — vendored
under `code_blocks/frameworks/lm/components/esp_tinylm/` until it is published
as a component). Everything below is measured there, not estimated here.

## The measured numbers

| example | model | speed | quality |
|---|---|---|---|
| `story_generation` | `story.bin` — 2 094 512 B | **26.7 tok/s** (S31), 17.6 tok/s (S3) | TinyStories-quality prose: characters, plot, a moral |
| `nlu_tool_calling` | `nlu.bin` — **270 272 B** | — | **88.8%** exact-match command → JSON |
| `llama2_dense_reference` | Karpathy stories15M | bandwidth-bound | the baseline PLE is measured against |

Cost, from the examples' own `sdkconfig.defaults`: **16 MB flash, PSRAM
required**, model embedded with `EMBED_FILES` into a 4 MB factory partition.
Targets: **ESP32-S31, ESP32-S3, ESP32-P4** — S31 hardware-validated, P4 PIE
path build-verified and awaiting a hardware run.

## Why it works when the obvious approach does not

Two ideas, neither of which involves TFLite Micro or esp-dl.

**Per-Layer Embeddings (PLE)**, re-implemented clean-room from Google's
Gemma 3n description. A dense 15M model has to stream ~7.5 MB per token and is
bandwidth-bound on an MCU. PLE splits the model: a **dim-96 compute core of
~0.7 MB** does the work, while the model's *knowledge* sits in a big
`ple_table` memory-mapped from flash and touched **~6 rows per token** — near
zero bandwidth. Small to stream, and still coherent.

**Hand-written vector assembly** for the int4×int8 matmul, per chip:
- **S31 / P4** — PIE (`xespv`): `esp.vld.128`, in-register nibble unpack,
  `esp.vmulas.s8.xacc` (~3× over scalar)
- **S3** — Espressif SIMD/Vector Extension: `ee.vld.128`, `ee.vsr.32` unpack,
  `ee.vmulas.s8.accx` (2.1× over scalar)
- everything else — portable scalar fallback

PLE makes the model small to stream; the assembly makes each token fast to
compute. Neither alone is enough.

## Where this document was previously wrong

An earlier revision concluded "not possible on-device, and not close". It was
wrong in two specific ways, recorded so the reasoning is not repeated:

1. **It assumed TFLM and esp-dl were the only paths.** "Neither runtime has an
   attention operator, and TFLM's static shapes forbid a decode loop" is *true*
   and *irrelevant* — `esp-tinylm` implements the transformer directly in C and
   assembly, manages its own KV cache, and never touches either runtime. The
   runtime gap was never the binding constraint.
2. **It costed the wrong model.** It anchored on a 1B-parameter model at
   ~1000 MB and concluded a ~1000× gap. The models that matter here are three
   to four orders of magnitude smaller, and PLE shrinks the *streamed* working
   set again on top of that.

The general lesson: for on-device ML, "the runtime does not support it" is a
statement about the runtime, not about the silicon.

## What this means for `frameworks/ml`

`esp_tinylm` is already **an IDF component with a clean API** —
`esp_tinylm_load()`, `esp_tinylm_forward()`, `esp_tinylm_get_info()`, an opaque
handle, `esp_err_t` returns, and automatic per-target kernel selection. That is
the same shape `frameworks/ml` wraps for TFLM, so the integration path is
short:

- A **framework block** (or a second backend inside `frameworks/ml`) owning the
  model handle and the token loop, exactly as `app_tflite` owns the interpreter
  and arena today.
- **Driver blocks** for the two proven use cases — the tool-call parser is the
  more valuable of the pair, because it turns an utterance into a structured
  action that can drive the param bus directly, with no cloud round trip.
- **Model manifests** in `models/`, with `source.kind: component` pointing at
  `esp_tinylm`, reusing the schema already in place. The fields differ from a
  CNN's — vocab, context length and tok/s replace input geometry and arena —
  so the schema needs a `kind: lm` variant.

The constraints to encode: **16 MB flash, PSRAM, S31/S3/P4 only**, and models
that link into the app image rather than a data partition — so the same
OTA-slot pressure that blocks `mobilenet_v3` applies, and the 2 MB story model
would need a partition table this catalogue does not yet have. The **270 KB
NLU model has no such problem** and is the obvious first block.

## What is still true

Cloud-backed language interaction via `frameworks/agents` (esp-agents-firmware)
remains the right choice when the device has connectivity and the task needs a
frontier model. On-device `esp-tinylm` is the right choice when it must work
offline, privately, or without latency — and, at 270 KB for tool calling, it is
now cheap enough that "offline" is a design choice rather than a sacrifice.

---
*Written 2026-09 against esp-tinylm @ llama2-s31, esp-dl 3.3.1,
esp-tflite-micro 1.4.0, esp-nn 1.3.1.*
