#define {{prefix}}_SPK_PORT        {{cfg.port}}
#define {{prefix}}_SPK_MCLK_GPIO   {{cfg.mclk_gpio}}
#define {{prefix}}_SPK_BCLK_GPIO   {{cfg.bclk_gpio}}
#define {{prefix}}_SPK_WS_GPIO     {{cfg.ws_gpio}}
#define {{prefix}}_SPK_DOUT_GPIO   {{cfg.dout_gpio}}
#define {{prefix}}_SPK_PA_GPIO     {{cfg.pa_ctrl_gpio}}
#define {{prefix}}_SPK_PA_ACTIVE   {{cfg.pa_active_level}}
#define {{prefix}}_SPK_CODEC_PW    {{cfg.codec_pw_gpio}}
#define {{prefix}}_SPK_I2C_PORT    {{cfg.i2c_port}}
#define {{prefix}}_SPK_I2C_ADDR    {{cfg.i2c_addr}}
#define {{prefix}}_SPK_SAMPLE_RATE {{cfg.sample_rate}}
/* Samples per i2s_channel_write. 256 @16 kHz = 16 ms of audio per write,
 * which bounds how fast a tone/mute change takes effect. */
#define {{prefix}}_SPK_CHUNK       256
