#define {{prefix}}_CAM_PWDN   {{cfg.pin_pwdn}}
#define {{prefix}}_CAM_RESET  {{cfg.pin_reset}}
#define {{prefix}}_CAM_XCLK   {{cfg.pin_xclk}}
#define {{prefix}}_CAM_SDA    {{cfg.pin_sda}}
#define {{prefix}}_CAM_SCL    {{cfg.pin_scl}}
#define {{prefix}}_CAM_D7     {{cfg.pin_d7}}
#define {{prefix}}_CAM_D6     {{cfg.pin_d6}}
#define {{prefix}}_CAM_D5     {{cfg.pin_d5}}
#define {{prefix}}_CAM_D4     {{cfg.pin_d4}}
#define {{prefix}}_CAM_D3     {{cfg.pin_d3}}
#define {{prefix}}_CAM_D2     {{cfg.pin_d2}}
#define {{prefix}}_CAM_D1     {{cfg.pin_d1}}
#define {{prefix}}_CAM_D0     {{cfg.pin_d0}}
#define {{prefix}}_CAM_VSYNC  {{cfg.pin_vsync}}
#define {{prefix}}_CAM_DE     {{cfg.pin_de}}
#define {{prefix}}_CAM_PCLK   {{cfg.pin_pclk}}
#define {{prefix}}_CAM_XCLK_HZ {{cfg.xclk_hz}}
#define {{prefix}}_CAM_SCCB_HZ {{cfg.sccb_hz}}
#define {{prefix}}_CAM_PX_THRESH {{cfg.pixel_threshold}}
#define {{prefix}}_CAM_AREA_PCT  {{cfg.area_threshold_pct}}
#define {{prefix}}_CAM_HOLD_MS   {{cfg.hold_ms}}
#define {{prefix}}_CAM_POLL_MS   {{cfg.poll_ms}}
/* Diff budget: QQVGA. Asked of the sensor as GREY; a sensor that only offers
   a larger or a colour format is sub-sampled to this many luma samples. */
#define {{prefix}}_CAM_W 160
#define {{prefix}}_CAM_H 120
/* How many capture buffers the driver keeps queued. Two is the minimum that
   lets the sensor fill one while we diff the other. */
#define {{prefix}}_CAM_BUFS 2
