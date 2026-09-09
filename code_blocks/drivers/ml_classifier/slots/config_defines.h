/* {{prefix}} — TFLite Micro classifier */
#define {{prefix}}_TFL_INTERVAL_MS   {{cfg.interval_ms}}
/* Tensor arena. esp-nn's optimized kernels need scratch the generic-C path
 * does not, so the size depends on the CHIP as well as the model — see
 * arena_scratch_kb. A convolutional model wants a non-zero scratch here; the
 * default reference model (one FullyConnected) does not. */
#if CONFIG_NN_OPTIMIZED
#define {{prefix}}_TFL_ARENA_BYTES   (({{cfg.arena_base_kb}} + {{cfg.arena_scratch_kb}}) * 1024)
#else
#define {{prefix}}_TFL_ARENA_BYTES   ({{cfg.arena_base_kb}} * 1024)
#endif
#define {{prefix}}_TFL_THRESHOLD_PCT {{cfg.threshold_pct}}
