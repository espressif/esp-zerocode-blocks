#define {{prefix}}_PD_PWDN     {{cfg.pin_pwdn}}
#define {{prefix}}_PD_RESET    {{cfg.pin_reset}}
#define {{prefix}}_PD_XCLK     {{cfg.pin_xclk}}
#define {{prefix}}_PD_SDA      {{cfg.pin_sda}}
#define {{prefix}}_PD_SCL      {{cfg.pin_scl}}
#define {{prefix}}_PD_D7       {{cfg.pin_d7}}
#define {{prefix}}_PD_D6       {{cfg.pin_d6}}
#define {{prefix}}_PD_D5       {{cfg.pin_d5}}
#define {{prefix}}_PD_D4       {{cfg.pin_d4}}
#define {{prefix}}_PD_D3       {{cfg.pin_d3}}
#define {{prefix}}_PD_D2       {{cfg.pin_d2}}
#define {{prefix}}_PD_D1       {{cfg.pin_d1}}
#define {{prefix}}_PD_D0       {{cfg.pin_d0}}
#define {{prefix}}_PD_VSYNC    {{cfg.pin_vsync}}
#define {{prefix}}_PD_HREF     {{cfg.pin_href}}
#define {{prefix}}_PD_PCLK     {{cfg.pin_pclk}}
#define {{prefix}}_PD_XCLK_HZ  {{cfg.xclk_freq_hz}}
#define {{prefix}}_PD_INTERVAL_MS {{cfg.interval_ms}}
/* Tensor arena, in the shape upstream uses. esp-nn's assembly kernels need a
 * scratch buffer that the generic-C path does not, so the size is a property
 * of the CHIP as well as the model: one scalar cannot be right on both. This
 * is the exact structure of esp-tflite-micro's own person_detection example
 * (kTensorArenaSize = 100*1024 + scratchBufSize). */
#if CONFIG_NN_OPTIMIZED
#define {{prefix}}_PD_ARENA_BYTES (({{cfg.arena_base_kb}} + {{cfg.arena_scratch_kb}}) * 1024)
#else
#define {{prefix}}_PD_ARENA_BYTES ({{cfg.arena_base_kb}} * 1024)
#endif
#define {{prefix}}_PD_THRESHOLD_PCT {{cfg.threshold_pct}}

/* The model's input geometry, which is also the camera frame size — there is
 * deliberately no resize step between the sensor and the tensor. */
#define {{prefix}}_PD_W 96
#define {{prefix}}_PD_H 96
/* Output index of the "person" class in the upstream graph (0 is no-person). */
#define {{prefix}}_PD_PERSON_INDEX 1
